#pragma once 

#include "cdjay.h"
#include <MIDI.h>



// put function declarations here:
void midiSetup();

void handleNoteOn(byte channel, byte pitch, byte velocity);
void handleNoteOff(byte channel, byte pitch, byte velocity);


