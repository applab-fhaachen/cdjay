#include "cdjay.h"
#include "cdjay_hid.h"

// USB HID Object
Adafruit_USBD_HID usb_hid;

// HID report descriptor using TinyUSB's template
// Generic In Out with 64 bytes report (max)
// uint8_t const desc_hid_report[] = {
//     TUD_HID_REPORT_DESC_GENERIC_INOUT(64)
// };
uint8_t const desc_hid_report[] = {0x06, 0xA0, 0xFF, 0x09, 0x01, 0xA1, 0x01, 0x09, 0x02, 0xA1, 0x00, 0x06, 0xA1, 0xFF, 0x09, 0x03  
, 0x09, 0x04, 0x15, 0x80, 0x25, 0x7F, 0x35, 0x00, 0x45, 0xFF, 0x75, 0x08, 0x95, 0x14, 0x81, 0x02,
0x09, 0x05, 0x09, 0x06, 0x15, 0x80, 0x25, 0x7F, 0x35, 0x00, 0x45, 0xFF, 0x75, 0x08, 0x95, 0x24, 0x91, 0x02, 0xC0, 0xC0 };

// Function Headers
uint16_t get_report_callback(uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen);
void set_report_callback(uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize);

bool activeState = false;


void hidSetup() {
  usb_hid.enableOutEndpoint(true);
  usb_hid.setPollInterval(1);
  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  usb_hid.setStringDescriptor(HIDDescription);
  usb_hid.setReportCallback(get_report_callback, set_report_callback);
  usb_hid.begin();
}


void hidLoop() {
    #ifdef TINYUSB_NEED_POLLING_TASK
    // Manual call tud_task since it isn't called by Core's background
    TinyUSBDevice.task();
    #endif

    bool btn_pressed = (digitalRead(BOOTSEL) == activeState);

    if (usb_hid.ready()) {
        if (btn_pressed) {
        // send volume down (0x00EA)
        // usb_hid.sendReport16(RID_CONSUMER_CONTROL, HID_USAGE_CONSUMER_VOLUME_DECREMENT);
        uint8_t msg[64] = {0xff, 0x20, 0x80};

        usb_hid.sendReport(0, msg, 64);
        // has_consumer_key = true;
        }
    }
}


// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t get_report_callback(uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
  // not used in this example
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) reqlen;

  uint8_t msg[] = {0xff, 0x20, 0x80};
  
  buffer[0] = 0xff;
  buffer[1] = 0x20;
  buffer[2] = 0x80;

  digitalWrite(LED_BUILTIN, activeState);
  activeState = !activeState;

  return 3;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void set_report_callback(uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
  // This example doesn't use multiple report and report ID
  (void) report_id;
  (void) report_type;
  // echo back anything we received from host
  usb_hid.sendReport(0, buffer, bufsize);
}