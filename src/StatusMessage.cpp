#include "StatusMessage.h"
#include <Arduino.h>

// Where each button sits: frame byte and bit number, in Button order.
// [CHANGED] Bit numbers instead of masks, read with Arduino bitRead().
struct ButtonPos { uint8_t byte, bit; };
static const ButtonPos BUTTON_POS[] = {
    {14, 7}, {14, 5}, {14, 6}, {14, 4}, {14, 3}, {14, 2}, {14, 1}, {14, 0}, // Cue Rev CueOut CueIn Beat8 Beat4 Beat2 Beat1
    {15, 1}, {15, 0},                                                       // Reloop Play
    {16, 3}, {16, 2}, {16, 1}, {16, 0},                                     // SearchFwd SearchRev TrackSearchFwd TrackSearchRev
    {17, 6}, {17, 5}, {17, 4}, {17, 3}, {17, 2}, {17, 1}, {17, 0},         // MasterTempo Tempo Vinyl Eject QuickReturn FolderFwd FolderRev
    {18, 5}, {18, 4}, {18, 3}, {18, 2}, {18, 1}, {18, 0},                  // TimeMode TextMode Delete Memory CallRev CallFwd
};
static_assert(sizeof(BUTTON_POS) / sizeof(BUTTON_POS[0]) == (size_t)Button::Count, "one entry per button");

// Checksum: sum of bytes 0..18 with end-around carry (one's complement style).
static uint8_t checksum(const uint8_t *frame) {
    uint16_t sum = 0;
    for (uint8_t i = 0; i < 19; ++i) {
        sum += frame[i];
        sum = (sum & 0xFF) + (sum >> 8);
    }
    return (uint8_t)sum;
}

bool StatusMessage::isValid(const uint8_t frame[BUFFER_LENGTH]) {
    return frame[0] == 0x01 && frame[1] == 0x10 && checksum(frame) == frame[19];
}

StatusMessage StatusMessage::createMessage(const uint8_t frame[BUFFER_LENGTH]) {
    StatusMessage m;
    // [CHANGED] Arduino word(high, low) instead of the hand-written be16().
    m.pitch    = word(frame[4], frame[5]);
    m.jogPulse = word(frame[8], frame[9]);
    for (uint8_t i = 0; i < (uint8_t)Button::Count; ++i)
        if (bitRead(frame[BUTTON_POS[i].byte], BUTTON_POS[i].bit)) bitSet(m.buttonMask, i);
    return m;
}
