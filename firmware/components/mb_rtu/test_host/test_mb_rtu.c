/* test_mb_rtu.c - host Unity tests for mb_rtu (spec 012).
 * Fault frames carry a valid CRC (built with mb_rtu_crc16) unless the CRC is the fault. */

#include <string.h>
#include "unity.h"
#include "mb_rtu.h"

#define ADDR 1u
#define PAT_U16 0xBEEFu
#define PAT_U8  0xA5u

/* Fill frame[0..n-3] already set; append CRC low first; return total length. */
static size_t seal(uint8_t *frame, size_t body_len)
{
    uint16_t crc = mb_rtu_crc16(frame, body_len);
    frame[body_len] = (uint8_t)(crc & 0xFFu);
    frame[body_len + 1u] = (uint8_t)(crc >> 8);
    return body_len + 2u;
}

/* Normal reply frame with the given byte-count field and data bytes; returns length. */
static size_t make_reply(uint8_t *f, uint8_t addr, uint8_t fc_byte, uint8_t count, const uint16_t *v, size_t nregs)
{
    f[0] = addr;
    f[1] = fc_byte;
    f[2] = count;
    for (size_t i = 0; i < nregs; i++) {
        f[3 + 2 * i] = (uint8_t)(v[i] >> 8);
        f[4 + 2 * i] = (uint8_t)(v[i] & 0xFFu);
    }
    return seal(f, 3 + 2 * nregs);
}

static size_t make_exc(uint8_t *f, uint8_t addr, uint8_t fc_byte, uint8_t code)
{
    f[0] = addr;
    f[1] = fc_byte;
    f[2] = code;
    return seal(f, 3);
}

/* Function-code bytes are taken from a valid builder frame / reply so the test file
 * needs no function-code literals of its own except the "other function" bytes below. */
static uint8_t fc_read(void)
{
    uint8_t req[MB_RTU_REQ_LEN];
    TEST_ASSERT_EQUAL_INT(8, mb_rtu_build_fc03(ADDR, 0, 1, req));
    return req[1];
}

static uint8_t fc_exc(void) { return (uint8_t)(fc_read() | 0x80u); }

static mb_rtu_res_t parse(uint16_t qty, const uint8_t *rx, size_t len, uint16_t *val, uint8_t *exc)
{
    return mb_rtu_parse_fc03_reply(ADDR, qty, rx, len, val, exc);
}

/* Parse expecting BAD_FRAME with val/exc untouched except exc zeroed. */
static void expect_bad(uint16_t qty, const uint8_t *rx, size_t len)
{
    uint16_t val[MB_RTU_MAX_QTY];
    uint8_t exc = PAT_U8;
    for (size_t i = 0; i < MB_RTU_MAX_QTY; i++) val[i] = PAT_U16;
    TEST_ASSERT_EQUAL_INT(MB_RTU_BAD_FRAME, parse(qty, rx, len, val, &exc));
    TEST_ASSERT_EQUAL_UINT8(0, exc);
    for (size_t i = 0; i < MB_RTU_MAX_QTY; i++) TEST_ASSERT_EQUAL_HEX16(PAT_U16, val[i]);
}

/* ---- CRC ---- */

TEST_CASE("mb_rtu crc: check value, spec example, empty, residue", "[mb_rtu]")
{
    /* external: CRC-16/MODBUS check value (CRC catalogue) */
    TEST_ASSERT_EQUAL_HEX16(0x4B37, mb_rtu_crc16((const uint8_t *)"123456789", 9));
    /* external, not re-checked against the PDF: Modbus over Serial Line V1.02 example 02 07 -> 0x1241 */
    const uint8_t ex[] = { 0x02, 0x07 };
    TEST_ASSERT_EQUAL_HEX16(0x1241, mb_rtu_crc16(ex, sizeof(ex)));
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, mb_rtu_crc16(ex, 0));
    /* residue: CRC over frame + its CRC bytes (low first) is 0 */
    uint8_t f[MB_RTU_REQ_LEN];
    TEST_ASSERT_EQUAL_INT(8, mb_rtu_build_fc03(ADDR, 8, 2, f));
    TEST_ASSERT_EQUAL_HEX16(0, mb_rtu_crc16(f, sizeof(f)));
}

/* ---- builder ---- */

