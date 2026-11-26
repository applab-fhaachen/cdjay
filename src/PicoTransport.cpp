// CDJ-800 front panel link on PIO + DMA (Pico SDK, bundled with Arduino-Pico).
//
// How it works
// ------------
//  - RX state machine (cdj_spi.pio): on every real rising SCK edge it reads
//    one data bit and pushes it into its FIFO. A DMA channel copies each bit
//    into one byte of rx_bits[] - no CPU work while a frame comes in.
//  - TX state machine: on every real falling SCK edge it puts the next bit
//    of the display frame on GP19. A second DMA channel feeds it the 20 bytes.
//  - A 100 us timer watches how far the RX DMA has written. When the bus has
//    been quiet for 400 us (panel: ~450 us frame, ~1.8 ms pause) the burst is
//    finished: it is handed to the main loop, and both state machines and
//    DMA channels are restarted with the next display group.
//  - The main loop (pico_spi_slave_read_frame) packs bits into bytes and
//    finds the frame by its header + checksum.
//
// [CHANGED] What was replaced (old version: see git history):
//  - PIO program hand-built with pio_encode_*()     -> cdj_spi.pio (+ generated header)
//  - manual search for a free PIO / state machine   -> pio_claim_free_sm_and_add_program_for_gpio_range()
//  - PIO interrupt per bit + per-bit timestamps     -> DMA (dma_channel_configure / _transfer_to_buffer_now)
//  - hardware SPI block for TX, its IRQ refill,
//    forced-low CS and per-frame spi_init() reset   -> TX state machine + DMA
//  - manual SM disable/clear/restart/jump sequence  -> pio_sm_init()
//  - timing-based split of merged transfers and
//    bit-offset search 0..7                         -> one search for header + checksum
//
// Resources used: 2 state machines (12 instructions) of one PIO block,
// 2 DMA channels, 1 repeating timer. The hardware SPI blocks, the other
// state machines and PIO blocks stay free (e.g. for I2S audio).

#include "PicoTransport.h"
#include <string.h>
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "pico/time.h"
#include "cdj_spi.pio.h"

#ifndef PICO_SPI_IDLE_US
#define PICO_SPI_IDLE_US 400   // bus quiet this long = burst finished
#endif
#ifndef PICO_SPI_POLL_US
#define PICO_SPI_POLL_US 100   // how often the timer checks
#endif
#define RX_MAX_BITS 320        // longest burst kept (a frame is 160 bits)
#define FRAME_BITS  (BUFFER_LENGTH * 8)

static_assert(PICO_SPI_PIN_SCK == PICO_SPI_PIN_RX + 2, "cdj_spi_rx reads SCK as in pin 2");

static PIO  pio_rx, pio_tx;
static uint sm_rx, sm_tx, offset_rx, offset_tx;
static pio_sm_config config_rx, config_tx;
static int  dma_rx, dma_tx;

static uint8_t rx_bits[RX_MAX_BITS];     // one byte (0 or 1) per received bit, written by DMA
static uint8_t ready_bits[RX_MAX_BITS];  // last finished burst, for the main loop
static uint16_t ready_nbits = 0;
static volatile bool ready = false;

static uint8_t display[DISPLAY_GROUPS][BUFFER_LENGTH];
static uint8_t tx_frame[BUFFER_LENGTH];
static uint8_t tx_group = 0;

static repeating_timer_t idle_timer;
static uint16_t last_count = 0;
static uint32_t last_change_us = 0;

// Number of bits the RX DMA has written so far.
static uint16_t bits_received(void) {
    return (uint16_t)((const uint8_t *)dma_channel_hw_addr(dma_rx)->write_addr - rx_bits);
}

static void start_rx(void) {
    pio_sm_init(pio_rx, sm_rx, offset_rx, &config_rx);  // stop, clear FIFOs, reset to start
    dma_channel_transfer_to_buffer_now(dma_rx, rx_bits, RX_MAX_BITS);
    pio_sm_set_enabled(pio_rx, sm_rx, true);
}

static void start_tx(uint8_t group) {
    memcpy(tx_frame, display[group], BUFFER_LENGTH);
    tx_frame[0] = (uint8_t)(group << 4);
    uint8_t sum = 0;
    for (uint8_t i = 1; i <= 17; ++i) sum += tx_frame[i];
    tx_frame[18] = sum;
    tx_frame[19] = 0;

    pio_sm_init(pio_tx, sm_tx, offset_tx, &config_tx);
    dma_channel_transfer_from_buffer_now(dma_tx, tx_frame, BUFFER_LENGTH);
    pio_sm_set_enabled(pio_tx, sm_tx, true);
}

