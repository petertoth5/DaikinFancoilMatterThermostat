/* mb_link_write.c - FC06 write path over rs485_port and mb_rtu_write (spec 019b). Same shape as the read
 * path in mb_link.c: guards, frame build, one transaction, classification. No retry. */
#include "mb_link_write.h"

#include <stdbool.h>
#include <stddef.h>

#include "daikin_regs.h"
#include "mb_rtu.h"
#include "mb_rtu_write.h"
#include "rs485_port.h"

_Static_assert(RS485_LEN_MORE == MB_RTU_LEN_MORE && RS485_LEN_UNKNOWN == MB_RTU_LEN_UNKNOWN,
               "port and codec use the same length sentinels");

#define SLAVE_ADDR_MIN    1u      /* 0 is broadcast (D21) */
#define SLAVE_ADDR_MAX    247u    /* Modbus serial-line limit (D19, D21) */

static struct {
    bool configured;
    uint32_t resp_timeout_ms;
    uint32_t baud;
} s_params;

esp_err_t mb_link_write_configure(const mb_link_config_t *cfg)
{
    if (cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    s_params.resp_timeout_ms = cfg->resp_timeout_ms;
    s_params.baud = cfg->port.baud;
    s_params.configured = true;
    return ESP_OK;
}

/* Whitelist (D9): only registers with a write mask, and only values inside their fixed range. */
static bool write_allowed(uint16_t reg, uint16_t value)
{
    const dr_reg_t *r = daikin_regs_find(reg);
    return r != NULL && daikin_regs_write_mask(reg) != 0u && daikin_regs_in_range(r, value);
}

/* len_fn for the port: an unknown length is "more bytes" (only the deadline ends the frame). */
static size_t reply_len_adapter(const uint8_t *rx, size_t n, void *ctx)
{
    (void)ctx;
    const size_t len = mb_rtu_fc06_reply_len(rx, n);
    return len == MB_RTU_LEN_UNKNOWN ? RS485_LEN_MORE : len;
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

mb_link_res_t mb_link_write(uint8_t addr, uint16_t reg, uint16_t value, uint8_t *exc_code)
{
    if (exc_code == NULL) {
        return MBL_ERROR;
    }
    *exc_code = 0;
    if (addr < SLAVE_ADDR_MIN || addr > SLAVE_ADDR_MAX || !write_allowed(reg, value)) {
        return MBL_REFUSED;
    }
    if (!s_params.configured) {
        return MBL_ERROR;
    }

    uint8_t req[MB_RTU_REQ_LEN];
    if (mb_rtu_build_fc06(addr, reg, value, req) != (int)MB_RTU_REQ_LEN) {
        return MBL_REFUSED;     /* unreachable: the guards above cover every build rejection */
    }

    /* One byte more than the echo, so trailing bytes reach the classifier. */
    uint8_t rx[MB_RTU_REQ_LEN + 1];
    size_t rx_len = 0;
    const esp_err_t err = rs485_port_transact(req, MB_RTU_REQ_LEN, rx, sizeof rx, &rx_len, reply_len_adapter,
                                              NULL, s_params.resp_timeout_ms, mb_rtu_t35_us(s_params.baud));
    if (err != ESP_OK) {
        rs485_port_close();     /* a closed port makes the next mb_link_read fail and close the link state */
        return MBL_ERROR;
    }
    return to_link_result(mb_rtu_parse_fc06_reply(req, rx, rx_len, exc_code));
}
