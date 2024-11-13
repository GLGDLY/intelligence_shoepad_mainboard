#include "debug.h"

#include "os.h"

#include <freertos/FreeRTOS.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#ifdef DEBUG

char debug_buf[DEBUG_BUF_SIZE] = {0};
uint32_t debug_buf_head = 0;
uint32_t debug_buf_end = 0;
SemaphoreHandle_t debug_buf_mux = NULL;

inline uint32_t debug_get_available_slots() {
	return (debug_buf_end + DEBUG_BUF_SIZE - debug_buf_head) % DEBUG_BUF_SIZE;
}
inline void debug_buf_shift_head() { debug_buf_head = (debug_buf_head + 1) % DEBUG_BUF_SIZE; }

void debug_write_nolock(const char* buf, const size_t len) {
	if (debug_buf_end + len > DEBUG_BUF_SIZE) {
		const size_t len1 = DEBUG_BUF_SIZE - debug_buf_end;
		memcpy(&debug_buf[debug_buf_end], buf, len1);
		memcpy(&debug_buf[0], &buf[len1], len - len1);
		debug_buf_end = len - len1;
	} else {
		memcpy(&debug_buf[debug_buf_end], buf, len);
		debug_buf_end += len;
	}
}

esp_log_level_t debug_read_once_nolock(char* buf, const size_t max_len) {
	esp_log_level_t level = debug_buf[debug_buf_head];
	debug_buf_shift_head();
	int i = 0;
	while (debug_buf[debug_buf_head] != '\0' && i < max_len - 1) {
		buf[i++] = debug_buf[debug_buf_head];
		debug_buf_shift_head();
	}
	buf[i] = '\0';
	debug_buf_shift_head();
	return level;
}

static char print_buf[DEBUG_BUF_SIZE] = {0};

void debug_print(const esp_log_level_t level, const char* format, ...) {
	va_list args;
	va_start(args, format);

	print_buf[0] = level;
	vsnprintf(print_buf + 1, DEBUG_BUF_SIZE - 1, format, args);

	const size_t len = strlen(print_buf) + 1; // include null terminator

	if (len > debug_get_available_slots()) { // buffer full
		return;
	}

	xSemaphoreTake(debug_buf_mux, portMAX_DELAY);
	debug_write_nolock(print_buf, len);
	xSemaphoreGive(debug_buf_mux);

	extern RtosStaticTask_t debug_task;
	if (debug_task.handle != NULL && eTaskGetState(debug_task.handle) == eBlocked) {
		xTaskNotifyGive(debug_task.handle);
	}

	va_end(args);
}

void debug_init() { debug_buf_mux = xSemaphoreCreateMutex(); }

void debug_thread(void* par) {
	debug_init();

	char _buf[DEBUG_BUF_SIZE] = {0};
	while (1) {
		ulTaskNotifyTake(pdTRUE, ms_to_ticks(1000));
		ESP_LOGI(TAG, "Debug thread running");
		ESP_LOGI(TAG, "Available slots: %d", debug_get_available_slots());
		if (debug_buf_head != debug_buf_end) {
			xSemaphoreTake(debug_buf_mux, portMAX_DELAY);

			while (debug_buf_head != debug_buf_end) {
				const esp_log_level_t level = debug_read_once_nolock(_buf, DEBUG_BUF_SIZE);
				ESP_LOG_LEVEL_LOCAL(level, TAG, "%s", _buf);
			}

			xSemaphoreGive(debug_buf_mux);
		}
	}
}

#else // DEBUG

void debug_thread(void* par) { vTaskDelete(NULL); }

#endif // DEBUG