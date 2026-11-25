#include <esp_event.h>
#include <esp_log.h>
#include <esp_matter.h>
#include <esp_netif.h>
#include <esp_wifi.h>
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

// Same calls and order as the stack's own Wi-Fi init (ESP32Utils::InitWiFiStack), which then skips its copy.
// Wi-Fi is initialised, not started.
esp_err_t wifi_stack_init()
{
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK) {
        return err;
    }
    // INVALID_STATE: the default loop already exists, as esp_matter::start also accepts.
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    if (!esp_netif_get_handle_from_ifkey("WIFI_STA_DEF") && !esp_netif_create_default_wifi_sta()) {
        return ESP_FAIL;
    }
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    return esp_wifi_init(&cfg);
}

// The stack promotes an AP-only mode to STA+AP and keeps STA+AP, so the mode must be STA before
// esp_matter::start; nothing changes it after esp_wifi_start. Also covers the boot after a factory reset.
esp_err_t wifi_force_station_only()
{
    wifi_mode_t mode = WIFI_MODE_NULL;
    esp_err_t err = esp_wifi_get_mode(&mode);
    if (err != ESP_OK) {
        return err;
    }
    switch (mode) {
    case WIFI_MODE_STA:
        ESP_LOGI(TAG, "Wi-Fi mode after init STA");
        return ESP_OK;
    case WIFI_MODE_NULL:
        ESP_LOGI(TAG, "Wi-Fi mode after init NULL, setting STA");
        break;
    default:
        ESP_LOGW(TAG, "Wi-Fi mode after init %s, forcing STA", mode == WIFI_MODE_AP ? "AP" : "STA+AP");
        break;
    }
    return esp_wifi_set_mode(WIFI_MODE_STA);
}

}  // namespace

extern "C" void app_main(void)
{
    if (!ok(nvs_flash_init(), "nvs_flash_init")) {
        return;
    }
    if (!ok(wifi_stack_init(), "wifi_stack_init") || !ok(wifi_force_station_only(), "wifi_force_station_only")) {
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
