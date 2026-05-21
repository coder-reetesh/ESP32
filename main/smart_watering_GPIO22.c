#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_timer.h"

static const char *TAG = "SMART_PUMP";

#define RELAY_GPIO        22
#define MOISTURE_CHAN     ADC_CHANNEL_7

#define DRY_THRESHOLD     3800
#define WET_THRESHOLD     2300
#define MAX_PUMP_RUNTIME  30000000
#define MIN_PUMP_RUNTIME  2000000

typedef struct {
    int id;
    int raw_adc;
    int64_t timestamp;
} sensor_data_t;

adc_oneshot_unit_handle_t adc1_handle = NULL;
QueueHandle_t dataQueue = NULL;

static uint8_t pump_state = 0;
static int64_t last_on_time = 0;
static int sample_count = 0;

void init_adc(void) {
    ESP_LOGI("SMART_PUMP", "Initializing ADC...");
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc1_handle));

    adc_oneshot_chan_cfg_t chan_config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, MOISTURE_CHAN, &chan_config));
    ESP_LOGI("SMART_PUMP", "ADC ready (GPIO35)");
}

void init_relay(void) {
    ESP_LOGI("SMART_PUMP", "Initializing relay on GPIO%d...", RELAY_GPIO);
    gpio_reset_pin(RELAY_GPIO);
    gpio_set_direction(RELAY_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_drive_capability(RELAY_GPIO, GPIO_DRIVE_CAP_3);

    gpio_set_level(RELAY_GPIO, 1);  // HIGH = pump OFF at startup
    ESP_LOGI("SMART_PUMP", "Relay ready, GPIO%d = %d (pump OFF)", RELAY_GPIO, gpio_get_level(RELAY_GPIO));
}

void pump_on(void) {
    if (pump_state == 0) {
        gpio_set_level(RELAY_GPIO, 0);
        pump_state = 1;
        last_on_time = esp_timer_get_time();
        ESP_LOGW("SMART_PUMP", "PUMP ON  (GPIO%d=%d)", RELAY_GPIO, gpio_get_level(RELAY_GPIO));
    }
}

void pump_off(void) {
    if (pump_state == 1) {
        gpio_set_level(RELAY_GPIO, 1);
        pump_state = 0;
        ESP_LOGI("SMART_PUMP", "PUMP OFF (GPIO%d=%d)", RELAY_GPIO, gpio_get_level(RELAY_GPIO));
    }
}

void moisture_sensor_task(void *pvParameters) {
    int raw_val = 0;
    sensor_data_t outdata;
    ESP_LOGI("SMART_PUMP", "Sensor task running");
    while (1) {
        if (adc_oneshot_read(adc1_handle, MOISTURE_CHAN, &raw_val) == ESP_OK) {
            if (raw_val > 4095) raw_val = 4095;
            if (raw_val < 0)    raw_val = 0;
            outdata.id        = sample_count++;
            outdata.raw_adc   = raw_val;
            outdata.timestamp = esp_timer_get_time();
            const char *s = (raw_val > DRY_THRESHOLD) ? "DRY"
                          : (raw_val < WET_THRESHOLD)  ? "WET" : "MEDIUM";
            ESP_LOGI("SMART_PUMP", "[Sensor] ADC=%d | %s", raw_val, s);
            xQueueSend(dataQueue, &outdata, pdMS_TO_TICKS(1000));
        } else {
            ESP_LOGE("SMART_PUMP", "ADC read failed!");
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

//     GPIO is initialized in app_main BEFORE this task is created.
//     NEVER put gpio_reset_pin/gpio_set_direction here — race condition!
void pump_control_task(void *pvParameters) {
    sensor_data_t inData;
    int64_t now = 0;
    static int wet_count = 0;
    int64_t now = 0;
    
    ESP_LOGI("SMART_PUMP", "Pump task running | DRY>%d | WET<%d", DRY_THRESHOLD, WET_THRESHOLD);
    //ESP_LOGI(TAG, "Pump task running");

   while (1) {
        if (xQueueReceive(dataQueue, &inData, pdMS_TO_TICKS(5000)) == pdPASS) {
            now = esp_timer_get_time();
            if (pump_state == 0 && inData.raw_adc > DRY_THRESHOLD) 
            {
                ESP_LOGW("SMART_PUMP", "DRY! ADC=%d -> pump ON", inData.raw_adc);
                pump_on();
            } 
            else if (pump_state == 1 && inData.raw_adc < WET_THRESHOLD) 
            {
                int64_t ran = (now - last_on_time) / 1000000;
                if ((now - last_on_time) >= MIN_PUMP_RUNTIME) 
                {
                    ESP_LOGI("SMART_PUMP", "WET! ADC=%d -> pump OFF (ran %llds)", inData.raw_adc, ran);
                    pump_off();
                } 
                else 
                {
                    ESP_LOGI("SMART_PUMP", "WET but min runtime not met (%llds so far)", ran);
                }
            } 
            else if(pump_state == 1)
            {
                int64_t ran = (now - last_on_time) / 1000000;
                if ((now - last_on_time) >= MAX_PUMP_RUNTIME) {
                    ESP_LOGE("SMART_PUMP", "TIMEOUT! Ran %llds -> force OFF", ran);
                    pump_off();
                }
            }
        } else {
            ESP_LOGE("SMART_PUMP", "No sensor data 5s -> safety OFF");
            pump_off();
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void relay_test_task(void *pvParameters) {
    ESP_LOGI("SMART_PUMP", "===== RELAY TEST: toggling every 2s =====");
    int n = 0;
    while (1) {
        ESP_LOGI("SMART_PUMP", "[%d] OFF", n);
        gpio_set_level(RELAY_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(2000));
        ESP_LOGI("SMART_PUMP", "[%d] ON <- motor should spin", n++);
        gpio_set_level(RELAY_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void app_main(void) {
    ESP_LOGI("SMART_PUMP", "====== SMART WATERING SYSTEM ======");

    // 1. GPIO FIRST — must be ready before any task runs (no race condition)
    init_relay();

    // 2. Settle delay
    vTaskDelay(pdMS_TO_TICKS(100));

    // 3. ADC
    init_adc();

    // 4. Queue
    dataQueue = xQueueCreate(20, sizeof(sensor_data_t));
    if (!dataQueue) { ESP_LOGE("SMART_PUMP", "Queue failed!"); return; }

    // 5. Pump task FIRST at higher priority — waiting before sensor sends anything
    xTaskCreate(pump_control_task,    "pump_task",   4096, NULL, 6, NULL);

    // 6. Sensor task SECOND
    xTaskCreate(moisture_sensor_task, "sensor_task", 4096, NULL, 5, NULL);

    ESP_LOGI("SMART_PUMP", "=== RUNNING ===");

    // To test relay without sensor, uncomment:
    // xTaskCreate(relay_test_task, "relay_test", 2048, NULL, 5, NULL);
}
// ESP_VERSION_6_0_0