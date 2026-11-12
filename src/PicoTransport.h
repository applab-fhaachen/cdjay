#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "StatusMessage.h"

#ifdef __cplusplus
extern "C" {
#endif

// Default RP2350 / Pico 2 SPI0 Slave Pins
// Pin 16 = SPI0 RX  (Pico receives data from CDJ panel)
// Pin 19 = SPI0 TX  (Pico sends display/LED feedback to CDJ panel)
// Pin 18 = SPI0 SCK (Pico receives SPI clock from CDJ panel)
// Pin 17 = SPI0 CSn (Pico receives Chip Select from CDJ panel)
#ifndef PICO_SPI_SLAVE_INSTANCE
#define PICO_SPI_SLAVE_INSTANCE spi0
#endif

#ifndef PICO_SPI_PIN_RX
#define PICO_SPI_PIN_RX   16
#endif

#ifndef PICO_SPI_PIN_CS
#define PICO_SPI_PIN_CS   17
#endif

#ifndef PICO_SPI_PIN_SCK
#define PICO_SPI_PIN_SCK  18
#endif

#ifndef PICO_SPI_PIN_TX
#define PICO_SPI_PIN_TX   19
#endif

/**
 * Initialize Pico 2 SPI as a SLAVE in SPI Mode 3 (CPOL=1, CPHA=1)
 */
void pico_spi_slave_init(void);

/**
 * Check if a complete 20-byte packet has arrived from the CDJ-800
 * @param out_msg Filled with the decoded StatusMessage when true is returned
 * @return true if a valid packet was received and verified by checksum
 */
bool pico_spi_slave_read_packet(StatusMessage *out_msg);

#ifdef __cplusplus
}
#endif
