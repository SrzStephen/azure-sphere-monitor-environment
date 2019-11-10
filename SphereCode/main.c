
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h> 
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include "applibs_versions.h"
#include "azure_iot_utilities.h"
#include "connection_strings.h"
#include <applibs/storage.h>
#include <applibs/log.h>
#include <applibs/wificonfig.h>
#include <azureiot/iothub_device_client_ll.h>
#define UART_STRUCTS_VERSION 1
#include <applibs/uart.h>
#include <applibs/gpio.h>
#include "mt3620_avnet_dev.h"
#include "sd1306.h"
#include "mh_z19.h"
#include "pms7003.h"
#include "parson.h"
#include <pthread.h>
#include "polltime.h"

pthread_mutex_t lock;
//if unset default wait time between reports is 30 sec
int wait_time = 30;


JSON_Value *root_value;
JSON_Object *root_object;
extern IOTHUB_DEVICE_CLIENT_LL_HANDLE iothubClientHandle;
static int uartFd = -1;
static bool connectedToIoTHub = false;
GPIO_Value_Type gpvalue;
static int epollFd = -1;
static int azureIotDoWorkTimerFd = -1;
static volatile sig_atomic_t terminationRequired = false;
bool global_connected = 0;
long last_response = 0;
WifiConfig_ConnectedNetwork network;
char dustString[14];
char co2string[14];
char time_since_last[14];
char window_status[14];

static void *SetupHeapMessage(const char *messageFormat, size_t maxLength, ...)
{
	va_list args;
	va_start(args, maxLength);
	char *message =
		malloc(maxLength + 1); // Ensure there is space for the null terminator put by vsnprintf.
	if (message != NULL) {
		vsnprintf(message, maxLength, messageFormat, args);
	}
	va_end(args);
	return message;
}
static int DirectMethodCall(const char *methodName, const char *payload, size_t payloadSize, char **responsePayload, size_t *responsePayloadSize)
{
	// there is only one method supported "polltime" that should look like { "polltime":5 }
	Log_Debug("\nDirect Method called %s\n", methodName);
	Log_Debug("\nPayload %s/n", payload);
	if (!strcmp(methodName, "polltime"))
	{
		static const char response[] =
			"{ \"success\" : false, \"message\" : \"Unknown method\" }";
		size_t responseMaxLength = sizeof(response) + strlen(payload);
		*responsePayload = SetupHeapMessage(response, responseMaxLength, strlen(response));
		*responsePayloadSize = strlen(*responsePayload);
		return 200;
	}

	int polltime = -1;
	JSON_Object *json_obj = json_parse_string(payload);
	Log_Debug(json_serialize_to_string_pretty(json_obj));
	if (json_object_has_value(json_object(json_obj), "polltime") == 1)
	{
		if (json_object_has_value_of_type(json_object(json_obj), "polltime", JSONNumber) == 1)
		{
			polltime = json_object_get_number(json_object(json_obj), "polltime");

			if (change_polling_time(polltime))
			{
				static const char response[] =
					"{ \"success\" : true, \"message\" : \"Polling time updated\" }";
				size_t responseMaxLength = sizeof(response) + strlen(payload);
				*responsePayload = SetupHeapMessage(response, responseMaxLength, strlen(response));
			}
			else
			{
				static const char response[] =
					"{ \"success\" : true, \"message\" : \"Failed to update polling time\" }";
				size_t responseMaxLength = sizeof(response) + strlen(payload);
				*responsePayload = SetupHeapMessage(response, responseMaxLength, strlen(response));
			}

		}
		else
		{
			static const char response[] =
				"{ \"success\" : false, \"message\" : \"Polltime isn't a number\" }";
			size_t responseMaxLength = sizeof(response) + strlen(payload);
			*responsePayload = SetupHeapMessage(response, responseMaxLength, strlen(response));
		}

	}
	else
	{
		static const char response[] =
			"{ \"success\" : false, \"message\" : \"Invalid JSON format\" }";
		size_t responseMaxLength = sizeof(response) + strlen(payload);
		*responsePayload = SetupHeapMessage(response, responseMaxLength, strlen(response));

	}
	json_value_free(json_obj);


	*responsePayloadSize = strlen(*responsePayload);
	return 200;


}

static void IoTHubConnectionStatusChanged(bool connected)
{
	global_connected = connected;
	Log_Debug("Connection status %d\n", connected);
	connectedToIoTHub = connected;
}
static void MessageReceived(const char *payload)
{
	Log_Debug("Message received from the Azure IoT Hub: %d\n", payload);
}
static void SendMessageCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void *context)
{
	Log_Debug("INFO: Message received by IoT Hub. Result is: %d\n", result);
	last_response = time(NULL);
}

static void SetupAzureClient(void)
{
	if (iothubClientHandle != NULL)
	{
		IoTHubDeviceClient_LL_Destroy(iothubClientHandle);
	}
	if (!AzureIoT_SetupClient()) {
		Log_Debug("ERROR: Failed to set up IoT Hub client\n");
	}
	AzureIoT_DoPeriodicTasks();
	sleep(10);
	AzureIoT_SetConnectionStatusCallback(&IoTHubConnectionStatusChanged);
	AzureIoT_SetDirectMethodCallback(&DirectMethodCall);
	AzureIoT_SetMessageReceivedCallback(&SendMessageCallback);



}

