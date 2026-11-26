#include "PanelDisplay.h"
#include "PicoTransport.h"

namespace panel_display {
namespace {

// Bits of one 7-segment digit in a display byte.
//      a
//    f   b
//      g
//    e   c
//      d
enum : uint8_t {
    SEG_A = 0x02, SEG_B = 0x04, SEG_F = 0x08, SEG_G = 0x10,
    SEG_C = 0x20, SEG_E = 0x40, SEG_D = 0x80,
};

const uint8_t DIGIT[10] = {
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,          // 0
    SEG_B | SEG_C,                                          // 1
    SEG_A | SEG_B | SEG_G | SEG_E | SEG_D,                  // 2
    SEG_A | SEG_B | SEG_G | SEG_C | SEG_D,                  // 3
    SEG_F | SEG_G | SEG_B | SEG_C,                          // 4
    SEG_A | SEG_F | SEG_G | SEG_C | SEG_D,                  // 5
    SEG_A | SEG_F | SEG_G | SEG_E | SEG_C | SEG_D,          // 6
    SEG_A | SEG_B | SEG_C,                                  // 7
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,  // 8
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G,          // 9
};

// Tempo flag byte (group 1, byte 11).
enum : uint8_t {
    TEMPO_PLUS_BAR = 0x01,  // vertical bar; with the minus bar it forms "+"
    TEMPO_MINUS    = 0x02,
    TEMPO_WIDE     = 0x04,
    TEMPO_MT       = 0x08,
    TEMPO_BASE     = 0x90,  // fixed marks of the tempo field ("TEMPO", "%")
    TEMPO_DP_2     = 0x20,  // decimal point before the last two digits: 3.25
    TEMPO_DP_1     = 0x40,  // decimal point before the last digit:      12.5
};

const uint8_t FIELD_FIRST_BYTE = 11; // ones digit; tens = 12, hundreds = 13

void put(uint8_t group, uint8_t index, uint8_t value) {
    pico_spi_slave_set_display_byte(group, index, value);
}

// Three-digit field, ones digit at byte 11. Leading zeros are blank unless
// min_digits asks for them (tempo "0.00" needs all three).
void put_number(uint8_t group, uint16_t value, uint8_t min_digits) {
    if (value > 999) value = 999;
    for (uint8_t i = 0; i < 3; ++i) {
        const bool shown = i < min_digits || value > 0;
        put(group, FIELD_FIRST_BYTE + i, shown ? DIGIT[value % 10] : 0);
        value /= 10;
    }
}

void blank_field(uint8_t group) {
    for (uint8_t i = 0; i < 3; ++i) put(group, FIELD_FIRST_BYTE + i, 0);
}

} // namespace

void set_bpm(uint16_t bpm) { put_number(3, bpm > 399 ? 399 : bpm, 1); }
void clear_bpm() { blank_field(3); }

void set_track(uint16_t track) { put_number(4, track, 1); }
void clear_track() { blank_field(4); }

void set_tempo(int16_t hundredths, bool wide, bool master_tempo) {
    uint8_t flags = TEMPO_BASE;
    if (wide) flags |= TEMPO_WIDE;
    if (master_tempo) flags |= TEMPO_MT;
    if (hundredths > 0) flags |= TEMPO_PLUS_BAR | TEMPO_MINUS;
    else if (hundredths < 0) flags |= TEMPO_MINUS;

    // Only three digits: pick the resolution that fits.
    //   normal: 0.00 .. 9.99 in hundredths, 10.0 .. 16.0 in tenths
    //   wide:   0.0 .. 99.9 in tenths, 100 as a whole number
    uint16_t v = (uint16_t)(hundredths < 0 ? -hundredths : hundredths);
    uint8_t min_digits;
    if (!wide && v < 1000) {
        flags |= TEMPO_DP_2; min_digits = 3;              // 3.25
    } else {
        v /= 10;                                          // tenths
        if (v < 1000) { flags |= TEMPO_DP_1; min_digits = 2; }  // 12.5 / 0.5
        else { v /= 10; min_digits = 1; }                 // 100
    }

    put(1, FIELD_FIRST_BYTE, flags);
    put(1, FIELD_FIRST_BYTE + 1, 0);
    put_number(2, v, min_digits);
}

void clear_tempo() {
    put(1, FIELD_FIRST_BYTE, 0);
    put(1, FIELD_FIRST_BYTE + 1, 0);
    blank_field(2);
}

void clear() {
    clear_bpm();
    clear_track();
    clear_tempo();
}

} // namespace panel_display
