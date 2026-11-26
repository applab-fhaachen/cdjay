#pragma once
#include <stdint.h>

// CDJ-800 front panel VFD: numeric fields.
//
// The panel shows whatever the main board (the Pico) puts in the six 20-byte
// display groups that PicoTransport sends back on every SPI frame. This
// module knows where the 7-segment fields sit in those groups.
//
// Positions and segment bits come from the protocol notes of
// github.com/AlejandroPerez92/cdj800spi (reverse engineered). This is an
// independent implementation; no code was copied.
//
//   field          group  bytes            digit order
//   BPM            3      11, 12, 13       ones, tens, hundreds
//   track number   4      11, 12, 13       ones, tens, hundreds
//   tempo %        2      11, 12, 13       ones, tens, hundreds
//   tempo flags    1      11 (12 = 0)      sign, decimal point, WIDE, MT
//
// The 9-character text line (groups 0..4, bytes 1..10) needs the panel's
// dot-matrix font and is not handled here yet.

namespace panel_display {

/** Blank every numeric field written by this module. */
void clear();

/** Show a BPM value, 0..399 (saturates). Leading zeros are blank. */
void set_bpm(uint16_t bpm);
void clear_bpm();

/** Show a track number, 0..999 (saturates). Leading zeros are blank. */
void set_track(uint16_t track);
void clear_track();

/**
 * Show the tempo deviation in hundredths of a percent (e.g. -325 = -3.25 %).
 * Normal ranges: up to +-16.00 %. wide = true: up to +-100.0 %, shown with
 * one decimal. master_tempo lights the MT mark next to the tempo field.
 */
void set_tempo(int16_t hundredths, bool wide, bool master_tempo);
void clear_tempo();

} // namespace panel_display
