#include "mqtt_app.h"

#include "debug.h"
#include "globals.h"
#include "mqtt_utils.h"
#include "os.h"

#include <esp_event.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <esp_system.h>
#include <mqtt_client.h>
#include <nvs_flash.h>
#include <stdlib.h>

esp_mqtt_client_handle_t client;
esp_mqtt_status_t mqtt_status = STATUS_OFFLINE;

static const char app_topics[] = "app/#";

static char esp_id[6 * 2 + 1] = {0};
static char status_topic[sizeof(esp_id) + 4 + 7] = {0}; // sizeof(esp_id) already include space for /0
void esp_id_init(void) {
	uint8_t mac[6];
	esp_read_mac(mac, ESP_MAC_WIFI_STA);
	sprintf(esp_id, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	sprintf(status_topic, "esp/%s/status", esp_id);
}

void mqtt_publish_sensor_data(const uint8_t sensor_id, const char* data) {
	if (mqtt_status != STATUS_ONLINE) {
		return;
	}
	char data_topic[sizeof(esp_id) + 4 + 2 + 3] = {0};
	sprintf(data_topic, "esp/%s/d%d", esp_id, sensor_id);
	esp_mqtt_client_publish(client, data_topic, data, strlen(data), 1, 0);
}

static void mqtt_connection_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id,
										  void* event_data) {
	switch (event_id) {
		case MQTT_EVENT_CONNECTED: {
			LOGI("MQTT_EVENT_CONNECTED");
			mqtt_status = STATUS_ONLINE;
			esp_mqtt_client_subscribe(client, app_topics, 2);
		} break;
		case MQTT_EVENT_DISCONNECTED: {
			LOGI("MQTT_EVENT_DISCONNECTED");
			mqtt_status = STATUS_OFFLINE;
			esp_mqtt_client_reconnect(client);
		} break;
		case MQTT_EVENT_ERROR: {
			LOGI("MQTT_EVENT_ERROR");
			mqtt_status = STATUS_OFFLINE;
			esp_mqtt_client_reconnect(client);
		} break;
		default: break;
	}
}

static void mqtt_topic_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
	// esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
	// LOGI("TOPIC: %.*s", event->topic_len, event->topic);
	// LOGI("DATA: %.*s", event->data_len, event->data);
}

static void mqtt_data_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
	esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
	LOGI("TOPIC: %.*s", event->topic_len, event->topic);
	LOGI("DATA: %.*s", event->data_len, event->data);
}

static void mqtt_event_handler(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id,
							   void* event_data) {
	switch (event_id) {
		case MQTT_EVENT_ERROR:
		case MQTT_EVENT_CONNECTED:
		case MQTT_EVENT_DISCONNECTED:
		case MQTT_EVENT_BEFORE_CONNECT: {
			mqtt_connection_event_handler(event_handler_arg, event_base, event_id, event_data);
		} break;
		case MQTT_EVENT_SUBSCRIBED:
		case MQTT_EVENT_UNSUBSCRIBED:
		case MQTT_EVENT_PUBLISHED: {
			mqtt_topic_event_handler(event_handler_arg, event_base, event_id, event_data);
		}
		case MQTT_EVENT_DATA: {
			mqtt_data_event_handler(event_handler_arg, event_base, event_id, event_data);
		}
		default: break; // ignore
	}
}


void mqtt5_app_start(void) {
	esp_id_init();

	connect_to_wifi();

	char broker_ip[16] = {0};
	while (!find_mqtt_ip(broker_ip)) {
		delay(ms_to_ticks(NET_RETRY_INTERVAL_MS));
	}
	char broker_url[16 + 7 + 5] = {0};
	sprintf(broker_url, "mqtt://%s:1883", broker_ip);
	LOGI("Connecting to mqtt broker: %s", broker_url);

	const char will_msg[] = {STATUS_OFFLINE + '0', '\0'};
	esp_mqtt_client_config_t mqtt5_cfg = {
		.broker.address.uri = broker_url,
		.broker.address.port = 1883,
		.credentials.client_id = esp_id,
		.session.protocol_ver = MQTT_PROTOCOL_V_5,
		.session.last_will.topic = status_topic,
		.session.last_will.qos = 2,
		.session.last_will.msg = will_msg,
		.session.last_will.msg_len = sizeof(will_msg),
	};

	client = esp_mqtt_client_init(&mqtt5_cfg);

	// /* The last argument may be used to pass data to the event handler, in this
	//  * example mqtt_event_handler */
	esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, mqtt_event_handler, NULL);
	esp_mqtt_client_start(client);
}