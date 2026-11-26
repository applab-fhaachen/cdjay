#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "StatusMessage.h"

// CDJ-800 front panel link, Pico 2 (RP2350) side. The panel is the SPI
// master: it drives SCK, sends a 20-byte status frame and at the same time
// clocks in a 20-byte display frame from us (SPI mode 3, ~1 MHz).
//
// [CHANGED] Runs entirely on two PIO state machines + two DMA channels
// (programs in cdj_spi.pio). The hardware SPI block is no longer used, and
// the CPU no longer takes an interrupt per received bit. See PicoTransport.cpp.
//
//   GP16 <- panel data out (status: buttons, jog, pitch)
//   GP17    panel chip select: not used (it glitches; frames are cut by bus idle)
//   GP18 <- panel clock
//   GP19 -> panel data in (display / LED frames)
#ifndef PICO_SPI_PIN_RX
#define PICO_SPI_PIN_RX   16
#endif
#ifndef PICO_SPI_PIN_SCK
#define PICO_SPI_PIN_SCK  18
#endif
#ifndef PICO_SPI_PIN_TX
#define PICO_SPI_PIN_TX   19
#endif

// The display is sent as six 20-byte groups (0..5), one per frame.
// Byte 0 = group << 4, bytes 1..17 = data, byte 18 = sum of bytes 1..17, byte 19 = 0.
#define DISPLAY_GROUPS 6

// [CHANGED] Link statistics (frame counters, histograms) removed with the debug code.

/** Start the PIO/DMA slave. False if no free PIO state machine or program space. */
bool pico_spi_slave_init(void);

/**
 * Fetch the newest valid status frame (header + checksum checked).
 * [CHANGED] Returns false for no new frame and for bad frames alike.
 */
bool pico_spi_slave_read_frame(uint8_t out[BUFFER_LENGTH]);

/** Set data byte `index` (1..17) of display group `group` (0..5). */
bool pico_spi_slave_set_display_byte(uint8_t group, uint8_t index, uint8_t value);
uint8_t pico_spi_slave_get_display_byte(uint8_t group, uint8_t index);
