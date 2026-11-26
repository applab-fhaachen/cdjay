/**
 * CDJay - CDJ-800 front panel -> USB-MIDI (registers as "PIONEER CDJ-850")
 *
 * The Pico replaces the CDJ-800 MAIN board. The front panel is the SPI master
 * and polls us with 20-byte status frames; we answer with display frames.
 *
 * Serial (115200, USB CDC) prints a status line once per second plus every
 * button change, so the first hardware test can be read in a serial monitor.
 */

#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>
#include "StatusMessage.h"
#include "PanelDisplay.h"
#include "PicoTransport.h"

Adafruit_USBD_MIDI usb_midi;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);

#define PIN_STATUS_LED  LED_BUILTIN
#define MIDI_CH         1

// On the CDJ-800 test board GP15 is the 3.3 V supply (VA) of the TXS0108E
// level shifter. It must be driven HIGH before SPI can see anything.
#define PIN_SHIFTER_VA  15

// Breadboard buttons (GP15 / GP14 to GND). Off by default: GP15 now powers
// the level shifter, so the buttons cannot be used on the CDJ test board.
#ifndef ENABLE_BREADBOARD_BUTTONS
#define ENABLE_BREADBOARD_BUTTONS 0
#endif
#define PIN_BTN_PLAY    15
#define PIN_BTN_CUE     14

// CDJ-850 MIDI layout, matching Mixxx's bundled "Pioneer CDJ-850" mapping
// (channel 1 = [Channel1]).
#define MIDI_NOTE_PLAY_PAUSE    0x00  // play
#define MIDI_NOTE_CUE           0x01  // cue_default
#define MIDI_NOTE_SEARCH_FWD    0x02  // fwd
#define MIDI_NOTE_SEARCH_REV    0x03  // back
#define MIDI_NOTE_TRACK_NEXT    0x04  // [Playlist] SelectNextTrack
#define MIDI_NOTE_TRACK_PREV    0x05  // [Playlist] SelectPrevTrack
#define MIDI_NOTE_LOOP_IN       0x06  // loop_in
#define MIDI_NOTE_LOOP_OUT      0x07  // loop_out
#define MIDI_NOTE_RELOOP        0x08  // reloop_exit
#define MIDI_NOTE_DELETE        0x0D  // delete last used hot cue
#define MIDI_NOTE_MASTER_TEMPO  0x11  // keylock
#define MIDI_NOTE_HOTCUE_1      0x18  // hotcue 1..4 (on the Beat 1/2/4/8 buttons)
#define MIDI_NOTE_HOTCUE_2      0x19
#define MIDI_NOTE_HOTCUE_3      0x1A
#define MIDI_NOTE_HOTCUE_4      0x1C
#define MIDI_NOTE_REV           0x21  // reverse toggle
#define MIDI_NOTE_EJECT         0x2F  // eject
#define MIDI_NOTE_UNMAPPED_BASE 0x60  // other buttons: 0x60 + Button index (unused by Mixxx)
#define MIDI_CC_TEMPO           0x1D  // rate
#define MIDI_NOTE_JOG_TOUCH     0x20  // scratch2_enable (top plate touched in Vinyl mode)
#define MIDI_CC_JOG             0x30  // jog_wheel (pitch bend), relative around 0x40
#define MIDI_CC_JOG_SCRATCH     0x10  // jog_scratch, relative around 0x40

// Raw pitch endpoints. The real fader range is not known yet: read
// "pitch=" in the serial log at both ends of the fader and put them here.
#ifndef PITCH_RAW_MIN
#define PITCH_RAW_MIN 4388   // measured on the CDJ-800 panel, 2026-11-18
#endif
#ifndef PITCH_RAW_MAX
#define PITCH_RAW_MAX 65472  // measured on the CDJ-800 panel, 2026-11-18
#endif

// Direction of the analog controls. Mixxx's "Pioneer CDJ-850" mapping
// inverts CC 0x1D, so the CC value must fall as the fader moves toward "+".
// The CDJ-800 raw pitch value runs the other way, so it is flipped here.
// Set to 0 (e.g. -DPITCH_INVERT=0) if the fader ends up backwards again.
#ifndef PITCH_INVERT
#define PITCH_INVERT 1
#endif
// Set to 1 if turning the jog clockwise moves the track backwards in Mixxx.
#ifndef JOG_INVERT
#define JOG_INVERT 0
#endif

