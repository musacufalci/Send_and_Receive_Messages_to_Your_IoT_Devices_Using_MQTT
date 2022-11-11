
/*
	****************************************************************************************
	Filename   :    mqtt.c
	Description:    Send and Receive Messages to your IoT Devices (ESP32) using MQTT.
			
	Generated by Musa ÇUFALCI 2019
	Copyright (c) Musa ÇUFALCI All rights reserved.
	*****************************************************************************************
*/


/*-----------------------------------------------------------------------------
Filename: mqtt.c
Description: mqtt function
Generated by Musa ÇUFALCI. 2019
----------------------------------------------------------------------------- */
/*
-----------------------------------------------------------------------------
A U T H O R I D E N T I T Y
-----------------------------------------------------------------------------
Musa  ÇUFALCI
-----------------------------------------------------------------------------
R E V I S I O N H I S T O R Y
-----------------------------------------------------------------------------
-----------------------------------------------------------------------------
2019 MK Added First creating after project editing
-----------------------------------------------------------------------------
*/


#include <string.h>
#include <stdlib.h>

#include "sdkconfig.h"

#include "MQTTClient.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "mbedtls/platform.h"
#include "mbedtls/net.h"
#include "mbedtls/debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/certs.h"

#define EXAMPLE_WIFI_SSID "TP-LINK_34DE"
#define EXAMPLE_WIFI_PASS "E665RMFN66"

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
   
const int CONNECTED_BIT = BIT0;

/* Constants that aren't configurable in menuconfig */

#define MQTT_SERVER "iot.eclipse.org"
#define MQTT_PORT 1883
#define MQTT_BUF_SIZE 1000
#define MQTT_WEBSOCKET 1  // 0=no 1=yes

static unsigned char mqtt_sendBuf[MQTT_BUF_SIZE];
static unsigned char mqtt_readBuf[MQTT_BUF_SIZE];

static const char *TAG = "MQTTS";

/* FreeRTOS event group to signal when we are connected & ready to make a request */
EventGroupHandle_t wifi_event_group;

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}


static void initialise_wifi(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_WIFI_SSID,
            .password = EXAMPLE_WIFI_PASS,
        },
    };
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

static void mqtt_task(void *pvParameters)
{
	int ret;
	Network network;

    while(1) {
        /* Wait for the callback to set the CONNECTED_BIT in the
           event group.
        */
        xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                            false, true, portMAX_DELAY);
        ESP_LOGI(TAG, "Connected to AP");

		ESP_LOGI(TAG, "Start MQTT Task ...");

		MQTTClient client;
		NetworkInit(&network);
		network.websocket = MQTT_WEBSOCKET;

		ESP_LOGI(TAG,"NetworkConnect %s:%d ...",MQTT_SERVER,MQTT_PORT);
		NetworkConnect(&network, MQTT_SERVER, MQTT_PORT);
		ESP_LOGI(TAG,"MQTTClientInit  ...");
		MQTTClientInit(&client, &network,
			2000,          	//command_timeout_ms
			mqtt_sendBuf,  	//sendbuf,
			MQTT_BUF_SIZE, 	//sendbuf_size,
			mqtt_readBuf,  	//readbuf,
			MQTT_BUF_SIZE  	//readbuf_size
		);

		MQTTString clientId = MQTTString_initializer;
		clientId.cstring = "ESP32MQTT";

		MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
		data.clientID          = clientId;
		data.willFlag          = 0;
		data.MQTTVersion       = 4; // 3 = 3.1 4 = 3.1.1
		data.keepAliveInterval = 60;
		data.cleansession      = 1;

		ESP_LOGI(TAG,"MQTTConnect  ...");
		ret = MQTTConnect(&client, &data);
		if (ret != SUCCESS) {
			ESP_LOGI(TAG, "MQTTConnect not SUCCESS: %d", ret);
			goto exit;
		}

		char msgbuf[100];
		for (int i=0;i<5;i++) {

			MQTTMessage message;
			sprintf(msgbuf, "field1=%d&field2=%lf",(uint8_t)(esp_random()&0xFF),(double)((esp_random()&0xFFFF)/10));

			ESP_LOGI(TAG, "MQTTPublish  ... %s",msgbuf);
			message.qos = QOS0;
			message.retained = false;
			message.dup = false;
			message.payload = (void*)msgbuf;
			message.payloadlen = strlen(msgbuf)+1;

			ret = MQTTPublish(&client, "ortem/test", &message);
			if (ret != SUCCESS) {
				ESP_LOGI(TAG, "MQTTPublish not SUCCESS: %d", ret);
				goto exit;
			}
			for(int countdown = 30; countdown >= 0; countdown--) {
				if(countdown%10==0) {
					ESP_LOGI(TAG, "%d...", countdown);
				}
				vTaskDelay(1000 / portTICK_RATE_MS);
			}
		}
		exit:
			MQTTDisconnect(&client);
			NetworkDisconnect(&network);
			for(int countdown = 60; countdown >= 0; countdown--) {
				if(countdown%10==0) {
					ESP_LOGI(TAG, "%d...", countdown);
				}
				vTaskDelay(1000 / portTICK_RATE_MS);
			}
			ESP_LOGI(TAG, "Starting again!");
    }
 }

