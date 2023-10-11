#include <string.h>
#include <stdio.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_adc/adc_continuous.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "rom/gpio.h"
#include "freertos/queue.h"
// ADC
#define EXAMPLE_ADC_UNIT ADC_UNIT_1
#define _EXAMPLE_ADC_UNIT_STR(unit) #unit
#define EXAMPLE_ADC_UNIT_STR(unit) _EXAMPLE_ADC_UNIT_STR(unit)
#define EXAMPLE_ADC_CONV_MODE ADC_CONV_SINGLE_UNIT_1
#define EXAMPLE_ADC_ATTEN ADC_ATTEN_DB_0
#define EXAMPLE_ADC_BIT_WIDTH SOC_ADC_DIGI_MAX_BITWIDTH

#define EXAMPLE_ADC_OUTPUT_TYPE ADC_DIGI_OUTPUT_FORMAT_TYPE1
#define EXAMPLE_ADC_GET_CHANNEL(p_data) ((p_data)->type1.channel)
#define EXAMPLE_ADC_GET_DATA(p_data) ((p_data)->type1.data)

#define EXAMPLE_READ_LEN 256

// LED
#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_HIGH_SPEED_MODE
#define LEDC_TIMER_BIT_NUM 10
#define LEDC_BASE_FREQ_HZ 5000
#define LED_GPIO_PIN1 18
#define LED_GPIO_PIN2 19
#define LED_GPIO_PIN3 4
#define LEDC_TEST_CH_NUM (3)

#define LEDC_CHANNEL_1 LEDC_CHANNEL_1
#define LEDC_CHANNEL_2 LEDC_CHANNEL_2
#define LEDC_CHANNEL_3 LEDC_CHANNEL_3

uint32_t led_duty_cycles[LEDC_TEST_CH_NUM] = {0};

// BUTTON
#define BUTTON_GPIO GPIO_NUM_0

static adc_channel_t channel[1] = {ADC_CHANNEL_6};

static TaskHandle_t s_task_handle;
static const char *TAG = "EXAMPLE";

QueueHandle_t xQueue;

static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data)
{
    BaseType_t mustYield = pdFALSE;
    vTaskNotifyGiveFromISR(s_task_handle, &mustYield);

    return (mustYield == pdTRUE);
}

// Config LED pwm

void led_pwm_init()
{

    // Configure LED PWM timer
    ledc_timer_config_t ledc_timer_cfg = {
        .duty_resolution = LEDC_TIMER_BIT_NUM,
        .freq_hz = LEDC_BASE_FREQ_HZ,
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER};
    ledc_timer_config(&ledc_timer_cfg);

    // Configure LED PWM channel
    int ch;
    ledc_channel_config_t ledc_channel_cfg[LEDC_TEST_CH_NUM] = {
        {.channel = LEDC_CHANNEL_1,
         .duty = 0,
         .gpio_num = LED_GPIO_PIN1,
         .speed_mode = LEDC_MODE,
         .hpoint = 0,
         .timer_sel = LEDC_TIMER},
        {.channel = LEDC_CHANNEL_2,
         .duty = 0,
         .gpio_num = LED_GPIO_PIN2,
         .speed_mode = LEDC_MODE,
         .hpoint = 0,
         .timer_sel = LEDC_TIMER},
        {.channel = LEDC_CHANNEL_3,
         .duty = 0,
         .gpio_num = LED_GPIO_PIN3,
         .speed_mode = LEDC_MODE,
         .hpoint = 0,
         .timer_sel = LEDC_TIMER},
    };
    for (ch = 0; ch < LEDC_TEST_CH_NUM; ch++)
    {
        ledc_channel_config(&ledc_channel_cfg[ch]);
    }
}

// Congfig Button
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
}

// Congif continuos ADC

