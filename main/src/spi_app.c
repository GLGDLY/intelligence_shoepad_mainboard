#include "spi_app.h"

#include "MLX90393_cmds.h"
#include "esp_log.h"
#include "freertos/projdefs.h"
#include "globals.h"
#include "os.h"
#include "portmacro.h"
#include "soc/soc.h"
#include "spi_gpio_helper.h"

#include <driver/spi_master.h>
#include <sdkconfig.h>
#include <stdint.h>


/* Globals */
spi_device_handle_t spi;
SemaphoreHandle_t spi_mux = NULL;

uint32_t dev_ready = 0;
portMUX_TYPE dev_ready_lock = portMUX_INITIALIZER_UNLOCKED;

mlx90393_data_t mlx90393_data[NUM_OF_SPI_DEV] = {0};

const uint32_t bitfield_all_spi_dev_ready = (1 << NUM_OF_SPI_DEV) - 1;

/* Methods */
void spi_app_init(void) {
	spi_mux = xSemaphoreCreateMutex();

	ESP_LOGI(TAG, "Initializing bus SPI%d...", SPI_HOST_ID + 1);

	spi_bus_config_t buscfg = {
		.miso_io_num = SPI_PIN_MISO, // MISO
		.mosi_io_num = SPI_PIN_MOSI, // MOSI
		.sclk_io_num = SPI_PIN_CLK,	 // SCLK
		.quadwp_io_num = -1,
		.quadhd_io_num = -1,
		.isr_cpu_id = ESP_INTR_CPU_AFFINITY_AUTO,
	};

	spi_device_interface_config_t devcfg = {
		.clock_speed_hz = SPI_MASTER_FREQ_10M, // 10 MHz
		.mode = 3,							   // CPOL = 1, CPHA = 1
		.spics_io_num = -1,
		.queue_size = 8,
	};

	// Initialize the SPI bus
	esp_err_t ret;
	ret = spi_bus_initialize(SPI_HOST_ID, &buscfg, SPI_DMA_CH_AUTO);
	ESP_ERROR_CHECK(ret);
	ret = spi_bus_add_device(SPI_HOST_ID, &devcfg, &spi);
	ESP_ERROR_CHECK(ret);

	spi_cs_init();
	spi_drdy_init();
	spi_sync_init();
	ESP_LOGI(TAG, "SPI init success");
}

void spi_tx_request(spi_cmd_t* cmd) {
	if (cmd->len <= 0)
		return;

	// ESP_LOGI(TAG, "id: %d, tx0: %x", cmd->dev_id, cmd->tx_data[0]);

	xSemaphoreTake(spi_mux, portMAX_DELAY);

	spi_cs(cmd->dev_id);

	spi_transaction_t tx = {
		.length = cmd->len * 8, // byte to bit
		.tx_buffer = cmd->tx_data,
		.rx_buffer = cmd->rx_data,
	};
	esp_err_t ret = spi_device_polling_transmit(spi, &tx);
	ESP_ERROR_CHECK(ret);

	spi_cs_clear();

	xSemaphoreGive(spi_mux);
}

extern RtosStaticTask_t spi_app_task;

void spi_drdy_intr_handler(void* arg) {
	taskENTER_CRITICAL_ISR(&dev_ready_lock);
	dev_ready |= 1 << ((uint64_t)arg);
	taskEXIT_CRITICAL_ISR(&dev_ready_lock);

	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	if (dev_ready == bitfield_all_spi_dev_ready) {
		xHigherPriorityTaskWoken = pdTRUE;
		vTaskNotifyGiveFromISR(spi_app_task.handle, &xHigherPriorityTaskWoken);
	}
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void spi_sync_falling_edge_handler(void* arg) {
	// ESP_LOGI(TAG, "Sync signal detected on timer: %d", mcpwm);
	vTaskNotifyGiveFromISR(spi_app_task.handle, NULL);
}

void spi_post_init(void) {
	uint8_t tx_data[1] = {0};
	uint8_t rx_data[1] = {0};
	spi_transaction_t tx = {
		.length = 8,
		.tx_buffer = tx_data,
		.rx_buffer = rx_data,
	};
	esp_err_t ret = spi_device_polling_transmit(spi, &tx);
	ESP_ERROR_CHECK(ret);

	FOR_EACH_SPI_DEV(i) {
		while (1) {
			mlx90393_status_t status;
			status = mlx90393_RT_request(i);
			if (mlx90393_RM_data_is_valid(status)) {
				ESP_LOGI(TAG, "Reset SPI dev: %d success: %x", i, status.raw);
			} else {
				ESP_LOGE(TAG, "Reset SPI dev: %d failed: %x", i, status.raw);
				goto retry;
			}

			status = mlx90393_SM_request(i);
			if (mlx90393_RM_data_is_valid(status)) {
				ESP_LOGI(TAG, "Init SPI dev: %d success: %x", i, status.raw);
			} else {
				ESP_LOGE(TAG, "Init SPI dev: %d failed: %x", i, status.raw);
				goto retry;
			}

			const uint8_t reg_data[2] = {0x0, 0x0};

			status = mlx90393_WR_request(i, 0x01, reg_data);
			if (mlx90393_RM_data_is_valid(status)) {
				ESP_LOGI(TAG, "Write reg: 0x01, Data: 0x%02x%02x", reg_data[0], reg_data[1]);
			} else {
				ESP_LOGE(TAG, "Failed to write reg: 0x01: %x", status.raw);
				goto retry;
			}

			mlx90393_reg_data_t reg_ret = mlx90393_RR_request(i, 0x01);
			if (mlx90393_RM_data_is_valid(reg_ret.status)) {
				if (reg_ret.data[0] == reg_data[0] && reg_ret.data[1] == reg_data[1]) {
					ESP_LOGI(TAG, "Read reg: 0x01, Data: 0x%02x%02x", reg_ret.data[0], reg_ret.data[1]);
				} else {
					ESP_LOGE(TAG, "Read assert failed: 0x%02x%02x != 0x%02x%02x", reg_ret.data[0], reg_ret.data[1],
							 reg_data[0], reg_data[1]);
					goto retry;
				}
			} else {
				ESP_LOGE(TAG, "Failed to read reg: 0x01: %x", reg_ret.status.raw);
				goto retry;
			}

			break;

		retry:
			delay(50);
		}
	}
}


void spi_app_thread(void* par) {
	spi_app_init();

	spi_post_init();

	spi_sync_start();

#ifdef DEBUG
	uint32_t last_ticks = xTaskGetTickCount();
#endif

	while (1) {
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

		uint32_t drdy = spi_drdy_get();
		if (drdy) {
			FOR_EACH_SPI_DEV(i) {
				if (drdy & (1 << i)) {
					mlx90393_data[i] = mlx90393_RM_request(i);
					dev_ready &= ~(1 << i); // clear bit
				} else {
					memset(&mlx90393_data[i], 0, sizeof(mlx90393_data_t));
				}
			}
		}

#ifdef DEBUG
		if (xTaskGetTickCount() - last_ticks >= 1000) {
			FOR_EACH_SPI_DEV(i) {
				ESP_LOGI(TAG, "Dev: %d, T: %d, X: %d, Y: %d, Z: %d", i, mlx90393_data[i].T, mlx90393_data[i].X,
						 mlx90393_data[i].Y, mlx90393_data[i].Z);
			}
			extern uint64_t timer_cnt;
			ESP_LOGI(TAG, "%lld--------------------------------------------", timer_cnt);
			last_ticks = xTaskGetTickCount();
		}
#endif
	}
}
