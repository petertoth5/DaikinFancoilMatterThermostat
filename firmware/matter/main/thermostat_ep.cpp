#include "thermostat_ep.h"

#include <cinttypes>

#include <esp_log.h>

using namespace esp_matter;
using namespace chip::app::Clusters;

namespace {

const char *TAG = "thermostat_ep";

// Values in 0.01 degC (Matter temperature unit). Architecture section 5: placeholders until task 016.
constexpr int16_t kPlaceholderSetpoint = 2000;
constexpr int16_t kPlaceholderLimitMin = 500;
constexpr int16_t kPlaceholderLimitMax = 4000;
// ControlSequenceOfOperation 4 = CoolingAndHeating; SystemMode 4 = Heat (chip data model, external).
constexpr uint8_t kControlSequenceCoolingAndHeating = 4;
constexpr uint8_t kSystemModeHeat = 4;

uint16_t s_endpoint_id = 0;
bool s_own_update_active = false;  // only touched in the Matter context

struct LimitAttribute {
    uint32_t id;
    uint16_t flags;
    int16_t value;
};

// The setpoint limits are not created by cluster::thermostat::create(). Created without NONVOLATILE
// (architecture section 3 "NVS wear"). The setpoints and SystemMode are NONVOLATILE in esp_matter and
// cannot be changed through the public API: open item, see the task 015 report.
esp_err_t add_limit_attributes(cluster_t *cluster)
{
    using namespace Thermostat::Attributes;
    const LimitAttribute limits[] = {
        {MinHeatSetpointLimit::Id, ATTRIBUTE_FLAG_WRITABLE, kPlaceholderLimitMin},
        {MaxHeatSetpointLimit::Id, ATTRIBUTE_FLAG_WRITABLE, kPlaceholderLimitMax},
        {MinCoolSetpointLimit::Id, ATTRIBUTE_FLAG_WRITABLE, kPlaceholderLimitMin},
        {MaxCoolSetpointLimit::Id, ATTRIBUTE_FLAG_WRITABLE, kPlaceholderLimitMax},
        {AbsMinHeatSetpointLimit::Id, ATTRIBUTE_FLAG_NONE, kPlaceholderLimitMin},
        {AbsMaxHeatSetpointLimit::Id, ATTRIBUTE_FLAG_NONE, kPlaceholderLimitMax},
        {AbsMinCoolSetpointLimit::Id, ATTRIBUTE_FLAG_NONE, kPlaceholderLimitMin},
        {AbsMaxCoolSetpointLimit::Id, ATTRIBUTE_FLAG_NONE, kPlaceholderLimitMax},
    };
    for (const LimitAttribute &limit : limits) {
        if (!attribute::create(cluster, limit.id, limit.flags, esp_matter_int16(limit.value))) {
            ESP_LOGE(TAG, "cannot create attribute 0x%08" PRIX32, limit.id);
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

endpoint::thermostat::config_t placeholder_config()
{
    endpoint::thermostat::config_t config;
    cluster::thermostat::config_t &t = config.thermostat;
    t.control_sequence_of_operation = kControlSequenceCoolingAndHeating;
    t.system_mode = kSystemModeHeat;
    t.feature_flags = cluster::thermostat::feature::heating::get_id() | cluster::thermostat::feature::cooling::get_id();
    t.features.heating.occupied_heating_setpoint = kPlaceholderSetpoint;
    t.features.cooling.occupied_cooling_setpoint = kPlaceholderSetpoint;
    return config;  // local_temperature stays null
}

}  // namespace

bool thermostat_ep_is_external_write(attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id,
                                     uint16_t thermostat_endpoint_id, bool own_update_active)
{
    return type == attribute::PRE_UPDATE && endpoint_id == thermostat_endpoint_id &&
           cluster_id == Thermostat::Id && !own_update_active;
}

esp_err_t thermostat_ep_create(node_t *node)
{
    endpoint::thermostat::config_t config = placeholder_config();
    endpoint_t *endpoint = endpoint::thermostat::create(node, &config, ENDPOINT_FLAG_NONE, nullptr);
    if (!endpoint) {
        return ESP_FAIL;
    }
    s_endpoint_id = endpoint::get_id(endpoint);
    return add_limit_attributes(cluster::get(endpoint, Thermostat::Id));
}

uint16_t thermostat_ep_id()
{
    return s_endpoint_id;
}

esp_err_t thermostat_ep_attribute_cb(attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id,
                                     uint32_t attribute_id, esp_matter_attr_val_t *, void *)
{
    if (thermostat_ep_is_external_write(type, endpoint_id, cluster_id, s_endpoint_id, s_own_update_active)) {
        ESP_LOGW(TAG, "rejected external write to attribute 0x%08" PRIX32, attribute_id);
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_OK;
}

esp_err_t thermostat_ep_update_own(uint32_t attribute_id, esp_matter_attr_val_t *val)
{
    s_own_update_active = true;
    esp_err_t err = attribute::update(s_endpoint_id, Thermostat::Id, attribute_id, val);
    s_own_update_active = false;
    return err;
}
