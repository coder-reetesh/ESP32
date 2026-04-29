
#ifndef ESP_VERSION_6_0_0
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"

#define AOUT_PIN 34   // ADC1_CHANNEL_6 (GPIO34)
#define RELAY_GPIO 23

void app_main(void)
{
    esp_adc_cal_characteristics_t adc1_chars;
    
        //      Configure ADC
    adc1_config_width(ADC_WIDTH_BIT_12);
    
        //      ADC_ATTEN_DB_11	150 ~ 2450	11 dB attenuation
    adc1_config_channel_atten(AOUT_PIN, ADC_ATTEN_DB_11);

        //      Reset Relay 
    gpio_reset_pin(RELAY_GPIO);

        //      Enable Relay
    gpio_set_direction(RELAY_GPIO, GPIO_MODE_OUTPUT);
    
        //      Calibrate ADC
    esp_adc_cal_characterize(
        ADC_UNIT_1,
        ADC_ATTEN_DB_11,
        ADC_WIDTH_BIT_12,
        0,
        &adc_chars
    );
     while (1)
    {   
        //      Read sensor value
        int raw = adc1_get_raw(AOUT_PIN);
        
        //      Convert to volage
        uint32_t voltage = esp_adc_cal_raw_to_voltage(raw, &adc_chars);
        
        //      Raw voltage and Actual voltage
        printf("Raw: %d | Voltage: %ld mV\n", raw, voltage);

        //      Condition for
        if (raw > 2500)
        {
            printf("Raw: %d | Soil Dry\n", raw);
        //      Relay with pump ON
            gpio_set_level(RELAY_GPIO, 1); // ON
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
        else 
        {
            printf("Raw: %d | Soil wet\n", raw);
        //      Relay with pump Off
            gpio_set_level(RELAY_GPIO, 0); // OFF
        }
        
        //      Delay
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

#else   //ESP_VERSION_6_0_0

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "hal/adc_types.h" // Ensure this is present
#define AOUT_PIN ADC_CHANNEL_7
//#define AOUT_PIN ADC_CHANNEL_6   // GPIO 34 is ADC1_CHANNEL_6 on ESP32
#define RELAY_GPIO 23

// Handle for ADC and Calibration
static adc_oneshot_unit_handle_t adc1_handle;
static adc_cali_handle_t adc1_cali_handle = NULL;

void app_main(void)
{
    // 1. Initialize ADC Unit
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc1_handle));

    // 2. Configure ADC Channel
    adc_oneshot_chan_cfg_t chan_config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, AOUT_PIN, &chan_config));

    // 3. Initialize Calibration
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_cali_create_scheme_line_fitting(&cali_config, &adc1_cali_handle));

    // 4. GPIO Setup
    gpio_reset_pin(RELAY_GPIO);
    gpio_set_direction(RELAY_GPIO, GPIO_MODE_OUTPUT);

    while (1) 
    {   
        int raw;
        // 5. Read ADC
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, AOUT_PIN, &raw));
        
        // 6. Convert to voltage
        int voltage;
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, raw, &voltage));
        
        printf("Raw: %d | Voltage: %d mV\n", raw, voltage);

        // Condition for relay
        if (raw > 2500)
        {
            printf("Raw: %d | Soil Dry\n", raw);
            gpio_set_level(RELAY_GPIO, 1); // ON
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
        else 
        {
            printf("Raw: %d | Soil wet\n", raw);
            gpio_set_level(RELAY_GPIO, 0); // OFF
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

#endif //ESP_VERSION_6_0_0