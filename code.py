import board
import digitalio
import usb_midi
import adafruit_midi
import time
from adafruit_midi.note_on import NoteOn
from adafruit_midi.note_off import NoteOff
from midi_mapping import MIDI_MAPPING

# Hardware Setup
led = digitalio.DigitalInOut(board.LED)
# led = digitalio.DigitalInOut(board.GP15)
# led.direction = digitalio.Direction.OUTPUT
# button = digitalio.DigitalInOut(board.GP14)
# button.pull = digitalio.Pull.UP

# MIDI Setup
midi = adafruit_midi.MIDI(midi_in=usb_midi.ports[0], in_channel=0, 
                          midi_out=usb_midi.ports[1], out_channel=0)

playing = False

while True:
    msg = midi.send(NoteOn(60, 127))

    # Empfange Feedback von djay
    msg = midi.receive()
    if isinstance(msg, NoteOn) and msg.note == 60:
        playing = msg.velocity > 0
    elif isinstance(msg, NoteOff) and msg.note == 60:
        playing = False
    else:
        playing = False


    # 3. LED blinken lassen, wenn djay "Play" meldet
    if playing:
        led.value = not led.value # Wechselt den Status (Blinken)
        time.sleep(0.1)
    else:
        led.value = False
