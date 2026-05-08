#include <bsp/board_api.h>
#include <tusb.h>


#define _PID_MAP(itf, n)  ( (CFG_TUD_##itf) << (n) )
#define USB_PID           (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 1) | _PID_MAP(HID, 2))


tusb_desc_device_t const device_desc = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,

    .bDeviceClass = 0xEF,
    .bDeviceSubClass = 0x02,
    .bDeviceProtocol = 0x01,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor = 0x0358, // PIONEER Corporation.
    .idProduct = USB_PID,
    .bcdDevice = 0x0112,

    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    
    .bNumConfigurations = 0x01
};

char const *string_desc_arr[] = {
    (const char []) {0x09, 0x04},
    "PIONEER Corporation.",
    "PIONEER CDJ-850",
    "0",
    "PIONEER CDJ-850 MIDI",
    "PIONEER CDJ-850 HID"
};

enum {
    ITF_NUM_MIDI = 0,
    ITF_NUM_MIDI_STREAMING,
    ITF_NUM_HID,
    ITF_NUM_TOTAL
};

#define EPNUM_MIDI_OUT 0x03
#define EPNUM_MIDI_IN  0x83
#define EPNUM_HID_OUT  0x06
#define EPNUM_HID_IN   0x87

// IAD for MIDI: 8 bytes (groups Audio Control + MIDI Streaming interfaces)
#define TUD_MIDI_IAD_LEN 8
#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MIDI_IAD_LEN + TUD_MIDI_DESC_LEN + TUD_HID_INOUT_DESC_LEN)

static uint8_t const hid_report_desc[] = {
    0x06, 0xA0, 0xFF, 0x09, 0x01, 0xA1, 0x01, 0x09,
    0x02, 0xA1, 0x00, 0x06, 0xA1, 0xFF, 0x09, 0x03,
    0x09, 0x04, 0x15, 0x80, 0x25, 0x7F, 0x35, 0x00,
    0x45, 0xFF, 0x75, 0x08, 0x95, 0x14, 0x81, 0x02,
    0x09, 0x05, 0x09, 0x06, 0x15, 0x80, 0x25, 0x7F,
    0x35, 0x00, 0x45, 0xFF, 0x75, 0x08, 0x95, 0x24,
    0x91, 0x02, 0xC0, 0xC0
};

uint8_t config_desc[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0xC0, 0),

    // Interface Association Descriptor (IAD) for MIDI Audio Class
    // bLength=8, bDescriptorType=0x0B (IAD), bFirstInterface, bInterfaceCount, bFunctionClass, bFunctionSubClass, bFunctionProtocol, iFunction
    0x08, 0x0B, ITF_NUM_MIDI, 0x02, 0x01, 0x00, 0x00, 0x00,

    TUD_MIDI_DESCRIPTOR(
        ITF_NUM_MIDI,
        4,
        EPNUM_MIDI_OUT,
        EPNUM_MIDI_IN,
        64
    ),

    TUD_HID_INOUT_DESCRIPTOR(
        ITF_NUM_HID,
        5,
        HID_ITF_PROTOCOL_NONE,
        sizeof(hid_report_desc),
        EPNUM_HID_OUT,
        EPNUM_HID_IN,
        36,
        1
    ),
};


//callback functions
uint8_t const *tud_descriptor_device_cb(void) {
    return (uint8_t const *)&device_desc;
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void) index;  //we avoid the unused variable error while keeping the function's signature intact
    return config_desc;
}

// buffer to hold the string descriptor during the request | plus 1 for the null terminator
static uint16_t _desc_str[32 + 1];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void) langid;

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 + 2);
        return _desc_str;
    }

    const char *str = string_desc_arr[index];
    size_t len = strlen(str);

    for (size_t i = 0; i < len; i++) {
        _desc_str[1 + i] = str[i];
    }

    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 + len * 2);
    return _desc_str;
}

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
    (void) instance;
    return hid_report_desc;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) bufsize;
}

void tud_hid_set_protocol_cb(uint8_t instance, uint8_t protocol) {
    (void) instance;
    (void) protocol;
}

bool tud_hid_set_idle_cb(uint8_t instance, uint8_t idle_rate) {
    (void) instance;
    (void) idle_rate;
    return true;
}