// Burst finished: hand it over, then restart RX and TX for the next one.
static void close_burst(void) {
    dma_channel_abort(dma_rx);
    dma_channel_abort(dma_tx);
    const uint16_t nbits = bits_received();

    if (nbits >= FRAME_BITS) {
        memcpy(ready_bits, rx_bits, nbits);
        ready_nbits = nbits;
        ready = true;
        tx_group = (uint8_t)((tx_group + 1) % DISPLAY_GROUPS);
    } else {
        tx_group = 0;           // e.g. the panel's short pre-transfer
    }
    start_rx();
    start_tx(tx_group);
}

static bool on_idle_timer(repeating_timer_t *) {
    const uint16_t n = bits_received();
    const uint32_t now = time_us_32();
    if (n != last_count) {
        last_count = n;
        last_change_us = now;
    } else if (n > 0 && now - last_change_us >= PICO_SPI_IDLE_US) {
        close_burst();
        last_count = 0;
    }
    return true;
}

static uint8_t pack_byte(const uint8_t *bits) {
    uint8_t v = 0;
    for (uint8_t j = 0; j < 8; ++j) v = (uint8_t)((v << 1) | bits[j]);
    return v;
}

static void pack_frame(const uint8_t *bits, uint8_t out[BUFFER_LENGTH]) {
    for (uint8_t i = 0; i < BUFFER_LENGTH; ++i) out[i] = pack_byte(bits + i * 8);
}

static void setup_dma(void) {
    dma_rx = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(dma_rx);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_read_increment(&c, false);   // always the RX FIFO
    channel_config_set_write_increment(&c, true);   // next byte of rx_bits
    channel_config_set_dreq(&c, pio_get_dreq(pio_rx, sm_rx, false));
    dma_channel_configure(dma_rx, &c, rx_bits, &pio_rx->rxf[sm_rx], RX_MAX_BITS, false);

    dma_tx = dma_claim_unused_channel(true);
    c = dma_channel_get_default_config(dma_tx);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_read_increment(&c, true);    // next byte of tx_frame
    channel_config_set_write_increment(&c, false);  // always the TX FIFO
    channel_config_set_dreq(&c, pio_get_dreq(pio_tx, sm_tx, true));
    dma_channel_configure(dma_tx, &c, &pio_tx->txf[sm_tx], tx_frame, BUFFER_LENGTH, false);
}

bool pico_spi_slave_init(void) {
    memset(display, 0, sizeof(display));

    gpio_init(PICO_SPI_PIN_RX);
    gpio_init(PICO_SPI_PIN_SCK);
    gpio_pull_up(PICO_SPI_PIN_SCK);

    if (!pio_claim_free_sm_and_add_program_for_gpio_range(&cdj_spi_rx_program, &pio_rx, &sm_rx,
            &offset_rx, PICO_SPI_PIN_RX, 4, true) ||
        !pio_claim_free_sm_and_add_program_for_gpio_range(&cdj_spi_tx_program, &pio_tx, &sm_tx,
            &offset_tx, PICO_SPI_PIN_RX, 4, true)) {
        return false;  // no free state machine / program space
    }
    config_rx = cdj_spi_rx_config(offset_rx, PICO_SPI_PIN_RX, PICO_SPI_PIN_SCK);
    config_tx = cdj_spi_tx_config(offset_tx, PICO_SPI_PIN_SCK, PICO_SPI_PIN_TX);
    pio_gpio_init(pio_tx, PICO_SPI_PIN_TX);
    pio_sm_set_consecutive_pindirs(pio_tx, sm_tx, PICO_SPI_PIN_TX, 1, true);

    setup_dma();
    start_rx();
    start_tx(0);
    add_repeating_timer_us(-PICO_SPI_POLL_US, on_idle_timer, NULL, &idle_timer);
    return true;
}

bool pico_spi_slave_read_frame(uint8_t out[BUFFER_LENGTH]) {
    static uint8_t bits[RX_MAX_BITS];
    uint16_t nbits = 0;

    const uint32_t s = save_and_disable_interrupts();
    const bool has = ready;
    if (has) {
        nbits = ready_nbits;
        memcpy(bits, ready_bits, nbits);
        ready = false;
    }
    restore_interrupts(s);
    if (!has) return false;

    // The panel sometimes sends a short transfer just before the status
    // frame and both arrive as one burst. Look for the frame (header 01 10
    // + checksum) at every bit position, newest first.
    for (int start = nbits - FRAME_BITS; start >= 0; --start) {
        if (pack_byte(bits + start) != 0x01) continue;  // quick header check
        pack_frame(bits + start, out);
        if (StatusMessage::isValid(out)) return true;
    }
    return false;  // no valid frame in this burst
}

bool pico_spi_slave_set_display_byte(uint8_t group, uint8_t index, uint8_t value) {
    if (group >= DISPLAY_GROUPS || index < 1 || index > 17) return false;
    display[group][index] = value;  // sent with the next frame of this group
    return true;
}

uint8_t pico_spi_slave_get_display_byte(uint8_t group, uint8_t index) {
    if (group >= DISPLAY_GROUPS || index > 17) return 0;
    return display[group][index];
}
