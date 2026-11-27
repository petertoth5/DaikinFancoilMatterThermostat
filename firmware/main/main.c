/* main.c - phase-1 probe image entry (spec 006, 019). Transceiver is held in receive first, then the console
 * REPL is created and the `mb` command registered, then bus_task and the REPL start. The REPL exists before
 * bus_task so that the console driver setup cannot race bus_task's first output (P4). No NVS, Wi-Fi or Matter. */

#include "bus_task.h"
#include "esp_console.h"
#include "esp_log.h"
#include "mb_link.h"
#include "sdkconfig.h"

/* Console never on the bus UART. Symbols: ESP-IDF v6.0.2 components/esp_stdio/Kconfig
 * (CONFIG_ESP_CONSOLE_UART = UART console selected, CONFIG_ESP_CONSOLE_UART_NUM = its port). */
#if CONFIG_ESP_CONSOLE_UART && (CONFIG_ESP_CONSOLE_UART_NUM == CONFIG_DMT_BUS_UART_PORT)
#error "The console UART must not be the bus UART (CONFIG_DMT_BUS_UART_PORT)"
#endif

#define REPL_PROMPT          "dmt>"
#define REPL_STACK_BYTES     8192    /* mb regs/status/stats hold a bus_snapshot_t (about 2.6 KB) on the stack */
#define MS_PER_S             1000u

static const char *TAG = "main";

/* hold_err is filled by app_main's first statement. */
static bus_task_config_t s_cfg = {
    .link = {
        .port = {
            .uart_port = CONFIG_DMT_BUS_UART_PORT,
            .tx_gpio = CONFIG_DMT_BUS_TX_GPIO,
            .rx_gpio = CONFIG_DMT_BUS_RX_GPIO,
            .de_gpio = CONFIG_DMT_BUS_DE_GPIO,
            .baud = CONFIG_DMT_BUS_BAUD,
        },
        .resp_timeout_ms = CONFIG_DMT_BUS_RESP_TIMEOUT_MS,
    },
    .sched = {
        .addr = CONFIG_DMT_BUS_SLAVE_ADDR,
        .gap_ms = CONFIG_DMT_BUS_GAP_MS,
        .fast_ms = CONFIG_DMT_POLL_FAST_S * MS_PER_S,
        .slow_ms = CONFIG_DMT_POLL_SLOW_S * MS_PER_S,
        .offline_probe_ms = CONFIG_DMT_BUS_OFFLINE_PROBE_S * MS_PER_S,
        .retries = CONFIG_DMT_BUS_READ_RETRIES,
        .offline_after = CONFIG_DMT_BUS_OFFLINE_AFTER,
    },
    .boot_silence_ms = CONFIG_DMT_BUS_BOOT_SILENCE_MS,
};

/* Runtime twin of the #error above; the console must be proven off the bus UART before bus_task starts. */
static bool console_uses_bus_uart(void)
{
#if CONFIG_ESP_CONSOLE_UART
    esp_console_dev_uart_config_t dev_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    return dev_config.channel == s_cfg.link.port.uart_port;
#else
    return false;
#endif
}

static esp_err_t create_repl(esp_console_repl_t **repl)
{
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = REPL_PROMPT;
    repl_config.task_stack_size = REPL_STACK_BYTES;
#if CONFIG_ESP_CONSOLE_UART
    esp_console_dev_uart_config_t dev_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    return esp_console_new_repl_uart(&dev_config, &repl_config, repl);
#elif CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    esp_console_dev_usb_serial_jtag_config_t dev_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    return esp_console_new_repl_usb_serial_jtag(&dev_config, &repl_config, repl);
#else
#error "Unsupported console: select the UART or USB Serial/JTAG console"
#endif
}

/* Creates the REPL and registers the commands; it is started only after bus_task. */
static esp_err_t prepare_console(esp_console_repl_t **repl)
{
    esp_err_t err = create_repl(repl);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_console_register_help_command();
    if (err != ESP_OK) {
        return err;
    }
    return mb_console_register(&s_cfg);
}

void app_main(void)
{
    s_cfg.hold_err = mb_link_hold_receive(&s_cfg.link);

    /* Failure here and in the console setup below: no bus_task, so the hold above stays until reboot. */
    if (console_uses_bus_uart()) {
        ESP_LOGE(TAG, "console UART is the bus UART; bus_task and console not started");
        return;
    }

    /* Keeps the driver-install INFO line ("queue free spaces", uart.c, IDF v6.0.2) off bus_task's output. */
    esp_log_level_set("uart", ESP_LOG_WARN);

    esp_console_repl_t *repl = NULL;
    esp_err_t err = prepare_console(&repl);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "console setup failed: %s; bus_task not started", esp_err_to_name(err));
        return;
    }
    err = bus_task_start(&s_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bus_task_start failed: %s", esp_err_to_name(err));
        return;
    }
    err = esp_console_start_repl(repl);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "console start failed: %s", esp_err_to_name(err));
    }
}
