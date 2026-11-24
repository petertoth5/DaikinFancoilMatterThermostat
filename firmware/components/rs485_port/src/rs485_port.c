/* rs485_port.c - bus UART, DE signal, silence check, one blocking transaction (spec 014).
 * DE order rules (docs/spikes/002-esp-modbus.md S5, S10): half-duplex mode is set before the pins are
 * routed (a routed RTS in plain UART mode sits high); DE level 0 is written before its output is
 * enabled after every driver delete. gpio_reset_pin is never used on DE (it enables the pull-up). */
#include "rs485_port.h"

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_intr_alloc.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "sdkconfig.h"

#if !CONFIG_UART_ISR_IN_IRAM
#error "rs485_port requires CONFIG_UART_ISR_IN_IRAM=y: DE release depends on the TX_DONE ISR running during flash/NVS stalls"
#endif

#define UART_PORT_MIN          1      /* UART0 is the console */
#define UART_PORT_MAX          2
#define REQUEST_LEN            8u     /* phase 1: one read request, tx_len must equal this */
#define RX_BUFFER_BYTES        256    /* driver ring buffer, must exceed the 128 byte HW FIFO */
#define NO_TX_BUFFER           0      /* uart_write_bytes then writes straight into the HW FIFO */
#define SILENCE_EVENT_QUEUE    20
#define RX_FULL_THRESHOLD_BYTES 1     /* each received byte reaches the ring buffer at once */
#define RESP_TIMEOUT_MAX_MS    2000u
#define BITS_PER_CHAR          11u    /* conservative: start + 8 data + parity + stop */
#define TX_DONE_MARGIN_MS      20u
#define US_PER_MS              1000u
#define US_PER_S               1000000u
#define UART_INTR_FLAGS        ESP_INTR_FLAG_IRAM  /* the driver forces it anyway; passing it avoids its "flag updated" log */
#define TICK_US                (US_PER_S / configTICK_RATE_HZ)

typedef struct {
    bool open;
    bool cfg_known;                 /* a cfg was accepted: close/hold know the DE and UART numbers */
    rs485_port_cfg_t cfg;
    rs485_port_counters_t counters;
} port_state_t;

static port_state_t s_port;

/* Reply being collected by one transaction. */
typedef struct {
    uint8_t *buf;
    size_t cap;
    size_t stored;                  /* bytes in buf, <= cap */
    size_t total;                   /* bytes received, including those beyond cap */
    size_t expected;                /* last len_fn result */
    int64_t last_byte_us;
    rs485_frame_len_fn len_fn;
    void *len_ctx;
} reply_t;

static void saturating_add(uint32_t *counter, uint32_t n)
{
    *counter = (n > UINT32_MAX - *counter) ? UINT32_MAX : *counter + n;
}

static int64_t min_i64(int64_t a, int64_t b)
{
    return a < b ? a : b;
}

static uint32_t us_to_ticks_ceil(int64_t us)
{
    if (us <= 0) {
        return 0;
    }
    return (uint32_t)((us + TICK_US - 1) / TICK_US);
}