// No valid frame for this long -> panel considered offline, notes released.
#define PANEL_TIMEOUT_MS 500

// Display group 5 indicator bits (byte 12 / byte 13) and the jog ring byte.
#define DISP_GROUP_LIGHTS 5
#define DISP_IDX_LIGHTS_A 12
#define DISP_IDX_LIGHTS_B 13
#define DISP_IDX_JOG      17

struct Light { uint8_t index, mask; };
static const Light LIGHT_PLAY   = {13, 0x20};
static const Light LIGHT_CUE    = {13, 0x10};
static const Light LIGHT_REV    = {13, 0x08};
static const Light LIGHT_CUEIN  = {13, 0x04};
static const Light LIGHT_CUEOUT = {13, 0x40};
static const Light LIGHT_BEAT8  = {13, 0x02};
static const Light LIGHT_BEAT4  = {13, 0x01};
static const Light LIGHT_BEAT2  = {12, 0x80};
static const Light LIGHT_BEAT1  = {12, 0x40};
static const Light LIGHT_MT     = {12, 0x08};
static const Light LIGHT_VINYL  = {12, 0x10};

static const char *const BUTTON_NAMES[] = {
    "Cue", "Rev", "CueOut", "CueIn", "Beat8", "Beat4", "Beat2", "Beat1", "Reloop", "Play",
    "SearchFwd", "SearchRev", "TrackSearchFwd", "TrackSearchRev", "MasterTempo",
    "Tempo", "Vinyl", "Eject", "QuickReturn", "FolderFwd", "FolderRev", "TimeMode",
    "TextMode", "Delete", "Memory", "CallRev", "CallFwd"
};

static uint32_t last_buttons = 0;
static uint16_t last_jog_pulse = 0;
static int      last_pitch_midi = -1;
static bool     panel_online = false;
static bool     jog_touched = false;   // scratch active (Vinyl on + jog moving)
static unsigned long last_jog_move_ms = 0;

// Vinyl mode: how long after the last jog movement scratch stays on.
#ifndef JOG_RELEASE_MS
#define JOG_RELEASE_MS 80
#endif
static unsigned long last_frame_ms = 0;
static uint32_t valid_frames = 0, bad_frames = 0;
static uint8_t  last_frame[BUFFER_LENGTH];
static StatusMessage last_msg;

// Local panel feedback so a test shows the TX direction works.
static bool playing = false, vinyl = false, master_tempo = false;
static int  jog_ring = 1; // 1..135

static bool last_play_wire = HIGH, last_cue_wire = HIGH;
static unsigned long last_wire_poll_ms = 0, last_report_ms = 0;

static void print_hex(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (data[i] < 0x10) Serial.print('0');
        Serial.print(data[i], HEX);
        Serial.print(' ');
    }
}

static void set_light(const Light &l, bool on) {
    uint8_t v = pico_spi_slave_get_display_byte(DISP_GROUP_LIGHTS, l.index);
    v = on ? (v | l.mask) : (v & ~l.mask);
    pico_spi_slave_set_display_byte(DISP_GROUP_LIGHTS, l.index, v);
}

static void set_jog_ring(uint8_t pos) {
    for (uint8_t g = 0; g < DISPLAY_GROUPS; ++g)
        pico_spi_slave_set_display_byte(g, DISP_IDX_JOG, pos);
}

