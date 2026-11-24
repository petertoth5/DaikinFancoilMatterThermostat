/* mb_rtu.c - pure Modbus RTU FC03 frame codec (spec 012). */

#include "mb_rtu.h"

/* The only function-code bytes in the component (Modbus Application Protocol, external). */
#define MB_RTU_FC_READ_HOLDING      0x03u
#define MB_RTU_FC_READ_HOLDING_EXC  0x83u

#define CRC_INIT        0xFFFFu
#define CRC_POLY        0xA001u   /* reflected 0x8005 */
#define BITS_PER_BYTE   8u
#define BYTE_MASK       0xFFu

#define ADDR_MIN        1u        /* wiki firmware-architecture D21: addresses 1..247 */
#define ADDR_MAX        247u

#define HDR_LEN         3u        /* addr, fc, byte count / exception code */
#define CRC_LEN         2u
#define EXC_REPLY_LEN   (HDR_LEN + CRC_LEN)
#define BYTES_PER_REG   2u
#define IDX_ADDR        0u
#define IDX_FC          1u
#define IDX_COUNT       2u        /* byte count (normal) or exception code */
#define IDX_DATA        3u

/* t3.5 (Modbus over Serial Line, external): 3.5 characters of 11 bits up to 19200 baud, fixed above. */
#define T35_BASE_BAUD_MAX  19200u
#define T35_FIXED_US       1750u
#define T35_NUM_US         (35u * 11u * 100000u)   /* 3.5 * 11 * 1e6 */

uint16_t mb_rtu_crc16(const uint8_t *buf, size_t len)
{
    uint16_t crc = CRC_INIT;
    for (size_t i = 0; i < len; i++) {
        crc ^= buf[i];
        for (unsigned bit = 0; bit < BITS_PER_BYTE; bit++) {
            crc = (crc & 1u) ? (uint16_t)((crc >> 1) ^ CRC_POLY) : (uint16_t)(crc >> 1);
        }
    }
    return crc;
}

static int params_valid(uint8_t addr, uint16_t qty)
{
    return addr >= ADDR_MIN && addr <= ADDR_MAX && qty >= 1u && qty <= MB_RTU_MAX_QTY;
}

int mb_rtu_build_fc03(uint8_t addr, uint16_t reg, uint16_t qty, uint8_t out[MB_RTU_REQ_LEN])
{
    if (out == NULL || !params_valid(addr, qty)) {
        return -1;
    }
    uint8_t frame[MB_RTU_REQ_LEN];
    frame[0] = addr;
    frame[1] = MB_RTU_FC_READ_HOLDING;
    frame[2] = (uint8_t)(reg >> BITS_PER_BYTE);
    frame[3] = (uint8_t)(reg & BYTE_MASK);
    frame[4] = (uint8_t)(qty >> BITS_PER_BYTE);
    frame[5] = (uint8_t)(qty & BYTE_MASK);
    uint16_t crc = mb_rtu_crc16(frame, MB_RTU_REQ_LEN - CRC_LEN);
    frame[6] = (uint8_t)(crc & BYTE_MASK);
    frame[7] = (uint8_t)(crc >> BITS_PER_BYTE);
    for (size_t i = 0; i < MB_RTU_REQ_LEN; i++) {
        out[i] = frame[i];
    }
    return (int)MB_RTU_REQ_LEN;
}

size_t mb_rtu_fc03_reply_len(uint16_t qty, const uint8_t *rx, size_t n)
{
    if (rx == NULL || n < 2u) {
        return MB_RTU_LEN_MORE;
    }
    if (rx[IDX_FC] == MB_RTU_FC_READ_HOLDING) {
        return HDR_LEN + CRC_LEN + BYTES_PER_REG * (size_t)qty;
    }
    if (rx[IDX_FC] == MB_RTU_FC_READ_HOLDING_EXC) {
        return EXC_REPLY_LEN;
    }
    return MB_RTU_LEN_UNKNOWN;
}

/* True when the last two bytes of frame[0..len) are the CRC (low first) of the bytes before them. */
static int crc_matches(const uint8_t *frame, size_t len)
{
    uint16_t got = (uint16_t)(frame[len - 2u] | (frame[len - 1u] << BITS_PER_BYTE));
    return mb_rtu_crc16(frame, len - CRC_LEN) == got;
}

static mb_rtu_res_t classify_exception(const uint8_t *rx, size_t len, uint8_t *exc_code)
{
    if (len != EXC_REPLY_LEN || !crc_matches(rx, len)) {
        return MB_RTU_BAD_FRAME;
    }
    if (rx[IDX_COUNT] == 0u) {   /* 0 is not an exception code (user decision 2026-11-24) */
        return MB_RTU_BAD_FRAME;
    }
    if (exc_code != NULL) {
        *exc_code = rx[IDX_COUNT];
    }
    return MB_RTU_EXCEPTION;
}

static mb_rtu_res_t classify_values(uint16_t qty, const uint8_t *rx, size_t len, uint16_t *val)
{
    size_t data_len = BYTES_PER_REG * (size_t)qty;
    if (rx[IDX_COUNT] != data_len || len != HDR_LEN + CRC_LEN + data_len) {
        return MB_RTU_BAD_FRAME;
    }
    if (!crc_matches(rx, len)) {
        return MB_RTU_BAD_FRAME;
    }
    for (size_t i = 0; i < qty; i++) {
        val[i] = (uint16_t)((rx[IDX_DATA + BYTES_PER_REG * i] << BITS_PER_BYTE) |
                            rx[IDX_DATA + BYTES_PER_REG * i + 1u]);
    }
    return MB_RTU_OK;
}

mb_rtu_res_t mb_rtu_parse_fc03_reply(uint8_t addr, uint16_t qty, const uint8_t *rx, size_t len,
                                     uint16_t *val, uint8_t *exc_code)
{
    if (exc_code != NULL) {
        *exc_code = 0;
    }
    if (len == 0u) {
        return MB_RTU_TIMEOUT;
    }
    if (rx == NULL || val == NULL || !params_valid(addr, qty)) {
        return MB_RTU_BAD_FRAME;
    }
    if (len < EXC_REPLY_LEN || rx[IDX_ADDR] != addr) {
        return MB_RTU_BAD_FRAME;
    }
    if (rx[IDX_FC] == MB_RTU_FC_READ_HOLDING_EXC) {
        return classify_exception(rx, len, exc_code);
    }
    if (rx[IDX_FC] == MB_RTU_FC_READ_HOLDING) {
        return classify_values(qty, rx, len, val);
    }
    return MB_RTU_BAD_FRAME;
}

uint32_t mb_rtu_t35_us(uint32_t baud)
{
    if (baud == 0u) {
        return 0u;
    }
    if (baud > T35_BASE_BAUD_MAX) {
        return T35_FIXED_US;
    }
    return (T35_NUM_US + baud - 1u) / baud;   /* ceil */
}
