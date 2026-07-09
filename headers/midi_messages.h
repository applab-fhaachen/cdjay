#ifndef MESSAGES_H
#define MESSAGES_H

#include <stdint.h>
#include <stddef.h>
#include "pico/stdlib.h"

#define MESSAGE_COUNT 12

#define PLAY_BUTTON_PIN 19
#define CUE_BUTTON_PIN 18
#define SEARCH_FWD_BUTTON_PIN 16
#define SEARCH_BWD_BUTTON_PIN 15
#define TRACK_SEARCH_FWD_BUTTON_PIN 14
#define TRACK_SEARCH_BWD_BUTTON_PIN 13
#define IN_CUE_BUTTON_PIN 14
#define OUT_CUE_BUTTON_PIN 13
#define RELOOP_EXIT_BUTTON_PIN 14
#define CUE_LOOP_FWD_BUTTON_PIN 13
#define CUE_LOOP_BWD_BUTTON_PIN 14
#define TEMPO_FACTOR_BUTTON_PIN 13

typedef struct {
  uint gpio;
  uint channel;
  uint value;
  uint velocity;
} MidiMsg_t;

//On messages (velocity=127)
// PIN , MIDI Channel, Value, Velocity
static const MidiMsg_t rising_messages[MESSAGE_COUNT] = {
  {PLAY_BUTTON_PIN, 0x90, 0, 127},            //PlAY_BUTTON_PIN,
  {CUE_BUTTON_PIN, 0x90, 1, 127},             //CUE
  {SEARCH_FWD_BUTTON_PIN, 0x90, 2, 127},      //SEARCH_FWD
  {SEARCH_BWD_BUTTON_PIN, 0x90, 3, 127},      //SEARCH_BWD,
  {TRACK_SEARCH_FWD_BUTTON_PIN, 0x90, 4, 127},//TRACK_SEARCH_FWD
  {TRACK_SEARCH_BWD_BUTTON_PIN, 0x90, 5, 127},//TRACK_SEARCH_BWD
  {IN_CUE_BUTTON_PIN, 0x90, 6, 127},          //In Cue
  {OUT_CUE_BUTTON_PIN, 0x90, 7, 127},         //Out Cue
  {RELOOP_EXIT_BUTTON_PIN, 0x90, 8, 127},     //Reloop Exit
  {CUE_LOOP_FWD_BUTTON_PIN, 0x90, 0x0B, 127}, //Cue Loop FWD
  {CUE_LOOP_BWD_BUTTON_PIN, 0x90, 0x0C, 127}, //Cue Loop BWD
  {TEMPO_FACTOR_BUTTON_PIN, 0x90, 10, 127},   //Tempo Factor
};

//Off messages (velocity=0)
// PIN , MIDI Channel, Value, Velocity
static const MidiMsg_t falling_messages[MESSAGE_COUNT] = {
  {PLAY_BUTTON_PIN, 0x90, 0, 0},            //PlAY_BUTTON_PIN,
  {CUE_BUTTON_PIN, 0x90, 1, 0},             //CUE
  {SEARCH_FWD_BUTTON_PIN, 0x90, 2, 0},      //SEARCH_FWD
  {SEARCH_BWD_BUTTON_PIN, 0x90, 3, 0},      //SEARCH_BWD,
  {TRACK_SEARCH_FWD_BUTTON_PIN, 0x90, 4, 0},//TRACK_SEARCH_FWD
  {TRACK_SEARCH_BWD_BUTTON_PIN, 0x90, 5, 0},//TRACK_SEARCH_BWD
  {IN_CUE_BUTTON_PIN, 0x90, 6, 0},          //In Cue
  {OUT_CUE_BUTTON_PIN, 0x90, 7, 0},         //Out Cue
  {RELOOP_EXIT_BUTTON_PIN, 0x90, 8, 0},     //Reloop Exit
  {CUE_LOOP_FWD_BUTTON_PIN, 0x90, 0x0B, 0},  //Cue Loop FWD
  {CUE_LOOP_BWD_BUTTON_PIN, 0x90, 0x0C, 0},  //Cue Loop BWD
  {TEMPO_FACTOR_BUTTON_PIN, 0x90, 10, 0},    //Tempo Factor
};

const MidiMsg_t *find_message(uint pin, bool pressed) {
  for (size_t i = 0; i < MESSAGE_COUNT; i++) {
    if (pressed && rising_messages[i].gpio == pin) {
      return &rising_messages[i];
    } else if (!pressed && falling_messages[i].gpio == pin) {
      return &falling_messages[i];
    }
  }
  return NULL; // Not found. Empty midi message.
}

#endif // MESSAGES_H
