#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "GPIO_TEST";

#define TEST_GPIO 22

void simple_gpio_test(void *pvParameters) {
    ESP_LOGI(TAG, "========== GPIO23 DIAGNOSTIC TEST ==========");
    ESP_LOGI(TAG, "Testing GPIO%d", TEST_GPIO);
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Instructions:");
    ESP_LOGI(TAG, "1. Connect multimeter between GPIO23 and GND");
    ESP_LOGI(TAG, "2. Watch for voltage changes in serial monitor");
    ESP_LOGI(TAG, "3. GPIO should toggle: LOW (0V) → HIGH (3.3V) every 1 second");
    ESP_LOGI(TAG, "");
    
    // Reset pin
    gpio_reset_pin(TEST_GPIO);
    
    // Set as output
    esp_err_t ret = gpio_set_direction(TEST_GPIO, GPIO_MODE_OUTPUT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ FAILED to set GPIO direction: %s", esp_err_to_name(ret));
        return;
    }
    
    ESP_LOGI(TAG, "✓ GPIO23 configured as OUTPUT");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Starting toggle test...");
    ESP_LOGI(TAG, "");
    
    int counter = 0;
    
    while (1) {
        // Set HIGH (3.3V)
        gpio_set_level(TEST_GPIO, 1);
        ESP_LOGI(TAG, "[%d] GPIO23 = HIGH (3.3V) ↑", counter);
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // Set LOW (0V)
        gpio_set_level(TEST_GPIO, 0);
        ESP_LOGI(TAG, "[%d] GPIO23 = LOW (0V) ↓", counter);
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        counter++;
        
        if (counter == 10) {
            ESP_LOGI(TAG, "");
            ESP_LOGI(TAG, "Test completed 10 cycles.");
            ESP_LOGI(TAG, "");
            ESP_LOGI(TAG, "✓ If you saw voltage change: GPIO23 is working!");
            ESP_LOGI(TAG, "✗ If voltage stayed same: GPIO23 is stuck");
            ESP_LOGI(TAG, "");
            ESP_LOGI(TAG, "Continuing test...");
            ESP_LOGI(TAG, "");
            counter = 0;
        }
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "GPIO23 TEST STARTING");
    ESP_LOGI(TAG, "");
    
    xTaskCreate(simple_gpio_test, "gpio_test", 2048, NULL, 5, NULL);
}
