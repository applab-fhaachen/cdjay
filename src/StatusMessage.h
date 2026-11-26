#pragma once
#include <stddef.h>
#include <stdint.h>

// One 20-byte status frame from the CDJ-800 front panel.
//
//   byte  0..1   header 01 10
//   byte  3      touch / heartbeat bits
//   byte  4..5   pitch fader (big endian)
//   byte  6..7   pitch reference
//   byte  8..9   jog counter (wraps at 16 bit)
//   byte 10..11  jog speed
//   byte 12      jog flags (0x80 moving, 0x40 direction)
//   byte 14..18  buttons
//   byte 19      checksum
//
// [CHANGED] Slimmed down: only the values the firmware reads are kept. The
// 27 separate btnXxx fields, the duplicate jogPresure/jogPressure, the
// unused jog flag fields, auxiliary13 and the raw[] copy were removed.
// Buttons are one bitmask, built once while decoding.

static const size_t BUFFER_LENGTH = 20;

enum class Button : uint8_t {
    Cue, Rev, CueOut, CueIn, Beat8, Beat4, Beat2, Beat1, Reloop, Play,
    SearchFwd, SearchRev, TrackSearchFwd, TrackSearchRev, MasterTempo,
    Tempo, Vinyl, Eject, QuickReturn, FolderFwd, FolderRev, TimeMode,
    TextMode, Delete, Memory, CallRev, CallFwd, Count
};

class StatusMessage {
public:
    /** Header 01 10 and checksum correct. */
    static bool isValid(const uint8_t frame[BUFFER_LENGTH]);
    /** Decode a frame (call isValid() first). */
    static StatusMessage createMessage(const uint8_t frame[BUFFER_LENGTH]);

    /** Bit n set = Button n held. */
    uint32_t buttons() const { return buttonMask; }

    uint32_t buttonMask = 0;
    uint16_t pitch = 0, jogPulse = 0;
    // [CHANGED] touch, pitchReference, jogSpeed, jogFlags removed: only the
    // debug report read them. Their positions are in the byte map above.
};