static uint8_t note_for(Button b) {
    switch (b) {
        case Button::Play:           return MIDI_NOTE_PLAY_PAUSE;
        case Button::Cue:            return MIDI_NOTE_CUE;
        case Button::Rev:            return MIDI_NOTE_REV;
        case Button::SearchFwd:      return MIDI_NOTE_SEARCH_FWD;
        case Button::SearchRev:      return MIDI_NOTE_SEARCH_REV;
        case Button::CueIn:          return MIDI_NOTE_LOOP_IN;
        case Button::CueOut:         return MIDI_NOTE_LOOP_OUT;
        case Button::Reloop:         return MIDI_NOTE_RELOOP;
        case Button::TrackSearchRev: return MIDI_NOTE_TRACK_PREV;
        case Button::TrackSearchFwd: return MIDI_NOTE_TRACK_NEXT;
        case Button::MasterTempo:    return MIDI_NOTE_MASTER_TEMPO;
        case Button::Delete:         return MIDI_NOTE_DELETE;
        case Button::Eject:          return MIDI_NOTE_EJECT;
        case Button::Beat1:          return MIDI_NOTE_HOTCUE_1;
        case Button::Beat2:          return MIDI_NOTE_HOTCUE_2;
        case Button::Beat4:          return MIDI_NOTE_HOTCUE_3;
        case Button::Beat8:          return MIDI_NOTE_HOTCUE_4;
        default: return MIDI_NOTE_UNMAPPED_BASE + static_cast<uint8_t>(b);
    }
}

// Release is Note On with velocity 0, like the real CDJ-850. Mixxx's
// "Pioneer CDJ-850" mapping only binds status 0x90; a real Note Off (0x80)
// is ignored, so Cue stayed "held" (track stuck at the cue point, Play
// dead) and Search / hot cues / scratch never released.
static void send_note(uint8_t note, bool down) {
    MIDI.sendNoteOn(note, down ? 127 : 0, MIDI_CH);
}

static void update_lights(uint32_t buttons) {
    auto held = [buttons](Button b) { return (buttons & (1UL << static_cast<uint8_t>(b))) != 0; };
    set_light(LIGHT_PLAY, playing);
    set_light(LIGHT_CUE, held(Button::Cue));
    set_light(LIGHT_REV, held(Button::Rev));
    set_light(LIGHT_CUEIN, held(Button::CueIn));
    set_light(LIGHT_CUEOUT, held(Button::CueOut));
    set_light(LIGHT_BEAT8, held(Button::Beat8));
    set_light(LIGHT_BEAT4, held(Button::Beat4));
    set_light(LIGHT_BEAT2, held(Button::Beat2));
    set_light(LIGHT_BEAT1, held(Button::Beat1));
    set_light(LIGHT_VINYL, vinyl);
    set_light(LIGHT_MT, master_tempo);
}

static void release_all(void) {
    for (uint8_t i = 0; i < static_cast<uint8_t>(Button::Count); ++i)
        if (last_buttons & (1UL << i)) send_note(note_for(static_cast<Button>(i)), false);
    last_buttons = 0;
    if (jog_touched) send_note(MIDI_NOTE_JOG_TOUCH, false);
    jog_touched = false;
    update_lights(0);
}

