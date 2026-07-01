#pragma once
#include<Arduino.h>
#include <Adafruit_TinyUSB.h>


// USB HID Object
Adafruit_USBD_HID usb_hid;

void hidSetup();