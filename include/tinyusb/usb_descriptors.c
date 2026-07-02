/*
 * usb_descriptors.c
 *
 * CDJ-850 USB Descriptor Replica
 * Extracted from Wireshark/USBPcap capture + verified with macOS System Information.
 * Configuration descriptor is 1:1 identical to the real CDJ-850 (198 bytes).
 *
 * Interface layout (5 interfaces, matching original):
 *   IF 0         Audio Control (UAC1) - USB→Speaker audio path, iIF="PIONEER CDJ-850"
 *   IF 1 Alt 0   Audio Streaming, 0 endpoints
 *   IF 1 Alt 1   Audio Streaming - EP 0x01 OUT Isoc 200B/1ms, PCM 44.1/48kHz stereo
 *   IF 2         Audio Control (UAC1) - dummy AC header anchoring the MIDI IF, iIF="PIONEER CDJ-850 MIDI"
 *   IF 3         MIDI Streaming - EP 0x83 IN Bulk 64B (send-only), iIF="USB MIDI Interface2"
 *   IF 4         HID (Vendor-defined) - EP 0x06 OUT Intr 36B/1ms + EP 0x87 IN Intr 20B/1ms, iIF="PIONEER CDJ-850 HID"
 *
 * Notes:
 *   - bDeviceClass = 0x00 (old-style composite, NOT 0xEF/IAD - this was the bug in the earlier version)
 *   - MIDI has only one bulk endpoint IN (0x83). The real CDJ-850 only sends MIDI to the host,
 *     it never receives MIDI. TinyUSB MIDI driver initialises with ep_in=0x83 / ep_out=0,
 *     so tud_midi_n_stream_write() works fine. Receiving MIDI from host is not supported.
 *   - Audio interfaces (0-2) are present in the descriptor for correct CDJ-850 recognition,
 *     but not implemented in TinyUSB (CFG_TUD_AUDIO=0). macOS will show an Audio device entry
 *     but DJ software (rekordbox, djay, Serato) does not activate the isochronous audio endpoint.
 *   - HID Report Descriptor is the original 52-byte vendor descriptor (Usage Page 0xFFA0/0xFFA1),
 *     not the simplified 8-byte placeholder. IN report = 20 bytes, OUT report = 36 bytes.
 *   - iSerialNumber = 0x00 (no serial string, matching CDJ-850 original)
 */

#include "bsp/board_api.h" 
#include "tusb.h"   

//--------------------------------------------------------------------+
// Device Descriptor
//--------------------------------------------------------------------+
// CRITICAL FIX: bDeviceClass must be 0x00, NOT 0xEF.
// The CDJ-850 uses pre-IAD composite (interface-level class declaration).
// Using 0xEF (IAD composite) caused macOS to reject the device because
// it expected IAD descriptors in the configuration that aren't present.
tusb_desc_device_t const device_desc = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,

    .bDeviceClass       = 0x00,   // Must be 0x00, NOT 0xEF - see note above
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = 0x08E4,  // Pioneer Corporation
    .idProduct          = 0x0159,  // CDJ-850
    .bcdDevice          = 0x0112,  // firmware version from original

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x00,    // CDJ-850 has no serial string (iSerialNumber=0 = none)
    .bNumConfigurations = 0x01
};

uint8_t const *tud_descriptor_device_cb(void) {
    return (uint8_t const *)&device_desc;
}

//--------------------------------------------------------------------+
// HID Report Descriptor (52 bytes)
// Verbatim from Wireshark capture (packets #100 + #202), confirmed by
// macOS System Information raw descriptor dump.
// Vendor-defined, Usage Page 0xFFA0 / 0xFFA1 (no standard usages).
// IN  report: 20 bytes (EP 0x87, Interrupt IN)
// OUT report: 36 bytes (EP 0x06, Interrupt OUT)
//--------------------------------------------------------------------+
uint8_t const desc_hid_report[] = {
    0x06, 0xA0, 0xFF,       // Usage Page (Vendor Defined 0xFFA0)
    0x09, 0x01,             // Usage (0x01)
    0xA1, 0x01,             // Collection (Application)
    0x09, 0x02,             //   Usage (0x02)
    0xA1, 0x00,             //   Collection (Physical)
    0x06, 0xA1, 0xFF,       //     Usage Page (Vendor Defined 0xFFA1)
    0x09, 0x03,             //     Usage (0x03)
    0x09, 0x04,             //     Usage (0x04)
    0x15, 0x80,             //     Logical Minimum (-128)
    0x25, 0x7F,             //     Logical Maximum (127)
    0x35, 0x00,             //     Physical Minimum (0)
    0x45, 0xFF,             //     Physical Maximum (255)
    0x75, 0x08,             //     Report Size (8)
    0x95, 0x14,             //     Report Count (20)   -> 20-byte IN report
    0x81, 0x02,             //     Input (Data, Var, Abs)
    0x09, 0x05,             //     Usage (0x05)
    0x09, 0x06,             //     Usage (0x06)
    0x15, 0x80,             //     Logical Minimum (-128)
    0x25, 0x7F,             //     Logical Maximum (127)
    0x35, 0x00,             //     Physical Minimum (0)
    0x45, 0xFF,             //     Physical Maximum (255)
    0x75, 0x08,             //     Report Size (8)
    0x95, 0x24,             //     Report Count (36)   -> 36-byte OUT report
    0x91, 0x02,             //     Output (Data, Var, Abs)
    0xC0,                   //   End Collection
    0xC0                    // End Collection
};

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
    (void)instance;
    return desc_hid_report;
}

