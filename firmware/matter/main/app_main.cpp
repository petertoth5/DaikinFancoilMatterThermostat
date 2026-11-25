#include <esp_log.h>
#include <esp_matter.h>
#include <nvs_flash.h>
#include <setup_payload/OnboardingCodesUtil.h>

#include "matter_bridge.h"
#include "reset_button.h"
#include "sim_console.h"
#include "thermostat_ep.h"

using namespace esp_matter;

namespace {

const char *TAG = "app_main";

void factory_reset_action()
{
    ESP_LOGW(TAG, "BOOT held: factory reset");
    factory_reset();
}

// Runs in the Matter task. Printing the codes once the server is up and no fabric exists covers boot and
// the boot after a factory reset (architecture section 5 "Commissioning").
void on_matter_event(const chip::DeviceLayer::ChipDeviceEvent *event, intptr_t)
{
    if (event->Type != chip::DeviceLayer::DeviceEventType::kServerReady) {
        return;
    }
    if (chip::Server::GetInstance().GetFabricTable().FabricCount() != 0) {
        return;
    }
    PrintOnboardingCodes(chip::RendezvousInformationFlags(chip::RendezvousInformationFlag::kBLE));
}

esp_err_t identify_cb(identification::callback_type_t, uint16_t, uint8_t, uint8_t, void *)
{
    return ESP_OK;
}

// Failures are logged and abort app_main: a half-built node must not be started.
bool ok(esp_err_t err, const char *what)
{
    if (err == ESP_OK) {
        return true;
    }
    ESP_LOGE(TAG, "%s failed: %s", what, esp_err_to_name(err));
    return false;
}

}  // namespace

extern "C" void app_main(void)
{
    if (!ok(nvs_flash_init(), "nvs_flash_init")) {
        return;
    }
    node::config_t node_config;
    node_t *node = node::create(&node_config, thermostat_ep_attribute_cb, identify_cb);
    if (!node) {
        ESP_LOGE(TAG, "node::create failed");
        return;
    }
    if (!ok(thermostat_ep_create(node), "thermostat_ep_create") ||
        !ok(reset_button_start(factory_reset_action), "reset_button_start") ||
        !ok(sim_console_start(), "sim_console_start") ||
        !ok(start(on_matter_event), "esp_matter::start") ||
        !ok(matter_bridge_start(thermostat_ep_id(), sim_console_snapshot, thermostat_ep_update_own),
            "matter_bridge_start")) {
        return;
    }
}
