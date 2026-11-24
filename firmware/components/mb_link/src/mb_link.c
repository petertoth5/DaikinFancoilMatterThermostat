/* mb_link.c - read path over rs485_port and mb_rtu (spec 014). Pure link logic: guards, frame build,
 * transaction, classification. No driver, FreeRTOS or logging calls. */
#include "mb_link.h"

#include <stddef.h>
#include <stdint.h>

#include "daikin_regs.h"
#include "esp_err.h"
#include "mb_rtu.h"
#include "rs485_port.h"

_Static_assert(RS485_LEN_MORE == MB_RTU_LEN_MORE && RS485_LEN_UNKNOWN == MB_RTU_LEN_UNKNOWN,
               "port and codec use the same length sentinels");

#define SLAVE_ADDR_MIN         1u      /* 0 is broadcast (D21) */
#define SLAVE_ADDR_MAX         247u    /* Modbus serial-line limit (D19, D21) */
#define RESP_TIMEOUT_MIN_MS    200u
#define RESP_TIMEOUT_MAX_MS    2000u
#define BAUD_MIN               1200u
#define BAUD_MAX               115200u

typedef struct {
    bool initialised;
    uint32_t resp_timeout_ms;
    uint32_t baud;
} link_state_t;

static link_state_t s_link;

static rs485_port_cfg_t to_port_cfg(const mb_link_port_cfg_t *p)
{
    const rs485_port_cfg_t cfg = {
        .uart_port = p->uart_port, .tx_gpio = p->tx_gpio, .rx_gpio = p->rx_gpio,
        .de_gpio = p->de_gpio, .baud = p->baud,
    };
    return cfg;
}

esp_err_t mb_link_hold_receive(const mb_link_config_t *cfg)
{
    if (cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    const rs485_port_cfg_t port_cfg = to_port_cfg(&cfg->port);
    return rs485_port_hold_receive(&port_cfg);
}

esp_err_t mb_link_silence_check(const mb_link_config_t *cfg, uint32_t ms, uint32_t *activity)
{
    if (cfg == NULL || activity == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_link.initialised) {
        return ESP_ERR_INVALID_STATE;
    }
    const rs485_port_cfg_t port_cfg = to_port_cfg(&cfg->port);
    return rs485_port_silence_check(&port_cfg, ms, activity);
}

esp_err_t mb_link_init(const mb_link_config_t *cfg)
{
    if (cfg == NULL || cfg->resp_timeout_ms < RESP_TIMEOUT_MIN_MS || cfg->resp_timeout_ms > RESP_TIMEOUT_MAX_MS
        || cfg->port.baud < BAUD_MIN || cfg->port.baud > BAUD_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_link.initialised) {
        return ESP_ERR_INVALID_STATE;
    }
    const rs485_port_cfg_t port_cfg = to_port_cfg(&cfg->port);
    const esp_err_t err = rs485_port_open(&port_cfg);
    if (err != ESP_OK) {
        return err;
    }
    s_link.initialised = true;
    s_link.resp_timeout_ms = cfg->resp_timeout_ms;
    s_link.baud = cfg->port.baud;
    return ESP_OK;
}

esp_err_t mb_link_deinit(void)
{
    if (!s_link.initialised) {
        return ESP_OK;
    }
    s_link.initialised = false;
    return rs485_port_close();
}

/* len_fn for the port: ctx points to the request's qty, validated by mb_link_read. An unknown or
   out-of-range length is "more bytes" (only the deadline ends the frame), never a clamped length. */
static size_t reply_len_adapter(const uint8_t *rx, size_t n, void *ctx)
{
    const uint16_t qty = *(const uint16_t *)ctx;
    const size_t len = mb_rtu_fc03_reply_len(qty, rx, n);

    if (len == MB_RTU_LEN_UNKNOWN || len > MB_RTU_MAX_REPLY_LEN) {
        return RS485_LEN_MORE;
    }
    return len;
}

static mb_link_res_t to_link_result(mb_rtu_res_t res)
{
    switch (res) {
    case MB_RTU_OK:
        return MBL_OK;
    case MB_RTU_EXCEPTION:
        return MBL_EXCEPTION;
    case MB_RTU_BAD_FRAME:
        return MBL_BAD_FRAME;
    case MB_RTU_TIMEOUT:
        return MBL_TIMEOUT;
    }
    return MBL_ERROR;
}

mb_link_res_t mb_link_read(uint8_t addr, uint16_t reg, uint16_t qty, uint16_t *val, uint8_t *exc_code)
{
    if (exc_code != NULL) {
        *exc_code = 0;
    }
    if (val == NULL || exc_code == NULL) {
        return MBL_ERROR;
    }
    if (addr < SLAVE_ADDR_MIN || addr > SLAVE_ADDR_MAX || !daikin_regs_read_allowed(reg, qty)) {
        return MBL_REFUSED;
    }
    if (!s_link.initialised) {
        return MBL_ERROR;
    }

    uint8_t req[MB_RTU_REQ_LEN];
    if (mb_rtu_build_fc03(addr, reg, qty, req) != (int)MB_RTU_REQ_LEN) {
        return MBL_REFUSED;     /* unreachable: the guards above cover every build rejection */
    }

    /* One byte more than the longest valid reply, so trailing bytes reach the classifier. */
    uint8_t rx[MB_RTU_MAX_REPLY_LEN + 1];
    size_t rx_len = 0;
    uint16_t reply_qty = qty;
    const esp_err_t err = rs485_port_transact(req, MB_RTU_REQ_LEN, rx, sizeof rx, &rx_len,
                                              reply_len_adapter, &reply_qty,
                                              s_link.resp_timeout_ms, mb_rtu_t35_us(s_link.baud));
    if (err != ESP_OK) {
        s_link.initialised = false;     /* the port has closed itself: no re-open without mb_link_init */
        rs485_port_close();
        return MBL_ERROR;
    }
    return to_link_result(mb_rtu_parse_fc03_reply(addr, qty, rx, rx_len, val, exc_code));
}

void mb_link_get_counters(mb_link_counters_t *out)
{
    if (out == NULL) {
        return;
    }
    rs485_port_counters_t port;
    rs485_port_get_counters(&port);
    out->measured = true;
    out->gap_bytes = port.gap_bytes;
    out->faults = port.faults;
    out->max_byte_interval_us = port.max_byte_interval_us;
}
