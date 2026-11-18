#include "PicoTransport.h"
#include <Arduino.h>
#include <string.h>
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "hardware/pio.h"
#include "hardware/pio_instructions.h"

// Framing model
// -------------
// The CS line on the CDJ-800 test board glitches high several times inside
// every frame (level shifter + long wires), so CS cannot be trusted. The
// PL022's own CS input is forced low (always selected) and frames are cut
// by bus idle time instead: the panel sends ~20 bytes in ~450 us, then is
// quiet for ~1.8 ms. After each frame the SPI block is reset, which
// re-aligns the bit counter and drops any unsent TX bytes, and the next
// display group is preloaded.
//
// Receive path: the SCK wire also glitches, and the PL022 counts every
// glitch as a bit (frames came in shifted by 1-2 bits). RX therefore runs
// on a PIO state machine that accepts a clock edge only if the new level is
// still there PICO_SPI_GLITCH_CYCLES system clocks later (~100 ns), then
// samples the data pin on the rising edge (mode 3). The PL022 now only
// transmits the display frames.

#ifndef PICO_SPI_IDLE_US
#define PICO_SPI_IDLE_US 400   // gap that ends a frame
#endif
#ifndef PICO_SPI_POLL_US
#define PICO_SPI_POLL_US 100   // idle check period
#endif

#ifndef PICO_SPI_SPLIT_US
#define PICO_SPI_SPLIT_US 100  // pause inside a burst that separates transfers
#endif
#ifndef PICO_SPI_GLITCH_CYCLES
#define PICO_SPI_GLITCH_CYCLES 15  // 0..31 extra cycles (~6.7 ns each @150 MHz)
#endif

static PIO rx_pio = nullptr;
static uint rx_sm = 0;
static uint rx_offset = 0;
static uint rx_pio_irq = 0;

#define RX_MAX_BITS 320
static uint8_t rx_bits[RX_MAX_BITS];
static uint32_t rx_bit_us[RX_MAX_BITS];
static uint8_t rx_buffer[BUFFER_LENGTH];
static volatile uint16_t rx_count = 0;   // bits received in the current burst
static volatile bool rx_overflow = false;

static uint8_t ready_buffer[BUFFER_LENGTH];
static volatile bool packet_ready = false;

static uint8_t display[DISPLAY_GROUPS][BUFFER_LENGTH];
static uint8_t tx_frame[BUFFER_LENGTH];
static volatile uint8_t tx_pos = BUFFER_LENGTH;
static volatile uint8_t tx_group = 0;

static PicoSpiStats stats;
static PicoSelLog sel_log[SEL_LOG_LEN];
static uint8_t sel_head = 0, sel_count = 0;
static volatile uint32_t last_byte_us = 0;
static uint32_t last_frame_end_us = 0;
static repeating_timer_t idle_timer;

static inline spi_hw_t *hw(void) { return spi_get_hw(PICO_SPI_SLAVE_INSTANCE); }

static inline uint spi_irq_num(void) {
    return (spi_get_index(PICO_SPI_SLAVE_INSTANCE) == 0) ? SPI0_IRQ : SPI1_IRQ;
}

// PL022 RX is ignored (glitchy); keep its FIFO empty so it never stalls.
static void discard_spi_rx(void) {
    spi_hw_t *h = hw();
    while (h->sr & SPI_SSPSR_RNE_BITS) (void)h->dr;
    h->icr = SPI_SSPICR_RTIC_BITS | SPI_SSPICR_RORIC_BITS;
}

static void drain_rx(void) {
    if (rx_pio->fdebug & (1u << (PIO_FDEBUG_RXSTALL_LSB + rx_sm))) {
        rx_overflow = true;
        rx_pio->fdebug = 1u << (PIO_FDEBUG_RXSTALL_LSB + rx_sm);
    }
    while (!pio_sm_is_rx_fifo_empty(rx_pio, rx_sm)) {
        const uint8_t bit = (uint8_t)(pio_sm_get(rx_pio, rx_sm) & 1u);
        const uint32_t now = time_us_32();
        last_byte_us = now;
        if (rx_count < RX_MAX_BITS) { rx_bits[rx_count] = bit; rx_bit_us[rx_count] = now; }
        if (rx_count < 0xffff) rx_count++;
    }
}

