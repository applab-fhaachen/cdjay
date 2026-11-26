/**
 * CDJay - CDJ-800 front panel -> USB-MIDI (registers as "PIONEER CDJ-850")
 *
 * The Pico replaces the CDJ-800 MAIN board. The front panel is the SPI master
 * and polls us with 20-byte status frames; we answer with display frames.
 *
 * Libraries used here:
 *  - Arduino-Pico core: millis(), constrain(), map(), bitRead(), bitWrite(),
 *    Serial (readStringUntil(), printf()), String
 *  - Adafruit TinyUSB: USB device + USB-MIDI interface
 *  - FortySevenEffects MIDI Library: MIDI.sendNoteOn() / sendControlChange()
 *
 * Serial (115200) is only used for the "bpm" and "track" commands.
 * [CHANGED] All debug code (PanelDebug, link statistics, button/status
 * printing, frame counters) was removed; it is in the git history.
 *
 * Change markers: lines tagged [CHANGED] were simplified in the clean-up;
 * the tag says which library function replaced the hand-written code.
 */

#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>
#include "StatusMessage.h"
#include "PicoTransport.h"
#include "PanelDisplay.h"

Adafruit_USBD_MIDI usb_midi;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);

#define PIN_STATUS_LED  LED_BUILTIN
#define MIDI_CH         1

// On the CDJ-800 test board GP15 is the 3.3 V supply (VA) of the TXS0108E
// level shifter. It must be driven HIGH before SPI can see anything.
#define PIN_SHIFTER_VA  15

// [CHANGED] The breadboard Play/Cue buttons on GP14/GP15 were removed: they
// were switched off (GP15 powers the level shifter) and could not be used.

// [CHANGED] The note_for() switch is now a lookup table, indexed by Button.
// CDJ-850 MIDI layout, matching Mixxx's bundled "Pioneer CDJ-850" mapping
// (channel 1 = [Channel1]). Buttons Mixxx does not use get 0x60 + index.
static const uint8_t NOTE_FOR[] = {
    0x01, // Cue            cue_default
    0x21, // Rev            reverse
    0x07, // CueOut         loop_out
    0x06, // CueIn          loop_in
    0x1C, // Beat8          hotcue 4
    0x1A, // Beat4          hotcue 3
    0x19, // Beat2          hotcue 2
    0x18, // Beat1          hotcue 1
    0x08, // Reloop         reloop_exit
    0x00, // Play           play
    0x02, // SearchFwd      fwd
    0x03, // SearchRev      back
    0x04, // TrackSearchFwd [Playlist] SelectNextTrack
    0x05, // TrackSearchRev [Playlist] SelectPrevTrack
    0x11, // MasterTempo    keylock
    0x6F, // Tempo          (unmapped)
    0x70, // Vinyl          (unmapped)
    0x2F, // Eject          eject
    0x72, // QuickReturn    (unmapped)
    0x73, // FolderFwd      (unmapped)
    0x74, // FolderRev      (unmapped)
    0x75, // TimeMode       (unmapped)
    0x76, // TextMode       (unmapped)
    0x0D, // Delete         delete last used hot cue
    0x78, // Memory         (unmapped)
    0x79, // CallRev        (unmapped)
    0x7A, // CallFwd        (unmapped)
};
static_assert(sizeof(NOTE_FOR) == (size_t)Button::Count, "one note per button");

#define MIDI_NOTE_JOG_TOUCH  0x20  // scratch2_enable (top plate touched in Vinyl mode)
#define MIDI_CC_TEMPO        0x1D  // rate
#define MIDI_CC_JOG          0x30  // jog_wheel (pitch bend), relative around 0x40
#define MIDI_CC_JOG_SCRATCH  0x10  // jog_scratch, relative around 0x40

// Raw pitch fader endpoints, measured on the CDJ-800 panel, 2026-11-18.
#ifndef PITCH_RAW_MIN
#define PITCH_RAW_MIN 4388
#endif
#ifndef PITCH_RAW_MAX
#define PITCH_RAW_MAX 65472
#endif
// Mixxx's CDJ-850 mapping inverts CC 0x1D, so the CC value must fall as the
// fader moves toward "+". The raw value runs the other way, so it is flipped.
#ifndef PITCH_INVERT
#define PITCH_INVERT 1
#endif
// Set to 1 if turning the jog clockwise moves the track backwards in Mixxx.
#ifndef JOG_INVERT
#define JOG_INVERT 0
#endif
// Vinyl mode: how long after the last jog movement scratch stays on.
#ifndef JOG_RELEASE_MS
#define JOG_RELEASE_MS 80
#endif
// No valid frame for this long -> panel considered offline, notes released.
#define PANEL_TIMEOUT_MS 500

