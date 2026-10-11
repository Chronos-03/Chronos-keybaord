#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "pins.h"

/* PCNT counts per physical detent: 4 for most EC11 types, 2 for half-step ones. */
#define ENCODER_COUNTS_PER_DETENT 4

typedef struct {
    int32_t delta;        /* detents since last poll, acceleration applied (+cw / -ccw) */
    bool    button_press; /* debounced press edge */
    bool    button_release;
} encoder_event_t;

esp_err_t encoder_init(void);

/* Call periodically (~100 Hz). Returns true if anything happened. */
bool encoder_poll(encoder_event_t *ev);

void encoder_deinit(void);
