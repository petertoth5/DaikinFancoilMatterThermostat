#pragma once

/* matter_bridge.h - feeds EP1 Thermostat attributes from a register-model snapshot (task 016).
 * The snapshot getter is the phase-2b seam: the simulation is replaced by the bus without touching this
 * component or mapping. The bridge knows neither the source nor any bus code. */

#include <cstdint>

#include <esp_err.h>
#include <esp_matter.h>

extern "C" {
#include "reg_model.h"  // phase-1 pure C header without extern "C" guards
}

/* Copies the current register model and the time of the copy (same clock the model was fed with) into the outputs. */
typedef void (*bridge_snapshot_fn_t)(reg_model_t *out, int64_t *now_ms);

/* Own update of a Thermostat attribute on the bridge's endpoint, called in the Matter context only
 * (thermostat_ep_update_own lifts the write guard around it). Injected because main depends on this component. */
typedef esp_err_t (*bridge_update_fn_t)(uint32_t attribute_id, esp_matter_attr_val_t *val);

/** Marks the three NONVOLATILE attributes for deferred persistence, schedules the first cycle at once
 * and then one cycle per second. Call after esp_matter::start() (needs the Matter work queue). */
esp_err_t matter_bridge_start(uint16_t endpoint_id, bridge_snapshot_fn_t get_snapshot, bridge_update_fn_t update_own);
