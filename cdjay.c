#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/gpio.h"
#include "pico/binary_info.h"


#include "bsp/board.h"
#include "tusb.h"
#include "cdj_button.h"
#include "button.h"
#include "encoder.h"
#include "midi_messages.h"
#include "pt6324.h"

// Pico W devices use a GPIO on the WIFI chip for the LED,
// so when building for Pico W, CYW43_WL_GPIO_LED_PIN will be defined
#ifdef CYW43_WL_GPIO_LED_PIN
#include "pico/cyw43_arch.h"
#endif 

#define S1 10
#define S2 11
#define S3 12
#define S4 13
#define S5 14

#define KD0 7
#define KD1 8
#define KD2 9

// dummy for hid report initialization
#define CDJ_OUT_REPORT_LEN 64
#define CDJ_IN_REPORT_LEN 20
#ifndef SPI_PIN_0
#define SPI_PIN_0 spi0
#endif

#ifndef CS_PIN
#define CS_PIN 4 
#endif

#ifndef RST_PIN
#define RSTB_PIN 3
#endif

#ifndef CLKB
#define CLKB_PIN 2
#endif

#ifndef DIN_PIN
#define DIN_PIN 5
#endif 

#ifndef CLKB_PIN
#define CLKB_PIN 2
#endif

#ifndef DOUT_PIN_6324
#define DOUT_PIN_6324 0
#endif
#ifndef DIN_PIN_6324
#define DIN_PIN_6324  3
#endif
#ifndef CLK_PIN_6324
#define CLK_PIN_6324  2
#endif
#ifndef STB_PIN_6324
#define STB_PIN_6324  1
#endif

enum  {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 100000,
  BLINK_SUSPENDED = 100,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;
// const uint LED_PIN = PLAY_LED_PIN;

// ----------------------------------------------------------------
// HID report state (20 bytes, vendor-defined Usage Page 0xFFA0/0xFFA1)
// Byte layout and button bit positions match the real CDJ-850 protocol,
// see headers/cdj_hid.h and headers/cdj_hid_map.h.
// ----------------------------------------------------------------
static uint8_t hid_in_report[CDJ_IN_REPORT_LEN];

// Status variables
bool playing = false; //just for testing if button intterupts work as expected
static bool message_sent = true; //just for testing. Default is true to avoid sending messages.

uint8_t msg[3];

// This is an example for a state of the play Button
void led_blinking_task(void);
void midi_task(uint8_t msg[3]);
void hid_task(void);

// void hold_button_cb(cdj_button_t *b);
// void trkb_button_cb(cdj_button_t *b);
void play_button_cb(cdj_button_t *b);
// void time_button_cb(cdj_button_t *b);
// void trkf_button_cb(cdj_button_t *b);
void cue_button_cb(cdj_button_t *b);
// void eject_button_cb(cdj_button_t *b);
// void jet_button_cb(cdj_button_t *b);
// void scnb_button_cb(cdj_button_t *b);
// void mt_button_cb(cdj_button_t *b);
// void zip_button_cb(cdj_button_t *b);
// void scnf_button_cb(cdj_button_t *b);
// void wah_button_cb(cdj_button_t *b);
void pico_set_led(bool led_on, bool play_led);
void status_led_blinking_task(bool play_led);

uint8_t *generate_midi_signal(uint8_t channel, uint8_t note, uint8_t velocity);
int led_init(void);
int button_init(void);
void led_pause_task(bool play_led);
void process_button_events(void);
uint8_t search_encoder_velocity(uint pin);
int pt_init(void);

int main() {
  board_init();
  // cdj_in_report_init(hid_in_report);
  int button_rc = button_init();
  
  //Check if everything is set up correctly, if not, stop the program here.
  // hard_assert(led_rc == PICO_OK);
  hard_assert(button_rc == PICO_OK);
  
  // Signal: slow blink = starting
  pico_set_led(true, false);
  sleep_ms(5000);
  pico_set_led(false, false);
  sleep_ms(5000);
  
  stdio_init_all();

  // init for button handling see: include/button/button.c
  button_system_init();

  // cdj_button_t *hold = create_cdj_button(S1, KD0, hold_button_cb);
  // cdj_button_t *trkb = create_cdj_button(S1, KD1, trkb_button_cb);
  cdj_button_t *play = create_cdj_button(S1, KD2, play_button_cb);

  // cdj_button_t *time = create_cdj_button(S2, KD0, time_button_cb);
  // cdj_button_t *trkf = create_cdj_button(S2, KD1, trkf_button_cb);
  cdj_button_t *cue = create_cdj_button(S2, KD2, cue_button_cb);

  // cdj_button_t *eject = create_cdj_button(S3, KD0, eject_button_cb);
  // cdj_button_t *jet = create_cdj_button(S3, KD1, jet_button_cb);
  // cdj_button_t *scnb = create_cdj_button(S3, KD2, scnb_button_cb);

  // cdj_button_t *mt = create_cdj_button(S4, KD0, mt_button_cb);
  // cdj_button_t *zip = create_cdj_button(S4, KD1, zip_button_cb);
  // cdj_button_t *scnf = create_cdj_button(S4, KD2, scnf_button_cb);

  // cdj_button_t *wah = create_cdj_button(S5, KD1, wah_button_cb);

  // Rotary search encoder: each detent pulses either the FWD or BWD pin.
  // Turning speed is derived from the time between pulses, see search_encoder_velocity().
  // Rotary encoders are a bit more complex than simple push buttons, so we use the queued version of the button handler.
  // button_t *search_fwd_button = create_button_queued(SEARCH_FWD_BUTTON_PIN, button_cb);
  // button_t *search_bwd_button = create_button(SEARCH_BWD_BUTTON_PIN, button_cb);

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
    tud_task();                       // tinyusb device task
    cdj_button_poll_events();         // handle debounced button callbacks in main context
    status_led_blinking_task(false);  // Blink the LED to show device status
    hid_task();                       // Send HID reports to the host
    
    // If the play button is pressed, send MIDI messages  
    if(playing) {
      status_led_blinking_task(true); // If playing, use the play LED for blinking.
    }

  }
}

