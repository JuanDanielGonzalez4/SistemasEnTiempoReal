#include "adc.h"

adc_oneshot_unit_handle_t adc1_handle;

QueueHandle_t ADC_QUEUE;

void adc_config(void)
{
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };

    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = EXAMPLE_ADC_ATTEN,
    };

    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, EXAMPLE_ADC1_CHAN0, &config));

    // Create a queue to handle ADC data
    ADC_QUEUE = xQueueCreate(10, sizeof(double));

    // Create a task to read ADC
    xTaskCreate(adc_read_task, "adc_read_task", 2048, NULL, 5, NULL);
}

void adc_read_task(void *pvParameters)
{
    int data;
    while (1)
    {
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL_4, &data));
        printf("valor adc: %d\n", data);
        double voltage_adc = data * VOLTAGE_REFERENCE / ADC_MAX_VALUE;
        double resistance = RESISTOR_REFERENCE * voltage_adc / (VOLTAGE_REFERENCE - voltage_adc);

        double temperature_kelvin = 1 / (A_COEFFICIENT + B_COEFFICIENT * log(resistance) + C_COEFFICIENT * pow(log(resistance), 3));
        double temperature_celsius = temperature_kelvin - 273.15;

        xQueueSend(ADC_QUEUE, &temperature_celsius, portMAX_DELAY);
        // if (xQueueReceive(ADC_QUEUE, &data, portMAX_DELAY))
        // {
        //     printf("Received: %d\n", data);
        // }
        vTaskDelay(pdMS_TO_TICKS(DELAY));
    }
}