#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "pins.h"

#define MATRIX_SCAN_HZ        100
#define MATRIX_DEBOUNCE_SCANS 2     /* state must be stable this many scans (2 x 10 ms) */
#define MATRIX_MAX_EVENTS     (MATRIX_ROWS * MATRIX_COLS) /* worst case: all keys change in one scan */

typedef struct {
    uint8_t row;
    uint8_t col;
    bool    pressed;
} matrix_event_t;

esp_err_t matrix_init(void);

/* Scan once. Returns number of debounced edge events written to out[].
 * Pass an array of at least MATRIX_MAX_EVENTS entries to avoid dropping
 * simultaneous key transitions. */
size_t matrix_scan(matrix_event_t *out, size_t max_events);

/* True if any key is currently held (used by power management). */
bool matrix_any_pressed(void);

void matrix_deinit(void);
