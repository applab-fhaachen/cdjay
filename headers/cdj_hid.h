#pragma once
/*
 * cdj_hid.h
 *
 * CDJ-850 HID protocol definitions
 * Source: https://swiftb0y.github.io/CDJHidProtocol/hid-analysis/control.html
 *
 * CDJ->Host (IN,  20 bytes): byte[0]=0x00, byte[1]=0x20, then controls
 * Host->CDJ (OUT, 36 bytes): byte[0]=0x00, byte[1]=0x21, then LED/display data
 */

#include <stdint.h>
#include <string.h>

// ----------------------------------------------------------------
// CDJ -> Host  (IN report, 20 bytes)
// ----------------------------------------------------------------

// Fixed header bytes
#define CDJ_IN_BYTE0        0x00
#define CDJ_IN_TYPE         0x20

// Byte 0x02 - buttons
#define CDJ_BTN_PLAY        (1 << 7)    // byte 2
#define CDJ_BTN_CUE         (1 << 6)
#define CDJ_BTN_SEARCH_FWD  (1 << 5)
#define CDJ_BTN_SEARCH_BWD  (1 << 4)
#define CDJ_BTN_TRACK_FWD   (1 << 3)
#define CDJ_BTN_LOOP_HALF   (1 << 2)   // Cue/Loop Call 1/2X
#define CDJ_BTN_LOOP_DBL    (1 << 1)   // Cue/Loop Call 2X

// Byte 0x03
#define CDJ_BTN_IN_CUE      (1 << 7)   // byte 3
#define CDJ_BTN_OUT         (1 << 6)
#define CDJ_BTN_RELOOP      (1 << 5)
#define CDJ_BTN_TIME_MODE   (1 << 2)
#define CDJ_BTN_MEMORY      (1 << 1)
#define CDJ_BTN_DELETE      (1 << 0)

// Byte 0x04
#define CDJ_BTN_JOG_MODE    (1 << 7)   // byte 4
// Jog direction: bits [6:5]  0b11=forward  0b10=backward  0b01/00=stationary
#define CDJ_JOG_DIR_MASK    (0b11 << 5)
#define CDJ_JOG_FORWARD     (0b11 << 5)
#define CDJ_JOG_BACKWARD    (0b10 << 5)
#define CDJ_JOG_STILL       (0b01 << 5)
#define CDJ_BTN_PLATTER     (1 << 4)   // platter touch
#define CDJ_BTN_TEMPO       (1 << 3)
#define CDJ_BTN_MASTER_TEMPO (1 << 2)
#define CDJ_BTN_TEMPO_RESET (1 << 1)
#define CDJ_BTN_NEEDLE_TOUCH (1 << 0)

// Byte 0x05
#define CDJ_LIBRARY_VISIBLE (1 << 7)   // byte 5
#define CDJ_BTN_QUANTIZE    (1 << 6)
#define CDJ_BTN_MASTER      (1 << 5)
#define CDJ_BTN_SYNC        (1 << 4)
#define CDJ_BTN_ROT_PRESS   (1 << 3)   // rotary encoder press
#define CDJ_BTN_BACK        (1 << 2)
#define CDJ_BTN_TAG_TRACK   (1 << 1)
#define CDJ_BTN_EJECT       (1 << 0)

// Byte 0x06
#define CDJ_BTN_SLIP        (1 << 7)   // byte 6
#define CDJ_BTN_LATCH_REV   (1 << 6)   // direction switch down
#define CDJ_BTN_SLIP_REV    (1 << 5)   // direction switch held up
#define CDJ_BTN_TRACK_FILTER (1 << 3)
#define CDJ_BTN_CALL_DELETE (1 << 2)

// Byte 0x08 - loop buttons
#define CDJ_BTN_LOOP_32     (1 << 7)   // byte 8
#define CDJ_BTN_LOOP_16     (1 << 6)
#define CDJ_BTN_LOOP_8      (1 << 5)
#define CDJ_BTN_LOOP_4      (1 << 4)
#define CDJ_BTN_LOOP_2      (1 << 3)
#define CDJ_BTN_LOOP_1      (1 << 2)

// Byte 0x09
#define CDJ_BTN_LOOP_QTR    (1 << 4)   // byte 9   1/4 loop
#define CDJ_BTN_LOOP_HALF2  (1 << 3)   //          1/2 loop
#define CDJ_BTN_BEAT_48     (1 << 2)

// Byte 0x0F - hotcues
#define CDJ_BTN_HOTCUE_A    (1 << 7)   // byte 15
#define CDJ_BTN_HOTCUE_B    (1 << 6)
#define CDJ_BTN_HOTCUE_C    (1 << 5)
#define CDJ_BTN_HOTCUE_D    (1 << 4)
#define CDJ_BTN_HOTCUE_E    (1 << 3)
#define CDJ_BTN_HOTCUE_F    (1 << 2)
#define CDJ_BTN_HOTCUE_G    (1 << 1)
#define CDJ_BTN_HOTCUE_H    (1 << 0)

// Continuous controls (all little-endian uint16 unless noted)
// Byte 0x11: Vinyl Speed Adjust Touch/Brake  (uint8, 0x00–0xFF)
// Byte 0x12: Vinyl Speed Adjust Release/Start(uint8, 0x00–0xFF)
// Bytes 0x13–0x14: Rotary encoder position   (uint16 LE, absolute)
// Bytes 0x15–0x16: Tempo slider              (uint16 LE, 1000 steps each direction)
// Bytes 0x17–0x18: Jog wheel position        (~9728 steps per turn, uint16 LE)
// Bytes 0x19–0x1A: Jog wheel speed           (uint16 LE, direction from byte 4)
// Bytes 0x1B–0x1C: Needle search position    (0–599, uint16 LE)

