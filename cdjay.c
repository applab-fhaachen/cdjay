#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/gpio.h"
#include "pico/binary_info.h"

#include "bsp/board.h"
#include "tusb.h"
#include "button.h"
#include "messages.h"

// Pico W devices use a GPIO on the WIFI chip for the LED,
// so when building for Pico W, CYW43_WL_GPIO_LED_PIN will be defined
#ifdef CYW43_WL_GPIO_LED_PIN
#include "pico/cyw43_arch.h"
#endif

#ifndef PLAY_LED_PIN
#define PLAY_LED_PIN 15
#endif

#ifndef PLAY_BUTTON_PIN
#define PLAY_BUTTON_PIN 18
#endif

enum  {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 100000,
  BLINK_SUSPENDED = 100,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;
const uint LED_PIN = PLAY_LED_PIN;

// Status variables
bool playing = false; //just for testing if button intterupts work as expected
static bool message_sent = true; //just for testing. Default is true to avoid sending messages.
uint8_t msg[3];

// This is an example for a state of the play Button
void led_blinking_task(void);
void midi_task(uint8_t msg[3]);
void button_cb(button_t *b);
void pico_set_led(bool led_on, bool play_led);
void status_led_blinking_task(bool play_led);
uint8_t *generate_midi_signal(uint8_t channel, uint8_t note, uint8_t velocity);
int led_init(void);
int button_init(void);
void led_pause_task(bool play_led);

int main() {
  board_init();
  int led_rc = led_init();
  int button_rc = button_init();
  
  //Check if everything is set up correctly, if not, stop the program here.
  hard_assert(led_rc == PICO_OK);
  hard_assert(button_rc == PICO_OK);
  
  // Signal: slow blink = starting
  pico_set_led(true, false);
  sleep_ms(5000);
  pico_set_led(false, false);
  sleep_ms(5000);
  
  stdio_init_all();
  // init for button handling see: include/button/button.c
  button_system_init();

  button_t *play_button = create_button(PLAY_BUTTON_PIN, button_cb);

  // Signal: medium blink = calling tusb_init
  pico_set_led(true, false);
  sleep_ms(200);
  pico_set_led(false, false);
  sleep_ms(200);
  
  tusb_init(); // tinyusb device initialization
  
  // Signal: fast blink = tusb_init done
  pico_set_led(true, false);
  sleep_ms(100);
  pico_set_led(false, false);
  sleep_ms(100);
  
  while (1)
  {
    tud_task(); // tinyusb device task
    status_led_blinking_task(false); // Blink the LED to show device status
    
    if (!message_sent){
      midi_task(msg);
      message_sent = true; // After message was send, ensure we do not send it again.
    }
    // If the play button is pressed, send MIDI messages  
    if(playing) {
      status_led_blinking_task(true); // If playing, use the play LED for blinking.
    }
  }
}
//--------------------------------------------------------------------+
// initialization
//--------------------------------------------------------------------+
// LED initialization
int led_init(void) {
  #if defined(PICO_DEFAULT_LED_PIN)
    // A device like Pico that uses a GPIO for the LED will define PICO_DEFAULT_LED_PIN
    // so we can use normal GPIO functionality to turn the led on and off
    // Just in case we war running on a pico w without CYW43_WL_GPIO_LED_PIN defined, we check for that first
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    return PICO_OK;
  #elif defined(PLAY_LED_PIN) && defined(CYW43_WL_GPIO_LED_PIN)
    // For Pico W devices we need to initialise the driver etc
    cyw43_arch_init(); //Initialise the board LED for connection feedback
    gpio_init(PLAY_LED_PIN);
    gpio_set_dir(PLAY_LED_PIN, GPIO_OUT);
    return PICO_OK;
  #elif defined(PLAY_LED_PIN)
    gpio_init(PLAY_LED_PIN);
    gpio_set_dir(PLAY_LED_PIN, GPIO_OUT);
    return PICO_OK;
  #else
    return PICO_ERROR_NOT_SUPPORTED;
  #endif
}

// Button initialization
int button_init(void) {
  #if defined(PLAY_BUTTON_PIN)
    gpio_init(PLAY_BUTTON_PIN);
    gpio_set_dir(PLAY_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(PLAY_BUTTON_PIN);
    return PICO_OK;
  #else
    return PICO_ERROR_NOT_SUPPORTED;
  #endif
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  blink_interval_ms = BLINK_NOT_MOUNTED;
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

void button_cb(button_t *b) {
  if (!b) return;

  const MidiMsg_t *message = find_message(b->pin, !b->state);
  if (!message) return;

  if (b->pin == PLAY_BUTTON_PIN && !b->state) {
    playing = !playing;
  }

  msg[0] = message->channel;
  msg[1] = message->value;
  msg[2] = message->velocity;
  message_sent = false;
}

//--------------------------------------------------------------------+
// MIDI Task
//--------------------------------------------------------------------+

void midi_task(uint8_t msg[3])
{
  tud_midi_n_stream_write(0, 0, msg, 3);
}

uint8_t *generate_midi_signal(uint8_t channel, uint8_t note, uint8_t velocity) {
  static uint8_t msg[3];
  msg[0] = channel;          // Note On - Channel n
  msg[1] = note;             // Note Number
  msg[2] = velocity;         // Velocity
  return msg;
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
// Turn the led on or off
void pico_set_led(bool led_on, bool play_led) {
  #if defined(PICO_DEFAULT_LED_PIN)
    // Just set the GPIO on or off
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
  #elif defined(CYW43_WL_GPIO_LED_PIN) && play_led == false
    // Pico W / Pico 2 W use the wireless chip LED
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
  #elif defined(PLAY_LED_PIN) && play_led == true
    gpio_put(PLAY_LED_PIN, led_on);
  #else
    // No LED defined, do nothing
  #endif
}

void status_led_blinking_task(bool play_led)
{
  static uint32_t start_ms = 0;
  static bool led_state = false;

  // Blink every interval ms
  uint32_t now_ms = time_us_32() / 1000;
  if (now_ms - start_ms < blink_interval_ms) return; // not enough time
  start_ms = now_ms;

  pico_set_led(led_state, play_led);
  led_state = 1 - led_state; // toggle
}

void led_pause_task(bool play_led) {
  pico_set_led(true, play_led);
}