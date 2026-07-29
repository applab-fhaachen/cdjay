#include <stdlib.h>
#include "hardware/gpio.h"
#include "cdj_button.h"
#include "button.h" // reuse listen(): one shared GPIO IRQ callback, many pins

/**
 * @struct s_line_column_t
 * @brief The buttons registered on one S line, looked up when that line's
 *        interrupt fires so the handler knows which K lines to check.
 */
typedef struct {
  cdj_button_t *buttons[MAX_K_PER_S];
  uint8_t count;
} s_line_column_t;

/**
 * @var columns
 * @brief One column per GPIO pin number, indexed directly by s_line
 */
static s_line_column_t columns[28] = {0};

static cdj_button_event_t event_queue[MAX_BUTTON_EVENTS];
static volatile uint8_t queue_head = 0;
static volatile uint8_t queue_tail = 0;

/**
 * @brief Queue a button edge for later processing in the main loop
 */
static void queue_button_event(cdj_button_t *b, bool state) {
  if (!b) return;

  uint8_t next_head = (queue_head + 1) % MAX_BUTTON_EVENTS;
  if (next_head != queue_tail) {
    event_queue[queue_head].button = b;
    event_queue[queue_head].state = state;
    queue_head = next_head;
  }
}

static void k_init(unsigned int k_pin) {
  gpio_init(k_pin);
  gpio_set_dir(k_pin, GPIO_IN);
  gpio_pull_up(k_pin);
}

static void s_init(unsigned int s_pin) {
  gpio_init(s_pin);
  gpio_set_dir(s_pin, GPIO_IN);
}

/**
 * @brief Fires on an S line's rising edge. Reads every K line registered on
 *        that column and queues an event for any that changed since the
 *        last scan, so onchange() below only ever fires on real edges.
 * @param argument The s_line_column_t* for the S line that triggered
 */
static void handle_s_line_interrupt(void *argument) {
  s_line_column_t *col = (s_line_column_t *)argument;

  for (uint8_t i = 0; i < col->count; i++) {
    cdj_button_t *b = col->buttons[i];
    bool pressed = gpio_get(b->k_line);
    if (pressed != b->state) {
      uint64_t now = time_us_64();
      if (now - b->last_change >= DEBOUNCE_US) {
        b->state = pressed;
        b->last_change = now;
        queue_button_event(b, pressed);
      }
      // else: ignore this edge, it's within the debounce window
    }
  }
}

static cdj_button_t *create_cdj_button_internal(unsigned int s_line, unsigned int k_line, void (*onchange)(cdj_button_t *)) {
  if (s_line >= 28 || k_line >= 28 || !onchange) return NULL;

  s_line_column_t *col = &columns[s_line];
  if (col->count >= MAX_K_PER_S) return NULL;

  cdj_button_t *b = (cdj_button_t *)(malloc(sizeof(cdj_button_t)));
  if (!b) return NULL;

  k_init(k_line);
  s_init(s_line);

  b->s_line = s_line;
  b->k_line = k_line;
  b->onchange = onchange;
  b->state = gpio_get(k_line);

  col->buttons[col->count++] = b;
  // Safe to call once per button on the same s_line: listen() just
  // re-registers the same (column) argument and is a no-op for the pin
  // if it's already enabled.
  listen(s_line, GPIO_IRQ_EDGE_RISE, handle_s_line_interrupt, col);

  return b;
}

cdj_button_t *create_cdj_button(unsigned int s_line, unsigned int k_line, void (*onchange)(cdj_button_t *)) {
  return create_cdj_button_internal(s_line, k_line, onchange);
}

int cdj_button_poll_events(void) {
  int count = 0;
  while (queue_tail != queue_head) {
    cdj_button_event_t *event = &event_queue[queue_tail];
    if (event->button && event->button->onchange) {
      event->button->onchange(event->button);
    }
    queue_tail = (queue_tail + 1) % MAX_BUTTON_EVENTS;
    count++;
  }
  return count;
}

void cdj_button_destroy(cdj_button_t *button) {
  if (button) {
    free(button);
  }
}
