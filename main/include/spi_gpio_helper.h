#ifndef _SPI_GPIO_HELPER_H
#define _SPI_GPIO_HELPER_H

#include "globals.h"
#include "macro_utils.h"
#include "os.h"

#include <driver/gpio.h>


/* Enums */
#define X_EXPAND_CS_DECODER_ENUM(NAME, ...) CS_DEC_##NAME,
typedef enum {
	SPI_CS_TABLE(X_EXPAND_CS_DECODER_ENUM) NUM_OF_CS_PIN,
} SPI_CS_PIN;
#undef X_EXPAND_CS_DECODER_ENUM

/* Defines */
#define NUM_OF_SPI_DEV (0 SPI_DRDY_TABLE(X_EXPAND_CNT))

/* Macros */
#define FOR_EACH_SPI_CS_BIT(i) for (int i = 0; i < NUM_OF_CS_DEC; i++)
#define FOR_EACH_SPI_DEV(i)	   for (int i = 0; i < NUM_OF_SPI_DEV; i++)

/* Function prototypes */
void spi_cs_init(void);
void spi_cs(uint8_t dev_id);
void spi_cs_clear(void);

void spi_drdy_init(void);
uint32_t spi_drdy_get(void);

#endif // _SPI_GPIO_HELPER_H