#ifndef _MQTT_APP_H
#define _MQTT_APP_H

#include "macro_utils.h"

#include <stdint.h>


#define ESP_MQTT_STATUS_TABLE(X) \
	X(STATUS_OFFLINE)            \
	X(STATUS_ONLINE)

typedef enum {
	ESP_MQTT_STATUS_TABLE(X_EXPAND_ENUM) NUM_OF_ESP_MQTT_STATUS,
} esp_mqtt_status_t;

void mqtt_publish_sensor_data(const uint8_t sensor_id, const char* data);

void mqtt_publish_sensor_cal_end(const uint8_t sensor_id);

void mqtt5_app_start(void);

#endif // _MQTT_APP_H