#pragma region TinyUSB callbacks
//--------------------------------------------------------------------+
// initialization
//--------------------------------------------------------------------+

// Button initialization
int button_init(void) {
  #if defined(S1) && defined(S2) && defined(S3) && defined(S4) && defined(S5) && defined(KD0) && defined(KD1) && defined(KD2)
  #else
    return PICO_ERROR_NOT_SUPPORTED;
  #endif
    return PICO_OK;
}

int pt_init(void) {
  #if defined(SPI_PIN_0) && defined(CLK_PIN_6324) && defined(DIN_PIN_6324) && defined(STB_PIN_6324) && defined(DOUT_PIN_6324)
    // All required pins are defined, proceed with initialization
    pt6324_t *pt6324_dev = (pt6324_t *)malloc(sizeof(pt6324_t));
    if (!pt6324_dev) {
     printf("Failed to allocate memory for PT6324 device\n");
     return -1;
    }

    // Initialize the PT6324 device
    pt6324_init(pt6324_dev, STB_PIN_6324, CLK_PIN_6324, DIN_PIN_6324, DOUT_PIN_6324);
    return PICO_OK;
  #else 
    return PICO_ERROR_NOT_SUPPORTED; // Required pins are not defined
  #endif
}
#pragma endregion

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
#pragma endregion

#pragma region Button callbacks
//--------------------------------------------------------------------+
// Button callback
//--------------------------------------------------------------------+

void play_button_cb(cdj_button_t *b){
  if (!b) return;

  playing = !b->state;                      // Toggle the playing state based on the button state
  message_sent = false;                     // Reset message sent flag to allow sending MIDI messages
  
  const MidiNode *node = find_message(2,1); // Generate MIDI message for Play button
  msg[0] = node->channel;                   // Set MIDI channel
  msg[1] = node->value;                     // Set MIDI value
  msg[2] = playing ? 127 : 0;               // Set velocity based
  
  midi_task(msg);                           // Send MIDI messages to the host
}

void cue_button_cb(cdj_button_t *b){
  if (!b) return;

  const MidiNode *node = find_message(2,2); // Generate MIDI message for Cue button
  msg[0] = node->channel;                   // Set MIDI channel
  msg[1] = node->value;                     // Set MIDI value
  msg[2] = b->state ? 127 : 0;              // Set velocity based on button state
  
  midi_task(msg);                           // Send MIDI messages to the host
}


