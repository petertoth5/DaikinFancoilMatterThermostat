/* mb_rtu_write.c - pure Modbus RTU FC06 frame codec (spec 019a). */

#include "mb_rtu_write.h"
#include <string.h>

/* The only write function-code bytes in the project (Modbus Application Protocol, external). */
#define MB_RTU_FC_WRITE_SINGLE      0x06u
#define MB_RTU_FC_WRITE_SINGLE_EXC  0x86u

#define BITS_PER_BYTE   8u
#define BYTE_MASK       0xFFu

#define ADDR_MIN        1u        /* wiki firmware-architecture D21: addresses 1..247 */
#define ADDR_MAX        247u

#define CRC_LEN         2u
#define ECHO_LEN        MB_RTU_REQ_LEN   /* the FC06 reply repeats the 8-byte request */
#define EXC_LEN         5u               /* addr, fc, code, crc lo/hi */
#define IDX_ADDR        0u
#define IDX_FC          1u
#define IDX_EXC_CODE    2u

static void put_crc(uint8_t *frame, size_t body_len)
{
    uint16_t crc = mb_rtu_crc16(frame, body_len);
    frame[body_len] = (uint8_t)(crc & BYTE_MASK);
    frame[body_len + 1u] = (uint8_t)(crc >> BITS_PER_BYTE);
}

static int crc_matches(const uint8_t *frame, size_t len)
{
    uint16_t got = (uint16_t)(frame[len - 2u] | (frame[len - 1u] << BITS_PER_BYTE));
    return mb_rtu_crc16(frame, len - CRC_LEN) == got;
}

int mb_rtu_build_fc06(uint8_t addr, uint16_t reg, uint16_t value, uint8_t out[MB_RTU_REQ_LEN])
{
    if (out == NULL || addr < ADDR_MIN || addr > ADDR_MAX) {
        return -1;
    }
    out[IDX_ADDR] = addr;
    out[IDX_FC] = MB_RTU_FC_WRITE_SINGLE;
    out[2] = (uint8_t)(reg >> BITS_PER_BYTE);
    out[3] = (uint8_t)(reg & BYTE_MASK);
    out[4] = (uint8_t)(value >> BITS_PER_BYTE);
    out[5] = (uint8_t)(value & BYTE_MASK);
    put_crc(out, MB_RTU_REQ_LEN - CRC_LEN);
    return (int)MB_RTU_REQ_LEN;
}

size_t mb_rtu_fc06_reply_len(const uint8_t *rx, size_t n)
{
    if (rx == NULL || n < 2u) {
        return MB_RTU_LEN_MORE;
    }
    if (rx[IDX_FC] == MB_RTU_FC_WRITE_SINGLE) {
        return ECHO_LEN;
    }
    if (rx[IDX_FC] == MB_RTU_FC_WRITE_SINGLE_EXC) {
        return EXC_LEN;
    }
    return MB_RTU_LEN_UNKNOWN;
}

static int is_echo_of(const uint8_t req[MB_RTU_REQ_LEN], const uint8_t *rx, size_t len)
{
    return len == ECHO_LEN && memcmp(rx, req, ECHO_LEN) == 0 && crc_matches(rx, len);
}

static int is_exception_for(const uint8_t req[MB_RTU_REQ_LEN], const uint8_t *rx, size_t len)
{
    return len == EXC_LEN && rx[IDX_ADDR] == req[IDX_ADDR] && rx[IDX_FC] == MB_RTU_FC_WRITE_SINGLE_EXC &&
           rx[IDX_EXC_CODE] != 0u && crc_matches(rx, len);   /* code 0 is no exception (as for FC03) */
}

mb_rtu_res_t mb_rtu_parse_fc06_reply(const uint8_t req[MB_RTU_REQ_LEN], const uint8_t *rx, size_t len,
                                     uint8_t *exc_code)
{
    if (exc_code != NULL) {
        *exc_code = 0;
    }
    if (len == 0u) {
        return MB_RTU_TIMEOUT;
    }
    if (req == NULL || rx == NULL || req[IDX_FC] != MB_RTU_FC_WRITE_SINGLE) {
        return MB_RTU_BAD_FRAME;
    }
    if (is_echo_of(req, rx, len)) {
        return MB_RTU_OK;
    }
    if (is_exception_for(req, rx, len)) {
        if (exc_code != NULL) {
            *exc_code = rx[IDX_EXC_CODE];
        }
        return MB_RTU_EXCEPTION;
    }
    return MB_RTU_BAD_FRAME;
}
