/*
 * http_server.c
 *
 *  Created on: Oct 20, 2021
 *      Author: kjagu
 */

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "sys/param.h"
#include <stdlib.h>
#include "freertos/queue.h"
#include "rgb_led.h"
#include "ntp.h"

#include "http_server.h"
#include "tasks_common.h"
#include "wifi_app.h"
#include "adc.h"
#include "cJSON.h"

// Tag used for ESP serial console messages
static const char TAG[] = "http_server";
// Wifi connect status
static int g_wifi_connect_status = NONE;

// Firmware update status
static int g_fw_update_status = OTA_UPDATE_PENDING;
// HTTP server task handle
static httpd_handle_t http_server_handle = NULL;

// HTTP server monitor task handle
static TaskHandle_t task_http_server_monitor = NULL;

// Queue handle used to manipulate the main queue of eventsº
static QueueHandle_t http_server_monitor_queue_handle;

QueueHandle_t temperatureQueue;

const esp_timer_create_args_t fw_update_reset_args = {
	.callback = &http_server_fw_update_reset_callback,
	.arg = NULL,
	.dispatch_method = ESP_TIMER_TASK,
	.name = "fw_update_reset"};
esp_timer_handle_t fw_update_reset;

extern QueueHandle_t ADC_QUEUE;

extern QueueHandle_t NTP_QUEUE;

// Embedded files: JQuery, index.html, app.css, app.js and favicon.ico files
extern const uint8_t jquery_3_3_1_min_js_start[] asm("_binary_jquery_3_3_1_min_js_start");
extern const uint8_t jquery_3_3_1_min_js_end[] asm("_binary_jquery_3_3_1_min_js_end");
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
extern const uint8_t app_css_start[] asm("_binary_app_css_start");
extern const uint8_t app_css_end[] asm("_binary_app_css_end");
extern const uint8_t app_js_start[] asm("_binary_app_js_start");
extern const uint8_t app_js_end[] asm("_binary_app_js_end");
extern const uint8_t favicon_ico_start[] asm("_binary_favicon_ico_start");
extern const uint8_t favicon_ico_end[] asm("_binary_favicon_ico_end");

/**
 * Checks the g_fw_update_status and creates the fw_update_reset timer if g_fw_update_status is true.
 */
static void http_server_fw_update_reset_timer(void)
{
	if (g_fw_update_status == OTA_UPDATE_SUCCESSFUL)
	{
		ESP_LOGI(TAG, "http_server_fw_update_reset_timer: FW updated successful starting FW update reset timer");

		// Give the web page a chance to receive an acknowledge back and initialize the timer
		ESP_ERROR_CHECK(esp_timer_create(&fw_update_reset_args, &fw_update_reset));
		ESP_ERROR_CHECK(esp_timer_start_once(fw_update_reset, 8000000));
	}
	else
	{
		ESP_LOGI(TAG, "http_server_fw_update_reset_timer: FW update unsuccessful");
	}
}

/**
 * HTTP server monitor task used to track events of the HTTP server
 * @param pvParameters parameter which can be passed to the task.
 */
static void http_server_monitor(void *parameter)
{
	http_server_queue_message_t msg;

	for (;;)
	{
		if (xQueueReceive(http_server_monitor_queue_handle, &msg, portMAX_DELAY))
		{
			switch (msg.msgID)
			{
			case HTTP_MSG_WIFI_CONNECT_INIT:
				ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_INIT");

				g_wifi_connect_status = HTTP_WIFI_STATUS_CONNECTING;

				break;

			case HTTP_MSG_WIFI_CONNECT_SUCCESS:
				ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_SUCCESS");

				g_wifi_connect_status = HTTP_WIFI_STATUS_CONNECT_SUCCESS;
				ntp_wifi_connection_received();

				break;

			case HTTP_MSG_WIFI_CONNECT_FAIL:
				ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_FAIL");

				g_wifi_connect_status = HTTP_WIFI_STATUS_CONNECT_FAILED;

				break;

			case HTTP_MSG_OTA_UPDATE_SUCCESSFUL:
				ESP_LOGI(TAG, "HTTP_MSG_OTA_UPDATE_SUCCESSFUL");
				g_fw_update_status = OTA_UPDATE_SUCCESSFUL;
				http_server_fw_update_reset_timer();

				break;

			case HTTP_MSG_OTA_UPDATE_FAILED:
				ESP_LOGI(TAG, "HTTP_MSG_OTA_UPDATE_FAILED");
				g_fw_update_status = OTA_UPDATE_FAILED;

				break;

			default:
				break;
			}
		}
	}
}