static void handle_cdj_packet(const StatusMessage &msg) {
    const uint32_t buttons = msg.buttons();
    const uint32_t changed = buttons ^ last_buttons;

    for (uint8_t i = 0; i < static_cast<uint8_t>(Button::Count); ++i) {
        if (!(changed & (1UL << i))) continue;
        const Button btn = static_cast<Button>(i);
        const bool down = (buttons & (1UL << i)) != 0;
        send_note(note_for(btn), down);
        if (down) {
            if (btn == Button::Play) playing = !playing;
            if (btn == Button::Vinyl) vinyl = !vinyl;
            if (btn == Button::MasterTempo) master_tempo = !master_tempo;
        }
        if (Serial) {
            Serial.print(down ? "DOWN " : "UP   ");
            Serial.print(BUTTON_NAMES[i]);
            Serial.print("   raw: ");
            print_hex(msg.raw, BUFFER_LENGTH);
            Serial.println();
        }
    }
    if (changed) update_lights(buttons);
    last_buttons = buttons;

    // Jog: the counter wraps at 16 bit; the signed cast handles that. The
    // first frame after (re)connect only sets the reference.
    const int16_t delta = static_cast<int16_t>(msg.jogPulse - last_jog_pulse);
    last_jog_pulse = msg.jogPulse;

    // Jog has two functions, switched by the Vinyl button:
    //  - Vinyl ON  -> scratch: while the jog turns, Note 0x20 (scratch2_enable)
    //    is held and the movement goes out on CC 0x10. JOG_RELEASE_MS after
    //    the jog stops, the note is released and normal playback resumes.
    //  - Vinyl OFF -> pitch bend / nudge on CC 0x30, never scratch.
    // The touch sensor in the plate is not decoded yet, so jog movement
    // stands in for "hand on the plate".
    const unsigned long now = millis();
    if (panel_online && delta != 0) last_jog_move_ms = now;
    const bool touched = vinyl && last_jog_move_ms != 0 && now - last_jog_move_ms < JOG_RELEASE_MS;
    if (touched != jog_touched) {
        jog_touched = touched;
        send_note(MIDI_NOTE_JOG_TOUCH, touched);
        if (Serial) Serial.println(touched ? "JOG scratch on" : "JOG scratch off");
    }

    if (panel_online && delta != 0) {
        const uint8_t cc = jog_touched ? MIDI_CC_JOG_SCRATCH : MIDI_CC_JOG;
        int remaining = JOG_INVERT ? -delta : delta;
        while (remaining != 0) {   // relative CC: 64 = no move, 65..127 fwd, 1..63 rev
            int step = constrain(remaining, -63, 63);
            MIDI.sendControlChange(cc, (uint8_t)(64 + step), MIDI_CH);
            remaining -= step;
        }
        jog_ring = ((jog_ring - 1 + delta) % 135 + 135) % 135 + 1;
        set_jog_ring((uint8_t)jog_ring);
    }

    long p = constrain((long)msg.pitch, (long)PITCH_RAW_MIN, (long)PITCH_RAW_MAX);
    int midi_pitch = (int)((p - PITCH_RAW_MIN) * 127L / ((long)PITCH_RAW_MAX - PITCH_RAW_MIN));
    if (PITCH_INVERT) midi_pitch = 127 - midi_pitch;
    if (midi_pitch != last_pitch_midi) {
        last_pitch_midi = midi_pitch;
        MIDI.sendControlChange(MIDI_CC_TEMPO, (uint8_t)midi_pitch, MIDI_CH);
    }
}

static void report(unsigned long now) {
    PicoSpiStats s;
    pico_spi_slave_get_stats(&s);
    Serial.print(panel_online ? "[panel ONLINE] " : "[panel offline] ");
    Serial.print("frames="); Serial.print(s.frames);
    Serial.print(" valid="); Serial.print(valid_frames);
    Serial.print(" badcrc="); Serial.print(bad_frames);
    Serial.print(" short="); Serial.print(s.short_frames);
    Serial.print(" long="); Serial.print(s.long_frames);
    Serial.print(" overrun="); Serial.print(s.overruns);
    // Wiring check: level + number of toggles seen while polling for 5 ms.
    static const uint8_t pins[4] = {PICO_SPI_PIN_RX, PICO_SPI_PIN_CS, PICO_SPI_PIN_SCK, PICO_SPI_PIN_TX};
    static const char *const names[4] = {"RX16", "CS17", "SCK18", "TX19"};
    uint32_t toggles[4] = {0, 0, 0, 0};
    bool prev[4];
    for (int i = 0; i < 4; ++i) prev[i] = gpio_get(pins[i]);
    const uint32_t t0 = micros();
    while (micros() - t0 < 5000) {
        for (int i = 0; i < 4; ++i) {
            bool v = gpio_get(pins[i]);
            if (v != prev[i]) { toggles[i]++; prev[i] = v; }
        }
    }
    Serial.print(" | pins");
    for (int i = 0; i < 4; ++i) {
        Serial.print(' '); Serial.print(names[i]); Serial.print('=');
        Serial.print(prev[i] ? 'H' : 'L'); Serial.print('/'); Serial.print(toggles[i]);
    }
    if (valid_frames) {
        Serial.print(" | pitch="); Serial.print(last_msg.pitch);
        Serial.print(" ref="); Serial.print(last_msg.pitchReference);
        Serial.print(" jog="); Serial.print(last_msg.jogPulse);
        Serial.print(" jogspd="); Serial.print(last_msg.jogSpeed);
        Serial.print(" jogflags=0x"); Serial.print(last_msg.jogFlags, HEX);
        Serial.print(" touch="); Serial.print(last_msg.touch);
    }
    if (s.frames) {
        Serial.print(" | last: ");
        print_hex(last_frame, BUFFER_LENGTH);
    }
    Serial.println();
    Serial.print("  bytes/frame histogram:");
    for (int i = 0; i < 22; ++i) {
        if (!s.len_hist[i]) continue;
        Serial.print(' ');
        if (i == 21) Serial.print(">20"); else Serial.print(i);
        Serial.print('x'); Serial.print(s.len_hist[i]);
    }
    Serial.print("   bit-offset histogram:");
    for (int i = 0; i < 9; ++i) {
        if (!s.off_hist[i]) continue;
        Serial.print(' ');
        if (i == 8) Serial.print("none"); else Serial.print(i);
        Serial.print('x'); Serial.print(s.off_hist[i]);
    }
    Serial.println();
    PicoSelLog log[SEL_LOG_LEN];
    const uint8_t n = pico_spi_slave_get_sel_log(log);
    Serial.print("  last bursts (bits/pre/offset/gapUs/maxGapUs@bit):");
    for (uint8_t i = 0; i < n; ++i) {
        Serial.print(' '); Serial.print(log[i].bits);
        Serial.print('/'); Serial.print(log[i].pre);
        Serial.print('/'); Serial.print(log[i].offset);
        Serial.print('/'); Serial.print(log[i].gap_us);
        Serial.print('/'); Serial.print(log[i].maxgap_us);
        Serial.print('@'); Serial.print(log[i].maxgap_at);
    }
    Serial.println();
    (void)now;
}

