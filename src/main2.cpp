/**
 * CDJay - Pioneer CDJ-850 USB-MIDI Controller (Version 2)
 * Integrates Alejandro's CDJ-800 SPI Status Decoder + Pico SPI Slave Transport + USB MIDI for Mixxx / Mac
 */

#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>
#include "StatusMessage.h"
#include "PicoTransport.h"

// USB MIDI instance
Adafruit_USBD_MIDI usb_midi;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);

// Pin Configuration for standalone breadboard testing (optional manual pins)
#define PIN_BTN_PLAY    15        // GP15: Play/Pause Button
#define PIN_BTN_CUE     14        // GP14: CUE Button
#define PIN_POT_PITCH   26        // GP26 / ADC0: Tempo / Pitch Slider
#define PIN_STATUS_LED  LED_BUILTIN

// Pioneer CDJ-850 Official MIDI Specification
#define MIDI_NOTE_PLAY_PAUSE    0x00      // Note 0: Play / Pause
#define MIDI_NOTE_CUE           0x01      // Note 1: CUE
#define MIDI_NOTE_REV           0x02      // Note 2: Reverse Direction
#define MIDI_NOTE_TRACK_PREV    0x04      // Note 4: Previous Track
#define MIDI_NOTE_TRACK_NEXT    0x05      // Note 5: Next Track
#define MIDI_NOTE_LOOP_IN       0x06      // Note 6: Loop In / Real-time Cue
#define MIDI_NOTE_LOOP_OUT      0x07      // Note 7: Loop Out
#define MIDI_NOTE_RELOOP        0x08      // Note 8: Reloop / Exit
#define MIDI_NOTE_MASTER_TEMPO  0x0A      // Note 10: Master Tempo
#define MIDI_NOTE_VINYL         0x13      // Note 19: Jog Mode Vinyl / CDJ
#define MIDI_CC_TEMPO           0x1D      // CC 29: Pitch / Tempo Slider
#define MIDI_CC_JOG             0x21      // CC 33: Relative Jog Wheel movement

// Previous states
static uint32_t last_buttons = 0;
static uint16_t last_jog_pulse = 0;
static int      last_pitch_midi = -1;

// Standalone test wire states
static bool last_play_wire = HIGH;
static bool last_cue_wire  = HIGH;
static unsigned long last_wire_poll_ms = 0;

/**
 * Process a decoded StatusMessage from the CDJ-800 panel
 * (Provided by Alejandro's reverse-engineered packet parser)
 */
void handle_cdj_packet(const StatusMessage &msg) {
    uint32_t current_buttons = msg.buttons();

    // Check each of the 27 buttons decoded by Alejandro
    for (uint8_t i = 0; i < static_cast<uint8_t>(Button::Count); ++i) {
        Button btn = static_cast<Button>(i);
        bool is_down = msg.down(btn);
        bool was_down = (last_buttons & (1UL << i)) != 0;

        if (is_down != was_down) {
            uint8_t velocity = is_down ? 127 : 0;

            switch (btn) {
                case Button::Play:
                    MIDI.sendNoteOn(MIDI_NOTE_PLAY_PAUSE, velocity, 1);
                    digitalWrite(PIN_STATUS_LED, is_down ? HIGH : LOW);
                    break;
                case Button::Cue:
                    MIDI.sendNoteOn(MIDI_NOTE_CUE, velocity, 1);
                    break;
                case Button::Rev:
                    MIDI.sendNoteOn(MIDI_NOTE_REV, velocity, 1);
                    break;
                case Button::CueIn:
                    MIDI.sendNoteOn(MIDI_NOTE_LOOP_IN, velocity, 1);
                    break;
                case Button::CueOut:
                    MIDI.sendNoteOn(MIDI_NOTE_LOOP_OUT, velocity, 1);
                    break;
                case Button::Reloop:
                    MIDI.sendNoteOn(MIDI_NOTE_RELOOP, velocity, 1);
                    break;
                case Button::TrackSearchRev:
                    MIDI.sendNoteOn(MIDI_NOTE_TRACK_PREV, velocity, 1);
                    break;
                case Button::TrackSearchFwd:
                    MIDI.sendNoteOn(MIDI_NOTE_TRACK_NEXT, velocity, 1);
                    break;
                case Button::MasterTempo:
                    MIDI.sendNoteOn(MIDI_NOTE_MASTER_TEMPO, velocity, 1);
                    break;
                case Button::Vinyl:
                    MIDI.sendNoteOn(MIDI_NOTE_VINYL, velocity, 1);
                    break;
                default:
                    // Other buttons (Beat 1/2/4/8, Call, Memory, etc.)
                    break;
            }
        }
    }
    last_buttons = current_buttons;

    // Relative Jog Wheel calculation
    if (msg.jogActive) {
        int16_t delta = static_cast<int16_t>(msg.jogPulse - last_jog_pulse);
        if (delta != 0) {
            // Send relative jog CC (64 = center, >64 = forward, <64 = reverse)
            uint8_t jog_val = (delta > 0) ? (64 + min(delta, 63)) : (64 + max(delta, -63));
            MIDI.sendControlChange(MIDI_CC_JOG, jog_val, 1);
        }
    }
    last_jog_pulse = msg.jogPulse;

    // 16-bit Pitch Fader from CDJ front panel
    int midi_pitch = map(msg.pitch, 0, 65535, 0, 127);
    if (abs(midi_pitch - last_pitch_midi) >= 1) {
        last_pitch_midi = midi_pitch;
        MIDI.sendControlChange(MIDI_CC_TEMPO, midi_pitch, 1);
    }
}