void cdj_button_cb(cdj_button_t *b) {
  if (!b) return;

  bool pressed = !b->state;

  //JogWheel: scale velocity by how fast the encoder is being turned.
  // Search wheel: scale velocity by how fast the encoder is being turned.
  // if (pressed && (b->pin == SEARCH_FWD_BUTTON_PIN || b->pin == SEARCH_BWD_BUTTON_PIN)) {
  //   velocity = search_encoder_velocity(b->pin);
  // }
  //TODO: Send MIDI messages to the host when a button is pressed or released
  // msg[0] = message->channel;
  // msg[1] = message->value;
  // msg[2] = velocity;
  
  
  message_sent = false;
  midi_task(msg);     // Send MIDI messages to the host
    //TODO: Update the HID report state when a button is pressed or released
    // // ------------ HID: Update the State-Report --------------------
    // for (size_t i = 0; i < CDJ_HID_BUTTON_MAP_COUNT; i++) {
    //     if (cdj_hid_button_map[i].gpio == b->pin) {
    //         uint8_t idx  = cdj_hid_button_map[i].byte_offset;
    //         uint8_t mask = cdj_hid_button_map[i].mask;
    //         if (pressed) {
    //             hid_in_report[idx] |=  mask;
    //         } else {s
    //             hid_in_report[idx] &= ~mask;
    //         }
    //         break;
    //     }
    // }
}
#pragma endregion

#pragma region HID callbacks

//--------------------------------------------------------------------+
// HID Task
//--------------------------------------------------------------------+
// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(
    uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen) {
  // TODO not Implemented
  (void)instance;
  (void)report_id;
  (void)report_type;
  (void)buffer;
  (void)reqlen;

  return 0;
}
void tud_hid_set_report_cb(
    uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize) {
  (void)instance;

   if (report_type == HID_REPORT_TYPE_OUTPUT && bufsize >= CDJ_OUT_REPORT_LEN) {
        // buffer[0] = 0x00, buffer[1] = 0x21 (header, ignore)
        // buffer[2] bit7 = Play LED, bit6 = Cue LED, etc.
        // TODO: drive your LEDs here based on buffer contents
        // Example: bool play_led_on = (buffer[2] & CDJ_LED_PLAY) != 0;
        //          pico_set_led(play_led_on, true);
        (void)buffer;
    }
}

// Invoked when sent REPORT successfully to host
// Application can use this to send the next report
// Note: For composite reports, report[0] is report ID
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const *report, uint16_t len) {
  return; // Not used in this example
}

// Sends the current hid_in_report[] state every 10ms.
// In MIDI mode the report is all zeros (no HID activity).
// In HID mode the report reflects real button states.
void hid_task(void) {
    static uint32_t last_ms = 0;
    uint32_t now = board_millis();
    if (now - last_ms < 10) return;
    last_ms = now;
    if (!tud_hid_ready()) return;
    tud_hid_report(0, hid_in_report, sizeof(hid_in_report));
}
#pragma endregion

#pragma region MIDI callbacks
//--------------------------------------------------------------------+
// MIDI Task
//--------------------------------------------------------------------+

void midi_task(uint8_t msg[3])
{
  uint32_t written = tud_midi_n_stream_write(0, 0, msg, 3);
  printf("MIDI write returned %lu bytes\n", (unsigned long)written);
}
#pragma endregion

#pragma region rotary encoder callbacks
//--------------------------------------------------------------------+
// Search encoder speed -> MIDI velocity
//--------------------------------------------------------------------+
// Shortest inter-pulse interval we bother distinguishing (fastest spin -> velocity 127)
#define SEARCH_ENCODER_MIN_INTERVAL_US 3000
// Longest inter-pulse interval we still report a signal for (slowest spin -> velocity 1)
#define SEARCH_ENCODER_MAX_INTERVAL_US 150000

// Returns a MIDI velocity (1-127) derived from the time since the last pulse
// on this encoder pin. Faster turning (shorter interval) yields a higher velocity.
// FWD and BWD are tracked independently by encoder_pulse_interval_us() since
// they are separate physical contacts.
uint8_t search_encoder_velocity(uint pin) {
  uint32_t interval = encoder_pulse_interval_us(pin, SEARCH_ENCODER_MIN_INTERVAL_US, SEARCH_ENCODER_MAX_INTERVAL_US);
  return (uint8_t)encoder_interval_to_speed(interval, SEARCH_ENCODER_MIN_INTERVAL_US, SEARCH_ENCODER_MAX_INTERVAL_US, 1, 127);
}
#pragma endregion

#pragma region LED callbacks
//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
// Turn the led on or off
void pico_set_led(bool led_on, bool play_led) {
  #if defined(PICO_DEFAULT_LED_PIN)
    // Just set the GPIO on or off
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
  #elif defined(CYW43_WL_GPIO_LED_PIN)
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
#pragma endregion