// ---------------------------------------------------------------------------
// Jog-touch diagnostics. Nobody has decoded the touch sensor yet, and the
// panel may only report it after the main board enables Vinyl mode in a
// display frame. Two tools for finding out on the hardware:
//  1. Status watcher: prints every change of the status bytes that are not
//     buttons, pitch or jog counter (bytes 2, 3, 12, 13; the rotation bits
//     0xC0 of byte 12 are ignored so turning the jog does not spam).
//  2. Serial commands (type in the serial monitor, end with Enter):
//       d <group> <index> <hex>   set display byte, e.g. "d 5 12 10"
//       dump                      print all six display groups
//       sweep                     set each display byte (group 0..5,
//                                 byte 1..16) to FF for 0.5 s, one after
//                                 the other, then restore. Hold the plate
//                                 pressed while it runs and watch for
//                                 "STATUS" lines.
// ---------------------------------------------------------------------------
static const uint8_t WATCH_BYTES[] = {2, 3, 12, 13};
static uint8_t watch_prev[sizeof(WATCH_BYTES)];
static bool    watch_init = false;

static void watch_status(const uint8_t *raw) {
    for (size_t i = 0; i < sizeof(WATCH_BYTES); ++i) {
        const uint8_t b = WATCH_BYTES[i];
        // Byte 3 bits 0x03 toggle on their own (heartbeat). Byte 12 bits
        // 0xF0 follow jog rotation: 0x80 moving, 0x40 direction, 0x30 the
        // encoder phases (Gray code). Both would drown real changes.
        const uint8_t ignore = (b == 12) ? 0xF0 : (b == 3) ? 0x03 : 0x00;
        const uint8_t v = raw[b];
        if (watch_init && ((v ^ watch_prev[i]) & ~ignore) && Serial) {
            Serial.print("STATUS byte "); Serial.print(b);
            Serial.print(": "); if (watch_prev[i] < 0x10) Serial.print('0'); Serial.print(watch_prev[i], HEX);
            Serial.print(" -> "); if (v < 0x10) Serial.print('0'); Serial.print(v, HEX);
            Serial.print("   (vinyl="); Serial.print(vinyl ? "on" : "off");
            Serial.println(")");
        }
        watch_prev[i] = v;
    }
    watch_init = true;
}

// Two sweep modes:
//  sweep          all groups, whole byte = FF, 0.5 s each (touch search)
//  bits <group>   one group, one bit at a time, 1.5 s each (find which
//                 bit lights which VFD segment, e.g. the VINYL symbol)
static bool     sweep_on = false, sweep_bits = false;
static uint8_t  sweep_group = 0, sweep_index = 1, sweep_bit = 0, sweep_saved = 0;
static unsigned long sweep_ms = 0;

