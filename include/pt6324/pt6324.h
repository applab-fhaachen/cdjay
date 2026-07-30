#ifndef PT6324_H
#define PT6324_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/gpio.h"
#include "hardware/spi.h"

// ---- Command opcodes (datasheet §Commands) --------------------------------
// Cmd 1: Display mode  (0b0000 xxxx) — sets #digits × 24 segments
#define PT6324_CMD_MODE         0x00
#define PT6324_MODE_4_24        0x00  // 4 digits, 24 seg  (1/4 duty — reserved patterns)
#define PT6324_MODE_5_24        0x01
#define PT6324_MODE_6_24        0x02
#define PT6324_MODE_7_24        0x03
#define PT6324_MODE_8_24        0x00  // 8  digits, 24 seg  (see table)
#define PT6324_MODE_9_24        0x08
#define PT6324_MODE_10_24       0x09
#define PT6324_MODE_11_24       0x0A
#define PT6324_MODE_12_24       0x0B
#define PT6324_MODE_13_24       0x0C
#define PT6324_MODE_14_24       0x0D
#define PT6324_MODE_15_24       0x0E
#define PT6324_MODE_16_24       0x0F  // power-on default

// Cmd 2: Data setting  (0b0100 xxxx)
#define PT6324_CMD_DATA         0x40
#define PT6324_DATA_WRITE       0x00  // write to display RAM, auto-increment
#define PT6324_DATA_WRITE_FIXED 0x04  // write to display RAM, fixed address
#define PT6324_DATA_READ_KEY    0x02  // read key matrix
#define PT6324_DATA_RESET       0x01  // one-time reset
// bit 3 = test mode (leave 0 for normal operation)

// Cmd 3: Address setting (0b11xx xxxx)  — valid addresses 0x00..0x2F
#define PT6324_CMD_ADDR         0xC0

// Cmd 4: Display control (0b1000 xxxx) — dimming + on/off
#define PT6324_CMD_DISP         0x80
#define PT6324_DIM_1_16         0x00
#define PT6324_DIM_2_16         0x01
#define PT6324_DIM_4_16         0x02
#define PT6324_DIM_10_16        0x03
#define PT6324_DIM_11_16        0x04
#define PT6324_DIM_12_16        0x05
#define PT6324_DIM_13_16        0x06
#define PT6324_DIM_14_16        0x07
#define PT6324_DISP_OFF         0x00
#define PT6324_DISP_ON          0x08


#define PT6324_CLK_MAX_HZ            2500000  // max clk speed for PT6324 is 2.5 Mhz

// ---- RAM layout ------------------------------------------------------------
// 48 bytes total (0x00..0x2F). Each digit uses 3 consecutive bytes:
//   digit N (1..16) starts at RAM address (N-1)*3
//   byte 0: SG1..SG8    byte 1: SG9..SG16    byte 2: SG17..SG24
// Only the low 4 bits of the 3rd byte carry SG17..SG20 in some panels — check
// your VFD's grid/segment wiring.
#define PT6324_RAM_SIZE         0x30
#define PT6324_BYTES_PER_DIGIT  3
#define PT6324_MAX_DIGITS       16

typedef struct {
    spi_inst_t *spi;
    uint pin_stb;
    uint pin_clk;
    uint pin_din;
    uint pin_dout;   // set to 0xFF if key read is unused
    uint8_t framebuf[PT6324_RAM_SIZE];
    uint16_t spi_clk_speed;
} pt6324_t;

void pt6324_init(pt6324_t *dev, uint stb, uint clk, uint din, uint dout);
void pt6324_set_mode(pt6324_t *dev, uint8_t mode);
void pt6324_set_display(pt6324_t *dev, bool on, uint8_t dim);
void pt6324_reset(pt6324_t *dev);

// Write the entire 48-byte framebuffer to the chip in one strobed frame.
void pt6324_flush(pt6324_t *dev);

// Write `len` bytes starting at RAM address `addr` (auto-increment).
void pt6324_write_ram(pt6324_t *dev, uint8_t addr, const uint8_t *data, size_t len);

// Read the 4-byte key matrix (K1/K2 × SG1..SG16). Returns true on success.
bool pt6324_read_keys(pt6324_t *dev, uint8_t keys[4]);

// Convenience: write one digit's 3 bytes into the framebuffer (does NOT flush).
void pt6324_set_digit(pt6324_t *dev, uint8_t digit, uint32_t segments24);

#endif