void  construct_json_object(struct PMS7003 p7003_dat, struct MHZ19_DATA mhz19_dat)
{
	root_value = json_value_init_object();
	root_object = json_value_get_object(root_value);
	if (p7003_dat.error >= 0)
	{
		json_object_dotset_number(root_object, "pms7003.PM1_0", p7003_dat.concPM1_0);
		json_object_dotset_number(root_object, "pms7003.PM2_5", p7003_dat.conc_PM2_5);
		json_object_dotset_number(root_object, "pms7003.PM10", p7003_dat.conc_PM10_0);
		json_object_dotset_number(root_object, "pms7003.r0_3", p7003_dat.raw0_3);
		json_object_dotset_number(root_object, "pms7003.r0_5", p7003_dat.raw0_5);
		json_object_dotset_number(root_object, "pms7003.r1_0", p7003_dat.raw1_0);
		json_object_dotset_number(root_object, "pms7003.r2_5", p7003_dat.raw2_5);
		json_object_dotset_number(root_object, "pms7003.r5_0", p7003_dat.raw5_0);
		json_object_dotset_number(root_object, "pms7003.r10", p7003_dat.raw10_0);
	}
	if (mhz19_dat.error >= 0)
	{
		json_object_dotset_number(root_object, "mhz19.co2", mhz19_dat.co2ppm);
		json_object_dotset_number(root_object, "mhz19.temp", mhz19_dat.temperature);
	}
	json_object_set_number(root_object, "window", gpvalue == GPIO_Value_High);

	AzureIoT_SendMessage(json_serialize_to_string_pretty(root_value));

	AzureIoT_TwinReportStateJson(json_serialize_to_string_pretty(root_value), (size_t)strlen(json_serialize_to_string_pretty(root_value)));

	json_value_free(root_object);
}


void init_screen(void)
{
	i2cFd = I2CMaster_Open(MT3620_RDB_HEADER4_ISU2_I2C);
	I2CMaster_SetBusSpeed(i2cFd, I2C_BUS_SPEED_STANDARD);
	I2CMaster_SetTimeout(i2cFd, 100);
	sd1306_init();
	clear_oled_buffer();
	sd1306_draw_string(OLED_TITLE_X, OLED_TITLE_Y, "YAY! up", FONT_SIZE_TITLE, white_pixel);
	sd1306_draw_string(OLED_LINE_1_X, OLED_LINE_1_Y, network.ssid, FONT_SIZE_TITLE, white_pixel);
	sd1306_refresh();

}

void update_screen(struct PMS7003 p7003_dat, struct MHZ19_DATA mhz19_dat)
{
	memset(dustString, 0, sizeof(dustString));
	memset(co2string, 0, sizeof(co2string));
	memset(time_since_last, 0, sizeof(time_since_last));
	memset(window_status, 0, sizeof(window_status));
	sprintf(dustString, "PM2.5: %i", p7003_dat.conc_PM2_5);
	sprintf(co2string, "co2: %i", mhz19_dat.co2ppm);
	sprintf(window_status, "Window %i", !(gpvalue == GPIO_Value_High));
	sprintf(time_since_last, "Last MSG %i", time(NULL) - last_response);

	clear_oled_buffer();
	sd1306_draw_string(OLED_TITLE_X, OLED_TITLE_Y, network.ssid, FONT_SIZE_TITLE, white_pixel);
	sd1306_draw_string(OLED_LINE_1_X, OLED_LINE_1_Y, dustString, FONT_SIZE_TITLE, white_pixel);
	sd1306_draw_string(OLED_LINE_3_X, OLED_LINE_3_Y, co2string, FONT_SIZE_TITLE, white_pixel);

	sd1306_refresh();
}

int main(int argc, char *argv[])
{
	init_lock();
	init_screen();
	int pin14 = GPIO_OpenAsInput(AVT_MODULE_GPIO16);
	int new_poll_time = -1;
	Log_Debug("pin status %i\n", pin14);
	UART_Config uartConfig;
	UART_InitConfig(&uartConfig);
	uartConfig.baudRate = 9600;
	int ISU0_fd = UART_Open(4, &uartConfig);
	int ISU1_fd = UART_Open(5, &uartConfig);
	struct PMS7003 pms7003_data;
	struct MHZ19_DATA mhz19;
	char ssid[128];
	memset(ssid, 0, 128);
	int res = WifiConfig_GetCurrentNetwork(&network);
	SetupAzureClient();
	memcpy(ssid, (char*)&network.ssid, network.ssidLength);
	Log_Debug("SSID: %s\n", ssid);
	Log_Debug("Version String: %s\n", argv[1]);
	Log_Debug("Network snr: %i\n", network.signalRssi);
	mhz19 = get_mh_z19_value(ISU0_fd);
	// sensor needs ~3 min to give stable readings
	//sleep(200);
	while (true)
	{
		AzureIoT_DoPeriodicTasks();
		mhz19 = get_mh_z19_value(ISU0_fd);
		Log_Debug("Co2 %i  \n", mhz19.co2ppm);
		Log_Debug("temp %i \n", mhz19.temperature);
		pms7003_data = get_pms7003(ISU1_fd);
		Log_Debug("Dust level %i \n", pms7003_data.raw0_3);
		GPIO_GetValue(pin14, &gpvalue);
		update_screen(pms7003_data, mhz19);
		//construct_json_object(pms7003_data, mhz19);
		Log_Debug("Window is %i \n", gpvalue == GPIO_Value_High);

		new_poll_time = get_polling_time();
		if (new_poll_time > 0)
		{
			wait_time = new_poll_time;
		}
		for (int ii = 0; ii < wait_time; ii = ii + 10)
		{
			AzureIoT_DoPeriodicTasks();
			sleep(10);
		}
	}
	return 0;
}