// Display: BPM = base BPM scaled by the pitch fader, like a real CDJ. Mixxx's
// CDJ-850 mapping does not send the BPM, so it is set by the "bpm" command.
#ifndef DEFAULT_BASE_BPM
#define DEFAULT_BASE_BPM 128.0f
#endif
// Pitch ranges cycled by the TEMPO button, in hundredths of a percent.
static const long    TEMPO_RANGES[] = {600, 1000, 1600, 10000};
static const uint8_t TEMPO_RANGE_COUNT = 4;
static const uint8_t TEMPO_RANGE_WIDE = 3;

// Indicator lights: display group 5, byte 12 / 13.
// [CHANGED] Stored as bit numbers instead of masks, so Arduino's bitWrite()
// can switch them.
#define DISP_GROUP_LIGHTS 5
#define DISP_IDX_JOG      17
struct Light { uint8_t index, bit; };
static const Light LIGHT_PLAY  = {13, 5};
static const Light LIGHT_VINYL = {12, 4};
static const Light LIGHT_MT    = {12, 3};
static const Light LIGHT_WIDE  = {12, 2};
// Lights that are on while their button is held.
struct HeldLight { Button button; Light light; };
static const HeldLight HELD_LIGHTS[] = {
    {Button::Cue,    {13, 4}}, {Button::Rev,   {13, 3}},
    {Button::CueIn,  {13, 2}}, {Button::CueOut, {13, 6}},
    {Button::Beat8,  {13, 1}}, {Button::Beat4, {13, 0}},
    {Button::Beat2,  {12, 7}}, {Button::Beat1, {12, 6}},
};

// --- state -----------------------------------------------------------------
static uint32_t last_buttons = 0;
static uint16_t last_jog_pulse = 0;
static int      last_pitch_midi = -1;
static bool     panel_online = false;
static bool     jog_touched = false;   // scratch active (Vinyl on + jog moving)
static unsigned long last_jog_move_ms = 0, last_frame_ms = 0;

static bool     playing = false, vinyl = false, master_tempo = false;
static int      jog_ring = 1;           // 1..135
static float    base_bpm = DEFAULT_BASE_BPM;
static uint8_t  tempo_range = 0;        // index into TEMPO_RANGES (starts at +-6 %)
static int16_t  tempo_hundredths = 0;
static uint16_t track_number = 1;
static bool     display_dirty = true;
static int16_t  shown_tempo = INT16_MIN;
static int      shown_bpm = -1;

// --- panel output ------------------------------------------------------------
static void set_light(const Light &l, bool on) {
    uint8_t v = pico_spi_slave_get_display_byte(DISP_GROUP_LIGHTS, l.index);
    bitWrite(v, l.bit, on);  // [CHANGED] Arduino bitWrite() instead of |= mask / &= ~mask
    pico_spi_slave_set_display_byte(DISP_GROUP_LIGHTS, l.index, v);
}

static void set_jog_ring(uint8_t pos) {
    for (uint8_t g = 0; g < DISPLAY_GROUPS; ++g)
        pico_spi_slave_set_display_byte(g, DISP_IDX_JOG, pos);
}

static void update_lights(uint32_t buttons) {
    // [CHANGED] One loop over HELD_LIGHTS + Arduino bitRead() replaces eight
    // separate set_light() calls and the held() lambda.
    for (const HeldLight &h : HELD_LIGHTS)
        set_light(h.light, bitRead(buttons, (uint8_t)h.button));
    set_light(LIGHT_PLAY, playing);
    set_light(LIGHT_VINYL, vinyl);
    set_light(LIGHT_MT, master_tempo);
    set_light(LIGHT_WIDE, tempo_range == TEMPO_RANGE_WIDE);
}

