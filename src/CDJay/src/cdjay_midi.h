#include "cdjay.h"
#include <MIDI.h>

// USB MIDI object
Adafruit_USBD_MIDI usb_midi;


// Create a new instance of the Arduino MIDI Library,
// and attach usb_midi as the transport.
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);


// put function declarations here:
void midiSetup();

void handleNoteOn(byte channel, byte pitch, byte velocity);
void handleNoteOff(byte channel, byte pitch, byte velocity);




// Variable that holds the current position in the sequence.
uint32_t midiPosition = 0;

// Store example melody as an array of note values
byte note_sequence[] = {
    74, 78, 81, 86, 90, 93, 98, 102, 57, 61, 66, 69, 73, 78, 81, 85, 88, 92, 97, 100, 97, 92, 88, 85, 81, 78,
    74, 69, 66, 62, 57, 62, 66, 69, 74, 78, 81, 86, 90, 93, 97, 102, 97, 93, 90, 85, 81, 78, 73, 68, 64, 61,
    56, 61, 64, 68, 74, 78, 81, 86, 90, 93, 98, 102
};