void setup() {
    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, LOW);

    // Standalone test pins with pullups
    pinMode(PIN_BTN_PLAY, INPUT_PULLUP);
    pinMode(PIN_BTN_CUE, INPUT_PULLUP);

    Serial.begin(115200);

    // Initialize Pico SPI in SLAVE mode (Slave to CDJ)
    pico_spi_slave_init();

    // Pioneer CDJ-850 USB Identifiers
    TinyUSBDevice.setManufacturerDescriptor("Pioneer DJ");
    TinyUSBDevice.setProductDescriptor("PIONEER CDJ-850");

    MIDI.begin(MIDI_CHANNEL_OMNI);

    unsigned long usb_wait_start = millis();
    while (!TinyUSBDevice.mounted() && (millis() - usb_wait_start) < 2000) {
        delay(1);
    }
}

void loop() {
    #ifdef TINYUSB_NEED_POLLING_TASK
    TinyUSBDevice.task();
    #endif

    MIDI.read();

    // -------------------------------------------------------------------------
    // 1. Check for incoming SPI packets from the CDJ front panel
    // -------------------------------------------------------------------------
    StatusMessage incoming_msg;
    if (pico_spi_slave_read_packet(&incoming_msg)) {
        handle_cdj_packet(incoming_msg);
    }

    // -------------------------------------------------------------------------
    // 2. Standalone wire testing (GP15 and GP14 on your breadboard)
    // -------------------------------------------------------------------------
    unsigned long now = millis();
    if (now - last_wire_poll_ms >= 10) {
        last_wire_poll_ms = now;

        bool current_play = digitalRead(PIN_BTN_PLAY);
        if (current_play != last_play_wire) {
            last_play_wire = current_play;
            if (current_play == LOW) {
                digitalWrite(PIN_STATUS_LED, HIGH);
                MIDI.sendNoteOn(MIDI_NOTE_PLAY_PAUSE, 127, 1);
            } else {
                digitalWrite(PIN_STATUS_LED, LOW);
                MIDI.sendNoteOff(MIDI_NOTE_PLAY_PAUSE, 0, 1);
            }
        }

        bool current_cue = digitalRead(PIN_BTN_CUE);
        if (current_cue != last_cue_wire) {
            last_cue_wire = current_cue;
            if (current_cue == LOW) {
                digitalWrite(PIN_STATUS_LED, HIGH);
                MIDI.sendNoteOn(MIDI_NOTE_CUE, 127, 1);
            } else {
                digitalWrite(PIN_STATUS_LED, LOW);
                MIDI.sendNoteOff(MIDI_NOTE_CUE, 0, 1);
            }
        }
    }
}
