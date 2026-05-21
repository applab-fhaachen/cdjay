#include <stdint.h>
#define MESSAGE_COUNT 6

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
};

//Off messages (velocity=0)
MidiMsg_t falling_messages[MESSAGE_COUNT] = {
  {18, 0x90, 0, 0},//PlAY_BUTTON_PIN, 
  {17, 0x90, 1, 0},//CUE_
  {16, 0x90, 2, 0},//SEARCH_FWD
  {15, 0x90, 3, 0},//SEARCH_BWD, 
  {14, 0x90, 4, 0},//TRACK_SEARCH_FWD
  {13, 0x90, 5, 0},//TRACK_SEARCH_BWD
};