static void continuous_adc_init(adc_channel_t *channel, uint8_t channel_num, adc_continuous_handle_t *out_handle)
{
    adc_continuous_handle_t handle = NULL;

    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 1024,
        .conv_frame_size = EXAMPLE_READ_LEN,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = 20 * 1000,
        .conv_mode = EXAMPLE_ADC_CONV_MODE,
        .format = EXAMPLE_ADC_OUTPUT_TYPE,
    };

    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
    dig_cfg.pattern_num = channel_num;

    adc_pattern[0].atten = EXAMPLE_ADC_ATTEN;
    adc_pattern[0].channel = channel[0] & 0x7;
    adc_pattern[0].unit = EXAMPLE_ADC_UNIT;
    adc_pattern[0].bit_width = EXAMPLE_ADC_BIT_WIDTH;

    dig_cfg.adc_pattern = adc_pattern;
    ESP_ERROR_CHECK(adc_continuous_config(handle, &dig_cfg));

    *out_handle = handle;
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
    xQueue = xQueueCreate(10, sizeof(unsigned long));
    led_pwm_init();
    xTaskCreate(&button_task, "buttonTask", 2048, NULL, 5, NULL);

    esp_err_t ret;
    uint32_t ret_num = 0;
    uint8_t result[EXAMPLE_READ_LEN] = {0};
    memset(result, 0xcc, EXAMPLE_READ_LEN);

    s_task_handle = xTaskGetCurrentTaskHandle();

    adc_continuous_handle_t handle = NULL;
    continuous_adc_init(channel, sizeof(channel) / sizeof(adc_channel_t), &handle);

    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = s_conv_done_cb,
    };
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(handle, &cbs, NULL));
    ESP_ERROR_CHECK(adc_continuous_start(handle));

    while (1)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        char unit[] = EXAMPLE_ADC_UNIT_STR(EXAMPLE_ADC_UNIT);

        int led_index = 0;
        while (1)
        {
            ret = adc_continuous_read(handle, result, EXAMPLE_READ_LEN, &ret_num, 0);
            if (ret == ESP_OK)
            {
                ESP_LOGI("TASK", "ret is %x, ret_num is %" PRIu32 " bytes", ret, ret_num);
                for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES)
                {
                    adc_digi_output_data_t *p = (adc_digi_output_data_t *)&result[i];
                    uint32_t chan_num = EXAMPLE_ADC_GET_CHANNEL(p);
                    uint32_t data = EXAMPLE_ADC_GET_DATA(p);
                    uint32_t max_adc_value = (1 << EXAMPLE_ADC_BIT_WIDTH) - 1;
                    uint32_t duty = (data * max_adc_value) / LEDC_BASE_FREQ_HZ;

                    int data2 = 0;
                    if (xQueueReceive(xQueue, &data2, (TickType_t)0) == pdTRUE)
                    {
                        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, 0);
                        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_2, 0);
                        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_3, 0);

                        if (led_index == 0)
                        {
                            ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, duty);
                        }
                        else if (led_index == 1)
                        {
                            ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_2, duty);
                        }
                        else
                        {
                            ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_3, duty);
                        }

                        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1);
                        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_2);
                        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_3);

                        led_index = (led_index + 1) % 3;
                    }
                    if (chan_num < SOC_ADC_CHANNEL_NUM(EXAMPLE_ADC_UNIT))
                    {
                        ESP_LOGI(TAG, "Unit: %s, Channel: %" PRIu32 ", Value: %" PRIx32, unit, chan_num, data);
                    }
                    else
                    {
                        ESP_LOGW(TAG, "Invalid data [%s_%" PRIu32 "_%" PRIx32 "]", unit, chan_num, data);
                    }

                    vTaskDelay(1);
                }
            }
            else if (ret == ESP_ERR_TIMEOUT)
            {
                // We try to read `EXAMPLE_READ_LEN` until API returns timeout, which means there's no available data
                break;
            }
        }
    }

    ESP_ERROR_CHECK(adc_continuous_stop(handle));
    ESP_ERROR_CHECK(adc_continuous_deinit(handle));
}