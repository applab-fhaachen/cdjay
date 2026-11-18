#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "StatusMessage.h"

// RP2350 / Pico 2 SPI0 slave. The CDJ-800 front panel is the SPI master: it
// drives SCK and CS, sends a 20-byte status frame on its data-out line and
// clocks in a 20-byte display frame from us at the same time (SPI mode 3).
//
//   GP16 = SPI0 RX  <- panel data out (status: buttons, jog, pitch)
//   GP17 = SPI0 CSn <- panel chip select (ignored: frames are cut by bus idle)
//   GP18 = SPI0 SCK <- panel clock (~1 MHz per service manual)
//   GP19 = SPI0 TX  -> panel data in (display / LED frames)
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

// The display is sent as six 20-byte groups (0..5), one group per CS
// selection. Byte 0 = group << 4, bytes 1..17 = data, byte 18 = sum of
// bytes 1..17 (mod 256), byte 19 = 0.
#define DISPLAY_GROUPS 6

struct PicoSpiStats {
    uint32_t frames;        // bursts with exactly 20 bytes
    uint32_t short_frames;  // bursts with fewer than 20 bytes
    uint32_t long_frames;   // bursts with more than 20 bytes
    uint32_t overruns;      // RX FIFO overflowed (bytes lost)
    uint32_t len_hist[22];  // bytes per frame: 0..20, [21] = more than 20
    uint32_t off_hist[9];   // bit offset of the valid frame: 0..7, [8] = not found
};

// Last byte bursts (frames), diagnostics.
#define SEL_LOG_LEN 24
struct PicoSelLog {
    uint16_t bits;       // bits (clock edges) in the burst
    uint16_t pre;        // bits of an earlier transfer merged in front
    int8_t   offset;     // bit offset where header + checksum matched, -1 = none
    uint32_t gap_us;     // time since the previous burst ended
    uint16_t maxgap_us;  // longest pause between two bits inside the burst
    uint16_t maxgap_at;  // bit index after that pause
};

#ifdef __cplusplus
extern "C" {
#endif

/** Start SPI0 as a mode-3 slave and begin answering the panel. */
void pico_spi_slave_init(void);

/**
 * Fetch the newest complete 20-byte frame (no checksum check).
 * Older frames that were not read in time are replaced by newer ones.
 */
bool pico_spi_slave_read_frame(uint8_t out[BUFFER_LENGTH]);

/** Fetch the newest frame and decode it if header and checksum are valid. */
bool pico_spi_slave_read_packet(StatusMessage *out_msg);

/** Set data byte `index` (1..17) of display group `group` (0..5). */
bool pico_spi_slave_set_display_byte(uint8_t group, uint8_t index, uint8_t value);
uint8_t pico_spi_slave_get_display_byte(uint8_t group, uint8_t index);

void pico_spi_slave_get_stats(struct PicoSpiStats *out);
/** Copy the selection log, oldest first. Returns number of entries. */
uint8_t pico_spi_slave_get_sel_log(struct PicoSelLog out[SEL_LOG_LEN]);

#ifdef __cplusplus
}
#endif