static void sweep_apply(void) {
    sweep_saved = pico_spi_slave_get_display_byte(sweep_group, sweep_index);
    const uint8_t v = sweep_bits ? (uint8_t)(sweep_saved | (1u << sweep_bit)) : 0xFF;
    pico_spi_slave_set_display_byte(sweep_group, sweep_index, v);
    Serial.print(sweep_bits ? "BITS" : "SWEEP");
    Serial.print(" group "); Serial.print(sweep_group);
    Serial.print(" byte "); Serial.print(sweep_index);
    if (sweep_bits) {
        Serial.print(" bit 0x");
        if ((1u << sweep_bit) < 0x10) Serial.print('0');
        Serial.print(1u << sweep_bit, HEX);
        Serial.print("   -> d "); Serial.print(sweep_group); Serial.print(' ');
        Serial.print(sweep_index); Serial.print(' '); Serial.println(v, HEX);
    } else {
        Serial.println(" = FF");
    }
}

static void sweep_step(unsigned long now) {
    if (!sweep_on || now - sweep_ms < (sweep_bits ? 1500u : 500u)) return;
    sweep_ms = now;
    pico_spi_slave_set_display_byte(sweep_group, sweep_index, sweep_saved);
    bool next_byte = true;
    if (sweep_bits && ++sweep_bit < 8) next_byte = false;
    if (next_byte) {
        sweep_bit = 0;
        // Byte 17 (jog ring) is skipped: it is a position, not segments.
        if (++sweep_index > 16) {
            sweep_index = 1;
            if (sweep_bits) sweep_group = DISPLAY_GROUPS; else ++sweep_group;
        }
    }
    if (sweep_group >= DISPLAY_GROUPS) {
        sweep_on = false;
        Serial.println("SWEEP done, display restored");
        return;
    }
    sweep_apply();
}

static void dump_display(void) {
    for (uint8_t g = 0; g < DISPLAY_GROUPS; ++g) {
        Serial.print("group "); Serial.print(g); Serial.print(": ");
        for (uint8_t i = 1; i <= 17; ++i) {
            const uint8_t v = pico_spi_slave_get_display_byte(g, i);
            if (v < 0x10) Serial.print('0');
            Serial.print(v, HEX); Serial.print(' ');
        }
        Serial.println();
    }
}

static void handle_command(char *line) {
    unsigned g, i, v;
    int t;
    if (sscanf(line, "d %u %u %x", &g, &i, &v) == 3) {
        if (pico_spi_slave_set_display_byte((uint8_t)g, (uint8_t)i, (uint8_t)v)) {
            Serial.print("display group "); Serial.print(g); Serial.print(" byte "); Serial.print(i);
            Serial.print(" = "); Serial.println(v, HEX);
        } else {
            Serial.println("bad group/index (group 0..5, index 1..17)");
        }
    } else if (strcmp(line, "dump") == 0) {
        dump_display();
    } else if (strcmp(line, "sweep") == 0) {
        if (!sweep_on) {
            sweep_on = true; sweep_bits = false; sweep_group = 0; sweep_index = 1;
            sweep_bit = 0; sweep_ms = millis();
            sweep_apply();
        }
    } else if (sscanf(line, "bits %u", &g) == 1 && g < DISPLAY_GROUPS) {
        if (!sweep_on) {
            sweep_on = true; sweep_bits = true; sweep_group = (uint8_t)g; sweep_index = 1;
            sweep_bit = 0; sweep_ms = millis();
            sweep_apply();
        }
    } else if (strcmp(line, "lamp") == 0) {
        panel_display::lampTest();
        Serial.println("lamp test: all display bytes 1..16 = FF ('clear' to undo)");
    } else if (strcmp(line, "clear") == 0) {
        panel_display::clear();
        update_lights(last_buttons);
        Serial.println("display cleared");
    } else if (sscanf(line, "bpm %u", &v) == 1) {
        Serial.println(panel_display::setBpm((uint16_t)v) ? "bpm set" : "bpm 0..399");
    } else if (sscanf(line, "track %u", &v) == 1) {
        Serial.println(panel_display::setTrack((uint16_t)v) ? "track set" : "track 0..999");
    } else if (sscanf(line, "tempo %d", &t) == 1) {
        Serial.println(panel_display::setTempo((int16_t)t, false, master_tempo)
                       ? "tempo set (hundredths of %)" : "tempo -1600..1600");
    } else if (strcmp(line, "stop") == 0) {
        if (sweep_on) {
            pico_spi_slave_set_display_byte(sweep_group, sweep_index, sweep_saved);
            sweep_on = false;
            Serial.println("SWEEP stopped, display restored");
        }
    } else if (line[0]) {
        Serial.println("commands: d <group> <index> <hex> | dump | sweep | bits <group> | stop");
        Serial.println("display:  lamp | clear | bpm <n> | track <n> | tempo <hundredths>");
    }
}

