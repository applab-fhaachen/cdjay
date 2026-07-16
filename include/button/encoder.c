/**
 * @file encoder.c
 * @brief Pulse-rate tracking for rotary encoders / step inputs on the Raspberry Pi Pico
 */

#include "pico/stdlib.h"
#include "encoder.h"

// RP2040/RP2350 expose GPIO 0-29
#define ENCODER_MAX_PINS 30

static uint32_t last_pulse_us[ENCODER_MAX_PINS] = {0};

uint32_t encoder_pulse_interval_us(uint pin, uint32_t min_interval_us, uint32_t max_interval_us) {
  if (pin >= ENCODER_MAX_PINS) return max_interval_us;

  uint32_t now = time_us_32();
  uint32_t interval = (last_pulse_us[pin] == 0) ? max_interval_us : (now - last_pulse_us[pin]);
  last_pulse_us[pin] = now;

  if (interval < min_interval_us) interval = min_interval_us;
  if (interval > max_interval_us) interval = max_interval_us;
  return interval;
}

uint32_t encoder_interval_to_speed(uint32_t interval_us, uint32_t min_interval_us, uint32_t max_interval_us,
                                    uint32_t min_value, uint32_t max_value) {
  if (interval_us < min_interval_us) interval_us = min_interval_us;
  if (interval_us > max_interval_us) interval_us = max_interval_us;

  uint32_t interval_range = max_interval_us - min_interval_us;
  if (interval_range == 0) return max_value;

  uint32_t from_min = interval_us - min_interval_us;
  uint32_t value_range = max_value - min_value;

  // Shorter interval (faster pulses) -> value closer to max_value
  return max_value - (from_min * value_range) / interval_range;
}
