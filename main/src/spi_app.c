#include "spi_app.h"

#include "MLX90393_cmds.h"
#include "config.h"
#include "debug.h"
#include "driver/spi_common.h"
#include "esp_err.h"
#include "mqtt_app.h"
#include "mqtt_timer.h"
#include "os.h"
#include "spi_gpio_helper.h"

#include <assert.h>
#include <driver/spi_master.h>
#include <esp_flash.h>
#include <esp_log.h>
#include <esp_partition.h>
#include <freertos/projdefs.h>
#include <portmacro.h>
#include <sdkconfig.h>


/* Globals */
spi_device_handle_t spi;
SemaphoreHandle_t spi_mux = NULL;

uint32_t dev_ready = 0;
portMUX_TYPE dev_ready_lock = portMUX_INITIALIZER_UNLOCKED;

volatile mlx90393_data_t mlx90393_data[NUM_OF_SPI_DEV] = {0};
SemaphoreHandle_t mlx90393_data_mux = NULL;
static void mlx90393_data_init(void) { mlx90393_data_mux = xSemaphoreCreateMutex(); }
static void mlx90393_data_lock(void) {
	if (mlx90393_data_mux == NULL) {
		mlx90393_data_init();
	}
	xSemaphoreTake(mlx90393_data_mux, portMAX_DELAY);
}
static void mlx90393_data_unlock(void) {
	if (mlx90393_data_mux == NULL) {
		mlx90393_data_init();
	}
	xSemaphoreGive(mlx90393_data_mux);
}

const uint32_t bitfield_all_spi_dev_ready = (1 << NUM_OF_SPI_DEV) - 1;

/* Methods */
void spi_app_init(void) {
	spi_mux = xSemaphoreCreateMutex();

	LOGI("Initializing bus SPI%d...", SPI_HOST_ID + 1);

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
	LOGI("SPI init success");
}

