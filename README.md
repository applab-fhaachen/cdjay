# CDjay

## Introduction
The success of Pioneer's CDJ-Players is based on their usability and performance-oriented controls. And while the technology has evolved from CDs over SD-Cards to USB-Keys or direct interaction with a laptop, the control interface itself hasn't changed much. 
Modern CDJs, such as the CDJ-850, CDJ-900, CDJ-2000, and newer offer the possibility to directly connect to a host software, such as Algoriddim DJay, Native Instruments' Traktor, Serato Scratch, in order to be used as controller. 
The goal of this project is to bring this capability to older CDJ-Models in order to keep their control interface. 

## Related Work
A number of other projects have looked at the same idea. Some turning the CDJ into a MIDI-based controller, others integrating a RaspberryPi and running Mixxx, and open source DJ-Software on it, essentially turning them into full-blown, standalone, media-players. 

## About this repository
This repo holds the firmware that turns a Pioneer CDJ-850 into a class-compliant USB controller. It runs on a Raspberry Pi Pico 2 W wired into the CDJ's front-panel button matrix, and it currently:
- Scans the CDJ's S1-S5 / KD0-KD2 button matrix (the display controller drives the S lines; the Pico reads back which K lines go high on each line, see `notes.md` and `include/button/cdj_button.h`) and debounces/queues press-release edges.
- Translates button edges into standard USB-MIDI note on/off messages (`headers/midi_messages.h`) sent over TinyUSB, so any MIDI-capable host (e.g. djay on an iPad) sees the CDJ as a controller.
- Includes a USB-HID descriptor scaffold matching the CDJ-850's own vendor-defined report layout (see `pioneer-800 description.md`) for a future, richer bidirectional mode (LED feedback from the host), though report population is not yet wired up.
- Tracks rotary/jog encoder pulse timing (`include/button/encoder.c`) to derive a speed-scaled MIDI velocity for the search wheel.

Only the Play and Cue buttons are connected end-to-end today (`cdjay.c`); the remaining matrix positions are mapped in `midi_messages.h` but not yet instantiated.

### Hardware
- Raspberry Pi Pico 2 W (`PICO_BOARD pico2_w`), wired to the CDJ-850's S1-S5 (GPIO 10-14) and KD0-KD2 (GPIO 7-9) matrix lines.
- Wiring reference: `image.png` / `notes.md`.

### Building
Requires the Raspberry Pi Pico SDK (v2.2.0) and toolchain, e.g. via the [Pico VS Code extension](https://marketplace.visualstudio.com/items?itemName=raspberry-pi.raspberry-pi-pico) or a manual SDK install pointed to by `PICO_SDK_PATH`.

```sh
mkdir -p build && cd build
cmake ..
make -j4
```

Flash `build/cdjay.uf2` to the Pico by holding BOOTSEL while plugging it in, then copying the file to the mass-storage drive that appears.

### Status
Done:
- Button matrix scanning, debouncing, and edge-queueing (S1-S5 x KD0-KD2).
- USB-MIDI transport via TinyUSB, with a full note-message lookup table for every matrix cell.
- Play and Cue buttons wired end-to-end: press/release -> MIDI note on/off.
- Rotary/jog encoder pulse-interval tracking, mapped to a MIDI-velocity range.
- USB-HID report descriptor matching the real CDJ-850 vendor report layout (not yet populated with live data).

In progress / not yet wired up:
- Remaining buttons (Hold, Time, Track Back/Forward, Eject, Jet, Scratch Back/Forward, Menu/Tag, Zip, Wah) are mapped in `midi_messages.h` and commented out in `cdjay.c`, but not instantiated as `cdj_button_t`s.
- Search/jog encoder velocity is computed (`search_encoder_velocity()`) but not yet fed into a MIDI message.
- `hid_in_report` is never updated from button state, so the HID path currently only sends an all-zero report.
- `tud_hid_set_report_cb` has a TODO for driving the CDJ's own LEDs from host feedback (e.g. Play/Cue LED state from djay).

Roadmap:
1. Instantiate the remaining `create_cdj_button()` calls and confirm each maps to the correct MIDI note against a real CDJ-850.
2. Wire jog/search-wheel velocity into MIDI CC or note messages.
3. Populate `hid_in_report` from button state and enable the HID path as an alternative/parallel transport to MIDI.
4. Implement LED feedback from host HID output reports so the CDJ's own display/LEDs reflect deck state (playing, cued, etc.).
5. Validate end-to-end with djay on iPad (and ideally a second host, e.g. a MIDI-only DAW) for both transports.

## USB-HID
The key is the USB-HID protocol which the newer models provide. Instead of registering as a USB-Midi Device, they have their own, proprietary protocol. While this conflicts with the idea of an open and flexible interface, it has the advantage of providing bi-directional communication, and thus, be able to show additional information directly on the display of the CDJ. The most common DJ software supports the HID protocol.

### Details
Technically, the CDJ registers as a USB device with a number of endpoints:
- The USB-HID interface for the control messages.
- a USB-audio interface for audio-output from the CDJ itself (relevant if you don't use the internal mixer of the software, but an external mixer). 


## Interface
Since we want to focus on interfacing the old CDJs with the modern world (USB-C, Software, etc.), we use iPad mini running Algoriddim's djay in single deck mode, which gives us all the benefits such as touch interaction, integration to streaming services, etc.. Any iPad being able to run djay is fine, although interfacing is easier with the usb-c models. 

### Installation
Install djay from the App-Store on your iPad. Technically, it is possible to run djay from an iPhone, however, it doesn't support single-deck mode on smaller devices. 
	
	
### Connecting
If you have an iPad with a lightning-connector, you need a lightning to usb-converter that supports USB OTG. This will make the iPad the host of your USB connection, and the CDJay the client. 
For the USB-Models, just connect the USB-C connection to your iPad. 


## Acknowledgements

This project was supported by...

## License
MIT License

All trademarks are property of their respective owners. This project is part of the education at FH Aachen University of Applied Sciences and not affiliated to any of the brand names. 
