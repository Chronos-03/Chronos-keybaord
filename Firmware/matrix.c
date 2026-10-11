#include "matrix.h"

#include <string.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

static const char *TAG = "matrix";

/* PCB matrix is COL2ROW: the diode anode is at the column/switch side and
 * cathode at the row. Pull columns high and drive one row low to read it. */
static const gpio_num_t s_rows[MATRIX_ROWS] = {
    PIN_ROW0, PIN_ROW1, PIN_ROW2, PIN_ROW3
};
static const gpio_num_t s_cols[MATRIX_COLS] = {
    PIN_COL0, PIN_COL1, PIN_COL2, PIN_COL3, PIN_COL4, PIN_COL5
};

static bool s_stable[MATRIX_ROWS][MATRIX_COLS];
static uint8_t s_count[MATRIX_ROWS][MATRIX_COLS];
static bool s_initialized;

static void reset_matrix_pins(void)
{
    for (int r = 0; r < MATRIX_ROWS; ++r) {
        (void)gpio_reset_pin(s_rows[r]);
    }
    for (int c = 0; c < MATRIX_COLS; ++c) {
        (void)gpio_reset_pin(s_cols[c]);
    }
    s_initialized = false;
}

esp_err_t matrix_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    memset(s_stable, 0, sizeof(s_stable));
    memset(s_count, 0, sizeof(s_count));

    for (int r = 0; r < MATRIX_ROWS; ++r) {
        gpio_config_t io = {
            .pin_bit_mask = 1ULL << s_rows[r],
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        esp_err_t err = gpio_config(&io);
        if (err != ESP_OK) {
            reset_matrix_pins();
            return err;
        }
        err = gpio_set_level(s_rows[r], 1);
        if (err != ESP_OK) {
            reset_matrix_pins();
            return err;
        }
    }

    for (int c = 0; c < MATRIX_COLS; ++c) {
        gpio_config_t io = {
            .pin_bit_mask = 1ULL << s_cols[c],
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        esp_err_t err = gpio_config(&io);
        if (err != ESP_OK) {
            reset_matrix_pins();
            return err;
        }
    }

    s_initialized = true;
    ESP_LOGI(TAG, "matrix ready (%dx%d, %d Hz, debounce %d scans)",
             MATRIX_ROWS, MATRIX_COLS, MATRIX_SCAN_HZ, MATRIX_DEBOUNCE_SCANS);
    return ESP_OK;
}

size_t matrix_scan(matrix_event_t *out, size_t max_events)
{
    if (!s_initialized || out == NULL || max_events == 0) {
        return 0;
    }

    size_t n = 0;
    for (int r = 0; r < MATRIX_ROWS; ++r) {
        if (gpio_set_level(s_rows[r], 0) != ESP_OK) {
            /* Do not leave a row selected if a GPIO operation fails. */
            (void)gpio_set_level(s_rows[r], 1);
            continue;
        }
        esp_rom_delay_us(5); /* allow the row/column network to settle */

        for (int c = 0; c < MATRIX_COLS; ++c) {
            const bool raw = gpio_get_level(s_cols[c]) == 0;
            if (raw == s_stable[r][c]) {
                s_count[r][c] = 0;
                continue;
            }

            /* Saturate rather than wrap. If the caller's event buffer is too
             * small, leave the transition pending and emit it on a later scan
             * instead of silently changing state and losing the event. */
            if (s_count[r][c] < MATRIX_DEBOUNCE_SCANS) {
                ++s_count[r][c];
            }
            if (s_count[r][c] >= MATRIX_DEBOUNCE_SCANS && n < max_events) {
                s_stable[r][c] = raw;
                s_count[r][c] = 0;
                out[n++] = (matrix_event_t) {
                    .row = (uint8_t)r,
                    .col = (uint8_t)c,
                    .pressed = raw,
                };
            }
        }

        (void)gpio_set_level(s_rows[r], 1);
    }
    return n;
}

bool matrix_any_pressed(void)
{
    if (!s_initialized) {
        return false;
    }
    for (int r = 0; r < MATRIX_ROWS; ++r) {
        for (int c = 0; c < MATRIX_COLS; ++c) {
            if (s_stable[r][c]) {
                return true;
            }
        }
    }
    return false;
}

void matrix_deinit(void)
{
    reset_matrix_pins();
}
