#include "cdjay.h"

#include "cdjay_midi.h"
#include "cdjay_display.h"
#include "cdjay_audio.h"

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

// Manual begin() is required on core without built-in support e.g. mbed rp2040
  if (!TinyUSBDevice.isInitialized()) {
    TinyUSBDevice.begin(0);
  }

  // Serial.begin(115200);
  audioSetup();
  midiSetup();
  // hidSetup();
  displaySetup();


  // If already enumerated, additional class driverr begin() e.g msc, hid, midi won't take effect until re-enumeration
  if (TinyUSBDevice.mounted()) {
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();
  }

}

void loop() {
  #ifdef TINYUSB_NEED_POLLING_TASK
  // Manual call tud_task since it isn't called by Core's background
  TinyUSBDevice.task();
  #endif

  // not enumerated()/mounted() yet: nothing to do
  if (!TinyUSBDevice.mounted()) {
    return;
  }

  // static uint32_t start_ms = 0;
  // if (millis() - start_ms > 266) {
  //   start_ms += 266;

  //   // Setup variables for the current and previous
  //   // positions in the note sequence.
  //   int previous = midiPosition - 1;

  //   // If we currently are at position 0, set the
  //   // previous position to the last note in the sequence.
  //   if (previous < 0) {
  //     previous = sizeof(note_sequence) - 1;
  //   }

  //   // Send Note On for current position at full velocity (127) on channel 1.
  //   MIDI.sendNoteOn(note_sequence[midiPosition], 127, 1);

  //   // Send Note Off for previous note.
  //   MIDI.sendNoteOff(note_sequence[previous], 0, 1);

  //   // Increment position
  //   midiPosition++;

  //   // If we are at the end of the sequence, start over.
  //   if (midiPosition >= sizeof(note_sequence)) {
  //     midiPosition = 0;
  //   }
  // }

  // read any new MIDI messages
  // MIDI.read();
}


