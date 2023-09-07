#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "rom/gpio.h"
#include "esp_log.h"

// Define our GPIOS
#define LED_GPIO GPIO_NUM_13
#define BUTTON_GPIO GPIO_NUM_0
#define BLINK_TIME_MS 100

// Create Global Queue
QueueHandle_t xQueue;

static void createQueue(void)
{
    xQueue = xQueueCreate(10, sizeof(unsigned long));
}

// Config Button and led

void configButtonLed(void)
{
    gpio_config_t btn_config;
    btn_config.intr_type = GPIO_INTR_NEGEDGE;
    btn_config.mode = GPIO_MODE_INPUT;              // Set as Input
    btn_config.pin_bit_mask = (1 << BUTTON_GPIO);   // Bitmask
    btn_config.pull_up_en = GPIO_PULLUP_DISABLE;    // Disable pullup
    btn_config.pull_down_en = GPIO_PULLDOWN_ENABLE; // Enable pulldown
    gpio_config(&btn_config);
    printf("Button configured\n");

    gpio_pad_select_gpio(LED_GPIO);                 // Set pin as GPIO
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT); // Set as Output
    printf("LED configured\n");
}

void taskToggle(void *args)
{
    bool IsToggled = false;
    int btn_state = 0;
    while (1)
    {
        if (xQueueReceive(xQueue, &btn_state, (TickType_t)0) == pdTRUE)
        {
            printf("Received");
            IsToggled = !IsToggled;
        }

        if (IsToggled)
        {
            gpio_set_level(LED_GPIO, 1);
            vTaskDelay(BLINK_TIME_MS / portTICK_PERIOD_MS);
            gpio_set_level(LED_GPIO, 0);
            vTaskDelay(BLINK_TIME_MS / portTICK_PERIOD_MS);
        }
        else
        {
            gpio_set_level(LED_GPIO, 0);
        }
    }
}

void button_task(void *pvParameter)
{
    int last_btn_state = 0;
    while (1)
    {
        int btn_state = gpio_get_level(BUTTON_GPIO);
        if (btn_state == 0 && last_btn_state == 1)
        {
            int data = 1;
            if (xQueueSend(xQueue, &data, (TickType_t)0) != pdTRUE)
            {
                printf("Failed to send data to queue\n");
            }
        }
        last_btn_state = btn_state;
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    createQueue();
    configButtonLed();
    xTaskCreate(&button_task, "buttonTask", 2048, NULL, 5, NULL);
    xTaskCreate(&taskToggle, "taskToggle", 2048, NULL, 5, NULL);
}
