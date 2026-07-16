/**
 * @file encoder.h
 * @brief Pulse-rate tracking for rotary encoders / step inputs on the Raspberry Pi Pico
 *
 * Hardware-generic timing helpers: given a GPIO pin that pulses once per
 * detent (e.g. via the button.h debounce/interrupt system), track how much
 * time elapses between pulses and map that to a linear speed value. Not tied
 * to MIDI or any other application-specific encoding.
 */

#ifndef PICO_ENCODER_H
#define PICO_ENCODER_H

#include "pico/stdlib.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Records a pulse on the given pin and returns the time since its
 * previous pulse, clamped to [min_interval_us, max_interval_us].
 *
 * Each pin maintains its own independent history, so e.g. the FWD and BWD
 * pins of a bidirectional encoder can be tracked without one direction's
 * pulses resetting the other's timing. The first pulse seen for a pin
 * returns max_interval_us (i.e. "as slow as possible").
 *
 * @param pin GPIO pin that just pulsed
 * @param min_interval_us Shortest interval to report (fastest pulses)
 * @param max_interval_us Longest interval to report (slowest pulses)
 * @return Interval in microseconds since the last pulse on this pin
 */
uint32_t encoder_pulse_interval_us(uint pin, uint32_t min_interval_us, uint32_t max_interval_us);

/**
 * @brief Linearly maps a pulse interval to a speed value: a shorter interval
 * (faster pulses) yields a value closer to max_value.
 *
 * @param interval_us Interval to map, clamped to [min_interval_us, max_interval_us]
 * @param min_interval_us Interval that maps to max_value (fastest)
 * @param max_interval_us Interval that maps to min_value (slowest)
 * @param min_value Output value for the slowest interval
 * @param max_value Output value for the fastest interval
 * @return Speed value in [min_value, max_value]
 */
uint32_t encoder_interval_to_speed(uint32_t interval_us, uint32_t min_interval_us, uint32_t max_interval_us,
                                    uint32_t min_value, uint32_t max_value);

#ifdef __cplusplus
}
#endif

#endif