// Pitch fader -> tempo deviation in hundredths of a percent for the current range.
static int16_t tempo_from_pitch(uint16_t raw) {
    const long range = TEMPO_RANGES[tempo_range];
    // [CHANGED] Arduino constrain() + map() replace the manual scaling.
    const long p = constrain((long)raw, (long)PITCH_RAW_MIN, (long)PITCH_RAW_MAX);
    long h = PITCH_INVERT ? map(p, PITCH_RAW_MIN, PITCH_RAW_MAX, -range, range)
                          : map(p, PITCH_RAW_MIN, PITCH_RAW_MAX, range, -range);
    if (labs(h) * 100L < range) h = 0;  // snap the middle to exactly 0.00 %
    // Same steps as the CDJ-800 shows: 0.02 % at +-6, 0.05 % at +-10/16, 0.5 % wide.
    const long step = range <= 600 ? 2 : range <= 1600 ? 5 : 50;
    return (int16_t)(lroundf((float)h / step) * step);  // [CHANGED] lroundf() instead of manual rounding
}

static void refresh_display(void) {
    const int bpm = (int)lroundf(base_bpm * (10000.0f + tempo_hundredths) / 10000.0f);
    if (!display_dirty && bpm == shown_bpm && tempo_hundredths == shown_tempo) return;
    display_dirty = false;
    shown_bpm = bpm;
    shown_tempo = tempo_hundredths;
    panel_display::set_bpm((uint16_t)max(bpm, 0));
    panel_display::set_tempo(tempo_hundredths, tempo_range == TEMPO_RANGE_WIDE, master_tempo);
    panel_display::set_track(track_number);
}

// --- MIDI ------------------------------------------------------------------------
// Release is Note On with velocity 0, like the real CDJ-850. Mixxx's
// "Pioneer CDJ-850" mapping only binds status 0x90; a real Note Off (0x80)
// is ignored, so Cue stayed "held" and Search / hot cues never released.
static void send_note(uint8_t note, bool down) {
    MIDI.sendNoteOn(note, down ? 127 : 0, MIDI_CH);
}

static void release_all(void) {
    for (uint8_t i = 0; i < (uint8_t)Button::Count; ++i)
        if (bitRead(last_buttons, i)) send_note(NOTE_FOR[i], false);  // [CHANGED] bitRead()
    last_buttons = 0;
    if (jog_touched) send_note(MIDI_NOTE_JOG_TOUCH, false);
    jog_touched = false;
    update_lights(0);
}

static void on_button_down(Button btn) {
    switch (btn) {
        case Button::Play:  playing = !playing; break;
        case Button::Cue:   playing = false; break;  // CDJ cue: jump back and pause
        case Button::Vinyl: vinyl = !vinyl; break;
        case Button::MasterTempo: master_tempo = !master_tempo; display_dirty = true; break;
        case Button::Tempo:
            tempo_range = (tempo_range + 1) % TEMPO_RANGE_COUNT;
            display_dirty = true;
            break;
        case Button::TrackSearchFwd: if (track_number < 999) { track_number++; display_dirty = true; } break;
        case Button::TrackSearchRev: if (track_number > 1)   { track_number--; display_dirty = true; } break;
        default: break;
    }
}

static void handle_cdj_packet(const StatusMessage &msg) {
    const uint32_t buttons = msg.buttons();
    const uint32_t changed = buttons ^ last_buttons;

    for (uint8_t i = 0; i < (uint8_t)Button::Count; ++i) {
        if (!bitRead(changed, i)) continue;               // [CHANGED] bitRead()
        const bool down = bitRead(buttons, i);
        send_note(NOTE_FOR[i], down);
        if (down) on_button_down((Button)i);
    }
    if (changed) update_lights(buttons);
    last_buttons = buttons;

    // Jog: the counter wraps at 16 bit; the signed cast handles that.
    const int16_t delta = (int16_t)(msg.jogPulse - last_jog_pulse);
    last_jog_pulse = msg.jogPulse;

    // Vinyl ON  -> scratch: Note 0x20 held while the jog turns, movement on CC 0x10.
    // Vinyl OFF -> pitch bend / nudge on CC 0x30. The touch sensor is not
    // decoded yet, so jog movement stands in for "hand on the plate".
    const unsigned long now = millis();
    if (panel_online && delta != 0) last_jog_move_ms = now;
    const bool touched = vinyl && last_jog_move_ms != 0 && now - last_jog_move_ms < JOG_RELEASE_MS;
    if (touched != jog_touched) {
        jog_touched = touched;
        send_note(MIDI_NOTE_JOG_TOUCH, touched);
    }

    if (panel_online && delta != 0) {
        const uint8_t cc = jog_touched ? MIDI_CC_JOG_SCRATCH : MIDI_CC_JOG;
        int remaining = JOG_INVERT ? -delta : delta;
        while (remaining != 0) {   // relative CC: 64 = no move, 65..127 fwd, 1..63 rev
            const int step = constrain(remaining, -63, 63);
            MIDI.sendControlChange(cc, (uint8_t)(64 + step), MIDI_CH);
            remaining -= step;
        }
        jog_ring = ((jog_ring - 1 + delta) % 135 + 135) % 135 + 1;
        set_jog_ring((uint8_t)jog_ring);
    }

    // [CHANGED] Arduino constrain() + map() replace the manual pitch scaling.
    const long p = constrain((long)msg.pitch, (long)PITCH_RAW_MIN, (long)PITCH_RAW_MAX);
    const int midi_pitch = PITCH_INVERT ? map(p, PITCH_RAW_MIN, PITCH_RAW_MAX, 127, 0)
                                        : map(p, PITCH_RAW_MIN, PITCH_RAW_MAX, 0, 127);
    if (midi_pitch != last_pitch_midi) {
        last_pitch_midi = midi_pitch;
        MIDI.sendControlChange(MIDI_CC_TEMPO, (uint8_t)midi_pitch, MIDI_CH);
    }

    tempo_hundredths = tempo_from_pitch(msg.pitch);
    refresh_display();
}

