#ifndef _DEBUG_H_
#define _DEBUG_H_

#include "config.h"

#ifdef DEBUG

	#include "esp_log.h"

void debug_print(const esp_log_level_t level, const char* format, ...) __attribute__((format(printf, 2, 3)));

	#define LOGI(format, ...) debug_print(ESP_LOG_INFO, format, ##__VA_ARGS__)
	#define LOGW(format, ...) debug_print(ESP_LOG_WARN, format, ##__VA_ARGS__)
	#define LOGE(format, ...) debug_print(ESP_LOG_ERROR, format, ##__VA_ARGS__)
	#define LOGD(format, ...) debug_print(ESP_LOG_DEBUG, format, ##__VA_ARGS__)

#else // DEBUG

	#define LOGI(format, ...)
	#define LOGW(format, ...)
	#define LOGE(format, ...)
	#define LOGD(format, ...)

	#ifdef ESP_LOG_LEVEL_LOCAL
		#undef ESP_LOG_LEVEL_LOCAL
	#endif
	#define ESP_LOG_LEVEL_LOCAL(level, tag, format, ...)

#endif // DEBUG

void debug_thread(void* par);

#endif // _DEBUG_H_
