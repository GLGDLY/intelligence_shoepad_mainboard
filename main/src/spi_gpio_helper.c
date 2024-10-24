#include "spi_gpio_helper.h"

#include "esp_log.h"
#include "globals.h"
#include "hal/gpio_types.h"
#include "os.h"
#include "soc/gpio_num.h"

#include <stdio.h>


/* Helper macros */
#define X_EXPAND_CS_CONSTRUCT(NAME, PIN) [CS_DEC_##NAME] = PIN,
#define X_EXPAND_CS_PIN_MASK(NAME, PIN)	 (1ULL << PIN) |

#define X_EXPAND_DRDY_CONSTRUCT(PIN) PIN,
#define X_EXPAND_DRDY_PIN_MASK(PIN)	 (1ULL << PIN) |

/* Constants */
const gpio_num_t SPI_CS_PINS[] = {SPI_CS_TABLE(X_EXPAND_CS_CONSTRUCT)};
const gpio_num_t SPI_DRDY_PINS[NUM_OF_SPI_DEV] = {SPI_DRDY_TABLE(X_EXPAND_DRDY_CONSTRUCT)};

void spi_cs_init(void) {
	ESP_LOGI(TAG, "SPI CS init");
	esp_err_t ret;

	gpio_config_t conf = {
		.pin_bit_mask = (SPI_CS_TABLE(X_EXPAND_CS_PIN_MASK) 0),
		.mode = GPIO_MODE_OUTPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE,
	};
	ret = gpio_config(&conf);
	ESP_ERROR_CHECK(ret);

	for (int i = 0; i < NUM_OF_CS_PIN; i++) {
		ret = gpio_set_level(SPI_CS_PINS[i], GPIO_HIGH);
		ESP_ERROR_CHECK(ret);
	}
}

void spi_cs(uint8_t dev_id) {
	if (dev_id >= NUM_OF_SPI_DEV || dev_id < 0) {
		ESP_LOGE(TAG, "Invalid device ID: %d", dev_id);
		return;
	}

	for (int i = 0; i < NUM_OF_CS_PIN; i++) {
		// if (dev_id & (1 << i)) {
		if (dev_id == i) {
			gpio_set_level(SPI_CS_PINS[i], GPIO_LOW);
		} else {
			gpio_set_level(SPI_CS_PINS[i], GPIO_HIGH);
		}
	}
}

void spi_cs_clear(void) {
	for (int i = 0; i < NUM_OF_CS_PIN; i++) {
		gpio_set_level(SPI_CS_PINS[i], GPIO_HIGH);
	}
}

__attribute__((weak)) void spi_drdy_intr_handler(void* arg){};

void spi_drdy_init(void) {
	ESP_LOGI(TAG, "SPI DRDY init");
	esp_err_t ret;

	gpio_config_t conf = {
		.pin_bit_mask = SPI_DRDY_TABLE(X_EXPAND_DRDY_PIN_MASK) 0,
		.mode = GPIO_MODE_INPUT,
		.pull_down_en = GPIO_PULLDOWN_ENABLE,
		.intr_type = GPIO_INTR_POSEDGE,
	};
	ret = gpio_config(&conf);
	ESP_ERROR_CHECK(ret);

	// ret = gpio_install_isr_service(ESP_INTR_FLAG_SHARED | ESP_INTR_FLAG_LOWMED);
	// ESP_ERROR_CHECK(ret);
	// FOR_EACH_SPI_DEV(i) {
	// 	ret = gpio_isr_handler_add(SPI_DRDY_PINS[i], spi_drdy_intr_handler, (void*)(uint64_t)i);
	// 	ESP_ERROR_CHECK(ret);
	// }
}

uint32_t spi_drdy_get(void) {
	uint32_t drdy = 0;
	FOR_EACH_SPI_DEV(i) { drdy |= gpio_get_level(SPI_DRDY_PINS[i]) << i; }
	return drdy;
}

/* SPI Sync */

__attribute__((weak)) void spi_sync_falling_edge_handler(void* arg) {}

gptimer_handle_t timer = NULL;
uint64_t timer_cnt = 0;

bool timer_isr_handler(struct gptimer_t* timer, const gptimer_alarm_event_data_t* event, void* arg) {
	timer_cnt++;
	static bool gpio_state = false;
	static uint8_t low_cnt = 1;
	if (gpio_state == 0 && low_cnt >= 10) {
		gpio_state = !gpio_state;
	} else if (gpio_state == 1) {
		gpio_state = !gpio_state;
		low_cnt = 0;
	}
	gpio_set_level(SPI_SYNC_PIN, !gpio_state);

	if (gpio_state == 0) {
		if (low_cnt++ == 0) { // falling edge
			extern RtosStaticTask_t spi_app_task;
			if (spi_app_task.handle != NULL && eTaskGetState(spi_app_task.handle) == eBlocked) {
				vTaskNotifyGiveFromISR(spi_app_task.handle, NULL);
			}
			return true;
		}
	}
	return false;
}

void spi_sync_init(void) {
	ESP_LOGI(TAG, "SPI SYNC init");
	esp_err_t ret;

	// spi sync is inverted
	gpio_config_t conf = {
		.pin_bit_mask = (1ULL << SPI_SYNC_PIN),
		.mode = GPIO_MODE_OUTPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE,
	};
	ret = gpio_config(&conf);
	ESP_ERROR_CHECK(ret);
	ret = gpio_set_level(SPI_SYNC_PIN, GPIO_HIGH);
	ESP_ERROR_CHECK(ret);

	gptimer_config_t timer_config = {
		.clk_src = GPTIMER_CLK_SRC_DEFAULT,
		.direction = GPTIMER_COUNT_UP,
		.resolution_hz = 1 * 1000 * 1000, // 1MHz, 1 tick = 1us
	};
	ret = gptimer_new_timer(&timer_config, &timer);
	ESP_ERROR_CHECK(ret);

	gptimer_event_callbacks_t cbs = {
		.on_alarm = timer_isr_handler,
	};
	ret = gptimer_register_event_callbacks(timer, &cbs, NULL);
	ESP_ERROR_CHECK(ret);

	gptimer_alarm_config_t alarm_config = {
		.reload_count = 0,
		.alarm_count = 100,
		.flags.auto_reload_on_alarm = true,
	};
	ret = gptimer_set_alarm_action(timer, &alarm_config);
	ESP_ERROR_CHECK(ret);
	ret = gptimer_enable(timer);
	ESP_ERROR_CHECK(ret);

	ESP_LOGI(TAG, "SPI SYNC init success");
}

void spi_sync_start(void) {
	esp_err_t ret;
	ret = gptimer_start(timer);
	ESP_ERROR_CHECK(ret);
}