static void on_pio_irq(void) { drain_rx(); }

static void restart_rx_sm(void) {
    pio_sm_set_enabled(rx_pio, rx_sm, false);
    pio_sm_clear_fifos(rx_pio, rx_sm);
    pio_sm_restart(rx_pio, rx_sm);
    pio_sm_exec(rx_pio, rx_sm, pio_encode_jmp(rx_offset));
    pio_sm_set_enabled(rx_pio, rx_sm, true);
}

static bool start_rx_pio(void) {
    // Glitch-filtered SPI mode-3 receiver, 6 instructions:
    //   0 lo:     wait 0 gpio SCK [d]   ; low seen, hold-off
    //   1         jmp pin lo            ; high again -> glitch, wait again
    //   2 hi:     wait 1 gpio SCK [d]   ; rising edge seen, hold-off
    //   3         jmp pin sample        ; still high -> real edge
    //   4         jmp hi                ; glitch -> wait for rise again
    //   5 sample: in pins, 1            ; sample data (pushed per bit)
    static uint16_t insns[6];
    const uint d = PICO_SPI_GLITCH_CYCLES;
    insns[0] = pio_encode_wait_gpio(false, PICO_SPI_PIN_SCK) | pio_encode_delay(d);
    insns[1] = pio_encode_jmp_pin(0);
    insns[2] = pio_encode_wait_gpio(true, PICO_SPI_PIN_SCK) | pio_encode_delay(d);
    insns[3] = pio_encode_jmp_pin(5);
    insns[4] = pio_encode_jmp(2);
    insns[5] = pio_encode_in(pio_pins, 1);
    static const pio_program_t prog = { insns, 6, -1 };

    PIO pios[] = { pio0, pio1, pio2 };
    for (PIO p : pios) {
        if (!pio_can_add_program(p, &prog)) continue;
        int sm = pio_claim_unused_sm(p, false);
        if (sm < 0) continue;
        rx_pio = p;
        rx_sm = (uint)sm;
        rx_offset = pio_add_program(p, &prog);
        break;
    }
    if (!rx_pio) return false;

    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, rx_offset, rx_offset + 5);
    sm_config_set_in_pins(&c, PICO_SPI_PIN_RX);
    sm_config_set_jmp_pin(&c, PICO_SPI_PIN_SCK);
    sm_config_set_in_shift(&c, false, true, 1);   // push every bit; CPU aligns bytes
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX); // 8-deep RX FIFO
    sm_config_set_clkdiv(&c, 1.0f);
    pio_sm_init(rx_pio, rx_sm, rx_offset, &c);

    // PIO reads input pads regardless of the pin function (SPI here).
    rx_pio_irq = pio_get_irq_num(rx_pio, 1);
    pio_set_irqn_source_enabled(rx_pio, 1,
        (pio_interrupt_source_t)(pis_sm0_rx_fifo_not_empty + rx_sm), true);
    irq_set_exclusive_handler(rx_pio_irq, on_pio_irq);
    irq_set_priority(rx_pio_irq, PICO_HIGHEST_IRQ_PRIORITY);
    irq_set_enabled(rx_pio_irq, true);
    pio_sm_set_enabled(rx_pio, rx_sm, true);
    return true;
}

static void fill_tx(void) {
    spi_hw_t *h = hw();
    while (tx_pos < BUFFER_LENGTH && (h->sr & SPI_SSPSR_TNF_BITS)) {
        h->dr = tx_frame[tx_pos++];
    }
    if (tx_pos < BUFFER_LENGTH) hw_set_bits(&h->imsc, SPI_SSPIMSC_TXIM_BITS);
    else hw_clear_bits(&h->imsc, SPI_SSPIMSC_TXIM_BITS);
}

