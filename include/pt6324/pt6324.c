#include "pt6324.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"

// Datasheet timing (VDD=3.3V worst case):
//   PW_CLK  >= 400 ns   -> half-period >= 200 ns
//   PW_STB  >= 1000 ns
//   t_setup >= 100 ns,  t_hold >= 100 ns
//   t_CLK-STB >= 1000 ns  (last CLK rise to STB rise)
//   t_wait  >= 1 µs      (between command byte and first read clock)
// Pico at 125 MHz — a bare gpio_put is ~10 ns, so we pad with sleep_us(1)
// on the coarse waits and rely on natural latency for bit timing.

static void stb_low(pt6324_t *d)  { gpio_put(d->pin_stb, 0); sleep_us(1); }
static void stb_high(pt6324_t *d) { sleep_us(1); gpio_put(d->pin_stb, 1); sleep_us(1); }

static inline uint8_t reverse8(uint8_t x)
{
    x = (x >> 4) | (x << 4);
    x = ((x & 0xCC) >> 2) | ((x & 0x33) << 2);
    x = ((x & 0xAA) >> 1) | ((x & 0x55) << 1);
    return x;
}

static void spi_send_byte(pt6324_t *dev, uint8_t byte) {
    byte = reverse8(byte);
    spi_write_blocking(dev->spi, &byte, 1);
}

// One-command frame: STB low → 1 byte → STB high.
static void cmd1(pt6324_t *d, uint8_t byte) {
    stb_low(d);
    spi_send_byte(d, byte);
    stb_high(d);
}

void pt6324_init(pt6324_t *dev, uint stb, uint clk, uint din, uint dout) {
    dev->pin_stb  = stb;
    dev->pin_clk  = clk;
    dev->pin_din  = din;
    dev->pin_dout = dout;
    dev->spi_clk_speed = PT6324_CLK_MAX_HZ;
    dev->spi = spi0; // Use SPI0 by default; can be changed later if needed

    spi_init(dev->spi, dev->spi_clk_speed);
    // PT6324 expects SPI Mode 3 (CPOL=1, CPHA=1): clock idle high, data
    // sampled on the rising edge. Bit order is LSB-first on the wire, but
    // the RP2040 PL022 peripheral only shifts MSB-first — reverse8() in
    // spi_send_byte()/spi_read_byte() emulates LSB-first in software.
    spi_set_format(dev->spi, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
    gpio_set_function(clk, GPIO_FUNC_SPI);
    gpio_set_function(din, GPIO_FUNC_SPI);
    
    if (dout != 0xFF) {
        gpio_set_function(dout, GPIO_FUNC_SPI);
        gpio_pull_up(dout);        // DOUT is open-drain — external 1k–10k
                                   // pull-up is recommended per datasheet.
    }

    // STB is manually strobed, not part of the SPI peripheral.
    gpio_init(stb);
    gpio_set_dir(stb, GPIO_OUT);
    gpio_put(stb, 1);

    for (int i = 0; i < PT6324_RAM_SIZE; i++) dev->framebuf[i] = 0x00;
    pt6324_set_mode(dev, PT6324_MODE_16_24);
    pt6324_flush(dev);
    pt6324_set_display(dev, true, PT6324_DIM_10_16);
}

void pt6324_reset(pt6324_t *dev) {
    cmd1(dev, PT6324_CMD_DATA | PT6324_DATA_RESET);
}

static uint8_t spi_read_byte(pt6324_t *d)
{
    uint8_t tx = 0xFF;
    uint8_t rx;

    spi_write_read_blocking(
        d->spi,
        &tx,
        &rx,
        1);

    return reverse8(rx);
}

void pt6324_set_mode(pt6324_t *dev, uint8_t mode) {
    cmd1(dev, PT6324_CMD_MODE | (mode & 0x0F));
}

void pt6324_set_display(pt6324_t *dev, bool on, uint8_t dim) {
    uint8_t byte = PT6324_CMD_DISP | (dim & 0x07) | (on ? PT6324_DISP_ON : 0);
    cmd1(dev, byte);
}

void pt6324_write_ram(pt6324_t *dev, uint8_t addr, const uint8_t *data, size_t len) {
    if (len > PT6324_RAM_SIZE || addr + len > PT6324_RAM_SIZE) return;
    uint8_t tx[1 + PT6324_RAM_SIZE];
    tx[0] = reverse8(
        PT6324_CMD_ADDR | addr);

    for(size_t i=0;i<len;i++)
        tx[i+1] = reverse8(data[i]);

    stb_low(dev);

    spi_write_blocking(
        dev->spi,
        tx,
        len + 1);

    stb_high(dev);
}

void pt6324_flush(pt6324_t *dev) {
    pt6324_write_ram(dev, 0x00, dev->framebuf, PT6324_RAM_SIZE);
}

bool pt6324_read_keys(pt6324_t *dev, uint8_t keys[4]) {
    if (dev->pin_dout == 0xFF) return false;

    stb_low(dev);
    spi_send_byte(dev, PT6324_CMD_DATA | PT6324_DATA_READ_KEY);

    // Datasheet: t_wait >= 1 µs between last CLK rise of the command byte
    // and first CLK fall of the read.
    sleep_us(2);

    for (int i = 0; i < 4; i++) keys[i] = spi_read_byte(dev);

    stb_high(dev);
    return true;
}

void pt6324_set_digit(pt6324_t *dev, uint8_t digit, uint32_t segments24) {
    if (digit == 0 || digit > PT6324_MAX_DIGITS) return;
    uint8_t base = (digit - 1) * PT6324_BYTES_PER_DIGIT;
    dev->framebuf[base + 0] =  segments24        & 0xFF;   // SG1..SG8
    dev->framebuf[base + 1] = (segments24 >> 8)  & 0xFF;   // SG9..SG16
    dev->framebuf[base + 2] = (segments24 >> 16) & 0xFF;   // SG17..SG24
}
