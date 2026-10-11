/*
 * GrandLine - pin map (ESP32-S3-WROOM-1-N4R8)
 * Extracted from splitkeybaord.kicad_pcb. Left and right halves share the
 * same pinout, so one header serves both.
 */
#pragma once

#include "driver/gpio.h"

/*
 * KNOWN HARDWARE ISSUE (PCB rev 1): TFT_CS (GPIO37) and TFT_PWM (GPIO36) are
 * electrically tied to the octal PSRAM inside the ESP32-S3-WROOM-1-N4R8.
 * Disabling PSRAM in software does NOT disconnect it. Do not trust the TFT or
 * its backlight on these pins. Fix: use a WROOM-1-N4/N8 (no PSRAM) module,
 * or move these two signals to free GPIOs (1, 3, 45, 46, 47, 48) in the next
 * PCB revision. Matrix, encoder and everything else are unaffected.
 */
#ifdef CONFIG_SPIRAM
#error "PSRAM must be disabled: GPIO36/37 are used by the TFT on this PCB"
#endif

/* ---- Key matrix (4 rows x 6 cols) ---- */
#define MATRIX_ROWS 4
#define MATRIX_COLS 6

#define PIN_ROW0 GPIO_NUM_4
#define PIN_ROW1 GPIO_NUM_5
#define PIN_ROW2 GPIO_NUM_6
#define PIN_ROW3 GPIO_NUM_7

#define PIN_COL0 GPIO_NUM_15
#define PIN_COL1 GPIO_NUM_16
#define PIN_COL2 GPIO_NUM_17
#define PIN_COL3 GPIO_NUM_18
#define PIN_COL4 GPIO_NUM_21
#define PIN_COL5 GPIO_NUM_38

/* ---- Rotary encoder ---- */
#define PIN_ENC_A  GPIO_NUM_10
#define PIN_ENC_B  GPIO_NUM_11
#define PIN_ENC_SW GPIO_NUM_12

/* ---- Shared SPI bus (TFT + NFC) ---- */
#define PIN_SPI_SCK  GPIO_NUM_40
#define PIN_SPI_MOSI GPIO_NUM_41
#define PIN_SPI_MISO GPIO_NUM_42

/* ---- TFT ---- */
#define PIN_TFT_CS  GPIO_NUM_37   /* HW CONFLICT: wired to in-module octal PSRAM on N4R8 */
#define PIN_TFT_DC  GPIO_NUM_8
#define PIN_TFT_RST GPIO_NUM_9
#define PIN_TFT_BL  GPIO_NUM_36   /* HW CONFLICT: wired to in-module octal PSRAM on N4R8 (net TFT_PWM) */

/* ---- NFC (J_LINK/NFC header, SPI) ---- */
#define PIN_NFC_CS  GPIO_NUM_39
#define PIN_NFC_RST GPIO_NUM_14

/* ---- Per-key backlight (PWM, LED cathode switched) ---- */
#define PIN_BACKLIGHT_PWM GPIO_NUM_13

/* ---- Power ---- */
#define PIN_HOST_PWR_EN GPIO_NUM_2

/* ---- USB (native) ---- */
#define PIN_USB_DM GPIO_NUM_19
#define PIN_USB_DP GPIO_NUM_20

/* ---- Inter-half UART header (net INTER_TX/RX; = UART0 pins) ---- */
#define PIN_INTER_TX GPIO_NUM_43
#define PIN_INTER_RX GPIO_NUM_44
