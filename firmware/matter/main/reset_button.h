#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* BOOT button (GPIO0, active low) held for RESET_BUTTON_HOLD_MS triggers the action once.
 * Hold time 5 s is an inferred value (task 015). */
#define RESET_BUTTON_POLL_MS 50u
#define RESET_BUTTON_HOLD_MS 5000u

typedef void (*reset_button_action_t)(void);

/* Pure: advance the held time by one poll. Returns true exactly once, on the poll where the
 * held time first reaches hold_ms; releasing the button restarts the count. */
bool reset_button_step(uint32_t *held_ms, bool pressed, uint32_t poll_ms, uint32_t hold_ms);

/* Start the polling task; `action` runs in that task after a long press. */
esp_err_t reset_button_start(reset_button_action_t action);

#ifdef __cplusplus
}
#endif
