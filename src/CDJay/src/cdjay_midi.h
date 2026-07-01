#pragma once 

#ifndef CDJAY_MIDI
#define CDJAY_MIDI 

#include "cdjay.h"
#include <MIDI.h>



// put function declarations here:
void midiSetup();
void midiLoop();

void handleNoteOn(byte channel, byte pitch, byte velocity);
void handleNoteOff(byte channel, byte pitch, byte velocity);

#endif

