#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "rom/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"

#define LED_GPIO GPIO_NUM_13
#define BUTTON_GPIO GPIO_NUM_0
#define BLINK_TIME_MS 100

static bool led_state = false;
static bool blink_state = true;

void isr_button_pressed(void *args)
{

    int btn_state = gpio_get_level(BUTTON_GPIO);

    if (btn_state == 1)
    { // Button is pressed
        blink_state = !blink_state;
    }
}

void task_button_to_led(void *pvParameter)
{
    // Configure button
    gpio_config_t btn_config;
    btn_config.intr_type = GPIO_INTR_ANYEDGE;
    btn_config.mode = GPIO_MODE_INPUT;              // Set as Input
    btn_config.pin_bit_mask = (1 << BUTTON_GPIO);   // Bitmask
    btn_config.pull_up_en = GPIO_PULLUP_DISABLE;    // Disable pullup
    btn_config.pull_down_en = GPIO_PULLDOWN_ENABLE; // Enable pulldown
    gpio_config(&btn_config);
    printf("Button configured\n");

    // Configure LED
    gpio_pad_select_gpio(LED_GPIO);                 // Set pin as GPIO
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT); // Set as Output
    printf("LED configured\n");

    // Configure interrupt and add handler
    gpio_install_isr_service(0);                                 // Start Interrupt Service Routine service
    gpio_isr_handler_add(BUTTON_GPIO, isr_button_pressed, NULL); // Add handler of interrupt
    printf("Interrupt configured\n");

    TickType_t xLastWakeTime;
    xLastWakeTime = xTaskGetTickCount();
    // Wait
    while (1)
    {
        if (blink_state)
        {
            led_state = !led_state;
            gpio_set_level(LED_GPIO, led_state);
        }
        else
        {
            led_state = false;
        }
        xTaskDelayUntil(&xLastWakeTime, BLINK_TIME_MS / portTICK_PERIOD_MS);
    }
}

void app_main()
{
    xTaskCreate(&task_button_to_led, "buttonToLED", 2048, NULL, 5, NULL);
}