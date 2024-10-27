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


RtosDefineTask(spi_app_task, spi_app_thread);

#ifdef DEBUG
RtosDefineTaskSized(debug_task, debug_thread, 4096);
#endif

void app_main(void) {
	ESP_LOGI(TAG, "[APP] Startup..");
	ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
	ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

	RtosStaticTaskCreateToCore(spi_app_task, 4, NULL, 0);

#ifdef DEBUG
	RtosStaticTaskCreateToCore(debug_task, 1, NULL, 1);
#endif

	// TODO: test and add back mqtt stuff later
	// mqtt5_app_start();
}

// stack overflow handler
void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName) {
	ESP_LOGE(TAG, "Stack overflow in task %s", pcTaskName);
	abort();
}
