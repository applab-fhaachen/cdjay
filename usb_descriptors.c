/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "bsp/board_api.h"
#include "tusb.h"

/* A combination of interfaces must have a unique product id, since PC will save device driver after the first plug.
 * Same VID/PID with different interface e.g MSC (first), then CDC (later) will possibly cause system error on PC.
 *
 * Auto ProductID layout's Bitmap:
 *   [MSB]         HID | MSC | CDC          [LSB]
 */
#define PID_MAP(itf, n)  ((CFG_TUD_##itf) ? (1 << (n)) : 0)
#define USB_PID           (0x4000 | PID_MAP(CDC, 0) | PID_MAP(MSC, 1) | PID_MAP(HID, 2) | \
                           PID_MAP(MIDI, 3) | PID_MAP(VENDOR, 4) )
//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+

tusb_desc_device_t const device_desc = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,

    .bDeviceClass = 0xEF,
    .bDeviceSubClass = 0x02,
    .bDeviceProtocol = 0x01,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor = 0x08E4, // PIONEER Corporation.
    .idProduct = 0x0157,
    .bcdDevice = 0x0112,

    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01
};

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const * tud_descriptor_device_cb(void) {
  return (uint8_t const *) &device_desc;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+
enum {
  ITF_NUM_MIDI = 0,
  ITF_NUM_MIDI_STREAMING,
  ITF_NUM_HID,
  ITF_NUM_TOTAL
};

#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_MIDI_DESC_LEN + TUD_HID_INOUT_DESC_LEN)

#if CFG_TUSB_MCU == OPT_MCU_LPC175X_6X || CFG_TUSB_MCU == OPT_MCU_LPC177X_8X || CFG_TUSB_MCU == OPT_MCU_LPC40XX
  // LPC 17xx and 40xx endpoint type (bulk/interrupt/iso) are fixed by its number
  // 0 control, 1 In, 2 Bulk, 3 Iso, 4 In etc ...
  #define EPNUM_MIDI_OUT  0x02
  #define EPNUM_MIDI_IN   0x82

#elif CFG_TUSB_MCU == OPT_MCU_CXD56
  // CXD56 USB driver has fixed endpoint type (bulk/interrupt/iso) and direction (IN/OUT) by its number
  // 0 control (IN/OUT), 1 Bulk (IN), 2 Bulk (OUT), 3 In (IN), 4 Bulk (IN), 5 Bulk (OUT), 6 In (IN)
  #define EPNUM_MIDI_OUT  0x02
  #define EPNUM_MIDI_IN   0x81

#elif CFG_TUD_ENDPOINT_ONE_DIRECTION_ONLY
  // MCUs that don't support a same endpoint number with different direction IN and OUT defined in tusb_mcu.h
  //    e.g EP1 OUT & EP1 IN cannot exist together
  #define EPNUM_MIDI_OUT  0x01
  #define EPNUM_MIDI_IN   0x82

#else
  #define EPNUM_MIDI_OUT  0x01
  #define EPNUM_MIDI_IN   0x81
#endif

// HID endpoints (choose values that don't collide with MIDI)
#define EPNUM_HID_OUT 0x03
#define EPNUM_HID_IN  0x83

// HID Report Descriptor (placed before configuration descriptors)
// Vendor-defined HID: 8 bytes IN, 8 bytes OUT
static uint8_t const hid_report_descriptor[] = {
  0x06, 0x00, 0xFF, // Usage Page (Vendor Defined 0xFF00)
  0x09, 0x01,       // Usage (0x01)
  0xA1, 0x01,       // Collection (Application)
  0x15, 0x00,       // Logical Minimum (0)
  0x26, 0xFF, 0x00, // Logical Maximum (255)
  0x75, 0x08,       // Report Size (8)
  0x95, 0x08,       // Report Count (8)
  0x09, 0x01,       // Usage
  0x81, 0x02,       // Input (Data,Var,Abs)
  0x09, 0x01,       // Usage
  0x91, 0x02,       // Output (Data,Var,Abs)
  0xC0              // End Collection
};

// Provide HID report descriptor to TinyUSB
uint8_t const* tud_hid_descriptor_report_cb(uint8_t instance)
{
  (void) instance;
  return hid_report_descriptor;
}

//--------------------------------------------------------------------+
// HID Callbacks (stubs)
// Provide minimal implementations for control transfers (GET/SET REPORT)
//--------------------------------------------------------------------+
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
  (void) instance; (void) report_id; (void) report_type; (void) buffer; (void) reqlen;
  // No data to send (return 0 length)
  return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
  (void) instance; (void) report_id; (void) report_type; (void) buffer; (void) bufsize;
  // No-op: application can handle received OUT reports here
}

