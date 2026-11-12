import time
import board
import digitalio
import analogio
import usb_midi
import adafruit_midi
from adafruit_midi.note_on import NoteOn
from adafruit_midi.note_off import NoteOff
from adafruit_midi.control_change import ControlChange

# ==============================================================================
# 1. MIDI USB SETUP
# ==============================================================================
# usb_midi.ports[1] is the USB-MIDI output endpoint on the Pico.
# out_channel=0 corresponds to standard MIDI Channel 1 (Channels are 0-indexed in code: 0 to 15).
midi = adafruit_midi.MIDI(midi_out=usb_midi.ports[1], out_channel=0)

# ==============================================================================
# 2. PIN CONFIGURATION (Pico 2W GPIOs)
# ==============================================================================

# Onboard LED (GP25 on Pico, or board.LED on Pico 2W)
led = digitalio.DigitalInOut(board.LED)
led.direction = digitalio.Direction.OUTPUT

# Play / Pause Button on GP15 (Internal pull-up resistor enabled)
btn_play = digitalio.DigitalInOut(board.GP15)
btn_play.direction = digitalio.Direction.INPUT
btn_play.pull = digitalio.Pull.UP

# CUE Button on GP14 (Internal pull-up resistor enabled)
btn_cue = digitalio.DigitalInOut(board.GP14)
btn_cue.direction = digitalio.Direction.INPUT
btn_cue.pull = digitalio.Pull.UP

# Tempo / Pitch Slider on GP26 (Analog-to-Digital Converter channel 0 / ADC0)
pot_pitch = analogio.AnalogIn(board.GP26)

# ==============================================================================
# 3. STATE TRACKING & DEBOUNCE VARIABLES
# ==============================================================================
# With Pull.UP, the pin reads True (HIGH) when NOT pressed, and False (LOW) when PRESSED.
last_play_state = True
last_cue_state = True

# Track the last sent MIDI slider value (0 to 127) to avoid flooding the USB bus
last_pitch_midi_val = -1

# Pioneer CDJ-850 Standard MIDI Note Mapping
NOTE_PLAY_PAUSE = 60  # Middle C (Standard Play/Pause trigger)
NOTE_CUE = 61         # C# (Standard CUE trigger)
CC_PITCH_SLIDER = 16  # General Purpose Continuous Controller 16

print("============================================")
print("CDJ-850 USB-MIDI Bridge Initialized on Pico!")
print("Waiting for physical inputs...")
print("============================================")

# ==============================================================================
# 4. MAIN LOOP
# ==============================================================================
while True:
    # --------------------------------------------------------------------------
    # A. Check PLAY / PAUSE Button (GP15)
    # --------------------------------------------------------------------------
    current_play = btn_play.value
    # Transition: HIGH -> LOW (Button was just pressed down)
    if not current_play and last_play_state:
        led.value = True  # Blink onboard LED
        # Send NoteOn: Note 60, Velocity 127 (Full strike)
        # Raw bytes: 0x90 0x3C 0x7F
        midi.send(NoteOn(NOTE_PLAY_PAUSE, 127))
        print("-> [MIDI TX] PLAY/PAUSE PRESSED (NoteOn 60, Vel 127)")

    # Transition: LOW -> HIGH (Button was released)
    elif current_play and not last_play_state:
        led.value = False
        # Send NoteOff: Note 60, Velocity 0
        # Raw bytes: 0x80 0x3C 0x00
        midi.send(NoteOff(NOTE_PLAY_PAUSE, 0))
        print("-> [MIDI TX] PLAY/PAUSE RELEASED (NoteOff 60, Vel 0)")

    last_play_state = current_play

    # --------------------------------------------------------------------------
    # B. Check CUE Button (GP14)
    # --------------------------------------------------------------------------
    current_cue = btn_cue.value
    if not current_cue and last_cue_state:
        led.value = True
        # Send NoteOn: Note 61, Velocity 127
        # Raw bytes: 0x90 0x3D 0x7F
        midi.send(NoteOn(NOTE_CUE, 127))
        print("-> [MIDI TX] CUE PRESSED (NoteOn 61, Vel 127)")

    elif current_cue and not last_cue_state:
        led.value = False
        # Send NoteOff: Note 61, Velocity 0
        # Raw bytes: 0x80 0x3D 0x00
        midi.send(NoteOff(NOTE_CUE, 0))
        print("-> [MIDI TX] CUE RELEASED (NoteOff 61, Vel 0)")

    last_cue_state = current_cue

    # --------------------------------------------------------------------------
    # C. Check Pitch / Tempo Slider (GP26 / ADC0)
    # --------------------------------------------------------------------------
    # analogio on RP2040 reads a 16-bit integer (0 to 65535).
    # MIDI Control Change requires a 7-bit integer (0 to 127).
    # Formula: (adc_value * 127) // 65535
    raw_adc = pot_pitch.value
    current_pitch_midi = (raw_adc * 127) // 65535

    # Only send a message if the slider actually moved by at least 1 MIDI step
    if current_pitch_midi != last_pitch_midi_val:
        # Raw bytes: 0xB0 0x10 <current_pitch_midi>
        midi.send(ControlChange(CC_PITCH_SLIDER, current_pitch_midi))
        print(f"-> [MIDI TX] Pitch Fader Moved: CC {CC_PITCH_SLIDER} = {current_pitch_midi}")
        last_pitch_midi_val = current_pitch_midi

    # Small delay (10ms) for switch contact debouncing
    time.sleep(0.01)