static esp_err_t check_cfg(const rs485_port_cfg_t *c)
{
    if (c == NULL || c->uart_port < UART_PORT_MIN || c->uart_port > UART_PORT_MAX || c->baud == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (c->tx_gpio < 0 || c->rx_gpio < 0 || c->de_gpio < 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (c->tx_gpio == c->rx_gpio || c->tx_gpio == c->de_gpio || c->rx_gpio == c->de_gpio) {
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

/* DE level 0 first, then the output; after that it is a plain GPIO output driving low
   (gpio_output_enable resets the matrix to the GPIO register: esp_driver_gpio/src/gpio.c:228).
   Every step is attempted; the first error is returned. */
static esp_err_t drive_de_low(int de_gpio)
{
    const esp_err_t level_err = gpio_set_level(de_gpio, 0);
    const esp_err_t pull_err = gpio_pullup_dis(de_gpio);
    const esp_err_t dir_err = gpio_set_direction(de_gpio, GPIO_MODE_OUTPUT);

    if (level_err != ESP_OK) {
        return level_err;
    }
    return (pull_err != ESP_OK) ? pull_err : dir_err;
}

static esp_err_t remember_cfg(const rs485_port_cfg_t *c)
{
    const esp_err_t err = check_cfg(c);
    if (err != ESP_OK) {
        return err;
    }
    s_port.cfg = *c;
    s_port.cfg_known = true;
    return ESP_OK;
}

esp_err_t rs485_port_hold_receive(const rs485_port_cfg_t *c)
{
    if (s_port.open) {
        return ESP_ERR_INVALID_STATE;
    }
    const esp_err_t err = remember_cfg(c);
    if (err != ESP_OK) {
        return err;
    }
    return drive_de_low(c->de_gpio);
}

esp_err_t rs485_port_close(void)
{
    s_port.open = false;
    if (!s_port.cfg_known) {
        return ESP_OK;
    }
    esp_err_t delete_err = ESP_OK;
    if (uart_is_driver_installed(s_port.cfg.uart_port)) {
        delete_err = uart_driver_delete(s_port.cfg.uart_port);
    }
    /* Hold receive regardless of the delete result. */
    const esp_err_t hold_err = drive_de_low(s_port.cfg.de_gpio);
    return (delete_err != ESP_OK) ? delete_err : hold_err;
}

static esp_err_t configure_uart_8n1(const rs485_port_cfg_t *c)
{
    const uart_config_t uart_cfg = {
        .baud_rate = (int)c->baud,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    return uart_param_config(c->uart_port, &uart_cfg);
}

static esp_err_t open_steps(const rs485_port_cfg_t *c)
{
    esp_err_t err = drive_de_low(c->de_gpio);
    if (err != ESP_OK) {
        return err;
    }
    err = configure_uart_8n1(c);
    if (err != ESP_OK) {
        return err;
    }
    err = uart_driver_install(c->uart_port, RX_BUFFER_BYTES, NO_TX_BUFFER, 0, NULL, UART_INTR_FLAGS);
    if (err != ESP_OK) {
        return err;
    }
    /* Mode before pins: no call between these two sets RTS active. */
    err = uart_set_mode(c->uart_port, UART_MODE_RS485_HALF_DUPLEX);
    if (err != ESP_OK) {
        return err;
    }
    err = uart_set_pin(c->uart_port, c->tx_gpio, c->rx_gpio, c->de_gpio, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        return err;
    }
    err = uart_set_rx_full_threshold(c->uart_port, RX_FULL_THRESHOLD_BYTES);
    if (err != ESP_OK) {
        return err;
    }
    /* uart_set_pin does not enable the RX pull-up; RO is high-impedance while DE and /RE are high. */
    return gpio_pullup_en(c->rx_gpio);
}

esp_err_t rs485_port_open(const rs485_port_cfg_t *c)
{
    const esp_err_t cfg_err = check_cfg(c);
    if (cfg_err != ESP_OK) {
        return cfg_err;
    }
    if (s_port.open) {
        return ESP_ERR_INVALID_STATE;
    }
    s_port.cfg = *c;
    s_port.cfg_known = true;

    const esp_err_t err = open_steps(c);
    if (err != ESP_OK) {
        rs485_port_close();
        return err;
    }
    s_port.open = true;
    return ESP_OK;
}

static bool is_error_event(uart_event_type_t type)
{
    return type == UART_BREAK || type == UART_BUFFER_FULL || type == UART_FIFO_OVF
           || type == UART_FRAME_ERR || type == UART_PARITY_ERR;
}

/* Waits for ms, counting error events from the driver queue; data bytes are counted from the ring buffer. */
static esp_err_t listen_for_activity(const rs485_port_cfg_t *c, QueueHandle_t events, uint32_t ms,
                                     uint32_t *activity)
{
    const int64_t end_us = esp_timer_get_time() + (int64_t)ms * US_PER_MS;
    uint32_t error_events = 0;
    uart_event_t event;

    for (;;) {
        const uint32_t ticks = us_to_ticks_ceil(end_us - esp_timer_get_time());
        if (ticks == 0) {
            break;
        }
        if (xQueueReceive(events, &event, ticks) == pdTRUE && is_error_event(event.type)) {
            saturating_add(&error_events, 1);
        }
    }
    size_t buffered = 0;
    const esp_err_t err = uart_get_buffered_data_len(c->uart_port, &buffered);
    if (err != ESP_OK) {
        return err;
    }
    *activity = error_events;
    saturating_add(activity, (uint32_t)buffered);
    return ESP_OK;
}

static esp_err_t silence_steps(const rs485_port_cfg_t *c, uint32_t ms, uint32_t *activity)
{
    QueueHandle_t events = NULL;
    esp_err_t err = drive_de_low(c->de_gpio);
    if (err != ESP_OK) {
        return err;
    }
    err = configure_uart_8n1(c);
    if (err != ESP_OK) {
        return err;
    }
    err = uart_driver_install(c->uart_port, RX_BUFFER_BYTES, NO_TX_BUFFER, SILENCE_EVENT_QUEUE, &events, UART_INTR_FLAGS);
    if (err != ESP_OK) {
        return err;
    }
    err = uart_set_mode(c->uart_port, UART_MODE_UART);
    if (err != ESP_OK) {
        return err;
    }
    /* Only RX is routed: TX, RTS and CTS stay unrouted, DE stays the plain GPIO at 0. */
    err = uart_set_pin(c->uart_port, UART_PIN_NO_CHANGE, c->rx_gpio, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        return err;
    }
    return listen_for_activity(c, events, ms, activity);
}

esp_err_t rs485_port_silence_check(const rs485_port_cfg_t *c, uint32_t ms, uint32_t *activity)
{
    if (activity == NULL || ms == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_port.open) {
        return ESP_ERR_INVALID_STATE;
    }
    const esp_err_t cfg_err = remember_cfg(c);
    if (cfg_err != ESP_OK) {
        return cfg_err;
    }
    *activity = 0;

    const esp_err_t err = silence_steps(c, ms, activity);
    const esp_err_t close_err = rs485_port_close();
    return (err != ESP_OK) ? err : close_err;
}

static void note_byte_interval(int64_t interval_us)
{
    if (interval_us < 0) {
        return;
    }
    const uint32_t clamped = (interval_us > UINT32_MAX) ? UINT32_MAX : (uint32_t)interval_us;
    if (clamped > s_port.counters.max_byte_interval_us) {
        s_port.counters.max_byte_interval_us = clamped;
    }
}

static void reply_add_byte(reply_t *r, uint8_t byte, int64_t now_us)
{
    if (r->total > 0) {
        note_byte_interval(now_us - r->last_byte_us);
    }
    r->last_byte_us = now_us;
    r->total++;
    if (r->stored >= r->cap) {
        return;     /* counted for the stop rule, discarded */
    }
    r->buf[r->stored++] = byte;
    r->expected = r->len_fn(r->buf, r->stored, r->len_ctx);
}

static bool reply_complete(const reply_t *r)
{
    if (r->expected == RS485_LEN_UNKNOWN) {
        return true;
    }
    return r->expected != RS485_LEN_MORE && r->total >= r->expected;
}

/* End-of-frame rule (spec 014, rule 5): before the expected length is reached only the deadline ends the
   collection, never silence. Once the length is reached (or unknown), or the deadline passed with a short
   frame, one silence window without a byte ends it; every byte restarts the window. Hard stop:
   deadline + one window. */
static esp_err_t collect_reply(int port, reply_t *r, int64_t deadline_us, int64_t window_us)
{
    const int64_t hard_stop_us = deadline_us + window_us;

    for (;;) {
        const int64_t now_us = esp_timer_get_time();
        int64_t wake_us;
        if (!reply_complete(r) && now_us < deadline_us) {
            wake_us = deadline_us;
        } else if (r->total == 0) {
            return ESP_OK;
        } else {
            wake_us = min_i64(r->last_byte_us + window_us, hard_stop_us);
        }
        if (now_us >= wake_us) {
            return ESP_OK;
        }
        uint8_t byte;
        const int got = uart_read_bytes(port, &byte, 1, us_to_ticks_ceil(wake_us - now_us));
        if (got < 0) {
            return ESP_FAIL;
        }
        if (got == 1) {
            reply_add_byte(r, byte, esp_timer_get_time());
        }
    }
}

static esp_err_t fail_and_close(esp_err_t err)
{
    saturating_add(&s_port.counters.faults, 1);
    rs485_port_close();
    return err;
}

/* Bytes that arrived since the previous transaction (late reply, foreign traffic): count, discard. */
static esp_err_t count_and_flush_gap_bytes(int port)
{
    size_t buffered = 0;
    esp_err_t err = uart_get_buffered_data_len(port, &buffered);
    if (err != ESP_OK) {
        return err;
    }
    saturating_add(&s_port.counters.gap_bytes, (uint32_t)buffered);
    return uart_flush_input(port);
}

static esp_err_t send_request(int port, const uint8_t *tx, uint32_t baud)
{
    if (uart_write_bytes(port, tx, REQUEST_LEN) != (int)REQUEST_LEN) {
        return ESP_FAIL;
    }
    const int64_t frame_us = (int64_t)REQUEST_LEN * BITS_PER_CHAR * US_PER_S / baud;
    const uint32_t ticks = us_to_ticks_ceil(frame_us + (int64_t)TX_DONE_MARGIN_MS * US_PER_MS);
    return uart_wait_tx_done(port, ticks);
}

static bool transact_args_valid(const uint8_t *tx, size_t tx_len, const uint8_t *rx, size_t rx_cap,
                                const size_t *rx_len, rs485_frame_len_fn len_fn,
                                uint32_t resp_timeout_ms, uint32_t silence_us)
{
    return tx != NULL && rx != NULL && rx_len != NULL && len_fn != NULL
           && tx_len == REQUEST_LEN && rx_cap >= 1
           && resp_timeout_ms >= 1 && resp_timeout_ms <= RESP_TIMEOUT_MAX_MS && silence_us > 0;
}

esp_err_t rs485_port_transact(const uint8_t *tx, size_t tx_len, uint8_t *rx, size_t rx_cap, size_t *rx_len,
                              rs485_frame_len_fn len_fn, void *len_ctx,
                              uint32_t resp_timeout_ms, uint32_t silence_us)
{
    if (!s_port.open) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!transact_args_valid(tx, tx_len, rx, rx_cap, rx_len, len_fn, resp_timeout_ms, silence_us)) {
        return ESP_ERR_INVALID_ARG;
    }
    *rx_len = 0;
    const int port = s_port.cfg.uart_port;

    esp_err_t err = count_and_flush_gap_bytes(port);
    if (err != ESP_OK) {
        return fail_and_close(err);
    }
    err = send_request(port, tx, s_port.cfg.baud);
    if (err != ESP_OK) {
        return fail_and_close(err);
    }

    /* A one-tick wait can expire at once, so the window is rounded up plus one tick. */
    const int64_t window_us = (int64_t)(us_to_ticks_ceil(silence_us) + 1) * TICK_US;
    const int64_t deadline_us = esp_timer_get_time() + (int64_t)resp_timeout_ms * US_PER_MS;
    reply_t reply = {
        .buf = rx, .cap = rx_cap, .expected = RS485_LEN_MORE, .len_fn = len_fn, .len_ctx = len_ctx,
    };
    err = collect_reply(port, &reply, deadline_us, window_us);
    if (err != ESP_OK) {
        return fail_and_close(err);
    }
    *rx_len = reply.stored;
    return ESP_OK;
}

void rs485_port_get_counters(rs485_port_counters_t *out)
{
    if (out != NULL) {
        *out = s_port.counters;
    }
}