TEST_CASE("mb_rtu build: layout", "[mb_rtu]")
{
    uint8_t f[MB_RTU_REQ_LEN];
    TEST_ASSERT_EQUAL_INT(8, mb_rtu_build_fc03(1, 0, 1, f));
    const uint8_t head[] = { 0x01, fc_read(), 0x00, 0x00, 0x00, 0x01 };
    TEST_ASSERT_EQUAL_UINT8_ARRAY(head, f, 6);
    uint16_t crc = mb_rtu_crc16(f, 6);
    TEST_ASSERT_EQUAL_UINT8(crc & 0xFFu, f[6]);
    TEST_ASSERT_EQUAL_UINT8(crc >> 8, f[7]);

    TEST_ASSERT_EQUAL_INT(8, mb_rtu_build_fc03(1, 0x00F2, 4, f));
    const uint8_t head2[] = { 0x01, fc_read(), 0x00, 0xF2, 0x00, 0x04 };
    TEST_ASSERT_EQUAL_UINT8_ARRAY(head2, f, 6);
    TEST_ASSERT_EQUAL_HEX16(0, mb_rtu_crc16(f, 8));
}

TEST_CASE("mb_rtu build: address and qty limits", "[mb_rtu]")
{
    uint8_t f[MB_RTU_REQ_LEN];
    TEST_ASSERT_EQUAL_INT(8, mb_rtu_build_fc03(247, 0, 1, f));
    TEST_ASSERT_EQUAL_UINT8(247, f[0]);

    const uint8_t bad_addr[] = { 0, 248, 255 };
    for (size_t i = 0; i < sizeof(bad_addr); i++) {
        memset(f, PAT_U8, sizeof(f));
        TEST_ASSERT_EQUAL_INT(-1, mb_rtu_build_fc03(bad_addr[i], 0, 1, f));
        for (size_t j = 0; j < sizeof(f); j++) TEST_ASSERT_EQUAL_UINT8(PAT_U8, f[j]);
    }
    const uint16_t bad_qty[] = { 0, 5, 256, 0xFFFF };
    for (size_t i = 0; i < sizeof(bad_qty) / sizeof(bad_qty[0]); i++) {
        memset(f, PAT_U8, sizeof(f));
        TEST_ASSERT_EQUAL_INT(-1, mb_rtu_build_fc03(1, 0, bad_qty[i], f));
        for (size_t j = 0; j < sizeof(f); j++) TEST_ASSERT_EQUAL_UINT8(PAT_U8, f[j]);
    }
    TEST_ASSERT_EQUAL_INT(-1, mb_rtu_build_fc03(1, 0, 1, NULL));
}

/* ---- reply length ---- */

TEST_CASE("mb_rtu reply_len", "[mb_rtu]")
{
    uint8_t rx[3] = { 1, fc_read(), 0xFF };   /* byte count 0xFF is wrong on purpose */
    TEST_ASSERT_EQUAL_UINT(MB_RTU_LEN_MORE, mb_rtu_fc03_reply_len(1, rx, 0));
    TEST_ASSERT_EQUAL_UINT(MB_RTU_LEN_MORE, mb_rtu_fc03_reply_len(1, rx, 1));
    TEST_ASSERT_EQUAL_UINT(7, mb_rtu_fc03_reply_len(1, rx, 3));
    TEST_ASSERT_EQUAL_UINT(9, mb_rtu_fc03_reply_len(2, rx, 3));
    TEST_ASSERT_EQUAL_UINT(11, mb_rtu_fc03_reply_len(3, rx, 3));
    TEST_ASSERT_EQUAL_UINT(13, mb_rtu_fc03_reply_len(4, rx, 3));
    TEST_ASSERT_EQUAL_UINT(13, mb_rtu_fc03_reply_len(4, rx, 2));
    rx[1] = fc_exc();
    TEST_ASSERT_EQUAL_UINT(5, mb_rtu_fc03_reply_len(4, rx, 2));
    const uint8_t other[] = { 0x04, 0x06, 0x90 };
    for (size_t i = 0; i < sizeof(other); i++) {
        rx[1] = other[i];
        TEST_ASSERT_EQUAL_UINT(MB_RTU_LEN_UNKNOWN, mb_rtu_fc03_reply_len(1, rx, 2));
    }
}

/* ---- classifier: OK ---- */

TEST_CASE("mb_rtu parse: OK for qty 1..4, host order", "[mb_rtu]")
{
    const uint16_t v[MB_RTU_MAX_QTY] = { 0x0000, 0xFFFF, 0x8000, 0x1234 };
    for (uint16_t qty = 1; qty <= MB_RTU_MAX_QTY; qty++) {
        uint8_t f[MB_RTU_MAX_REPLY_LEN];
        size_t len = make_reply(f, ADDR, fc_read(), (uint8_t)(2 * qty), v, qty);
        TEST_ASSERT_EQUAL_UINT(5u + 2u * qty, len);
        uint16_t out[MB_RTU_MAX_QTY];
        for (size_t i = 0; i < MB_RTU_MAX_QTY; i++) out[i] = PAT_U16;
        uint8_t exc = PAT_U8;
        TEST_ASSERT_EQUAL_INT(MB_RTU_OK, parse(qty, f, len, out, &exc));
        TEST_ASSERT_EQUAL_UINT8(0, exc);
        for (size_t i = 0; i < qty; i++) TEST_ASSERT_EQUAL_HEX16(v[i], out[i]);
        for (size_t i = qty; i < MB_RTU_MAX_QTY; i++) TEST_ASSERT_EQUAL_HEX16(PAT_U16, out[i]);
    }
}

