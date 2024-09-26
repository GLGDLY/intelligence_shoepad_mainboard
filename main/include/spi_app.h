#ifndef _SPI_APP_H
#define _SPI_APP_H

#include "globals.h"

#include <stddef.h>
#include <stdint.h>


/* Structs */
typedef struct {
	uint8_t dev_id;
	uint8_t *tx_data, *rx_data;
	size_t len;
} spi_cmd_t;

/* Function prototypes */
void spi_app_init(void);
void spi_app_thread(void* par);

void spi_tx_request(spi_cmd_t* cmd);

#endif // _SPI_APP_H