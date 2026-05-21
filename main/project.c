#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h" // Added for Queue support
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_timer.h"
#include "wifi_program.h"

static const char *TAG = "SMART_PUMP";

// Definitions
#define RELAY_GPIO      22
#define MOISTURE_CHAN   ADC_CHANNEL_7
#define DRY_THRESHOLD   2500
#define WET_THRESHOLD   1800
#define BUFFER_LEN     100

// Handles and Global Variables
adc_oneshot_unit_handle_t adc1_handle;
adc_cali_handle_t cali_handle = NULL;
QueueHandle_t dataQueue; // Handle for the queue

static uint8_t pump_state = 0;
static int64_t last_on_time = 0;

// FIX: Declared missing buffer variables
//int sensor_buffer[BUFFER_LEN];

typedef struct {
    int id;
    int value;
} sensor_data_t;


int buffer_index = 0;

void send_to_thingspeak(int moisture_value)
{
    char url[256];

    snprintf(url, sizeof(url),
             "http://api.thingspeak.com/update?api_key=%s&field1=%d",
             THINGSPEAK_WRITE_API_KEY,
             moisture_value);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK)
    /*{
        ESP_LOGI("THINGSPEAK", "Data Sent Successfully");
    }
    else
    {
        ESP_LOGE("THINGSPEAK", "Failed to send data");
    }*/
    {
        int status = esp_http_client_get_status_code(client);
        int64_t entry = esp_http_client_get_content_length(client);
        ESP_LOGI("THINGSPEAK", "HTTP Status: %d", status);

        esp_http_client_cleanup(client);
    }
    
}

void init_adc_v6(void) {
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc1_handle));

    adc_oneshot_chan_cfg_t chan_config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, MOISTURE_CHAN, &chan_config));

    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    adc_cali_create_scheme_line_fitting(&cali_config, &cali_handle);
}

void moisture_sensor_task(void *pvParameters) {
    int raw_val; // FIX: Declared raw_val locally
    sensor_data_t outdata;
    while (1)
    {
        if (adc_oneshot_read(adc1_handle, MOISTURE_CHAN, &raw_val) == ESP_OK)
        {
            outdata.id=buffer_index;
            outdata.value=raw_val;
            send_to_thingspeak(raw_val);
            if (xQueueSend(dataQueue, &outdata, pdMS_TO_TICKS(1000)) == pdPASS)
            {
                printf("Sender: Sent ID %d\n", outdata.id);
                buffer_index++;
            }
            else
            {
                printf("Sender: Queue full!\n");
            }
        
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void pump_control_task(void *pvParameters) {

gpio_config_t io_conf = {
    .pin_bit_mask = (1ULL << RELAY_GPIO),
    .mode         = GPIO_MODE_OUTPUT,
    .pull_up_en   = GPIO_PULLUP_DISABLE,   // keeps relay OFF at boot
    .pull_down_en = GPIO_PULLDOWN_ENABLE,
    .intr_type    = GPIO_INTR_DISABLE,
};
    gpio_config(&io_conf);
    gpio_set_level(RELAY_GPIO, 0); // Ensure OFF immediately

    sensor_data_t inData;

    while (1)
    {
        // Receive the average from the queue
        if (xQueueReceive(dataQueue, &inData, portMAX_DELAY))
        {
            printf("Receiver: Got ID %d with Value %d and pump_state: %u\n", inData.id, inData.value, pump_state);
            if (pump_state == 0 && inData.value > DRY_THRESHOLD)
            {
                ESP_LOGW(TAG, "Soil is DRY! Turning on pump.");
                gpio_set_level(RELAY_GPIO, 1);
                pump_state = 1;
                last_on_time = esp_timer_get_time();
            }
            else if(pump_state ==1 && inData.value < WET_THRESHOLD)
            {
                int64_t now = esp_timer_get_time();
                if ((now - last_on_time) > 2000000) // 2 seconds minimum
                {
                    ESP_LOGW(TAG, "Soil is WET! Turning off pump.");
                    gpio_set_level(RELAY_GPIO, 0);
                    pump_state = 0;
                }
            }
        }
    }
}
#if 1
void app_main(void) {

    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    wifi_init_sta(NULL);
    init_adc_v6();

    // FIX: Initialize the queue before starting tasks
    dataQueue = xQueueCreate(10, sizeof(sensor_data_t));

    xTaskCreate(moisture_sensor_task, "sensor_task", 8192, NULL, 5, NULL);//4096 change to 8192
    xTaskCreate(pump_control_task, "pump_task", 8192, NULL, 5, NULL);//4096 change to 8192
}
#endif
#if 0
void app_main(void) {

    // --- RELAY HARDWARE TEST ---
    static int count =5;
    gpio_reset_pin(RELAY_GPIO);
    gpio_set_direction(RELAY_GPIO, GPIO_MODE_OUTPUT);
    while(count--){
        printf("count: %d\n", count);
        ESP_LOGI("TEST", "Relay ON");
        vTaskDelay(pdMS_TO_TICKS(3000));  // Listen for relay click for 3s
        gpio_set_level(RELAY_GPIO, 1);
        ESP_LOGI("TEST", "Relay OFF");
        vTaskDelay(pdMS_TO_TICKS(3000));
        gpio_set_level(RELAY_GPIO, 0);
        
    }
    // --- END TEST ---

    init_adc_v6();
    dataQueue = xQueueCreate(10, sizeof(sensor_data_t));
    xTaskCreate(moisture_sensor_task, "sensor_task", 4096, NULL, 5, NULL);
    xTaskCreate(pump_control_task, "pump_task", 4096, NULL, 5, NULL);
}
#endif
 //ESP_VERSION_6_0_0