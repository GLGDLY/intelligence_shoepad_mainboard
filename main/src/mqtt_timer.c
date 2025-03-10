#include "mqtt_timer.h"

#include <driver/gptimer.h>

uint64_t mqtt_time_ms = 0;
gptimer_handle_t mqtt_timer = NULL;

bool timer_reset_flag[NUM_OF_SPI_DEV] = {false};

bool __mqtt_timer_isr_handler(struct gptimer_t* timer, const gptimer_alarm_event_data_t* event, void* arg) {
	__sync_fetch_and_add(&mqtt_time_ms, 1);
	return false;
}

void mqtt_timer_start() {
	esp_err_t ret;
	gptimer_config_t timer_config = {
		.clk_src = GPTIMER_CLK_SRC_DEFAULT,
		.direction = GPTIMER_COUNT_UP,
		.resolution_hz = 1000 * 1000, // 1 MHz, 1 us
	};
	ret = gptimer_new_timer(&timer_config, &mqtt_timer);
	ESP_ERROR_CHECK(ret);

	gptimer_event_callbacks_t cbs = {
		.on_alarm = __mqtt_timer_isr_handler,
	};
	ret = gptimer_register_event_callbacks(mqtt_timer, &cbs, NULL);
	ESP_ERROR_CHECK(ret);

	gptimer_alarm_config_t alarm_config = {
		.reload_count = 0,
		.alarm_count = 1000, // 1 ms
		.flags.auto_reload_on_alarm = true,
	};
	ret = gptimer_set_alarm_action(mqtt_timer, &alarm_config);
	ESP_ERROR_CHECK(ret);
	ret = gptimer_enable(mqtt_timer);
	ESP_ERROR_CHECK(ret);

	gptimer_start(mqtt_timer);
}

void mqtt_timer_reset() {
	mqtt_time_ms = 0;
	for (int i = 0; i < NUM_OF_SPI_DEV; i++) {
		timer_reset_flag[i] = true;
	}
	if (!mqtt_timer) {
		mqtt_timer_start();
	}
}

uint64_t mqtt_timer_get(int i) {
	if (!mqtt_timer) {
		return 0;
	}
	if (timer_reset_flag[i]) {
		timer_reset_flag[i] = false;
		return 0;
	}
	return mqtt_time_ms;
}

void mqtt_timer_stop() {
	if (mqtt_timer) {
		gptimer_disable(mqtt_timer);
		gptimer_del_timer(mqtt_timer);
		mqtt_timer = NULL;
	}
}