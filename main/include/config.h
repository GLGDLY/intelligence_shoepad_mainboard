#ifndef _CONFIG_H
#define _CONFIG_H

#include "macro_utils.h"

#include <assert.h>
#include <esp_log.h>
#include <esp_wifi_types_generic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* Configs */

// Global
#define TAG "mainboard"

#define WIFI_SSID "shoepad_wifi"
#define WIFI_PWD  "12345678"
#define WIFI_MODE WIFI_AUTH_WPA2_PSK

#define DATA_PUBLISH_HZ 100

// Debug
#define DEBUG
#define DEBUG_BUF_SIZE		512
#define DEBUG_PRINT_MAX_LEN 80

// #define DEBUG_ENABLE_SPI_PRINT_DATA
#define DEBUG_SPI_PRINT_INTVL_MS 1000

// Network
#define NET_RETRY_INTERVAL_MS 3000
#define MQTT_BUF_SIZE		  512

// SPI
#define SPI_SYNC_ALARM_US			  100
#define SPI_SYNC_LOW_TIME_ALARM_COUNT 30

#define SPI_PIN_MISO GPIO_NUM_7
#define SPI_PIN_MOSI GPIO_NUM_15
#define SPI_PIN_CLK	 GPIO_NUM_16
#define SPI_HOST_ID	 SPI2_HOST

#define SPI_CS_TABLE(X) \
	X(A, GPIO_NUM_11)   \
	X(B, GPIO_NUM_9)    \
	X(C, GPIO_NUM_3)    \
	X(D, GPIO_NUM_19)   \
	X(E, GPIO_NUM_18)

#define SPI_DRDY_TABLE(X) \
	X(GPIO_NUM_12)        \
	X(GPIO_NUM_10)        \
	X(GPIO_NUM_46)        \
	X(GPIO_NUM_20)        \
	X(GPIO_NUM_8)

#define SPI_SYNC_PIN GPIO_NUM_13

static_assert(0 SPI_CS_TABLE(X_EXPAND_CNT) == 0 SPI_DRDY_TABLE(X_EXPAND_CNT),
			  "SPI_CS_TABLE and SPI_DRDY_TABLE must have the same number of elements");
#define NUM_OF_SPI_DEV (0 SPI_DRDY_TABLE(X_EXPAND_CNT))

// SPI MLX90393 device
#define MLX90393_CMDS_TABLE(X) \
	X(SB, 0x1F, 1, 1)          \
	X(SW, 0x2F, 1, 1)          \
	X(SM, 0x3F, 1, 1)          \
	X(RM, 0x4F, 1, 9)          \
	X(RT, 0xF0, 1, 1)          \
	X(RR, 0x50, 2, 3)          \
	X(WR, 0x60, 4, 1)

/* Enums */
typedef enum {
	GPIO_LOW,
	GPIO_HIGH,
	NUM_OF_GPIO_LEVEL,
} GPIO_LEVEL;


#endif // _CONFIG_H