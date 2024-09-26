#ifndef _CONFIG_H
#define _CONFIG_H

#include <esp_log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* Configs */

// Global
#define TAG "mainboard"

#define DEBUG
#define DEBUG_SPI_PRINT_DELAY_MS 100

// SPI
#define SPI_GATE_TIMEOUT_MS 20

#define SPI_PIN_MISO GPIO_NUM_13
#define SPI_PIN_MOSI GPIO_NUM_14
#define SPI_PIN_CLK	 GPIO_NUM_21
#define SPI_HOST_ID	 SPI2_HOST

#define SPI_CS_TABLE(X) \
	X(A, GPIO_NUM_31)   \
	X(B, GPIO_NUM_30)   \
	X(C, GPIO_NUM_29)

#define SPI_DRDY_TABLE(X) \
	X(GPIO_NUM_18)        \
	X(GPIO_NUM_17)        \
	X(GPIO_NUM_16)        \
	X(GPIO_NUM_15)        \
	X(GPIO_NUM_7)         \
	X(GPIO_NUM_6)         \
	X(GPIO_NUM_5)         \
	X(GPIO_NUM_4)

// SPI MLX90393 device
#define MLX90393_CMDS_TABLE(X) \
	X(SB, 0x1F, 1)             \
	X(SW, 0x2F, 1)             \
	X(SM, 0x3F, 1)             \
	X(RM, 0x4F, 9)             \
	X(RT, 0xF0, 1)


/* Enums */
typedef enum {
	GPIO_LOW,
	GPIO_HIGH,
	NUM_OF_GPIO_LEVEL,
} GPIO_LEVEL;

#endif // _CONFIG_H