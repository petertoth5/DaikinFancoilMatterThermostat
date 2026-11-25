#pragma once

/* sim_console.h - owner of the simulated register source and its UART console (task 016).
 * Console: sim set <reg> <raw> | sim unsup <reg> | sim stale | sim offline | sim live | sim show */

#include <stdint.h>

#include <esp_err.h>

#include "reg_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Seeds the simulation, starts its 1 s tick and the console REPL. */
esp_err_t sim_console_start(void);

/** Snapshot getter for the Matter bridge (bridge_snapshot_fn_t): copies the model under the sim mutex. */
void sim_console_snapshot(reg_model_t *out, int64_t *now_ms);

#ifdef __cplusplus
}
#endif