/**
 * Jquery get handler is requested when accessing the web page.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_jquery_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "Jquery requested");

	httpd_resp_set_type(req, "application/javascript");
	httpd_resp_send(req, (const char *)jquery_3_3_1_min_js_start, jquery_3_3_1_min_js_end - jquery_3_3_1_min_js_start);

	return ESP_OK;
}

/**
 * Sends the index.html page.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_index_html_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "index.html requested");

	httpd_resp_set_type(req, "text/html");
	httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start);

	return ESP_OK;
}

/**
 * app.css get handler is requested when accessing the web page.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_app_css_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "app.css requested");

	httpd_resp_set_type(req, "text/css");
	httpd_resp_send(req, (const char *)app_css_start, app_css_end - app_css_start);

	return ESP_OK;
}

/**
 * app.js get handler is requested when accessing the web page.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_app_js_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "app.js requested");

	httpd_resp_set_type(req, "application/javascript");
	httpd_resp_send(req, (const char *)app_js_start, app_js_end - app_js_start);

	return ESP_OK;
}

/**
 * Sends the .ico (icon) file when accessing the web page.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_favicon_ico_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "favicon.ico requested");

	httpd_resp_set_type(req, "image/x-icon");
	httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_end - favicon_ico_start);

	return ESP_OK;
}

static esp_err_t http_server_adc_value_handler(httpd_req_t *req)
{
	double adc_value;

	// Attempt to receive the ADC value from the queue
	if (xQueueReceive(ADC_QUEUE, &adc_value, portMAX_DELAY))
	{
		char response[10];
		snprintf(response, sizeof(response), "%f", adc_value); // sending just the value
		httpd_resp_send(req, response, strlen(response));
	}
	else
	{
		// Handle the error, e.g., send a response indicating failure.
		httpd_resp_send_500(req);
	}

	return ESP_OK;
}

static esp_err_t http_server_ntp_value_handler(httpd_req_t *req)
{
	char ntp_value[64];

	// Attempt to receive the ADC value from the queue
	if (xQueueReceive(NTP_QUEUE, &ntp_value, portMAX_DELAY))
	{
		httpd_resp_send(req, ntp_value, strlen(ntp_value));
	}
	else
	{
		httpd_resp_send_500(req);
	}

	return ESP_OK;
}
/**
 * wifiConnect.json handler is invoked after the connect button is pressed
 * and handles receiving the SSID and password entered by the user
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
#include "cJSON.h"

static esp_err_t http_server_wifi_connect_json_handler(httpd_req_t *req)
{
	size_t header_len;
	char *header_value;
	char *ssid_str = NULL;
	char *pass_str = NULL;
	int content_length;

	ESP_LOGI(TAG, "/wifiConnect.json requested");

	// Get the "Content-Length" header to determine the length of the request body
	header_len = httpd_req_get_hdr_value_len(req, "Content-Length");
	if (header_len <= 0)
	{
		// Content-Length header not found or invalid
		// httpd_resp_send_err(req, HTTP_STATUS_411_LENGTH_REQUIRED, "Content-Length header is missing or invalid");
		ESP_LOGI(TAG, "Content-Length header is missing or invalid");
		return ESP_FAIL;
	}

	// Allocate memory to store the header value
	header_value = (char *)malloc(header_len + 1);
	if (httpd_req_get_hdr_value_str(req, "Content-Length", header_value, header_len + 1) != ESP_OK)
	{
		// Failed to get Content-Length header value
		free(header_value);
		// httpd_resp_send_err(req, HTTP_STATUS_BAD_REQUEST, "Failed to get Content-Length header value");
		ESP_LOGI(TAG, "Failed to get Content-Length header value");
		return ESP_FAIL;
	}

	// Convert the Content-Length header value to an integer
	content_length = atoi(header_value);
	free(header_value);

	if (content_length <= 0)
	{
		// Content length is not a valid positive integer
		// httpd_resp_send_err(req, HTTP_STATUS_BAD_REQUEST, "Invalid Content-Length value");
		ESP_LOGI(TAG, "Invalid Content-Length value");
		return ESP_FAIL;
	}

	// Allocate memory for the data buffer based on the content length
	char *data_buffer = (char *)malloc(content_length + 1);

	// Read the request body into the data buffer
	if (httpd_req_recv(req, data_buffer, content_length) <= 0)
	{
		// Handle error while receiving data
		free(data_buffer);
		// httpd_resp_send_err(req, HTTP_STATUS_INTERNAL_SERVER_ERROR, "Failed to receive request body");
		ESP_LOGI(TAG, "Failed to receive request body");
		return ESP_FAIL;
	}

	// Null-terminate the data buffer to treat it as a string
	data_buffer[content_length] = '\0';

	// Parse the received JSON data
	cJSON *root = cJSON_Parse(data_buffer);
	free(data_buffer);

	if (root == NULL)
	{
		// JSON parsing error
		// httpd_resp_send_err(req, HTTP_STATUS_BAD_REQUEST, "Invalid JSON data");
		ESP_LOGI(TAG, "Invalid JSON data");
		return ESP_FAIL;
	}

	cJSON *ssid_json = cJSON_GetObjectItem(root, "selectedSSID");
	cJSON *pwd_json = cJSON_GetObjectItem(root, "pwd");

	if (ssid_json == NULL || pwd_json == NULL || !cJSON_IsString(ssid_json) || !cJSON_IsString(pwd_json))
	{
		cJSON_Delete(root);
		// Missing or invalid JSON fields
		// httpd_resp_send_err(req, HTTP_STATUS_BAD_REQUEST, "Missing or invalid JSON data fields");
		ESP_LOGI(TAG, "Missing or invalid JSON data fields");
		return ESP_FAIL;
	}

	// Extract SSID and password from JSON
	ssid_str = strdup(ssid_json->valuestring);
	pass_str = strdup(pwd_json->valuestring);

	cJSON_Delete(root);

	// Now, you have the SSID and password in ssid_str and pass_str
	ESP_LOGI(TAG, "Received SSID: %s", ssid_str);
	ESP_LOGI(TAG, "Received Password: %s", pass_str);

	// Update the Wifi networks configuration and let the wifi application know
	wifi_config_t *wifi_config = wifi_app_get_wifi_config();
	// memset(wifi_config, 0x00, sizeof(wifi_config_t));
	memset(wifi_config->sta.ssid, 0x00, 32);
	memset(wifi_config->sta.password, 0x00, 64);
	memcpy(wifi_config->sta.ssid, ssid_str, strlen(ssid_str));
	memcpy(wifi_config->sta.password, pass_str, strlen(pass_str));

	wifi_app_send_message(WIFI_APP_MSG_CONNECTING_FROM_HTTP_SERVER);
	free(ssid_str);
	free(pass_str);

	return ESP_OK;
}

static esp_err_t http_server_temp_range_handler(httpd_req_t *req)
{
	size_t header_len;
	char *header_value;
	int high_temp_lvalue_num = 0;
	int high_temp_uvalue_num = 0;
	int medium_temp_lvalue_num = 0;
	int medium_temp_uvalue_num = 0;
	int low_temp_lvalue_num = 0;
	int low_temp_uvalue_num = 0;
	int r_value_first_led_num = 0;
	int g_value_first_led_num = 0;
	int b_value_first_led_num = 0;
	int r_value_second_led_num = 0;
	int g_value_second_led_num = 0;
	int b_value_second_led_num = 0;
	int r_value_third_led_num = 0;
	int g_value_third_led_num = 0;
	int b_value_third_led_num = 0;
	int content_length;

	ESP_LOGI(TAG, "/tempRange.json requested");

	// Get the "Content-Length" header to determine the length of the request body
	header_len = httpd_req_get_hdr_value_len(req, "Content-Length");
	if (header_len <= 0)
	{
		// Content-Length header not found or invalid
		// httpd_resp_send_err(req, HTTP_STATUS_411_LENGTH_REQUIRED, "Content-Length header is missing or invalid");
		ESP_LOGI(TAG, "Content-Length header is missing or invalid");
		return ESP_FAIL;
	}

	// Allocate memory to store the header value
	header_value = (char *)malloc(header_len + 1);
	if (httpd_req_get_hdr_value_str(req, "Content-Length", header_value, header_len + 1) != ESP_OK)
	{
		// Failed to get Content-Length header value
		free(header_value);
		// httpd_resp_send_err(req, HTTP_STATUS_BAD_REQUEST, "Failed to get Content-Length header value");
		ESP_LOGI(TAG, "Failed to get Content-Length header value");
		return ESP_FAIL;
	}

	// Convert the Content-Length header value to an integer
	content_length = atoi(header_value);
	free(header_value);

	if (content_length <= 0)
	{
		// Content length is not a valid positive integer
		// httpd_resp_send_err(req, HTTP_STATUS_BAD_REQUEST, "Invalid Content-Length value");
		ESP_LOGI(TAG, "Invalid Content-Length value");
		return ESP_FAIL;
	}

	// Allocate memory for the data buffer based on the content length
	char *data_buffer = (char *)malloc(content_length + 1);

	// Read the request body into the data buffer
	if (httpd_req_recv(req, data_buffer, content_length) <= 0)
	{
		// Handle error while receiving data
		free(data_buffer);
		// httpd_resp_send_err(req, HTTP_STATUS_INTERNAL_SERVER_ERROR, "Failed to receive request body");
		ESP_LOGI(TAG, "Failed to receive request body");
		return ESP_FAIL;
	}

	// Null-terminate the data buffer to treat it as a string
	data_buffer[content_length] = '\0';
	ESP_LOGI(TAG, "Received JSON data: %s", data_buffer);

	// Parse the received JSON data
	cJSON *root = cJSON_Parse(data_buffer);
	if (root == NULL)
	{
		const char *error_ptr = cJSON_GetErrorPtr();
		if (error_ptr != NULL)
		{
			ESP_LOGE(TAG, "Error before: %s", error_ptr);
		}
		return ESP_FAIL; // Or handle the error as needed
	}
	free(data_buffer);

	if (root == NULL)
	{
		// JSON parsing error
		// httpd_resp_send_err(req, HTTP_STATUS_BAD_REQUEST, "Invalid JSON data");
		ESP_LOGI(TAG, "Invalid JSON data");
		return ESP_FAIL;
	}

	cJSON *high_temp_lvalue_json = cJSON_GetObjectItem(root, "high_temp_lvalue");
	cJSON *high_temp_uvalue_json = cJSON_GetObjectItem(root, "high_temp_uvalue");
	cJSON *medium_temp_lvalue_json = cJSON_GetObjectItem(root, "medium_temp_lvalue");
	cJSON *medium_temp_uvalue_json = cJSON_GetObjectItem(root, "medium_temp_uvalue");
	cJSON *low_temp_lvalue_json = cJSON_GetObjectItem(root, "low_temp_lvalue");
	cJSON *low_temp_uvalue_json = cJSON_GetObjectItem(root, "low_temp_uvalue");
	cJSON *r_value_first_led_json = cJSON_GetObjectItem(root, "r_value_first_led");
	cJSON *g_value_first_led_json = cJSON_GetObjectItem(root, "g_value_first_led");
	cJSON *b_value_first_led_json = cJSON_GetObjectItem(root, "b_value_first_led");
	cJSON *r_value_second_led_json = cJSON_GetObjectItem(root, "r_value_second_led");
	cJSON *g_value_second_led_json = cJSON_GetObjectItem(root, "g_value_second_led");
	cJSON *b_value_second_led_json = cJSON_GetObjectItem(root, "b_value_second_led");
	cJSON *r_value_third_led_json = cJSON_GetObjectItem(root, "r_value_third_led");
	cJSON *g_value_third_led_json = cJSON_GetObjectItem(root, "g_value_third_led");
	cJSON *b_value_third_led_json = cJSON_GetObjectItem(root, "b_value_third_led");

	TemperatureValues tempVals;

	tempVals.high_temp_lvalue = atoi(cJSON_GetStringValue(high_temp_lvalue_json));
	tempVals.high_temp_uvalue = atoi(cJSON_GetStringValue(high_temp_uvalue_json));
	tempVals.medium_temp_lvalue = atoi(cJSON_GetStringValue(medium_temp_lvalue_json));
	tempVals.medium_temp_uvalue = atoi(cJSON_GetStringValue(medium_temp_uvalue_json));
	tempVals.low_temp_lvalue = atoi(cJSON_GetStringValue(low_temp_lvalue_json));
	tempVals.low_temp_uvalue = atoi(cJSON_GetStringValue(low_temp_uvalue_json));
	tempVals.r_value_first_led = atoi(cJSON_GetStringValue(r_value_first_led_json));
	tempVals.g_value_first_led = atoi(cJSON_GetStringValue(g_value_first_led_json));
	tempVals.b_value_first_led = atoi(cJSON_GetStringValue(b_value_first_led_json));
	tempVals.r_value_second_led = atoi(cJSON_GetStringValue(r_value_second_led_json));
	tempVals.g_value_second_led = atoi(cJSON_GetStringValue(g_value_second_led_json));
	tempVals.b_value_second_led = atoi(cJSON_GetStringValue(b_value_second_led_json));
	tempVals.r_value_third_led = atoi(cJSON_GetStringValue(r_value_third_led_json));
	tempVals.g_value_third_led = atoi(cJSON_GetStringValue(g_value_third_led_json));
	tempVals.b_value_third_led = atoi(cJSON_GetStringValue(b_value_third_led_json));

	cJSON_Delete(root);

	temperatureQueue = xQueueCreate(10, sizeof(TemperatureValues));
	xQueueSend(temperatureQueue, &tempVals, portMAX_DELAY);

	// Now, you have the SSID and password in ssid_str and pass_str
	ESP_LOGI(TAG, "Received Temp Range High: %d - %d", tempVals.high_temp_lvalue, tempVals.high_temp_uvalue);
	ESP_LOGI(TAG, "Received Temp Range Medium: %d - %d", tempVals.medium_temp_lvalue, tempVals.medium_temp_uvalue);
	ESP_LOGI(TAG, "Received Temp Range Low: %d - %d", tempVals.low_temp_lvalue, tempVals.low_temp_uvalue);
	ESP_LOGI(TAG, "Received first RGB values: %d - %d - %d", tempVals.r_value_first_led, tempVals.g_value_first_led, tempVals.b_value_first_led);

	rgb_led_http_received();
	return ESP_OK;
}

/**
 * wifiConnectStatus handler updates the connection status for the web page.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_wifi_connect_status_json_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "/wifiConnectStatus requested");

	char statusJSON[100];

	sprintf(statusJSON, "{\"wifi_connect_status\":%d}", g_wifi_connect_status);

	httpd_resp_set_type(req, "application/json");
	httpd_resp_send(req, statusJSON, strlen(statusJSON));

	return ESP_OK;
}

/**
 * Sets up the default httpd server configuration.
 * @return http server instance handle if successful, NULL otherwise.
 */
