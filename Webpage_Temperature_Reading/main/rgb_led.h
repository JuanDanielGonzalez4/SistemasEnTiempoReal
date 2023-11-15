/*
 * rgb_led.h
 *
 *  Created on: Oct 11, 2021
 *      Author: kjagu
 */

#ifndef MAIN_RGB_LED_H_
#define MAIN_RGB_LED_H_

// RGB LED GPIO
#define RGB_LED_RED_GPIO 21
#define RGB_LED_GREEN_GPIO 22
#define RGB_LED_BLUE_GPIO 23

// RGB LED color mix channels
#define RGB_LED_CHANNEL_NUM 3

// RGB LED configuration
typedef struct
{
	int channel;
	int gpio;
	int mode;
	int timer_index;
} ledc_info_t;
// ledc_info_t ledc_ch[RGB_LED_CHANNEL_NUM]; Move this declaration to the top of rgb_led.c to avoid linker errors

typedef struct TemperatureValuesLed
{
	int high_temp_lvalue;
	int high_temp_uvalue;
	int medium_temp_lvalue;
	int medium_temp_uvalue;
	int low_temp_lvalue;
	int low_temp_uvalue;
	int r_value_first_led;
	int g_value_first_led;
	int b_value_first_led;
	int r_value_second_led;
	int g_value_second_led;
	int b_value_second_led;
	int r_value_third_led;
	int g_value_third_led;
	int b_value_third_led;
} TemperatureValuesLed;
/**
 * Color to indicate WiFi application has started.
 */
void rgb_led_wifi_app_started(void);

/**
 * Color to indicate HTTP server has started.
 */
void rgb_led_http_server_started(void);

/**
 * Color to indicate that the ESP32 is connected to an access point.
 */
void rgb_led_wifi_connected(void);

void rgb_led_http_received(void);

#endif /* MAIN_RGB_LED_H_ */
