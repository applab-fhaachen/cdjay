#include "PanelDisplay.h"
#include "PicoTransport.h"

namespace {

// 7-segment patterns for 0..9; index 10 = blank.
const uint8_t DIGITS[11] = {0xEE, 0x24, 0xD6, 0xB6, 0x3C, 0xBA, 0xFA, 0x26, 0xFE, 0xBE, 0x00};

const uint8_t GROUP_TEMPO_FLAGS = 1, GROUP_TEMPO = 2, GROUP_BPM = 3, GROUP_TRACK = 4;
const uint8_t GROUP_LIGHTS = 5, IDX_WIDE_MT = 12, MASK_WIDE = 0x04, MASK_MT = 0x08;

void put(uint8_t group, uint8_t index, uint8_t value) {
    pico_spi_slave_set_display_byte(group, index, value);
}

// Three digits at bytes 11 (ones), 12 (tens), 13 (hundreds), no leading zeros.
void putNumber(uint8_t group, uint16_t n) {
    put(group, 11, DIGITS[n % 10]);
    put(group, 12, n >= 10 ? DIGITS[(n / 10) % 10] : DIGITS[10]);
    put(group, 13, n >= 100 ? DIGITS[(n / 100) % 10] : DIGITS[10]);
}

void clearNumber(uint8_t group) {
    for (uint8_t i = 11; i <= 13; ++i) put(group, i, 0);
}

void setLight(uint8_t mask, bool on) {
    uint8_t v = pico_spi_slave_get_display_byte(GROUP_LIGHTS, IDX_WIDE_MT);
    put(GROUP_LIGHTS, IDX_WIDE_MT, on ? (v | mask) : (v & ~mask));
}

}  // namespace

namespace panel_display {

void clear() {
    // Byte 17 (jog ring) is left alone; it is owned by the jog code.
    for (uint8_t g = 0; g < DISPLAY_GROUPS; ++g)
        for (uint8_t i = 1; i <= 16; ++i) put(g, i, 0);
}

void lampTest() {
    for (uint8_t g = 0; g < DISPLAY_GROUPS; ++g)
        for (uint8_t i = 1; i <= 16; ++i) put(g, i, 0xFF);
}

bool setBpm(uint16_t bpm) {
    if (bpm > 399) return false;
    putNumber(GROUP_BPM, bpm);
    return true;
}
void clearBpm() { clearNumber(GROUP_BPM); }

bool setTrack(uint16_t track) {
    if (track > 999) return false;
    putNumber(GROUP_TRACK, track);
    return true;
}
void clearTrack() { clearNumber(GROUP_TRACK); }

bool setTempo(int16_t hundredths, bool wide, bool masterTempo) {
    const int16_t limit = wide ? 10000 : 1600;
    if (hundredths < -limit || hundredths > limit) return false;

    // Flags: 0x90 base, 0x04 WIDE, 0x08 MT, 0x03 "+", 0x02 "-",
    // 0x20 / 0x40 select the decimal point position.
    uint8_t flags = 0x90 | (wide ? 0x04 : 0) | (masterTempo ? 0x08 : 0);
    int16_t value = wide ? hundredths / 10 : hundredths;
    if (value > 0) flags |= 0x03;
    else if (value < 0) { flags |= 0x02; value = -value; }
    if ((wide && value < 1000) || (!wide && value >= 1000)) flags |= 0x40;
    else if (!wide) flags |= 0x20;
    if (value >= 1000) value /= 10;

    put(GROUP_TEMPO_FLAGS, 11, flags);
    put(GROUP_TEMPO_FLAGS, 12, 0);
    put(GROUP_TEMPO, 11, DIGITS[value % 10]);
    put(GROUP_TEMPO, 12, DIGITS[(value / 10) % 10]);
    put(GROUP_TEMPO, 13, (wide && value < 100) ? 0 : DIGITS[(value / 100) % 10]);
    setLight(MASK_WIDE, wide);
    setLight(MASK_MT, masterTempo);
    return true;
}

void clearTempo() {
    put(GROUP_TEMPO_FLAGS, 11, 0);
    put(GROUP_TEMPO_FLAGS, 12, 0);
    clearNumber(GROUP_TEMPO);
    setLight(MASK_WIDE, false);
    setLight(MASK_MT, false);
}

}  // namespace panel_display
