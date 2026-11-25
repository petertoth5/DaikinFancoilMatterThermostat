#include "matter_bridge.h"

#include <cinttypes>
#include <cstring>

#include <esp_log.h>
#include <esp_timer.h>

#include "mapping.h"

using namespace esp_matter;
using namespace chip::app::Clusters;

namespace {

const char *TAG = "matter_bridge";

constexpr uint64_t kCyclePeriodUs = 1000000;  // spec: one cycle per second
constexpr uint32_t kChipLockWaitTicks = portMAX_DELAY;

struct Bridge {
    bridge_snapshot_fn_t get_snapshot;
    bridge_update_fn_t update_own;
    map_state_t map;
    map_thermostat_t last_pushed;  // per attribute group: last value that was accepted by esp_matter
    bool first_cycle;              // first cycle pushes everything (the stored NVS value may differ)
    uint32_t retry_bits;           // groups whose update failed and must be retried
    uint32_t logged_bits;          // groups whose current failure was already logged
    reg_model_t snapshot;          // static buffer, too big for the Matter task stack
};

Bridge s_bridge;
esp_timer_handle_t s_timer;

esp_err_t push(uint32_t attribute_id, esp_matter_attr_val_t val)
{
    return s_bridge.update_own(attribute_id, &val);
}

esp_err_t push_limits(const map_thermostat_t &cur)
{
    using namespace Thermostat::Attributes;
    const struct {
        uint32_t id;
        int16_t value;
    } limits[] = {
        {MinHeatSetpointLimit::Id, cur.min_heat},
        {MaxHeatSetpointLimit::Id, cur.max_heat},
        {MinCoolSetpointLimit::Id, cur.min_cool},
        {MaxCoolSetpointLimit::Id, cur.max_cool},
    };
    for (const auto &limit : limits) {
        esp_err_t err = push(limit.id, esp_matter_int16(limit.value));
        if (err != ESP_OK) {
            return err;
        }
    }
    return ESP_OK;
}

esp_err_t push_heat_setpoint(const map_thermostat_t &cur)
{
    return push(Thermostat::Attributes::OccupiedHeatingSetpoint::Id, esp_matter_int16(cur.heat_sp));
}

esp_err_t push_cool_setpoint(const map_thermostat_t &cur)
{
    return push(Thermostat::Attributes::OccupiedCoolingSetpoint::Id, esp_matter_int16(cur.cool_sp));
}

esp_err_t push_local_temperature(const map_thermostat_t &cur)
{
    nullable<int16_t> value;  // null
    if (cur.local_temp_valid) {
        value = nullable<int16_t>(cur.local_temp);
    }
    return push(Thermostat::Attributes::LocalTemperature::Id, esp_matter_nullable_int16(value));
}

esp_err_t push_system_mode(const map_thermostat_t &cur)
{
    return push(Thermostat::Attributes::SystemMode::Id, esp_matter_enum8(cur.system_mode));
}

void remember_pushed(uint32_t bit, const map_thermostat_t &cur)
{
    map_thermostat_t &last = s_bridge.last_pushed;
    switch (bit) {
    case MAP_CH_LIMITS:
        last.min_heat = cur.min_heat;
        last.max_heat = cur.max_heat;
        last.min_cool = cur.min_cool;
        last.max_cool = cur.max_cool;
        break;
    case MAP_CH_HEAT_SP:
        last.heat_sp = cur.heat_sp;
        break;
    case MAP_CH_COOL_SP:
        last.cool_sp = cur.cool_sp;
        break;
    case MAP_CH_LOCAL_TEMP:
        last.local_temp_valid = cur.local_temp_valid;
        last.local_temp = cur.local_temp;
        break;
    case MAP_CH_SYSTEM_MODE:
        last.system_mode = cur.system_mode;
        break;
    }
}

struct Group {
    uint32_t bit;
    esp_err_t (*push)(const map_thermostat_t &cur);
};

// Order matters: limits first, so a new setpoint never lies outside the published limits.
const Group kGroups[] = {
    {MAP_CH_LIMITS, push_limits},
    {MAP_CH_HEAT_SP, push_heat_setpoint},
    {MAP_CH_COOL_SP, push_cool_setpoint},
    {MAP_CH_LOCAL_TEMP, push_local_temperature},
    {MAP_CH_SYSTEM_MODE, push_system_mode},
};

void push_group(const Group &group, const map_thermostat_t &cur)
{
    Bridge &b = s_bridge;
    esp_err_t err = group.push(cur);
    if (err == ESP_OK) {
        remember_pushed(group.bit, cur);
        b.retry_bits &= ~group.bit;
        b.logged_bits &= ~group.bit;
        return;
    }
    b.retry_bits |= group.bit;
    if (!(b.logged_bits & group.bit)) {
        ESP_LOGE(TAG, "update of group 0x%02" PRIx32 " failed: %s (retried every cycle)", group.bit,
                 esp_err_to_name(err));
        b.logged_bits |= group.bit;
    }
}

// Runs in the Matter context.
void run_cycle(intptr_t)
{
    Bridge &b = s_bridge;
    int64_t now_ms = 0;
    b.get_snapshot(&b.snapshot, &now_ms);
    map_thermostat_t cur;
    mapping_thermostat(&b.map, &b.snapshot, now_ms, &cur);
    uint32_t changed = mapping_changed(&b.last_pushed, &cur, b.first_cycle) | b.retry_bits;
    b.first_cycle = false;
    for (const Group &group : kGroups) {
        if (changed & group.bit) {
            push_group(group, cur);
        }
    }
}

// Logs once when scheduling starts failing and once when it recovers, not every second.
void schedule_cycle()
{
    static bool s_schedule_failing = false;
    bool failed = chip::DeviceLayer::PlatformMgr().ScheduleWork(run_cycle) != CHIP_NO_ERROR;
    if (failed != s_schedule_failing) {
        if (failed) {
            ESP_LOGW(TAG, "cannot schedule a bridge cycle");
        } else {
            ESP_LOGI(TAG, "bridge cycles scheduled again");
        }
        s_schedule_failing = failed;
    }
}

void on_timer(void *)
{
    schedule_cycle();  // the producer side never takes the CHIP lock
}

// setpoints and SystemMode are NONVOLATILE in esp_matter and cannot be made volatile (task 016, NVS persistence).
esp_err_t defer_persistence(uint16_t endpoint_id, uint32_t attribute_id)
{
    attribute_t *attribute = attribute::get(endpoint_id, Thermostat::Id, attribute_id);
    if (!attribute) {
        return ESP_ERR_NOT_FOUND;
    }
    return attribute::set_deferred_persistence(attribute);
}

esp_err_t defer_nonvolatile_attributes(uint16_t endpoint_id)
{
    using namespace Thermostat::Attributes;
    const uint32_t ids[] = {OccupiedHeatingSetpoint::Id, OccupiedCoolingSetpoint::Id, SystemMode::Id};
    lock::ScopedChipStackLock chip_lock(kChipLockWaitTicks);
    for (uint32_t id : ids) {
        esp_err_t err = defer_persistence(endpoint_id, id);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "deferred persistence of 0x%08" PRIX32 " failed: %s", id, esp_err_to_name(err));
            return err;
        }
    }
    return ESP_OK;
}

esp_err_t start_timer()
{
    const esp_timer_create_args_t args = {.callback = on_timer, .arg = nullptr, .dispatch_method = ESP_TIMER_TASK,
                                          .name = "matter_bridge", .skip_unhandled_events = true};
    esp_err_t err = esp_timer_create(&args, &s_timer);
    if (err != ESP_OK) {
        return err;
    }
    return esp_timer_start_periodic(s_timer, kCyclePeriodUs);
}

}  // namespace

esp_err_t matter_bridge_start(uint16_t endpoint_id, bridge_snapshot_fn_t get_snapshot, bridge_update_fn_t update_own)
{
    if (!get_snapshot || !update_own || s_timer) {
        return ESP_ERR_INVALID_STATE;
    }
    Bridge &b = s_bridge;
    memset(&b, 0, sizeof(b));
    b.get_snapshot = get_snapshot;
    b.update_own = update_own;
    b.first_cycle = true;
    mapping_init(&b.map);
    b.last_pushed = b.map.kept;

    esp_err_t err = defer_nonvolatile_attributes(endpoint_id);
    if (err != ESP_OK) {
        return err;
    }
    schedule_cycle();  // first cycle at once, not after the first period
    return start_timer();
}
