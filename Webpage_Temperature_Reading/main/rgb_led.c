/*
 * rgb_led.c
 *
 *  Created on: Oct 11, 2021
 *      Author: kjagu
 */

#include <stdbool.h>
#include "freertos/FreeRTOS.h"

#include "driver/ledc.h"
#include "rgb_led.h"
#include "freertos/queue.h"

// RGB LED Configuration Array
ledc_info_t ledc_ch[RGB_LED_CHANNEL_NUM];

// handle for rgb_led_pwm_init
bool g_pwm_init_handle = false;

// ADC Queue
extern QueueHandle_t ADC_QUEUE;
extern QueueHandle_t temperatureQueue;

/**
 * Initializes the RGB LED settings per channel, including
 * the GPIO for each color, mode and timer configuration.
 */
static void
rgb_led_pwm_init(void)
{
	int rgb_ch;

	// Red
	ledc_ch[0].channel = LEDC_CHANNEL_0;
	ledc_ch[0].gpio = RGB_LED_RED_GPIO;
	ledc_ch[0].mode = LEDC_HIGH_SPEED_MODE;
	ledc_ch[0].timer_index = LEDC_TIMER_0;

	// Green
	ledc_ch[1].channel = LEDC_CHANNEL_1;
	ledc_ch[1].gpio = RGB_LED_GREEN_GPIO;
	ledc_ch[1].mode = LEDC_HIGH_SPEED_MODE;
	ledc_ch[1].timer_index = LEDC_TIMER_0;

	// Blue
	ledc_ch[2].channel = LEDC_CHANNEL_2;
	ledc_ch[2].gpio = RGB_LED_BLUE_GPIO;
	ledc_ch[2].mode = LEDC_HIGH_SPEED_MODE;
	ledc_ch[2].timer_index = LEDC_TIMER_0;

	// Configure timer zero
	ledc_timer_config_t ledc_timer =
		{
			.duty_resolution = LEDC_TIMER_8_BIT,
			.freq_hz = 100,
			.speed_mode = LEDC_HIGH_SPEED_MODE,
			.timer_num = LEDC_TIMER_0};
	ledc_timer_config(&ledc_timer);

	// Configure channels
	for (rgb_ch = 0; rgb_ch < RGB_LED_CHANNEL_NUM; rgb_ch++)
	{
		ledc_channel_config_t ledc_channel =
			{
				.channel = ledc_ch[rgb_ch].channel,
				.duty = 0,
				.hpoint = 0,
				.gpio_num = ledc_ch[rgb_ch].gpio,
				.intr_type = LEDC_INTR_DISABLE,
				.speed_mode = ledc_ch[rgb_ch].mode,
				.timer_sel = ledc_ch[rgb_ch].timer_index,
			};
		ledc_channel_config(&ledc_channel);
	}

	g_pwm_init_handle = true;
}

/**
 * Sets the RGB color.
 */
static void rgb_led_set_color(uint8_t red, uint8_t green, uint8_t blue)
{
	// Value should be 0 - 255 for 8 bit number
	ledc_set_duty(ledc_ch[0].mode, ledc_ch[0].channel, red);
	ledc_update_duty(ledc_ch[0].mode, ledc_ch[0].channel);

	ledc_set_duty(ledc_ch[1].mode, ledc_ch[1].channel, green);
	ledc_update_duty(ledc_ch[1].mode, ledc_ch[1].channel);

	ledc_set_duty(ledc_ch[2].mode, ledc_ch[2].channel, blue);
	ledc_update_duty(ledc_ch[2].mode, ledc_ch[2].channel);
}

void rgb_led_wifi_app_started(void)
{
	if (g_pwm_init_handle == false)
	{
		rgb_led_pwm_init();
	}

	rgb_led_set_color(255, 45, 0);
}

void rgb_led_http_server_started(void)
{
	if (g_pwm_init_handle == false)
	{
		rgb_led_pwm_init();
	}

	rgb_led_set_color(0, 170, 255);
}

void rgb_led_wifi_connected(void)
{
	if (g_pwm_init_handle == false)
	{
		rgb_led_pwm_init();
	}

	rgb_led_set_color(0, 255, 0);
}

void rgb_led_http_received(void)
{
	double adc_value;
	TemperatureValuesLed receivedData;

	if (g_pwm_init_handle == false)
	{
		rgb_led_pwm_init();
	}

	if (xQueueReceive(temperatureQueue, &receivedData, portMAX_DELAY) == pdPASS)
	{
		printf("Received: %d\n", receivedData.r_value_first_led);
	}
	// Attempt to receive the ADC value from the queue
	while (true)
	{
		if (xQueueReceive(ADC_QUEUE, &adc_value, portMAX_DELAY))
		{
			printf("Received in Led: %f\n", adc_value);
			if (adc_value >= receivedData.high_temp_lvalue && adc_value <= receivedData.high_temp_uvalue)
			{
				rgb_led_set_color(receivedData.r_value_first_led, receivedData.g_value_first_led, receivedData.b_value_first_led);
			}
			else if (adc_value >= receivedData.medium_temp_lvalue && adc_value <= receivedData.medium_temp_uvalue)
			{
				rgb_led_set_color(receivedData.r_value_second_led, receivedData.g_value_second_led, receivedData.b_value_second_led);
			}
			else
			{
				rgb_led_set_color(receivedData.r_value_third_led, receivedData.g_value_third_led, receivedData.b_value_third_led);
			}
		}
		vTaskDelay(1);
	}
}
