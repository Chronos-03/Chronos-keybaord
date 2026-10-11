#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include <stdlib.h>

#include "pins.h"
#include "matrix.h"
#include "encoder.h"

static const char *TAG = "grandline";

_Static_assert(MATRIX_SCAN_HZ > 0, "MATRIX_SCAN_HZ must be greater than zero");
_Static_assert(configTICK_RATE_HZ >= MATRIX_SCAN_HZ,
               "FreeRTOS tick rate must be >= matrix scan rate");

static void input_task(void *arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(1000 / MATRIX_SCAN_HZ);
    matrix_event_t matrix_events[MATRIX_MAX_EVENTS];
    encoder_event_t encoder_event;

    for (;;) {
        const size_t count = matrix_scan(matrix_events, MATRIX_MAX_EVENTS);
        for (size_t i = 0; i < count; ++i) {
            ESP_LOGI(TAG, "key r%u c%u %s",
                     (unsigned)matrix_events[i].row,
                     (unsigned)matrix_events[i].col,
                     matrix_events[i].pressed ? "DOWN" : "UP");
        }

        if (encoder_poll(&encoder_event)) {
            if (encoder_event.delta != 0) {
                ESP_LOGI(TAG, "encoder %+ld", (long)encoder_event.delta);
            }
            if (encoder_event.button_press) {
                ESP_LOGI(TAG, "encoder button DOWN");
            }
            if (encoder_event.button_release) {
                ESP_LOGI(TAG, "encoder button UP");
            }
        }
        vTaskDelayUntil(&last_wake, period);
    }
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition needs reset; erasing NVS");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_LOGI(TAG, "Grand Line input firmware starting");
    ESP_ERROR_CHECK(matrix_init());
    err = encoder_init();
    if (err != ESP_OK) {
        matrix_deinit();
        ESP_ERROR_CHECK(err);
    }

    BaseType_t task_ok = xTaskCreate(input_task, "input", 4096, NULL, 5, NULL);
    if (task_ok != pdPASS) {
        ESP_LOGE(TAG, "failed to create input task");
        encoder_deinit();
        matrix_deinit();
        abort();
    }

    /* Current milestone: serial-visible matrix and encoder events only.
     * USB HID/keymap, ESP-NOW split link, TFT, NFC, backlight, and runtime
     * power management are intentionally not advertised as implemented. */
}