// --- serial commands ---------------------------------------------------------------
// [CHANGED] Serial.readStringUntil() + String methods (trim, startsWith,
// substring, toFloat, toInt) replace the hand-written character buffer and
// sscanf() parsing.
static void poll_serial(void) {
    if (!Serial.available()) return;
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.startsWith("bpm ")) {
        const float b = line.substring(4).toFloat();
        if (b > 0.0f && b < 400.0f) {
            base_bpm = b; display_dirty = true; refresh_display();
            Serial.printf("base BPM = %.1f\n", base_bpm);
        } else {
            Serial.println("bpm must be 1..399");
        }
    } else if (line.startsWith("track ")) {
        const long t = line.substring(6).toInt();
        if (t >= 1 && t <= 999) {
            track_number = (uint16_t)t; display_dirty = true; refresh_display();
            Serial.printf("track = %u\n", track_number);
        } else {
            Serial.println("track must be 1..999");
        }
    } else if (line.length()) {
        Serial.println("commands: bpm <value> | track <n>");
    }
}

void setup() {
    // Power the level shifter first so the SPI lines are live before init.
    pinMode(PIN_SHIFTER_VA, OUTPUT);
    digitalWrite(PIN_SHIFTER_VA, HIGH);
    delay(10);
    pinMode(PIN_STATUS_LED, OUTPUT);

    if (!TinyUSBDevice.isInitialized()) TinyUSBDevice.begin(0);
    TinyUSBDevice.setManufacturerDescriptor("Pioneer DJ");
    TinyUSBDevice.setProductDescriptor("PIONEER CDJ-850");
    usb_midi.setStringDescriptor("PIONEER CDJ-850 MIDI");
    MIDI.begin(MIDI_CHANNEL_OMNI);
    Serial.begin(115200);
    Serial.setTimeout(50);  // readStringUntil() gives up after 50 ms without '\n'

    // The core starts USB (serial only) before setup(), so the host may have
    // enumerated the old descriptor. Disconnect long enough for a hub to
    // notice (~100 ms debounce), then reconnect so the host reads the full
    // descriptor with MIDI and the CDJ-850 name.
    TinyUSBDevice.detach();
    delay(300);
    TinyUSBDevice.attach();

    pico_spi_slave_init();
    set_jog_ring((uint8_t)jog_ring);
    update_lights(0);
    refresh_display();
}

void loop() {
#ifdef TINYUSB_NEED_POLLING_TASK
    TinyUSBDevice.task();
#endif
    MIDI.read();
    const unsigned long now = millis();

    uint8_t frame[BUFFER_LENGTH];
    if (pico_spi_slave_read_frame(frame)) {  // [CHANGED] only valid frames arrive here
        handle_cdj_packet(StatusMessage::createMessage(frame));
        last_frame_ms = now;
        panel_online = true;
    }

    if (panel_online && now - last_frame_ms > PANEL_TIMEOUT_MS) {
        panel_online = false;
        release_all();
    }

    poll_serial();

    // Onboard LED: steady when the panel talks to us, slow blink otherwise.
    digitalWrite(PIN_STATUS_LED, panel_online || (now / 500) % 2);
}
