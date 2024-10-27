#include "globals.h"
#include "mqtt_utils.h"
#include "os.h"

#include <esp_event.h>
#include <esp_netif.h>
#include <esp_system.h>
#include <mqtt_client.h>
#include <nvs_flash.h>
#include <protocol_examples_common.h>


void mqtt5_app_start(void) {
	esp_log_level_set("*", ESP_LOG_INFO);
	esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
	esp_log_level_set("mqtt_example", ESP_LOG_VERBOSE);
	esp_log_level_set("transport_base", ESP_LOG_VERBOSE);
	esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
	esp_log_level_set("transport", ESP_LOG_VERBOSE);
	esp_log_level_set("outbox", ESP_LOG_VERBOSE);

	connect_to_wifi();

	char broker_ip[16] = {0};
	while (!find_mqtt_ip(broker_ip)) {
		delay(ms_to_ticks(100));
	}
	char broker_url[16 + 7 + 5] = {0};
	sprintf(broker_url, "mqtt://%s:1883", broker_ip);
	ESP_LOGI(TAG, "Connecting to mqtt broker: %s", broker_url);

	esp_mqtt5_connection_property_config_t connect_property = {
		.session_expiry_interval = 10,
		.maximum_packet_size = 1024,
		.receive_maximum = 65535,
		.topic_alias_maximum = 2,
		.request_resp_info = true,
		.request_problem_info = true,
		.will_delay_interval = 10,
		.payload_format_indicator = true,
		.message_expiry_interval = 10,
		.response_topic = "/shoepad/data",
		.correlation_data = "123456",
		.correlation_data_len = 6,
	};

	esp_mqtt_client_config_t mqtt5_cfg = {
		.broker.address.uri = broker_url,
		.session.protocol_ver = MQTT_PROTOCOL_V_5,
		.network.disable_auto_reconnect = true,
		// .credentials.username = "123",
		// .credentials.authentication.password = "456",
		.session.last_will.topic = "/topic/will",
		.session.last_will.msg = "i will leave",
		.session.last_will.msg_len = 12,
		.session.last_will.qos = 2,
		.session.last_will.retain = true,
	};

	esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt5_cfg);

	// /* Set connection properties and user properties */
	// esp_mqtt5_client_set_user_property(&connect_property.user_property, user_property_arr, USE_PROPERTY_ARR_SIZE);
	// esp_mqtt5_client_set_user_property(&connect_property.will_user_property, user_property_arr,
	// USE_PROPERTY_ARR_SIZE); esp_mqtt5_client_set_connect_property(client, &connect_property);

	// /* If you call esp_mqtt5_client_set_user_property to set user properties, DO
	//  * NOT forget to delete them. esp_mqtt5_client_set_connect_property will
	//  * malloc buffer to store the user_property and you can delete it after
	//  */
	// esp_mqtt5_client_delete_user_property(connect_property.user_property);
	// esp_mqtt5_client_delete_user_property(connect_property.will_user_property);

	// /* The last argument may be used to pass data to the event handler, in this
	//  * example mqtt_event_handler */
	// esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt5_event_handler, NULL);
	// esp_mqtt_client_start(client);
}