TEST_CASE("mb_rtu parse: OK with exc_code NULL", "[mb_rtu]")
{
    const uint16_t v[1] = { 0x0102 };
    uint8_t f[MB_RTU_MAX_REPLY_LEN];
    size_t len = make_reply(f, ADDR, fc_read(), 2, v, 1);
    uint16_t out = 0;
    TEST_ASSERT_EQUAL_INT(MB_RTU_OK, parse(1, f, len, &out, NULL));
    TEST_ASSERT_EQUAL_HEX16(0x0102, out);
}

/* ---- classifier: EXCEPTION ---- */

TEST_CASE("mb_rtu parse: EXCEPTION codes 1..4 and others", "[mb_rtu]")
{
    const uint8_t codes[] = { 1, 2, 3, 4, 11, 255 };
    for (size_t i = 0; i < sizeof(codes); i++) {
        uint8_t f[MB_RTU_MAX_REPLY_LEN];
        size_t len = make_exc(f, ADDR, fc_exc(), codes[i]);
        uint16_t out[MB_RTU_MAX_QTY];
        for (size_t j = 0; j < MB_RTU_MAX_QTY; j++) out[j] = PAT_U16;
        uint8_t exc = PAT_U8;
        TEST_ASSERT_EQUAL_INT(MB_RTU_EXCEPTION, parse(2, f, len, out, &exc));
        TEST_ASSERT_EQUAL_UINT8(codes[i], exc);
        for (size_t j = 0; j < MB_RTU_MAX_QTY; j++) TEST_ASSERT_EQUAL_HEX16(PAT_U16, out[j]);
    }
}

/* ---- classifier: BAD_FRAME ---- */

TEST_CASE("mb_rtu parse: bad CRC in normal frame", "[mb_rtu]")
{
    const uint16_t v[1] = { 0x0102 };
    uint8_t f[MB_RTU_MAX_REPLY_LEN];
    size_t len = make_reply(f, ADDR, fc_read(), 2, v, 1);
    f[3] ^= 0x01u;                       /* data bit flipped, CRC now wrong */
    expect_bad(1, f, len);
    f[3] ^= 0x01u;
    f[len - 1u] ^= 0x01u;                /* CRC high byte flipped */
    expect_bad(1, f, len);
    f[len - 1u] ^= 0x01u;
    f[len - 2u] ^= 0x01u;                /* CRC low byte flipped */
    expect_bad(1, f, len);
}

TEST_CASE("mb_rtu parse: bad CRC in exception frame", "[mb_rtu]")
{
    uint8_t f[MB_RTU_MAX_REPLY_LEN];
    size_t len = make_exc(f, ADDR, fc_exc(), 2);
    f[2] ^= 0x01u;
    expect_bad(1, f, len);
    f[2] ^= 0x01u;
    f[len - 1u] ^= 0x01u;
    expect_bad(1, f, len);
    f[len - 1u] ^= 0x01u;
    f[len - 2u] ^= 0x01u;
    expect_bad(1, f, len);
}

TEST_CASE("mb_rtu parse: wrong address", "[mb_rtu]")
{
    const uint16_t v[1] = { 0x0102 };
    uint8_t f[MB_RTU_MAX_REPLY_LEN];
    size_t len = make_reply(f, ADDR + 1u, fc_read(), 2, v, 1);
    expect_bad(1, f, len);
    len = make_exc(f, ADDR + 1u, fc_exc(), 2);
    expect_bad(1, f, len);
}

TEST_CASE("mb_rtu parse: wrong function code", "[mb_rtu]")
{
    const uint8_t other[] = { 0x04, 0x06, 0x84 };
    const uint16_t v[1] = { 0x0102 };
    for (size_t i = 0; i < sizeof(other); i++) {
        uint8_t f[MB_RTU_MAX_REPLY_LEN];
        size_t len = make_reply(f, ADDR, other[i], 2, v, 1);
        expect_bad(1, f, len);
    }
}

TEST_CASE("mb_rtu parse: byte count mismatch", "[mb_rtu]")
{
    const uint16_t v[2] = { 0x0102, 0x0304 };
    uint8_t f[MB_RTU_MAX_REPLY_LEN];
    size_t len = make_reply(f, ADDR, fc_read(), 4, v, 2);   /* internally consistent, qty 1 requested */
    expect_bad(1, f, len);
    /* byte count says 2 but data holds 4 bytes: length and count disagree */
    len = make_reply(f, ADDR, fc_read(), 2, v, 2);
    expect_bad(2, f, len);
}