void spi_tx_request(spi_cmd_t* cmd) {
	if (cmd->len <= 0)
		return;

	// LOGI("id: %d, tx0: %x", cmd->dev_id, cmd->tx_data[0]);

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

// void spi_drdy_intr_handler(void* arg) {
// 	taskENTER_CRITICAL_ISR(&dev_ready_lock);
// 	dev_ready |= 1 << ((uint64_t)arg);
// 	taskEXIT_CRITICAL_ISR(&dev_ready_lock);

// 	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
// 	if (dev_ready == bitfield_all_spi_dev_ready) {
// 		xHigherPriorityTaskWoken = pdTRUE;
// 		vTaskNotifyGiveFromISR(spi_app_task.handle, &xHigherPriorityTaskWoken);
// 	}
// 	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
// }

void spi_sync_falling_edge_handler(void* arg) {
	// LOGI("Sync signal detected on timer: %d", mcpwm);
	if (spi_app_task.handle != NULL && eTaskGetState(spi_app_task.handle) == eBlocked) {
		vTaskNotifyGiveFromISR(spi_app_task.handle, NULL);
	}
}

bool spi_write_reg_with_assert(uint8_t dev_id, uint8_t reg, uint8_t* reg_data) {
	mlx90393_status_t status;
	status = mlx90393_WR_request(dev_id, reg, reg_data);
	if (mlx90393_RM_data_is_valid(status)) {
		LOGI("Write reg: 0x%02x, Data: 0x%02x%02x", reg, reg_data[0], reg_data[1]);
	} else {
		LOGE("Failed to write reg 0x%02x: %x", reg, status.raw);
		return false;
	}

	delay(10);

	mlx90393_reg_data_t reg_ret = mlx90393_RR_request(dev_id, reg);
	if (mlx90393_RM_data_is_valid(reg_ret.status)) {
		if (reg_ret.data[0] == reg_data[0] && reg_ret.data[1] == reg_data[1]) {
			LOGI("Read reg: 0x%02x, Data: 0x%02x%02x", reg, reg_ret.data[0], reg_ret.data[1]);
		} else {
			LOGE("Read assert failed: 0x%02x%02x != 0x%02x%02x", reg_ret.data[0], reg_ret.data[1], reg_data[0],
				 reg_data[1]);
			return false;
		}
	} else {
		LOGE("Failed to read reg 0x%02x: %x", reg, reg_ret.status.raw);
		return false;
	}
	return true;
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
				LOGI("Reset SPI dev: %d success: %x", i, status.raw);
			} else {
				LOGE("Reset SPI dev: %d failed: %x", i, status.raw);
				goto retry;
			}

			delay(10);

			uint8_t reg_data[2] = {0};

			reg_data[0] = 0x00; // BIST disabled
			reg_data[1] = 0x5C; // Hall plate spinning rate = DEFAULT, GAIN_SEL = 5
			if (!spi_write_reg_with_assert(i, 0x00, (uint8_t*)reg_data)) {
				goto retry;
			}

			delay(10);

			reg_data[0] = 0x08; // enable trigger for sync
			reg_data[1] = 0x00;
			if (!spi_write_reg_with_assert(i, 0x01, (uint8_t*)reg_data)) {
				goto retry;
			}

			delay(10);

			reg_data[0] = 0x02;
			reg_data[1] = 0xB4; // RES for magnetic measurement = 0
			if (!spi_write_reg_with_assert(i, 0x02, (uint8_t*)reg_data)) {
				goto retry;
			}

			delay(10);

			status = mlx90393_SM_request(i);
			if (mlx90393_assert_SM_mode(status)) {
				LOGI("Init SPI dev: %d success: %x", i, status.raw);
			} else {
				LOGE("Init SPI dev: %d failed: %x", i, status.raw);
				goto retry;
			}

			delay(10);

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

	delay(100);

#ifdef DEBUG
	uint32_t debug_last_ticks = xTaskGetTickCount();
#endif

	while (1) {
		uint32_t drdy = spi_drdy_get();
		if (drdy) {
			FOR_EACH_SPI_DEV(i) {
				if (drdy & (1 << i)) {
					mlx90393_data_t d = mlx90393_RM_request(i);
					mlx90393_data_lock();
					mlx90393_data[i] = d;
					mlx90393_data_unlock();
					dev_ready &= ~(1 << i); // clear bit
				}
				// else {
				// 	memset(&mlx90393_data[i], 0, sizeof(mlx90393_data_t));
				// }
			}
		}

#ifdef DEBUG
	#ifdef DEBUG_ENABLE_SPI_PRINT_DATA
		if (xTaskGetTickCount() - debug_last_ticks >= DEBUG_SPI_PRINT_INTVL_MS) {
			FOR_EACH_SPI_DEV(i) {
				mlx90393_data_lock();
				mlx90393_data_t d = mlx90393_data[i];
				mlx90393_data_unlock();
				LOGI("Dev: %d, T: %d, X: %d, Y: %d, Z: %d", i, d.T, d.X, d.Y, d.Z);
			}
			LOGI("--------------------------------------------");
			debug_last_ticks = xTaskGetTickCount();
		}
	#endif
#endif

		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
	}
}

/* Normalize sensor data */
struct {
	double T;
	double X;
	double Y;
	double Z;
} normalize_offset[NUM_OF_SPI_DEV] = {0};
bool is_normalization_ready[NUM_OF_SPI_DEV] = {0};
esp_partition_t flash_partition = {0};
bool found_partition = false;

void mlx_normalization_init(void) {
	const esp_partition_t* partition =
		esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "user");
	if (!partition) {
		LOGE("Partition not found");
		return;
	}
	flash_partition = *partition;
	found_partition = true;

	uint8_t buf[1 + sizeof(normalize_offset)] = {0};
	esp_err_t ret = esp_partition_read(&flash_partition, 0, &buf, sizeof(buf));
	if (ret != ESP_OK) {
		LOGE("Read flash failed: %d", ret);
		return;
	}
	if (buf[0] != 0xAB) {
		LOGE("Invalid flash magic number check: %x", buf[0]);
		return;
	}
	memcpy(normalize_offset, &buf[1], sizeof(normalize_offset));
	for (uint8_t i = 0; i < NUM_OF_SPI_DEV; i++) {
		is_normalization_ready[i] = true;
		mqtt_publish_sensor_cal_end(i);
	}
}

