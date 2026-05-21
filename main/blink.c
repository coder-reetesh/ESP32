#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define LED_PIN 12   // Change this if needed (GPIO22 is common)

void app_main(void)
{
    // Configure the GPIO pin as output
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    while (1)
    {
        gpio_set_level(LED_PIN, 1);  // LED ON
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        gpio_set_level(LED_PIN, 0);  // LED OFF
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}