#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "math.h"

#define EXAMPLE_ADC_ATTEN ADC_ATTEN_DB_11
#define EXAMPLE_ADC1_CHAN0 ADC_CHANNEL_4

#define DELAY 100
#define RESISTOR_REFERENCE 10000
#define VOLTAGE_REFERENCE 3300
#define ADC_MAX_VALUE 4096

#define A_COEFFICIENT 0.001129148
#define B_COEFFICIENT 0.000234125
#define C_COEFFICIENT 0.0000000876741
#define CELCIUS_RATE 10
void adc_config(void);

void adc_read_task();
