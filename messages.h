#include <stdint.h>
#define MESSAGE_COUNT 12

typedef struct {
  uint Gpio;
  uint8_t channel;
  uint8_t value;
  uint8_t velocity;
} MidiMsg_t;

//On messages (velocity=127)
MidiMsg_t rising_messages[MESSAGE_COUNT] = {
  {18, 0x90, 0, 127},//PlAY_BUTTON_PIN, 
  {17, 0x90, 1, 127},//CUE
  {16, 0x90, 2, 127},//SEARCH_FWD
  {15, 0x90, 3, 127},//SEARCH_BWD, 
  {14, 0x90, 4, 127},//TRACK_SEARCH_FWD
  {13, 0x90, 5, 127},//TRACK_SEARCH_BWD
  {14, 0x90, 6, 127},//In Cue
  {13, 0x90, 7, 127},//Out Cue
  {14, 0x90, 8, 127},//Reloop Exit
  {13, 0x90, 0x0B, 127},//Cue Loop FWD
  {14, 0x90, 0x0C, 127},//Cue Loop BWD
  {13, 0x90, 10, 127},//Tempo Factor
};

//Off messages (velocity=0)
MidiMsg_t falling_messages[MESSAGE_COUNT] = {
  {18, 0x90, 0, 0},//PlAY_BUTTON_PIN, 
  {17, 0x90, 1, 0},//CUE_
  {16, 0x90, 2, 0},//SEARCH_FWD
  {15, 0x90, 3, 0},//SEARCH_BWD, 
  {14, 0x90, 4, 0},//TRACK_SEARCH_FWD
  {13, 0x90, 5, 0},//TRACK_SEARCH_BWD
  {14, 0x90, 6, 0},//In Cue
  {13, 0x90, 7, 0},//Out Cue
  {14, 0x90, 8, 0},//Reloop Exit
  {13, 0x90, 0x0B, 0},//Cue Loop FWD
  {14, 0x90, 0x0C, 0},//Cue Loop BWD
  {13, 0x90, 10, 0},//Tempo Factor
};