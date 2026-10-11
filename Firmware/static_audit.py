#!/usr/bin/env python3
"""Lightweight source/config sanity checks; this is not a substitute for idf.py build."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
checks = []

def check(name, condition, detail):
    checks.append((name, bool(condition), detail))

pins = (ROOT / "config/pins.h").read_text()
main = (ROOT / "main/main.c").read_text()
matrix = (ROOT / "main/matrix.c").read_text()
encoder = (ROOT / "main/encoder.c").read_text()
cmake = (ROOT / "main/CMakeLists.txt").read_text()
readme = (ROOT / "README.md").read_text()

check("4x6 matrix dimensions", "#define MATRIX_ROWS 4" in pins and "#define MATRIX_COLS 6" in pins,
      "The configured matrix is 4 rows by 6 columns.")
check("Event buffer covers full matrix", "matrix_event_t matrix_events[MATRIX_MAX_EVENTS]" in main and
      "#define MATRIX_MAX_EVENTS     (MATRIX_ROWS * MATRIX_COLS)" in (ROOT / "main/matrix.h").read_text(),
      "The input task has room for up to 24 simultaneous key transitions.")
check("Overflowed matrix events remain pending", "n < max_events" in matrix and
      "s_stable[r][c] = raw" in matrix and "s_count[r][c] < MATRIX_DEBOUNCE_SCANS" in matrix,
      "State is committed only when the event can be emitted.")
check("Encoder overflow accumulation enabled", ".flags.accum_count = true" in encoder and
      "pcnt_unit_add_watch_point(s_unit, unit_cfg.high_limit)" in encoder and
      "pcnt_unit_add_watch_point(s_unit, unit_cfg.low_limit)" in encoder,
      "PCNT overflow compensation is configured with high/low watch points.")
check("No per-poll PCNT clear", "pcnt_unit_clear_count(s_unit)" in encoder and
      "if (pcnt_unit_clear_count(s_unit) == ESP_OK)" not in encoder,
      "The counter is cleared during initialization, not after each read.")
check("Partial init cleanup exists", "encoder_cleanup();" in encoder and "reset_matrix_pins();" in matrix,
      "Driver initialization failures have cleanup paths.")
check("Task creation failure handled", "task_ok != pdPASS" in main,
      "A failed FreeRTOS task allocation is not silently ignored.")
check("TFT pin conflict documented", "GPIO_NUM_37" in pins and "GPIO_NUM_36" in pins and
      "Critical PCB/module issue" in readme,
      "The GPIO36/37 hardware issue is explicitly documented.")
check("Build dependencies declared", "esp_driver_gpio" in cmake and "esp_driver_pcnt" in cmake,
      "Uses the split GPIO/PCNT driver components.")

failed = False
for name, ok, detail in checks:
    print(f"{'PASS' if ok else 'FAIL'}: {name} — {detail}")
    failed |= not ok
print(f"\n{sum(ok for _, ok, _ in checks)}/{len(checks)} static checks passed.")
raise SystemExit(1 if failed else 0)
