#pragma once

#include <cstdint>

#include <esp_err.h>
#include <esp_matter.h>

/* EP1 Thermostat (device type 0x0301): static placeholder values, read-only towards controllers.
 * Architecture section 5 "EP1 Thermostat". Task 016 feeds the values through
 * thermostat_ep_update_own(). */

/** Create EP1 on `node` and store its endpoint id. Call before esp_matter::start(). */
esp_err_t thermostat_ep_create(esp_matter::node_t *node);

/** Endpoint id of EP1; valid after a successful thermostat_ep_create(). */
uint16_t thermostat_ep_id();

/** esp_matter attribute callback: rejects external writes to the Thermostat cluster on EP1. */
esp_err_t thermostat_ep_attribute_cb(esp_matter::attribute::callback_type_t type, uint16_t endpoint_id,
                                     uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val,
                                     void *priv_data);

/** Own update of a Thermostat attribute on EP1. Call only from the Matter context (chip task or
 * under esp_matter::lock::ScopedChipStackLock): the write guard is lifted only around this call. */
esp_err_t thermostat_ep_update_own(uint32_t attribute_id, esp_matter_attr_val_t *val);

/** Pure decision used by the attribute callback: true when the update must be rejected. */
bool thermostat_ep_is_external_write(esp_matter::attribute::callback_type_t type, uint16_t endpoint_id,
                                     uint32_t cluster_id, uint16_t thermostat_endpoint_id, bool own_update_active);
