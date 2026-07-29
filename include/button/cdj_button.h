/**
 * @file cdj_button.h
 * @brief Scan-matrix button lib for CDJ-Pico project
 *
 * The S lines (S1-S5) are clocked externally by the display controller,
 * one at a time. While a given S line is high, the K lines (KD0-KD2) read
 * high for any button in that column that is currently pressed. This lib
 * hooks a GPIO interrupt on each S line's rising edge, reads the K lines
 * for the buttons registered on that column, and queues an event whenever
 * a button's pressed/released state actually changes.
 * 
 * @author Nuno Caetano Pereira Soares
 * @date 2026-07-28
 */

#ifndef CDJ_BUTTON_H
#define CDJ_BUTTON_H

#include "pico/stdlib.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @def MAX_BUTTON_EVENTS
 * @brief Maximum number of button events that can be queued
 */
#define MAX_BUTTON_EVENTS 16

/**
 * @def MAX_K_PER_S
 * @brief Maximum number of buttons (K lines) that can share one S line
 */
#define MAX_K_PER_S 3

/**
 * @struct cdj_button_t
 * @brief Represents one (s_line, k_line) cell of the button matrix
 */
typedef struct cdj_button_t {
  unsigned int s_line;
  unsigned int k_line;
  /**
   * @var state
   * @brief Pressed state as of the last scan, used to detect the edge
   */
  bool state;
  /**
   * @var onchange
   * @brief Called once per press/release edge, from cdj_button_poll_events()
   */
  void (*onchange)(struct cdj_button_t *button);

  /**
   * @var last_change
   * @brief Timestamp of the last state change, used for debouncing
   */
  uint64_t last_change;
} cdj_button_t;

/**
 * @struct cdj_button_event_t
 * @brief Represents a queued button edge (press or release)
 */
typedef struct {
  cdj_button_t *button;
  bool state;
} cdj_button_event_t;

/**
 * @brief Registers a button at the given (s_line, k_line) matrix cell
 * @param s_line GPIO pin of the S line (driven externally by the display controller)
 * @param k_line GPIO pin of the K line (read back while s_line is active)
 * @param onchange Callback invoked once per press/release edge
 * @return The new button, or NULL on failure (bad pins, no onchange, or
 *         more than MAX_K_PER_S buttons already registered on this s_line)
 */
cdj_button_t *create_cdj_button(unsigned int s_line, unsigned int k_line, void (*onchange)(cdj_button_t *));

/**
 * @brief Drains the event queue and invokes onchange() for each edge (call from main loop)
 * @return Number of events processed
 */
int cdj_button_poll_events(void);

/**
 * @brief Frees a button. Does not unregister it from its column or disable
 *        its interrupt, so only safe to call for buttons that no longer
 *        receive interrupts.
 * @param button Pointer to the button to destroy
 */
void cdj_button_destroy(cdj_button_t *button);

#ifdef __cplusplus
}
#endif

#endif