//--------------------------------------------------------------------+
// Configuration Descriptor (198 bytes)
// Verbatim reconstruction - verified byte-for-byte against Wireshark
// capture. The HID interface block is emitted with TinyUSB constants so
// the host sees a structurally correct HID interface descriptor.
//--------------------------------------------------------------------+
#define CDJ_HID_INOUT_DESCRIPTOR(_itfnum, _stridx, _epout, _epin) \
  9, TUSB_DESC_INTERFACE, _itfnum, 0, 2, TUSB_CLASS_HID, 0, 0, _stridx, \
  9, HID_DESC_TYPE_HID, 0x10, 0x01, 0, 1, HID_DESC_TYPE_REPORT, 0x34, 0x00, \
  7, TUSB_DESC_ENDPOINT, _epout, TUSB_XFER_INTERRUPT, 0x24, 0x00, 0x01, \
  7, TUSB_DESC_ENDPOINT, _epin, TUSB_XFER_INTERRUPT, 0x14, 0x00, 0x01

static uint8_t const desc_configuration[] =
{
    // ----------------------------------------------------------------
    // Configuration Header
    // wTotalLength=198, bNumInterfaces=5, bConfigurationValue=1,
    // iConfiguration=0, bmAttributes=0xC0 (self-powered), bMaxPower=0mA
    // ----------------------------------------------------------------
    0x09, 0x02, 0xC6, 0x00, 0x05, 0x01, 0x00, 0xC0, 0x00,

    // ----------------------------------------------------------------
    // Interface 0: Audio Control
    // bNumEndpoints=0, class=Audio(1), sub=Control(1), proto=0, iIF=3
    // ----------------------------------------------------------------
    0x09, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x03,

    // CS: AC Header - bcdADC=1.00, wTotalLength=30, bInCollection=1, baIF[0]=1
    0x09, 0x24, 0x01, 0x00, 0x01, 0x1E, 0x00, 0x01, 0x01,

    // CS: Input Terminal - ID=1, type=USB_STREAMING(0x0101), 2ch (L+R, config=0x0003)
    0x0C, 0x24, 0x02, 0x01, 0x01, 0x01, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00,

    // CS: Output Terminal - ID=2, type=SPEAKER(0x0301), srcID=1
    0x09, 0x24, 0x03, 0x02, 0x01, 0x03, 0x00, 0x01, 0x00,

    // ----------------------------------------------------------------
    // Interface 1, Alt 0: Audio Streaming (idle, no endpoints)
    // ----------------------------------------------------------------
    0x09, 0x04, 0x01, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00,

    // ----------------------------------------------------------------
    // Interface 1, Alt 1: Audio Streaming (active - PCM audio to speakers)
    // bNumEndpoints=1
    // ----------------------------------------------------------------
    0x09, 0x04, 0x01, 0x01, 0x01, 0x01, 0x02, 0x00, 0x00,

    // CS: AS General - bTerminalLink=1, bDelay=0, wFormatTag=PCM(0x0001)
    0x07, 0x24, 0x01, 0x01, 0x00, 0x01, 0x00,

    // CS: Format Type I - 2ch, 16-bit, 2 discrete sample rates
    // tSamFreq[0]=44100Hz, tSamFreq[1]=48000Hz
    0x0E, 0x24, 0x02, 0x01, 0x02, 0x02, 0x10, 0x02,
    0x44, 0xAC, 0x00,   // 44100 Hz (little-endian 3-byte)
    0x80, 0xBB, 0x00,   // 48000 Hz (little-endian 3-byte)

    // EP 0x01 OUT - Isochronous, wMaxPacketSize=200, bInterval=1ms
    0x09, 0x05, 0x01, 0x01, 0xC8, 0x00, 0x01, 0x00, 0x00,

    // CS: AS Endpoint - sample-freq control, no lock delay
    0x07, 0x25, 0x01, 0x01, 0x00, 0x00, 0x00,

    // ----------------------------------------------------------------
    // Interface 2: Audio Control (MIDI anchor)
    // Required by USB Audio Class 1.0: every MIDIStreaming interface
    // must be "owned" by an AudioControl interface. bNumEndpoints=0,
    // iIF=4 ("PIONEER CDJ-850 MIDI")
    // ----------------------------------------------------------------
    0x09, 0x04, 0x02, 0x00, 0x00, 0x01, 0x01, 0x00, 0x04,

    // CS: AC Header - bcdADC=1.00, wTotalLength=9, bInCollection=1, baIF[0]=3
    0x09, 0x24, 0x01, 0x00, 0x01, 0x09, 0x00, 0x01, 0x03,

    // ----------------------------------------------------------------
    // Interface 3: MIDI Streaming
    // bNumEndpoints=1 (IN only - CDJ-850 only sends MIDI to host)
    // iIF=5 ("USB MIDI Interface2")
    // ----------------------------------------------------------------
    0x09, 0x04, 0x03, 0x00, 0x01, 0x01, 0x03, 0x00, 0x05,

    // CS: MS Header - bcdMSC=1.00, wTotalLength=36
    // (non-standard: CDJ-850 includes the std EP descriptor in this count)
    0x07, 0x24, 0x01, 0x00, 0x01, 0x24, 0x00,

    // CS: MIDI OUT Jack EMBEDDED - ID=1, 1 pin, src=ExternalJack(ID=2)/pin1
    // This is the jack whose output goes into EP 0x83 toward the host
    0x09, 0x24, 0x03, 0x01, 0x01, 0x01, 0x02, 0x01, 0x00,

    // CS: MIDI IN Jack EXTERNAL - ID=2
    // Logical "external" MIDI source that feeds into Embedded Jack 1
    0x06, 0x24, 0x02, 0x02, 0x02, 0x00,

    // EP 0x83 IN - Bulk, wMaxPacketSize=64, bInterval=0 (Bulk has no interval)
    0x09, 0x05, 0x83, 0x02, 0x40, 0x00, 0x00, 0x00, 0x00,

    // CS: MIDI Endpoint - bNumEmbMIDIJack=1, assoc=JackID 1
    // Note: 5 bytes (non-standard, should be 7) - kept to match original
    0x05, 0x25, 0x01, 0x01, 0x01,

    // ----------------------------------------------------------------
    // Interface 4: HID (Vendor-defined)
    // bNumEndpoints=2, class=HID(3), sub=0, proto=0, iIF=6
    // ----------------------------------------------------------------
    CDJ_HID_INOUT_DESCRIPTOR(4, 6, 0x06, 0x87),
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return desc_configuration;
}

//--------------------------------------------------------------------+
// String Descriptors
// Matched to CDJ-850 original (from Wireshark + macOS System Info)
//--------------------------------------------------------------------+
static char const *string_desc_arr[] = {
    (const char[]){ 0x09, 0x04 },   // 0: Language ID = English (US) 0x0409
    "PIONEER Corporation.",          // 1: iManufacturer
    "PIONEER CDJ-850",               // 2: iProduct
    "PIONEER CDJ-850",               // 3: iInterface - IF 0 (Audio Control)
    "PIONEER CDJ-850 MIDI",          // 4: iInterface - IF 2 (Audio Control/MIDI anchor)
    "USB MIDI Interface2",           // 5: iInterface - IF 3 (MIDI Streaming)
    "PIONEER CDJ-850 HID",           // 6: iInterface - IF 4 (HID)
};

static uint16_t _desc_str[32 + 1];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    size_t chr_count;

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) {
            return NULL;
        }
        const char *str = string_desc_arr[index];
        chr_count = strlen(str);
        const size_t max_count = sizeof(_desc_str) / sizeof(_desc_str[0]) - 1;
        if (chr_count > max_count) chr_count = max_count;
        for (size_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = str[i];
        }
    }

    _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
    return _desc_str;
}