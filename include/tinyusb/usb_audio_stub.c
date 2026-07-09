/*
 * usbd_audio_stub.c
 *
 * WHY THIS FILE EXISTS:
 * The CDJ-850 configuration descriptor contains 3 Audio interfaces (IF 0, 1, 2)
 * that are required for correct device recognition by DJ software, but which the
 * RP2040 does not actually implement (CFG_TUD_AUDIO=0).
 *
 * When TinyUSB processes SET_CONFIGURATION it walks the descriptor byte-by-byte
 * and calls driver->open() for each interface. If no driver claims an Audio
 * interface (class=0x01), open() returns 0 bytes consumed. TinyUSB then loses
 * track of where the next interface starts, and everything after (MIDI IF3,
 * HID IF4) is parsed at the wrong offset. This causes the "interface does not
 * currently exist" error visible in USB Prober.
 *
 * This stub driver claims all Audio interfaces by correctly measuring and
 * returning the total byte size of each interface block (including all
 * class-specific descriptors and alternate settings). It opens NO actual
 * endpoints, so there is no audio functionality - just parser alignment.
 */

#include <stdio.h>

#include "tusb.h"
#include "device/usbd_pvt.h"

static void     audio_stub_init(void)           { }
static bool     audio_stub_deinit(void)         { return true; }
static void     audio_stub_reset(uint8_t rhport) { (void)rhport; }

// Called once per interface descriptor encountered.
// Must return total bytes consumed (interface + all CS descriptors + endpoints),
// or 0 to reject (hand off to next driver).
static uint16_t audio_stub_open(uint8_t rhport,
                                tusb_desc_interface_t const *itf_desc,
                                uint16_t max_len)
{
    (void)rhport;

    printf("AUDIO_STUB open: itf=%u class=0x%02x sub=0x%02x proto=0x%02x max_len=%u\n",
           itf_desc->bInterfaceNumber,
           itf_desc->bInterfaceClass,
           itf_desc->bInterfaceSubClass,
           itf_desc->bInterfaceProtocol,
           max_len);

    // Only claim Audio Control / Audio Streaming interfaces.
    // Leave MIDI Streaming (subclass 0x03) for TinyUSB's MIDI driver.
    if (itf_desc->bInterfaceClass != TUSB_CLASS_AUDIO ||
        itf_desc->bInterfaceSubClass == AUDIO_SUBCLASS_MIDI_STREAMING)
        return 0;

    uint8_t  const *p      = (uint8_t const *)itf_desc;
    uint16_t        total  = 0;
    uint8_t  const  this_n = itf_desc->bInterfaceNumber;

    while (total < max_len)
    {
        uint8_t len = p[0];
        if (len == 0) break;

        printf("AUDIO_STUB desc: type=0x%02x len=%u itf=%u total=%u\n",
               p[1],
               len,
               (p[1] == TUSB_DESC_INTERFACE) ? ((tusb_desc_interface_t const *)p)->bInterfaceNumber : 0xFF,
               total);

        // Stop when we reach a DIFFERENT interface number - that one belongs
        // to the next driver (MIDI or HID). Same interface number = another
        // alternate setting of the same Audio interface, consume it too.
        if (p[1] == TUSB_DESC_INTERFACE && total > 0)
        {
            if (((tusb_desc_interface_t const *)p)->bInterfaceNumber != this_n)
                break;
        }

        total += len;
        p     += len;
    }

    // If this Audio Control interface is just the anchor for a MIDI
    // Streaming interface (no real endpoints of its own), leave it alone.
    // TinyUSB's own midid_open() expects to be called with the AC-Control
    // interface first and walks forward to consume the MIDI Streaming
    // interface itself - if we swallow the AC interface here, midid_open()
    // never gets a valid starting point and MIDI never opens.
    if (total < max_len && p[0] != 0 && p[1] == TUSB_DESC_INTERFACE)
    {
        tusb_desc_interface_t const *next_itf = (tusb_desc_interface_t const *)p;
        if (next_itf->bInterfaceClass == TUSB_CLASS_AUDIO &&
            next_itf->bInterfaceSubClass == AUDIO_SUBCLASS_MIDI_STREAMING)
        {
            printf("AUDIO_STUB open: itf=%u is MIDI anchor, leaving for midid_open\n",
                   itf_desc->bInterfaceNumber);
            return 0;
        }
    }

    printf("AUDIO_STUB open done: itf=%u consumed=%u\n",
           itf_desc->bInterfaceNumber,
           total);

    return total;
}

// Handle class-specific control requests to Audio interfaces (e.g. SET_INTERFACE).
// We accept all of them silently so the host does not see a STALL.
static bool audio_stub_control_xfer_cb(uint8_t rhport, uint8_t stage,
                                       tusb_control_request_t const *req)
{
    (void)req;
    if (stage != CONTROL_STAGE_SETUP) return true;
    return tud_control_status(rhport, req);
}

// Never called - we open no endpoints. Required field in the struct.
static bool audio_stub_xfer_cb(uint8_t rhport, uint8_t ep_addr,
                                xfer_result_t result, uint32_t xferred_bytes)
{
    (void)rhport; (void)ep_addr; (void)result; (void)xferred_bytes;
    return true;
}

static usbd_class_driver_t const _audio_stub = {
#if CFG_TUSB_DEBUG >= 2
    .name            = "AUDIO_STUB",
#endif
    .init            = audio_stub_init,
    .deinit          = audio_stub_deinit,
    .reset           = audio_stub_reset,
    .open            = audio_stub_open,
    .control_xfer_cb = audio_stub_control_xfer_cb,
    .xfer_cb         = audio_stub_xfer_cb,
    .sof             = NULL,
};

// TinyUSB calls this weak function to discover additional class drivers.
// We return our single stub so TinyUSB includes it in its driver table.
usbd_class_driver_t const *usbd_app_driver_get_cb(uint8_t *driver_count)
{
    *driver_count = 1;
    return &_audio_stub;
}