static uint8_t const desc_fs_configuration[] = {
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

  // MIDI Interface
  TUD_MIDI_DESCRIPTOR(ITF_NUM_MIDI, 0, EPNUM_MIDI_OUT, (0x80 | EPNUM_MIDI_IN), 64),

  // HID Interface (Vendor-defined, 8-byte reports in/out)
  TUD_HID_INOUT_DESCRIPTOR(ITF_NUM_HID, 0, 0, sizeof(hid_report_descriptor), EPNUM_HID_OUT, EPNUM_HID_IN, 64, 10)
};

#if TUD_OPT_HIGH_SPEED
static uint8_t const desc_hs_configuration[] = {
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

  // MIDI Interface
  TUD_MIDI_DESCRIPTOR(ITF_NUM_MIDI, 0, EPNUM_MIDI_OUT, (0x80 | EPNUM_MIDI_IN), 512),

  // HID Interface (HS)
  TUD_HID_INOUT_DESCRIPTOR(ITF_NUM_HID, 0, 0, sizeof(hid_report_descriptor), EPNUM_HID_OUT, EPNUM_HID_IN, 64, 10)
};
#endif

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const * tud_descriptor_configuration_cb(uint8_t index) {
  (void) index; // for multiple configurations

#if TUD_OPT_HIGH_SPEED
  // Although we are highspeed, host may be fullspeed.
  return (tud_speed_get() == TUSB_SPEED_HIGH) ?  desc_hs_configuration : desc_fs_configuration;
#else
  return desc_fs_configuration;
#endif
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

// String Descriptor Index
enum {
  STRID_LANGID = 0,
  STRID_MANUFACTURER,
  STRID_PRODUCT,
  STRID_SERIAL,
};

// array of pointer to string descriptors
static char const *string_desc_arr[] = {
  (const char[]) { 0x09, 0x04 },  // 0: is supported language is English (0x0409)
  "PIONEER Corporation.",         // 1: Manufacturer
  "PIONEER CDJ-850",              // 2: Product
  NULL,                           // 3: Serials will use unique ID if possible
};

static uint16_t _desc_str[32 + 1];

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void) langid;
  size_t chr_count;

  switch ( index ) {
    case STRID_LANGID:
      memcpy(&_desc_str[1], string_desc_arr[0], 2);
      chr_count = 1;
      break;

    case STRID_SERIAL:
      chr_count = board_usb_get_serial(_desc_str + 1, 32);
      break;

    default:
      // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
      // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

      if (!(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))) {
        return NULL;
      }

      const char *str = string_desc_arr[index];

      // Cap at max char
      chr_count = strlen(str);
      const size_t max_count = sizeof(_desc_str) / sizeof(_desc_str[0]) - 1; // -1 for string type
      if ( chr_count > max_count ) {
        chr_count = max_count;
      }

      // Convert ASCII string into UTF-16
      for ( size_t i = 0; i < chr_count; i++ ) {
        _desc_str[1 + i] = str[i];
      }
      break;
  }

  // first byte is length (including header), second byte is string type
  _desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));

  return _desc_str;
}