bool mlx_normalize_offset(uint8_t i, mlx90393_data_t* d) {
	if (i >= NUM_OF_SPI_DEV) {
		return false;
	}

	if (!is_normalization_ready[i]) {
		// calculate
		static uint8_t cnt[NUM_OF_SPI_DEV] = {0};
		const uint8_t max_cnt = 100;

		if (cnt[i] < max_cnt) {
			if (cnt[i] == 0) {
				LOGI("Calibrating sensor %d: %d/%d", i, cnt[i], max_cnt);
				normalize_offset[i].T = 0;
				normalize_offset[i].X = 0;
				normalize_offset[i].Y = 0;
				normalize_offset[i].Z = 0;
			}
			normalize_offset[i].T += (double)d->T / max_cnt;
			normalize_offset[i].X += (double)d->X / max_cnt;
			normalize_offset[i].Y += (double)d->Y / max_cnt;
			normalize_offset[i].Z += (double)d->Z / max_cnt;
			++(cnt[i]);
		} else {
			is_normalization_ready[i] = true;
			cnt[i] = 0;
			mqtt_publish_sensor_cal_end(i);
			// write to flash
			if (!found_partition) {
				return false;
			}
			bool write_flash = true;
			for (uint8_t j = 0; j < NUM_OF_SPI_DEV; j++) {
				if (!is_normalization_ready[j]) {
					write_flash = false;
					break;
				}
			}
			if (!write_flash) {
				return false;
			}
			uint8_t buf[1 + sizeof(normalize_offset)] = {0};
			buf[0] = 0xAB;
			memcpy(&buf[1], normalize_offset, sizeof(normalize_offset));
			esp_err_t ret = esp_partition_erase_range(&flash_partition, 0, flash_partition.size);
			if (ret != ESP_OK) {
				LOGE("Erase flash failed: %d", ret);
				return false;
			}
			ret = esp_partition_write(&flash_partition, 0, &buf, sizeof(buf));
			if (ret != ESP_OK) {
				LOGE("Write flash failed: %d", ret);
			}
		}
		return false;
	} else {
		d->T -= normalize_offset[i].T;
		d->X -= normalize_offset[i].X;
		d->Y -= normalize_offset[i].Y;
		d->Z -= normalize_offset[i].Z;
		return true;
	}
}

inline void mlx_set_force_normalization(uint8_t i) {
	if (i >= NUM_OF_SPI_DEV) {
		return;
	}
	is_normalization_ready[i] = false;
}

/* Publish sensor data */
void spi_app_publish_thread(void* par) {
	mlx_normalization_init();
	char buf[256] = {0};
	while (1) {
		const TickType_t publish_delay = ms_to_ticks(1000 / DATA_PUBLISH_HZ);
		static_assert(publish_delay > 0, "Invalid DATA_PUBLISH_HZ");
		delay(publish_delay);

		mlx90393_data_t d[NUM_OF_SPI_DEV] = {0};
		mlx90393_data_lock();
		memcpy(d, mlx90393_data, sizeof(mlx90393_data));
		mlx90393_data_unlock();

		FOR_EACH_SPI_DEV(i) {
			if (!mlx_normalize_offset(i, &(d[i]))) {
				continue;
			}
			sprintf(buf, "%lld/%d,%d,%d,%d", mqtt_timer_get(i), d[i].T, d[i].X, d[i].Y, d[i].Z);
			mqtt_publish_sensor_data(i, buf);
			// printf("%d %d %d\n", d[i].X, d[i].Y, d[i].Z);
		}
	}
}
