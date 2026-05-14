#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/gpio.h"
#include "pico/binary_info.h"

#include "bsp/board.h"
#include "tusb.h"


// Pico W devices use a GPIO on the WIFI chip for the LED,
// so when building for Pico W, CYW43_WL_GPIO_LED_PIN will be defined
#ifdef CYW43_WL_GPIO_LED_PIN
#include "pico/cyw43_arch.h"
#endif

#ifndef PICO_RED_LED_GPIO_PIN
#define PICO_RED_LED_GPIO_PIN 13
#endif


enum  {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 100000,
  BLINK_SUSPENDED = 100,
};


int led_init(void) {
  #if defined(PICO_DEFAULT_LED_PIN)
    // A device like Pico that uses a GPIO for the LED will define PICO_DEFAULT_LED_PIN
    // so we can use normal GPIO functionality to turn the led on and off
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    return PICO_OK;
  #elif defined(CYW43_WL_GPIO_LED_PIN)
    // For Pico W devices we need to initialise the driver etc
    return cyw43_arch_init();
  #elif defined(PICO_RED_LED_GPIO_PIN)
    gpio_init(PICO_RED_LED_GPIO_PIN);
    gpio_set_dir(PICO_RED_LED_GPIO_PIN, GPIO_OUT);
    return PICO_OK;
  #else
    return PICO_ERROR_NOT_SUPPORTED;
  #endif
}

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;
const uint LED_PIN = PICO_RED_LED_GPIO_PIN;

void led_blinking_task(void);
void midi_task(void);
void pico_set_led(bool led_on);

int main() {
  board_init();
  int rc = led_init();
  hard_assert(rc == PICO_OK);
  
  // Signal: slow blink = starting
  pico_set_led(true);
  sleep_ms(5000);
  pico_set_led(false);
  sleep_ms(5000);
  
  stdio_init_all();
  printf("=== Pico MIDI Device Starting ===\n");
  
  // Signal: medium blink = calling tusb_init
  pico_set_led(true);
  sleep_ms(200);
  pico_set_led(false);
  sleep_ms(200);
  
  tusb_init();
  printf("TinyUSB initialized\n");
  
  // Signal: fast blink = tusb_init done
  pico_set_led(true);
  sleep_ms(100);
  pico_set_led(false);
  sleep_ms(100);
  printf("LED initialized\n");
  while (1)
  {
    tud_task(); // tinyusb device task
    led_blinking_task();
    midi_task();
  }
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
  printf("Device mounted\n");
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  blink_interval_ms = BLINK_NOT_MOUNTED;
  printf("Device unmounted\n");
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

//--------------------------------------------------------------------+
// MIDI Task
//--------------------------------------------------------------------+

// Variable that holds the current position in the sequence.
uint32_t note_pos = 0;

// Store example melody as an array of note values (First on is 'play' from the MIDI excel list)
uint8_t note_sequence[] =
{
  41, 43, 45, 46, 48, 50, 52, 53, 55, 57, 59, 60
};

void midi_task(void)
{
  static uint32_t start_ms = 0;
  uint8_t msg[3];

  // send note every 1000 ms
  uint32_t now_ms = time_us_32() / 1000;
  if (now_ms - start_ms < 286) return; // not enough time
  start_ms = now_ms;

  // Previous positions in the note sequence.
  int previous = note_pos - 1;

  // If we currently are at position 0, set the
  // previous position to the last note in the sequence.
  if (previous < 0) previous = sizeof(note_sequence) - 1;

  // Send Note On for current position at full velocity (127) on channel 1.
  msg[0] = 0x90;                    // Note On - Channel 1
  msg[1] = note_sequence[note_pos]; // Note Number
  msg[2] = 127;                     // Velocity
  tud_midi_n_stream_write(0, 0, msg, 3);

  // Send Note Off for previous note.
  msg[0] = 0x80;                    // Note Off - Channel 1
  msg[1] = note_sequence[previous]; // Note Number
  msg[2] = 0;                       // Velocity
  tud_midi_n_stream_write(0, 0, msg, 3);

  // Increment position
  note_pos++;

  // If we are at the end of the sequence, start over.
  if (note_pos >= sizeof(note_sequence)) note_pos = 0;
}

void generate_midi_signal(uint8_t channel, uint8_t note, uint8_t velocity) {
  uint8_t msg[3];
  msg[0] = channel;          // Note On - Channel n
  msg[1] = note;             // Note Number
  msg[2] = velocity;         // Velocity
  tud_midi_n_stream_write(0, 0, msg, 3);
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
// Turn the led on or off
void pico_set_led(bool led_on) {
  #if defined(PICO_DEFAULT_LED_PIN)
    // Just set the GPIO on or off
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
  #elif defined(CYW43_WL_GPIO_LED_PIN)
    // Pico W / Pico 2 W use the wireless chip LED
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
  #elif defined(PICO_RED_LED_GPIO_PIN)
    gpio_put(PICO_RED_LED_GPIO_PIN, led_on);
  #endif
}

void led_blinking_task(void)
{
  static uint32_t start_ms = 0;
  static bool led_state = false;

  // Blink every interval ms
  uint32_t now_ms = time_us_32() / 1000;
  if (now_ms - start_ms < blink_interval_ms) return; // not enough time
  start_ms = now_ms;

  pico_set_led(led_state);
  led_state = 1 - led_state; // toggle
}