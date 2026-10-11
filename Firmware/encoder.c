#include "encoder.h"

#include <limits.h>

#include "driver/gpio.h"
#include "driver/pulse_cnt.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "encoder";

#define GLITCH_FILTER_NS   1000
#define BUTTON_DEBOUNCE_MS 10
#define ACCEL_FAST_MS      30
#define ACCEL_MED_MS       70

#if ENCODER_COUNTS_PER_DETENT < 1
#error "ENCODER_COUNTS_PER_DETENT must be positive"
#endif

static pcnt_unit_handle_t s_unit;
static pcnt_channel_handle_t s_chan_a;
static pcnt_channel_handle_t s_chan_b;
static int32_t s_residual;
static int s_last_pcnt_count;
static int64_t s_last_detent_us;
static bool s_have_last_detent;
static bool s_btn_state;
static bool s_btn_raw;
static int64_t s_btn_change_us;
static bool s_initialized;

static void encoder_cleanup(void)
{
    if (s_unit != NULL) {
        (void)pcnt_unit_stop(s_unit);
        (void)pcnt_unit_disable(s_unit);
    }
    if (s_chan_b != NULL) {
        (void)pcnt_del_channel(s_chan_b);
        s_chan_b = NULL;
    }
    if (s_chan_a != NULL) {
        (void)pcnt_del_channel(s_chan_a);
        s_chan_a = NULL;
    }
    if (s_unit != NULL) {
        (void)pcnt_del_unit(s_unit);
        s_unit = NULL;
    }
    (void)gpio_reset_pin(PIN_ENC_A);
    (void)gpio_reset_pin(PIN_ENC_B);
    (void)gpio_reset_pin(PIN_ENC_SW);
    s_initialized = false;
}

#define TRY_ESP(call) do { \
    esp_err_t _err = (call); \
    if (_err != ESP_OK) { \
        ESP_LOGE(TAG, "%s failed: %s", #call, esp_err_to_name(_err)); \
        encoder_cleanup(); \
        return _err; \
    } \
} while (0)

