#pragma once
/*
 * cdj_hid_map.h
 *
 * Mapping: GPIO pin -> HID byte offset + bitmask (CDJ->Host IN report)
 * Based on cdj_hid.h definitions.
 *
 * Add pins here as you wire up more buttons to the Pico.
 */

#include "cdj_hid.h"
#include "cdj_button.h"

typedef struct {
    cdj_button_t *button; // pointer to the button structure
    uint8_t byte_offset;  // offset in the 20-byte IN report
    uint8_t mask;         // bitmask within that byte
} cdj_hid_button_map_t;

static const cdj_hid_button_map_t cdj_hid_button_map[] = {
    { PLAY_BUTTON_PIN,         2, CDJ_BTN_PLAY       },
    { CUE_BUTTON_PIN,          2, CDJ_BTN_CUE        },
    { SEARCH_FWD_BUTTON_PIN,   2, CDJ_BTN_SEARCH_FWD },
    { SEARCH_BWD_BUTTON_PIN,   2, CDJ_BTN_SEARCH_BWD },
    { SEARCH_SUBMIT_BUTTON_PIN, 2, CDJ_BTN_TRACK_FWD },
    { IN_CUE_BUTTON_PIN,       3, CDJ_BTN_IN_CUE     },
    { OUT_CUE_BUTTON_PIN,      3, CDJ_BTN_OUT        },
    { RELOOP_EXIT_BUTTON_PIN,  3, CDJ_BTN_RELOOP     },
};
#define CDJ_HID_BUTTON_MAP_COUNT (sizeof(cdj_hid_button_map) / sizeof(cdj_hid_button_map[0]))