static httpd_handle_t http_server_configure(void)
{
	// Generate the default configuration
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();

	// Create HTTP server monitor task
	xTaskCreatePinnedToCore(&http_server_monitor, "http_server_monitor", HTTP_SERVER_MONITOR_STACK_SIZE, NULL, HTTP_SERVER_MONITOR_PRIORITY, &task_http_server_monitor, HTTP_SERVER_MONITOR_CORE_ID);

	// Create the message queue
	http_server_monitor_queue_handle = xQueueCreate(3, sizeof(http_server_queue_message_t));

	// The core that the HTTP server will run on
	config.core_id = HTTP_SERVER_TASK_CORE_ID;

	// Adjust the default priority to 1 less than the wifi application task
	config.task_priority = HTTP_SERVER_TASK_PRIORITY;

	// Bump up the stack size (default is 4096)
	config.stack_size = HTTP_SERVER_TASK_STACK_SIZE;

	// Increase uri handlers
	config.max_uri_handlers = 20;

	// Increase the timeout limits
	config.recv_wait_timeout = 10;
	config.send_wait_timeout = 10;

	ESP_LOGI(TAG,
			 "http_server_configure: Starting server on port: '%d' with task priority: '%d'",
			 config.server_port,
			 config.task_priority);

	// Start the httpd server
	if (httpd_start(&http_server_handle, &config) == ESP_OK)
	{
		ESP_LOGI(TAG, "http_server_configure: Registering URI handlers");

		// register query handler
		httpd_uri_t jquery_js = {
			.uri = "/jquery-3.3.1.min.js",
			.method = HTTP_GET,
			.handler = http_server_jquery_handler,
			.user_ctx = NULL};
		httpd_register_uri_handler(http_server_handle, &jquery_js);

		// register index.html handler
		httpd_uri_t index_html = {
			.uri = "/",
			.method = HTTP_GET,
			.handler = http_server_index_html_handler,
			.user_ctx = NULL};
		httpd_register_uri_handler(http_server_handle, &index_html);

		// register app.css handler
		httpd_uri_t app_css = {
			.uri = "/app.css",
			.method = HTTP_GET,
			.handler = http_server_app_css_handler,
			.user_ctx = NULL};
		httpd_register_uri_handler(http_server_handle, &app_css);

		// register app.js handler
		httpd_uri_t app_js = {
			.uri = "/app.js",
			.method = HTTP_GET,
			.handler = http_server_app_js_handler,
			.user_ctx = NULL};
		httpd_register_uri_handler(http_server_handle, &app_js);

		// register favicon.ico handler
		httpd_uri_t favicon_ico = {
			.uri = "/favicon.ico",
			.method = HTTP_GET,
			.handler = http_server_favicon_ico_handler,
			.user_ctx = NULL};
		httpd_register_uri_handler(http_server_handle, &favicon_ico);

		// Register the ADC value handler
		httpd_uri_t adc_value = {
			.uri = "/adc_value",
			.method = HTTP_GET,
			.handler = http_server_adc_value_handler,
			.user_ctx = NULL};
		httpd_register_uri_handler(http_server_handle, &adc_value);

		httpd_uri_t ntp_value = {
			.uri = "/ntp_value",
			.method = HTTP_GET,
			.handler = http_server_ntp_value_handler,
			.user_ctx = NULL};
		httpd_register_uri_handler(http_server_handle, &ntp_value);

		// Register the Range value handler
		httpd_uri_t temp_range = {
			.uri = "/tempRange.json",
			.method = HTTP_POST,
			.handler = http_server_temp_range_handler,
			.user_ctx = NULL};
		httpd_register_uri_handler(http_server_handle, &temp_range);

		// register wifiConnect.json handler
		httpd_uri_t wifi_connect_json = {
			.uri = "/wifiConnect.json",
			.method = HTTP_POST,
			.handler = http_server_wifi_connect_json_handler,
			.user_ctx = NULL};
		httpd_register_uri_handler(http_server_handle, &wifi_connect_json);

		// register wifiConnectStatus.json handler
		httpd_uri_t wifi_connect_status_json = {
			.uri = "/wifiConnectStatus",
			.method = HTTP_POST,
			.handler = http_server_wifi_connect_status_json_handler,
			.user_ctx = NULL};
		httpd_register_uri_handler(http_server_handle, &wifi_connect_status_json);

		return http_server_handle;
	}

	return NULL;
}

void http_server_start(void)
{
	if (http_server_handle == NULL)
	{
		http_server_handle = http_server_configure();
	}
}

void http_server_stop(void)
{
	if (http_server_handle)
	{
		httpd_stop(http_server_handle);
		ESP_LOGI(TAG, "http_server_stop: stopping HTTP server");
		http_server_handle = NULL;
	}
	if (task_http_server_monitor)
	{
		vTaskDelete(task_http_server_monitor);
		ESP_LOGI(TAG, "http_server_stop: stopping HTTP server monitor");
		task_http_server_monitor = NULL;
	}
}

BaseType_t http_server_monitor_send_message(http_server_message_e msgID)
{
	http_server_queue_message_t msg;
	msg.msgID = msgID;
	return xQueueSend(http_server_monitor_queue_handle, &msg, portMAX_DELAY);
}

void http_server_fw_update_reset_callback(void *arg)
{
	ESP_LOGI(TAG, "http_server_fw_update_reset_callback: Timer timed-out, restarting the device");
	esp_restart();
}