static void load_tx_group(uint8_t group) {
    memcpy(tx_frame, display[group], BUFFER_LENGTH);
    tx_frame[0] = (uint8_t)(group << 4);
    uint8_t sum = 0;
    for (uint8_t i = 1; i <= 17; ++i) sum += tx_frame[i];
    tx_frame[18] = sum;
    tx_frame[19] = 0;
    tx_pos = 0;
    fill_tx();
}

static void configure_spi_hw(void) {
    // spi_init() resets the block, which also empties both FIFOs. In slave
    // mode the baud rate is irrelevant; SCK comes from the panel.
    spi_init(PICO_SPI_SLAVE_INSTANCE, 1000000);
    spi_set_format(PICO_SPI_SLAVE_INSTANCE, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
    spi_set_slave(PICO_SPI_SLAVE_INSTANCE, true);
    hw()->icr = SPI_SSPICR_RTIC_BITS | SPI_SSPICR_RORIC_BITS;
    hw()->imsc = SPI_SSPIMSC_RXIM_BITS | SPI_SSPIMSC_RTIM_BITS | SPI_SSPIMSC_RORIM_BITS;
}

static void on_spi_irq(void) {
    discard_spi_rx();
    fill_tx();
}

static void bits_to_bytes(uint16_t offset, uint8_t out[BUFFER_LENGTH]) {
    for (uint8_t i = 0; i < BUFFER_LENGTH; ++i) {
        uint8_t v = 0;
        for (uint8_t j = 0; j < 8; ++j) v = (uint8_t)((v << 1) | rx_bits[offset + i * 8 + j]);
        out[i] = v;
    }
}

static void close_frame(uint32_t now) {
    const uint16_t nbits = rx_count > RX_MAX_BITS ? RX_MAX_BITS : rx_count;
    const bool overflow = rx_overflow;
    rx_count = 0;
    rx_overflow = false;

    // The panel also sends short transfers (~8-9 clocks) a few hundred us
    // before the status frame; they can merge into one burst. Start at the
    // last pause > PICO_SPI_SPLIT_US that still leaves a full frame.
    uint16_t start = 0;
    for (uint16_t i = 1; i < nbits; ++i) {
        if (rx_bit_us[i] - rx_bit_us[i - 1] > PICO_SPI_SPLIT_US &&
            nbits - i >= BUFFER_LENGTH * 8) start = i;
    }

    // Then find the bit offset where "01 10" + checksum line up.
    int8_t found = -1;
    if (nbits - start >= BUFFER_LENGTH * 8) {
        const uint16_t max_off = (uint16_t)(nbits - start - BUFFER_LENGTH * 8);
        for (uint16_t off = 0; off <= max_off && off < 8 && found < 0; ++off) {
            bits_to_bytes(start + off, rx_buffer);
            if (StatusMessage::isValid(rx_buffer)) found = (int8_t)off;
        }
        if (found < 0) bits_to_bytes(start, rx_buffer); // show what arrived
    }

    uint16_t maxgap = 0, maxgap_at = 0;
    for (uint16_t i = 1; i < nbits; ++i) {
        const uint32_t g = rx_bit_us[i] - rx_bit_us[i - 1];
        if (g > maxgap) { maxgap = (uint16_t)(g > 0xffff ? 0xffff : g); maxgap_at = i; }
    }

    const uint16_t nbytes = nbits / 8;
    stats.len_hist[nbytes > BUFFER_LENGTH ? 21 : nbytes]++;
    if (nbits - start >= BUFFER_LENGTH * 8) stats.off_hist[found < 0 ? 8 : found]++;
    PicoSelLog &e = sel_log[sel_head];
    e.bits = nbits;
    e.pre = start;
    e.offset = found;
    e.gap_us = now - last_frame_end_us;
    e.maxgap_us = maxgap;
    e.maxgap_at = maxgap_at;
    last_frame_end_us = now;
    sel_head = (uint8_t)((sel_head + 1) % SEL_LOG_LEN);
    if (sel_count < SEL_LOG_LEN) sel_count++;

    if (overflow) stats.overruns++;
    if (nbits - start >= BUFFER_LENGTH * 8) {
        memcpy(ready_buffer, rx_buffer, BUFFER_LENGTH);
        packet_ready = true;  // main loop validates the checksum
        stats.frames++;
        if (nbits - start > BUFFER_LENGTH * 8) stats.long_frames++;
        tx_group = (uint8_t)((tx_group + 1) % DISPLAY_GROUPS);
    } else {
        stats.short_frames++;
        tx_group = 0;
    }
    // Reset every frame: re-aligns bits and flushes TX, then preload.
    restart_rx_sm();
    configure_spi_hw();
    load_tx_group(tx_group);
}

static bool on_idle_timer(repeating_timer_t *t) {
    (void)t;
    uint32_t s = save_and_disable_interrupts();
    drain_rx();
    const uint32_t now = time_us_32();
    if (rx_count > 0 && now - last_byte_us >= PICO_SPI_IDLE_US) close_frame(now);
    restore_interrupts(s);
    return true;
}

void pico_spi_slave_init(void) {
    memset(display, 0, sizeof(display));
    memset(&stats, 0, sizeof(stats));
    rx_count = 0;
    rx_overflow = false;
    packet_ready = false;
    tx_group = 0;

    configure_spi_hw();

    gpio_set_function(PICO_SPI_PIN_RX,  GPIO_FUNC_SPI);
    gpio_set_function(PICO_SPI_PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PICO_SPI_PIN_TX,  GPIO_FUNC_SPI);
    gpio_set_function(PICO_SPI_PIN_CS,  GPIO_FUNC_SPI);
    // Ignore the glitchy CS wire: the SPI block always sees itself selected.
    gpio_set_inover(PICO_SPI_PIN_CS, GPIO_OVERRIDE_LOW);
    gpio_pull_up(PICO_SPI_PIN_SCK);

    load_tx_group(0);
    last_byte_us = last_frame_end_us = time_us_32();

    if (!start_rx_pio()) {
        stats.overruns = 0xffffffffu; // visible in the log: no free PIO
        return;
    }

    // PL022 now only transmits; its IRQ refills TX and discards RX.
    const uint spi_irq = spi_irq_num();
    irq_set_exclusive_handler(spi_irq, on_spi_irq);
    irq_set_priority(spi_irq, PICO_HIGHEST_IRQ_PRIORITY);
    irq_set_enabled(spi_irq, true);

    add_repeating_timer_us(-PICO_SPI_POLL_US, on_idle_timer, NULL, &idle_timer);
}

bool pico_spi_slave_read_frame(uint8_t out[BUFFER_LENGTH]) {
    bool has = false;
    uint32_t s = save_and_disable_interrupts();
    if (packet_ready) {
        memcpy(out, ready_buffer, BUFFER_LENGTH);
        packet_ready = false;
        has = true;
    }
    restore_interrupts(s);
    return has;
}

bool pico_spi_slave_read_packet(StatusMessage *out_msg) {
    uint8_t packet[BUFFER_LENGTH];
    if (!pico_spi_slave_read_frame(packet) || !StatusMessage::isValid(packet)) return false;
    *out_msg = StatusMessage::createMessage(packet);
    return true;
}

bool pico_spi_slave_set_display_byte(uint8_t group, uint8_t index, uint8_t value) {
    if (group >= DISPLAY_GROUPS || index < 1 || index > 17) return false;
    display[group][index] = value; // single byte store; picked up at the next frame end
    return true;
}

uint8_t pico_spi_slave_get_display_byte(uint8_t group, uint8_t index) {
    if (group >= DISPLAY_GROUPS || index > 17) return 0;
    return display[group][index];
}

void pico_spi_slave_get_stats(PicoSpiStats *out) {
    uint32_t s = save_and_disable_interrupts();
    *out = stats;
    restore_interrupts(s);
}

uint8_t pico_spi_slave_get_sel_log(PicoSelLog out[SEL_LOG_LEN]) {
    uint32_t s = save_and_disable_interrupts();
    const uint8_t n = sel_count;
    for (uint8_t i = 0; i < n; ++i)
        out[i] = sel_log[(sel_head + SEL_LOG_LEN - n + i) % SEL_LOG_LEN];
    restore_interrupts(s);
    return n;
}
