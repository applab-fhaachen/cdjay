#include "cdjay.h"

#include "cdjay_midi.h"
#include "cdjay_display.h"
#include "cdjay_audio.h"
#include "cdjay_hid.h"


void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  TinyUSBDevice.clearConfiguration();

  TinyUSBDevice.setConfigurationAttribute(sizeof(tusb_desc_device_qualifier_t));
  TinyUSBDevice.setConfigurationAttribute(TUSB_DESC_DEVICE_QUALIFIER);
  TinyUSBDevice.setConfigurationAttribute(0x0200);

  TinyUSBDevice.setConfigurationAttribute(0x00);
  TinyUSBDevice.setConfigurationAttribute(0x00);
  TinyUSBDevice.setConfigurationAttribute(0x00);  

  TinyUSBDevice.setConfigurationAttribute(64);  
  TinyUSBDevice.setConfigurationAttribute(1);  
  TinyUSBDevice.setConfigurationAttribute(0x0);  

  TinyUSBDevice.setVersion(0x0200);
  TinyUSBDevice.setDeviceVersion(0x0112);

  TinyUSBDevice.setManufacturerDescriptor(Manufacturer);
  TinyUSBDevice.setID(VID, PID);
  TinyUSBDevice.setProductDescriptor(Product);


  // Manual begin() is required on core without built-in support e.g. mbed rp2040
  if (!TinyUSBDevice.isInitialized()) {
    TinyUSBDevice.begin(0);
  }
  
  // Serial.begin(115200);
  audioSetup();
  midiSetup();
  hidSetup();
  displaySetup();


  // If already enumerated, additional class driverr begin() e.g msc, hid, midi won't take effect until re-enumeration
  if (TinyUSBDevice.mounted()) {
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();
  }
  digitalWrite(LED_BUILTIN, HIGH);
}

void loop() {
  #ifdef TINYUSB_NEED_POLLING_TASK
  // Manual call tud_task since it isn't called by Core's background
  TinyUSBDevice.task();
  #endif

  // not enumerated()/mounted() yet: nothing to do
  if (!TinyUSBDevice.mounted()) {
    return;
  }

  audioLoop();
  midiLoop();
  hidLoop();

}


