#ifndef _CONFIG_H
#define _CONFIG_H

#include <esp_log.h>
// #include <esp_wifi_types_generic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* Configs */

// Global
#define TAG "mainboard"

#define WIFI_SSID "ssid"
#define WIFI_PWD  "password"
#define WIFI_MODE WIFI_AUTH_WPA2_PSK

#define DEBUG
#define DEBUG_SPI_PRINT_DELAY_MS 0

// SPI
#define SPI_SYNC_ALARM_US			  100
#define SPI_SYNC_LOW_TIME_ALARM_COUNT 30

#define SPI_PIN_MISO GPIO_NUM_10
#define SPI_PIN_MOSI GPIO_NUM_9
#define SPI_PIN_CLK	 GPIO_NUM_8
#define SPI_HOST_ID	 SPI2_HOST

/*
#define SPI_CS_TABLE(X) \
	X(A, GPIO_NUM_38)   \
	X(B, GPIO_NUM_37)   \
	X(C, GPIO_NUM_36)
*/

#define SPI_CS_TABLE(X) \
	X(A, GPIO_NUM_7)   


#define SPI_DRDY_TABLE(X) \
	X(GPIO_NUM_6)        


#define SPI_SYNC_PIN GPIO_NUM_3

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


/* Internal */
#ifndef DEBUG
	#ifdef ESP_LOG_LEVEL_LOCAL
		#undef ESP_LOG_LEVEL_LOCAL
	#endif
	#define ESP_LOG_LEVEL_LOCAL(level, tag, format, ...)
#endif // DEBUG

#endif // _CONFIG_H