static void poll_serial(void) {
    static char buf[40];
    static uint8_t len = 0;
    while (Serial.available()) {
        const char c = (char)Serial.read();
        if (c == '\r' || c == '\n') {
            buf[len] = 0; len = 0;
            handle_command(buf);
        } else if (len < sizeof(buf) - 1) {
            buf[len++] = c;
        }
    }
}

void setup() {
    // Power the level shifter first so the SPI lines are live before init.
    pinMode(PIN_SHIFTER_VA, OUTPUT);
    digitalWrite(PIN_SHIFTER_VA, HIGH);
    delay(10);

    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, LOW);
#if ENABLE_BREADBOARD_BUTTONS
    pinMode(PIN_BTN_PLAY, INPUT_PULLUP);
    pinMode(PIN_BTN_CUE, INPUT_PULLUP);
#endif

    if (!TinyUSBDevice.isInitialized()) TinyUSBDevice.begin(0);
    TinyUSBDevice.setManufacturerDescriptor("Pioneer DJ");
    TinyUSBDevice.setProductDescriptor("PIONEER CDJ-850");
    usb_midi.setStringDescriptor("PIONEER CDJ-850 MIDI");
    MIDI.begin(MIDI_CHANNEL_OMNI);
    Serial.begin(115200);

    // The core starts USB (serial only) before setup(), so the host may
    // already have enumerated - or, behind a slow hub, be in the middle of
    // enumerating - the old descriptor. Always disconnect long enough for
    // a hub to notice (its debounce is ~100 ms), then reconnect so the host
    // reads the full descriptor with MIDI and the CDJ-850 name. A short or
    // conditional reconnect left the device as "Unnamed Device" with no
    // serial port behind a USB 2.0 hub.
    TinyUSBDevice.detach();
    delay(300);
    TinyUSBDevice.attach();

    pico_spi_slave_init();
    set_jog_ring((uint8_t)jog_ring);
    update_lights(0);
}

void loop() {
#ifdef TINYUSB_NEED_POLLING_TASK
    TinyUSBDevice.task();
#endif
    MIDI.read();

    const unsigned long now = millis();

    uint8_t frame[BUFFER_LENGTH];
    if (pico_spi_slave_read_frame(frame)) {
        memcpy(last_frame, frame, BUFFER_LENGTH);
        if (StatusMessage::isValid(frame)) {
            const StatusMessage msg = StatusMessage::createMessage(frame);
            watch_status(frame);
            handle_cdj_packet(msg);
            last_msg = msg;
            valid_frames++;
            last_frame_ms = now;
            if (!panel_online && Serial) Serial.println("panel connected");
            panel_online = true;
        } else {
            bad_frames++;
        }
    }

    if (panel_online && now - last_frame_ms > PANEL_TIMEOUT_MS) {
        panel_online = false;
        release_all();
        if (Serial) Serial.println("panel timeout: all notes released");
    }

    poll_serial();
    sweep_step(now);

    // Onboard LED: steady when the panel talks to us, slow blink otherwise.
    digitalWrite(PIN_STATUS_LED, panel_online ? HIGH : ((now / 500) & 1));

    if (now - last_report_ms >= 1000) {
        last_report_ms = now;
        if (Serial) report(now);
    }

#if ENABLE_BREADBOARD_BUTTONS
    if (now - last_wire_poll_ms >= 10) {
        last_wire_poll_ms = now;
        bool play = digitalRead(PIN_BTN_PLAY);
        if (play != last_play_wire) {
            last_play_wire = play;
            send_note(MIDI_NOTE_PLAY_PAUSE, play == LOW);
        }
        bool cue = digitalRead(PIN_BTN_CUE);
        if (cue != last_cue_wire) {
            last_cue_wire = cue;
            send_note(MIDI_NOTE_CUE, cue == LOW);
        }
    }
#endif
}
