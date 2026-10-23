/*
 * Copyright esp-vim authors. Licensed under the Apache License, Version 2.0
 * (licenses/Apache-2.0.txt).
 *
 * Derived from esp-vim's esp_kbd_hid.h for meshcore-adv-ui: only the
 * keys-held report and modifier bits are kept; the HID report-map parsing and
 * terminal byte encoding were removed.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Modifier bits, as in a HID boot report's first byte. */
#define KBD_MOD_CTRL   (0x01 | 0x10)
#define KBD_MOD_SHIFT  (0x02 | 0x20)
#define KBD_MOD_ALT    (0x04 | 0x40)

#define KBD_REPORT_MAX_KEYS 16     /* held at once; more are ignored */

/* The keys held (HID usages, page 7) and the modifiers. */
typedef struct {
    uint8_t mods;
    uint8_t n;
    uint8_t key[KBD_REPORT_MAX_KEYS];
    bool has_mods;
    bool rollover;
} kbd_report_t;

#ifdef __cplusplus
}
#endif
