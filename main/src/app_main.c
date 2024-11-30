/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "debug.h"
#include "globals.h"
#include "mqtt_app.h"
#include "os.h"
#include "spi_app.h"

#ifdef DEBUG
EXT_RAM_BSS_ATTR RtosDefineTaskSized(debug_task, debug_thread, 4096);
#endif
RtosDefineTaskSized(spi_app_task, spi_app_thread, 2048);
RtosDefineTaskSized(spi_app_publish_task, spi_app_publish_thread, 2048);


void app_main(void) {
	esp_log_level_set("*", ESP_LOG_NONE);
	esp_log_level_set(TAG, ESP_LOG_INFO);
	// esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
	// esp_log_level_set("mqtt_example", ESP_LOG_VERBOSE);
	// esp_log_level_set("transport_base", ESP_LOG_VERBOSE);
	// esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
	// esp_log_level_set("transport", ESP_LOG_VERBOSE);
	// esp_log_level_set("outbox", ESP_LOG_VERBOSE);

	ESP_LOGI(TAG, "[APP] Startup..");
	ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
	ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

#ifdef DEBUG
	RtosStaticTaskCreateToCore(debug_task, 1, NULL, 1);
#endif

	RtosStaticTaskCreateToCore(spi_app_task, 4, NULL, 0);
	RtosStaticTaskCreateToCore(spi_app_publish_task, 4, NULL, 1);
	mqtt5_app_start();
}

// stack overflow handler
void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName) {
	ESP_LOGE(TAG, "Stack overflow in task %s", pcTaskName);
	abort();
}