TEST_CASE("mb_rtu parse: short frames", "[mb_rtu]")
{
    const uint16_t v[2] = { 0x0102, 0x0304 };
    uint8_t f[MB_RTU_MAX_REPLY_LEN];
    size_t len = make_reply(f, ADDR, fc_read(), 2, v, 1);
    for (size_t n = 1; n <= 4; n++) expect_bad(1, f, n);
    len = make_reply(f, ADDR, fc_read(), 4, v, 2);
    expect_bad(2, f, len - 1u);          /* qty-2 reply missing its last byte */
    len = make_exc(f, ADDR, fc_exc(), 2);
    expect_bad(1, f, len - 1u);
}

TEST_CASE("mb_rtu parse: trailing bytes", "[mb_rtu]")
{
    const uint16_t v[1] = { 0x0102 };
    uint8_t f[MB_RTU_MAX_REPLY_LEN + 1];
    size_t len = make_reply(f, ADDR, fc_read(), 2, v, 1);
    f[len] = 0x00;
    expect_bad(1, f, len + 1u);
    len = make_exc(f, ADDR, fc_exc(), 2);
    f[len] = 0x00;
    expect_bad(1, f, len + 1u);
}

TEST_CASE("mb_rtu parse: exception code 0", "[mb_rtu]")
{
    uint8_t f[MB_RTU_MAX_REPLY_LEN];
    size_t len = make_exc(f, ADDR, fc_exc(), 0);
    expect_bad(1, f, len);
}

TEST_CASE("mb_rtu parse: caller errors", "[mb_rtu]")
{
    const uint16_t v[1] = { 0x0102 };
    uint8_t f[MB_RTU_MAX_REPLY_LEN];
    size_t len = make_reply(f, ADDR, fc_read(), 2, v, 1);
    uint16_t out = PAT_U16;
    uint8_t exc = PAT_U8;
    TEST_ASSERT_EQUAL_INT(MB_RTU_BAD_FRAME, mb_rtu_parse_fc03_reply(ADDR, 1, NULL, len, &out, &exc));
    TEST_ASSERT_EQUAL_INT(MB_RTU_BAD_FRAME, mb_rtu_parse_fc03_reply(0, 1, f, len, &out, &exc));
    TEST_ASSERT_EQUAL_INT(MB_RTU_BAD_FRAME, mb_rtu_parse_fc03_reply(248, 1, f, len, &out, &exc));
    TEST_ASSERT_EQUAL_INT(MB_RTU_BAD_FRAME, mb_rtu_parse_fc03_reply(ADDR, 0, f, len, &out, &exc));
    TEST_ASSERT_EQUAL_INT(MB_RTU_BAD_FRAME, mb_rtu_parse_fc03_reply(ADDR, 5, f, len, &out, &exc));
    TEST_ASSERT_EQUAL_HEX16(PAT_U16, out);
    TEST_ASSERT_EQUAL_UINT8(0, exc);
    /* OK-shaped frame but no value buffer: never reported OK */
    TEST_ASSERT_EQUAL_INT(MB_RTU_BAD_FRAME, mb_rtu_parse_fc03_reply(ADDR, 1, f, len, NULL, &exc));
}

/* ---- TIMEOUT ---- */

TEST_CASE("mb_rtu parse: TIMEOUT on empty reception", "[mb_rtu]")
{
    uint8_t f[1] = { 0 };
    uint16_t out = PAT_U16;
    uint8_t exc = PAT_U8;
    TEST_ASSERT_EQUAL_INT(MB_RTU_TIMEOUT, parse(1, f, 0, &out, &exc));
    TEST_ASSERT_EQUAL_UINT8(0, exc);
    TEST_ASSERT_EQUAL_HEX16(PAT_U16, out);
    TEST_ASSERT_EQUAL_INT(MB_RTU_TIMEOUT, parse(1, NULL, 0, &out, &exc));
}

/* ---- t3.5 ---- */

TEST_CASE("mb_rtu t35", "[mb_rtu]")
{
    /* external: Modbus over Serial Line, 3.5 chars of 11 bits; fixed 1750 us above 19200 baud */
    TEST_ASSERT_EQUAL_UINT32(4011, mb_rtu_t35_us(9600));
    TEST_ASSERT_EQUAL_UINT32(2006, mb_rtu_t35_us(19200));
    TEST_ASSERT_EQUAL_UINT32(1750, mb_rtu_t35_us(19201));
    TEST_ASSERT_EQUAL_UINT32(32084, mb_rtu_t35_us(1200));
    TEST_ASSERT_EQUAL_UINT32(1750, mb_rtu_t35_us(38400));
    TEST_ASSERT_EQUAL_UINT32(1750, mb_rtu_t35_us(115200));
    TEST_ASSERT_EQUAL_UINT32(0, mb_rtu_t35_us(0));
}
