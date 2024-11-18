#include "debug.h"

#include "os.h"
#include "str_seq_buf.h"

#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#ifdef DEBUG

DEFINE_STR_SEQ_BUF(debug_buf, DEBUG_BUF_SIZE);

static char print_buf[DEBUG_BUF_SIZE] = {0};

void debug_print(const esp_log_level_t level, const char* format, ...) {
	va_list args;
	va_start(args, format);

	print_buf[0] = level;
	vsnprintf(print_buf + 1, DEBUG_BUF_SIZE - 1, format, args);

	const size_t len = strlen(print_buf) + 1; // include null terminator

	strbuf_write(&debug_buf, print_buf, len);

	extern RtosStaticTask_t debug_task;
	if (debug_task.handle != NULL && eTaskGetState(debug_task.handle) == eBlocked) {
		xTaskNotifyGive(debug_task.handle);
	}

	va_end(args);
}

static void debug_output_action(const char* str) {
	const esp_log_level_t level = str[0];
	ESP_LOG_LEVEL_LOCAL(level, TAG, "%s", str + 1);
}

void debug_thread(void* par) {
	while (1) {
		ulTaskNotifyTake(pdTRUE, ms_to_ticks(1000));
		ESP_LOGI(TAG, "Debug thread running");
		ESP_LOGI(TAG, "Available slots: %d", (int)strbuf_get_available_slots(&debug_buf));
		strbuf_read_all_with_action(&debug_buf, debug_output_action, DEBUG_BUF_SIZE);
	}
}

#else // DEBUG

void debug_thread(void* par) { vTaskDelete(NULL); }

#endif // DEBUG