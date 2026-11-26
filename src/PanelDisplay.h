#pragma once
#include <stdint.h>

// CDJ-800 front panel VFD, written through the display groups that
// PicoTransport sends to the panel (6 groups x data bytes 1..17).
//
// Layout (group, bytes):
//   text matrix     groups 0..4, bytes 1..10   (9 characters, not used yet)
//   tempo flags     group 1, byte 11           sign, decimal point, WIDE, MT
//   tempo digits    group 2, bytes 11..13      ones, tens, hundreds
//   BPM digits      group 3, bytes 11..13      ones, tens, hundreds
//   track digits    group 4, bytes 11..13      ones, tens, hundreds
//   progress/marks  group 5, bytes 1..9
//   indicators      group 5, bytes 12..13
//   jog ring        byte 17 of every group
// Positions follow the protocol notes of github.com/AlejandroPerez92/cdj800spi
// and still need confirming on our panel.

namespace panel_display {

void clear();
void lampTest();                       // every segment of bytes 1..16 on

bool setBpm(uint16_t bpm);             // 0..399
void clearBpm();
bool setTrack(uint16_t track);         // 0..999
void clearTrack();
// Tempo in hundredths of a percent (-1600..1600, or -10000..10000 if wide).
bool setTempo(int16_t hundredths, bool wide, bool masterTempo);
void clearTempo();

}  // namespace panel_display