esp_err_t encoder_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    gpio_config_t ab = {
        .pin_bit_mask = (1ULL << PIN_ENC_A) | (1ULL << PIN_ENC_B),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    TRY_ESP(gpio_config(&ab));

    gpio_config_t sw = {
        .pin_bit_mask = 1ULL << PIN_ENC_SW,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    TRY_ESP(gpio_config(&sw));

    pcnt_unit_config_t unit_cfg = {
        .low_limit = -1000,
        .high_limit = 1000,
        .flags.accum_count = true,
    };
    TRY_ESP(pcnt_new_unit(&unit_cfg, &s_unit));

    pcnt_glitch_filter_config_t filter_cfg = {
        .max_glitch_ns = GLITCH_FILTER_NS,
    };
    TRY_ESP(pcnt_unit_set_glitch_filter(s_unit, &filter_cfg));

    pcnt_chan_config_t chan_a_cfg = {
        .edge_gpio_num = PIN_ENC_A,
        .level_gpio_num = PIN_ENC_B,
    };
    TRY_ESP(pcnt_new_channel(s_unit, &chan_a_cfg, &s_chan_a));

    pcnt_chan_config_t chan_b_cfg = {
        .edge_gpio_num = PIN_ENC_B,
        .level_gpio_num = PIN_ENC_A,
    };
    TRY_ESP(pcnt_new_channel(s_unit, &chan_b_cfg, &s_chan_b));

    /* Quadrature configuration follows Espressif's PCNT rotary-encoder example.
     * Reverse the reported direction in the keymap if the physical encoder's
     * clockwise direction is opposite to the desired UI convention. */
    TRY_ESP(pcnt_channel_set_edge_action(
        s_chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE,
        PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    TRY_ESP(pcnt_channel_set_level_action(
        s_chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
        PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
    TRY_ESP(pcnt_channel_set_edge_action(
        s_chan_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE,
        PCNT_CHANNEL_EDGE_ACTION_DECREASE));
    TRY_ESP(pcnt_channel_set_level_action(
        s_chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
        PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    /* Accumulate PCNT overflows so polling does not need to clear the counter
     * after every read (which can otherwise lose an edge in the read/clear
     * window). The limits must be registered as watch points for compensation. */
    TRY_ESP(pcnt_unit_add_watch_point(s_unit, unit_cfg.high_limit));
    TRY_ESP(pcnt_unit_add_watch_point(s_unit, unit_cfg.low_limit));
    TRY_ESP(pcnt_unit_enable(s_unit));
    TRY_ESP(pcnt_unit_clear_count(s_unit));
    TRY_ESP(pcnt_unit_start(s_unit));

    s_residual = 0;
    s_last_pcnt_count = 0;
    s_last_detent_us = 0;
    s_have_last_detent = false;
    s_btn_state = (gpio_get_level(PIN_ENC_SW) == 0);
    s_btn_raw = s_btn_state;
    s_btn_change_us = esp_timer_get_time();
    s_initialized = true;
    ESP_LOGI(TAG, "encoder ready (counts/detent=%d, initial button=%s)",
             ENCODER_COUNTS_PER_DETENT, s_btn_state ? "pressed" : "released");
    return ESP_OK;
}

bool encoder_poll(encoder_event_t *ev)
{
    if (!s_initialized || ev == NULL) {
        return false;
    }
    *ev = (encoder_event_t){0};
    const int64_t now = esp_timer_get_time();

    int count = 0;
    esp_err_t err = pcnt_unit_get_count(s_unit, &count);
    if (err == ESP_OK) {
        /* PCNT is configured with overflow accumulation, so use a difference
         * from the last total rather than clearing the counter on every poll. */
        const int64_t delta_count = (int64_t)count - (int64_t)s_last_pcnt_count;
        s_last_pcnt_count = count;
        if (delta_count != 0) {
            /* A poll interval is short enough that this should fit int32_t for
             * a hand-operated encoder; clamp defensively if the input is noisy. */
            int64_t pending = (int64_t)s_residual + delta_count;
            if (pending > INT32_MAX) pending = INT32_MAX;
            if (pending < INT32_MIN) pending = INT32_MIN;
            s_residual = (int32_t)pending;

            const int32_t detents = s_residual / ENCODER_COUNTS_PER_DETENT;
            if (detents != 0) {
                s_residual -= detents * ENCODER_COUNTS_PER_DETENT;
                int multiplier = 1;
                if (s_have_last_detent) {
                    const int64_t dt_ms = (now - s_last_detent_us) / 1000;
                    multiplier = (dt_ms < ACCEL_FAST_MS) ? 4 :
                                 (dt_ms < ACCEL_MED_MS) ? 2 : 1;
                }
                s_last_detent_us = now;
                s_have_last_detent = true;
                ev->delta = detents * multiplier;
            }
        }
    } else {
        ESP_LOGW(TAG, "PCNT read failed: %s", esp_err_to_name(err));
    }

    const bool raw_pressed = gpio_get_level(PIN_ENC_SW) == 0;
    if (raw_pressed != s_btn_raw) {
        s_btn_raw = raw_pressed;
        s_btn_change_us = now;
    }
    if (s_btn_raw != s_btn_state &&
        (now - s_btn_change_us) >= (BUTTON_DEBOUNCE_MS * 1000LL)) {
        s_btn_state = s_btn_raw;
        ev->button_press = s_btn_state;
        ev->button_release = !s_btn_state;
    }

    return ev->delta != 0 || ev->button_press || ev->button_release;
}

void encoder_deinit(void)
{
    encoder_cleanup();
    s_residual = 0;
    s_last_pcnt_count = 0;
    s_last_detent_us = 0;
    s_have_last_detent = false;
}