// ----------------------------------------------------------------
// Host -> CDJ  (OUT report, 36 bytes)
// ----------------------------------------------------------------

#define CDJ_OUT_BYTE0       0x00
#define CDJ_OUT_TYPE        0x21

// Byte 0x02 - LED + display
#define CDJ_LED_PLAY        (1 << 7)   // byte 2
#define CDJ_LED_CUE         (1 << 6)
// bits [1:0]: cursor color  0b00=white  0b01/0b10=red  0b11=TODO

// Byte 0x03
#define CDJ_LED_IN_CUE      (1 << 7)   // byte 3
#define CDJ_LED_OUT         (1 << 6)
#define CDJ_LED_RELOOP      (1 << 5)
#define CDJ_DISP_TIME_ELAPSED (1 << 3) // 1=elapsed, 0=remaining
#define CDJ_DISP_AUTO_CUE   (1 << 2)
#define CDJ_LED_ROT_RING    (1 << 0)

// Byte 0x04
// bits [7:4]: tempo range  0b0001=±6%  0b0010=±10%  0b0011=±16%  0b0100=WIDE
#define CDJ_TEMPO_RANGE_6   (0b0001 << 4)
#define CDJ_TEMPO_RANGE_10  (0b0010 << 4)
#define CDJ_TEMPO_RANGE_16  (0b0011 << 4)
#define CDJ_TEMPO_RANGE_WIDE (0b0100 << 4)
#define CDJ_LED_TEMPO_RESET (1 << 3)
#define CDJ_LED_MASTER_TEMPO (1 << 2)  // key-lock
#define CDJ_LED_JOG_WHITE   (1 << 1)   // jog outside ring, white
#define CDJ_LED_JOG_RED     (1 << 0)   // jog outside ring, red

// Byte 0x05
#define CDJ_LED_MASTER      (1 << 7)
#define CDJ_LED_SYNC        (1 << 6)
// bits [5:4]: sync display  0b10/0b01=grey+icon  0b00=show tempo delta
#define CDJ_LED_SLIP        (1 << 3)
#define CDJ_LED_REVERSE     (1 << 2)
// bits [1:0]: jog mode  0b01=vinyl  0b00=CDJ  0b10/0b11=off

// Byte 0x06
// bits [7:5]: quantize resolution  0b001=1beat  0b010=1/2  ...  0b101=1/16
#define CDJ_QUANT_1BEAT     (0b001 << 5)
#define CDJ_QUANT_HALF      (0b010 << 5)
#define CDJ_QUANT_QUARTER   (0b011 << 5)
#define CDJ_QUANT_EIGHTH    (0b100 << 5)
#define CDJ_QUANT_16TH      (0b101 << 5)
#define CDJ_QUANT_COLOR_RED (1 << 4)   // 1=red, 0=grey
#define CDJ_LED_QUANTIZE    (1 << 3)

// Byte 0x09
#define CDJ_DISP_JOG        (1 << 7)   // 1=jog display on, 0=plain white ring
#define CDJ_DISP_CONTINUE   (1 << 6)   // continue mode
#define CDJ_DISP_BPM        (1 << 5)   // BPM display visible

// Bytes 0x0B–0x0E: elapsed time (Time-Struct)
// Bytes 0x0F–0x12: track length (Time-Struct)
// Bytes 0x15: whole BPM (uint8)
// Byte  0x16 [7:4]: fractional BPM (single digit)
// Bytes 0x17–0x18: tempo delta (int16 LE, percent * 100)
// Bytes 0x22–0x25: elapsed time slip mode (Time-Struct)

// ----------------------------------------------------------------
// Helper: Time-Struct  (4 bytes, used in OUT report)
// ----------------------------------------------------------------
typedef struct {
    uint8_t  minutes;
    uint8_t  seconds;      // 0–59
    uint16_t milliseconds; // LE, 0–999
} cdj_time_t;

static inline void cdj_time_pack(uint8_t *dest, uint32_t total_ms) {
    uint32_t ms  = total_ms % 1000;
    uint32_t s   = (total_ms / 1000) % 60;
    uint32_t min = (total_ms / 60000);
    dest[0] = (uint8_t)min;
    dest[1] = (uint8_t)s;
    dest[2] = (uint8_t)(ms & 0xFF);        // LE LSB
    dest[3] = (uint8_t)((ms >> 8) & 0xFF); // LE MSB
}

// ----------------------------------------------------------------
// Report buffers and helpers
// ----------------------------------------------------------------
#define CDJ_IN_REPORT_LEN  20
#define CDJ_OUT_REPORT_LEN 36

// Initialize an IN report with the fixed header and all controls zeroed
static inline void cdj_in_report_init(uint8_t *buf) {
    memset(buf, 0, CDJ_IN_REPORT_LEN);
    buf[0] = CDJ_IN_BYTE0;
    buf[1] = CDJ_IN_TYPE;
}

// Initialize an OUT report buffer for parsing
static inline void cdj_out_report_init(uint8_t *buf) {
    memset(buf, 0, CDJ_OUT_REPORT_LEN);
    buf[0] = CDJ_OUT_BYTE0;
    buf[1] = CDJ_OUT_TYPE;
}