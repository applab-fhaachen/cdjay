/**
 * CDJay - Pioneer CDJ-850 USB-MIDI Controller
 * PlatformIO + Earle Philhower RP2040/RP2350 Core + Adafruit TinyUSB
 */

#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>

// USB MIDI instance
Adafruit_USBD_MIDI usb_midi;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);

// Pin Configuration
#define PIN_BTN_PLAY    15        // GP15: Play/Pause Button
#define PIN_BTN_CUE     14        // GP14: CUE Button
#define PIN_POT_PITCH   26        // GP26 / ADC0: Tempo / Pitch Slider
#define PIN_STATUS_LED  LED_BUILTIN

// Pioneer CDJ-850 Official MIDI Specification
#define MIDI_NOTE_PLAY_PAUSE    0x00      // Note 0: Play / Pause
#define MIDI_NOTE_CUE           0x01      // Note 1: CUE
#define MIDI_CC_TEMPO           0x1D      // CC 29: Pitch / Tempo Slider

// State tracking variables
static bool last_play_state = HIGH;
static bool last_cue_state  = HIGH;
static int  last_pitch_midi = -1;

void setup() {
    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, LOW);

    // Buttons configured with internal pull-ups (Active LOW)
    pinMode(PIN_BTN_PLAY, INPUT_PULLUP);
    pinMode(PIN_BTN_CUE, INPUT_PULLUP);

    // Enable CDC Serial for debugging & auto-reset
    Serial.begin(115200);

    // Configure USB Descriptors to emulate Pioneer CDJ-850
    TinyUSBDevice.setManufacturerDescriptor("Pioneer DJ");
    TinyUSBDevice.setProductDescriptor("PIONEER CDJ-850");

    // Initialize USB-MIDI
    MIDI.begin(MIDI_CHANNEL_OMNI);

    while (!TinyUSBDevice.mounted()) {
        delay(1);
    }
}

void loop() {
    #ifdef TINYUSB_NEED_POLLING_TASK
    TinyUSBDevice.task();
    #endif

    // Flush any incoming MIDI messages from host
    MIDI.read();

    // -------------------------------------------------------------------------
    // A. PLAY / PAUSE Button (GP15 -> CDJ-850 Note 0)
    // -------------------------------------------------------------------------
    bool current_play = digitalRead(PIN_BTN_PLAY);
    if (current_play != last_play_state) {
        last_play_state = current_play;
        if (current_play == LOW) { // Button Pressed
            digitalWrite(PIN_STATUS_LED, HIGH);
            MIDI.sendNoteOn(MIDI_NOTE_PLAY_PAUSE, 127, 1);
        } else { // Button Released
            digitalWrite(PIN_STATUS_LED, LOW);
            MIDI.sendNoteOff(MIDI_NOTE_PLAY_PAUSE, 0, 1);
        }
    }

    // -------------------------------------------------------------------------
    // B. CUE Button (GP14 -> CDJ-850 Note 1)
    // -------------------------------------------------------------------------
    bool current_cue = digitalRead(PIN_BTN_CUE);
    if (current_cue != last_cue_state) {
        last_cue_state = current_cue;
        if (current_cue == LOW) { // Button Pressed
            digitalWrite(PIN_STATUS_LED, HIGH);
            MIDI.sendNoteOn(MIDI_NOTE_CUE, 127, 1);
        } else { // Button Released
            digitalWrite(PIN_STATUS_LED, LOW);
            MIDI.sendNoteOff(MIDI_NOTE_CUE, 0, 1);
        }
    }

    // -------------------------------------------------------------------------
    // C. Pitch / Tempo Slider (GP26 / ADC0 -> CDJ-850 CC 29)
    // -------------------------------------------------------------------------
    int raw_adc = analogRead(PIN_POT_PITCH); // 10-bit: 0 to 1023
    int current_pitch_midi = map(raw_adc, 0, 1023, 0, 127); // Scale to 0 - 127

    // Jitter filter to prevent floating pin noise from flooding the MIDI bus
    if (abs(current_pitch_midi - last_pitch_midi) >= 2) {
        last_pitch_midi = current_pitch_midi;
        MIDI.sendControlChange(MIDI_CC_TEMPO, current_pitch_midi, 1);
    }

    delay(10); // Debounce
}
