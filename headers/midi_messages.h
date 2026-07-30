#ifndef MESSAGES_H
#define MESSAGES_H

#include <stdint.h>
#include <stddef.h>
#include "pico/stdlib.h"

typedef struct {
  uint channel;
  uint value;
} MidiNode;

//On messages (velocity=127)
// PIN , MIDI Channel, Value, Velocity
static const MidiNode button_messages[3][5] = {
  { //KD0
    {0x90, 2},       //HOLD   S1
    {0x90, 2},       //TIME   S2
    {0x90, 1},       //EJECT  S3
    {0x90, 0},       //MT     S4
    {}               //Empty entry S5
  },
  { //KD1
    {0x90, 4},      //TRKB  S1
    {0x90, 6},      //TRKF  S2
    {0x90, 3},      //JET   S3
    {0x90, 3},      //ZIP   S4
    {0x90, 3},      //WAH   S5
  },
  { //KD2
    {0x90, 7},      //Play  S1
    {0x90, 8},      //CUE   S2
    {0x90, 0x0B},   //SCNB  S3
    {0x90, 0x0C},   //SCNF  S4
    {},             //Empty entry S5
  } 
};

/**
 * @brief Finds the MIDI message corresponding to a given button's S line and K line.
 * @param s_line The S line of the button (0-4)
 * @param k_line The K line of the button (0-2)
 * @return A pointer to the corresponding MidiNode, or NULL if out of bounds.
 */
const MidiNode *find_message(uint s_line, uint k_line) {
  if (k_line >= 3 || s_line >= 5) {
    return NULL; // Out of bounds
  }
  const MidiNode *msg = &button_messages[k_line][s_line];
  return msg;
}

#endif // MESSAGES_H
