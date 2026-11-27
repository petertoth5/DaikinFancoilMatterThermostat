/* test_mb_rtu_write.c - host Unity tests for the FC06 codec (spec 019a).
 * Frame vectors: Modbus Application Protocol (external) and CRCs computed independently with a
 * stand-alone bitwise CRC-16/MODBUS script (python3, poly 0xA001, init 0xFFFF), not with the C code.
 * Fault frames carry a valid CRC (mb_rtu_crc16) unless the CRC is the fault. */

#include <string.h>
#include "unity.h"
#include "mb_rtu_write.h"

#define ADDR 1u
#define PAT_U8 0xA5u
#define ECHO_LEN 8u
#define EXC_LEN  5u
#define EXC_ILLEGAL_VALUE 3u

/* Function-code bytes come from a built frame so the test needs no literal beyond the vectors. */
static uint8_t fc_write(void)
{
    uint8_t req[MB_RTU_REQ_LEN];
    TEST_ASSERT_EQUAL_INT(8, mb_rtu_build_fc06(ADDR, 1, 3, req));
    return req[1];
}

static uint8_t fc_write_exc(void) { return (uint8_t)(fc_write() | 0x80u); }

static size_t seal(uint8_t *frame, size_t body_len)
{
    uint16_t crc = mb_rtu_crc16(frame, body_len);
    frame[body_len] = (uint8_t)(crc & 0xFFu);
    frame[body_len + 1u] = (uint8_t)(crc >> 8);
    return body_len + 2u;
}

static size_t make_exc(uint8_t *f, uint8_t addr, uint8_t fc_byte, uint8_t code)
{
    f[0] = addr;
    f[1] = fc_byte;
    f[2] = code;
    return seal(f, 3);
}

static void build_req(uint8_t req[MB_RTU_REQ_LEN])
{
    TEST_ASSERT_EQUAL_INT(8, mb_rtu_build_fc06(ADDR, 231, 210, req));
}

static void expect_bad(const uint8_t req[MB_RTU_REQ_LEN], const uint8_t *rx, size_t len)
{
    uint8_t exc = PAT_U8;
    TEST_ASSERT_EQUAL_INT(MB_RTU_BAD_FRAME, mb_rtu_parse_fc06_reply(req, rx, len, &exc));
    TEST_ASSERT_EQUAL_UINT8(0, exc);
}

/* ---- builder ---- */

TEST_CASE("mb_rtu_write build: independent CRC vectors", "[mb_rtu_write]")
{
    uint8_t f[MB_RTU_REQ_LEN];
    /* Modbus spec style example: slave 1, register 1, value 3 -> CRC 98 0B (python3 reference CRC) */
    TEST_ASSERT_EQUAL_INT(8, mb_rtu_build_fc06(1, 1, 3, f));
    const uint8_t v1[] = { 0x01, 0x06, 0x00, 0x01, 0x00, 0x03, 0x98, 0x0B };
    TEST_ASSERT_EQUAL_UINT8_ARRAY(v1, f, 8);
    /* setpoint 21.0 C into register 231 (0x00E7) -> CRC B9 A0 */
    TEST_ASSERT_EQUAL_INT(8, mb_rtu_build_fc06(1, 231, 210, f));
    const uint8_t v2[] = { 0x01, 0x06, 0x00, 0xE7, 0x00, 0xD2, 0xB9, 0xA0 };
    TEST_ASSERT_EQUAL_UINT8_ARRAY(v2, f, 8);
    /* highest address, register 201, value 0x0190 -> CRC 4C 9E */
    TEST_ASSERT_EQUAL_INT(8, mb_rtu_build_fc06(247, 201, 0x0190, f));
    const uint8_t v3[] = { 0xF7, 0x06, 0x00, 0xC9, 0x01, 0x90, 0x4C, 0x9E };
    TEST_ASSERT_EQUAL_UINT8_ARRAY(v3, f, 8);
    TEST_ASSERT_EQUAL_HEX16(0, mb_rtu_crc16(f, 8));   /* CRC residue */
}

TEST_CASE("mb_rtu_write build: address limits and NULL", "[mb_rtu_write]")
{
    uint8_t f[MB_RTU_REQ_LEN];
    const uint8_t bad_addr[] = { 0, 248, 255 };   /* 0 is the broadcast address: never written */
    for (size_t i = 0; i < sizeof(bad_addr); i++) {
        memset(f, PAT_U8, sizeof(f));
        TEST_ASSERT_EQUAL_INT(-1, mb_rtu_build_fc06(bad_addr[i], 231, 210, f));
        for (size_t j = 0; j < sizeof(f); j++) {
            TEST_ASSERT_EQUAL_UINT8(PAT_U8, f[j]);
        }
    }
    TEST_ASSERT_EQUAL_INT(8, mb_rtu_build_fc06(1, 0, 0, f));
    TEST_ASSERT_EQUAL_INT(8, mb_rtu_build_fc06(247, 0xFFFF, 0xFFFF, f));
    TEST_ASSERT_EQUAL_INT(-1, mb_rtu_build_fc06(1, 231, 210, NULL));
}

/* ---- reply length ---- */

TEST_CASE("mb_rtu_write reply_len: echo, exception, unknown", "[mb_rtu_write]")
{
    uint8_t rx[2] = { 1, 0 };
    TEST_ASSERT_EQUAL_UINT(MB_RTU_LEN_MORE, mb_rtu_fc06_reply_len(rx, 0));
    TEST_ASSERT_EQUAL_UINT(MB_RTU_LEN_MORE, mb_rtu_fc06_reply_len(rx, 1));
    TEST_ASSERT_EQUAL_UINT(MB_RTU_LEN_MORE, mb_rtu_fc06_reply_len(NULL, 2));
    rx[1] = fc_write();
    TEST_ASSERT_EQUAL_UINT(ECHO_LEN, mb_rtu_fc06_reply_len(rx, 2));
    rx[1] = fc_write_exc();
    TEST_ASSERT_EQUAL_UINT(EXC_LEN, mb_rtu_fc06_reply_len(rx, 2));
    const uint8_t other[] = { 0x00, 0x03, 0x83, 0x05, 0x10, 0x87 };   /* read, read exc, others */
    for (size_t i = 0; i < sizeof(other); i++) {
        rx[1] = other[i];
        TEST_ASSERT_EQUAL_UINT(MB_RTU_LEN_UNKNOWN, mb_rtu_fc06_reply_len(rx, 2));
    }
}

/* ---- parse ---- */

TEST_CASE("mb_rtu_write parse: echo is OK", "[mb_rtu_write]")
{
    uint8_t req[MB_RTU_REQ_LEN];
    build_req(req);
    uint8_t exc = PAT_U8;
    TEST_ASSERT_EQUAL_INT(MB_RTU_OK, mb_rtu_parse_fc06_reply(req, req, ECHO_LEN, &exc));
    TEST_ASSERT_EQUAL_UINT8(0, exc);
    TEST_ASSERT_EQUAL_INT(MB_RTU_OK, mb_rtu_parse_fc06_reply(req, req, ECHO_LEN, NULL));
}

TEST_CASE("mb_rtu_write parse: exception", "[mb_rtu_write]")
{
    uint8_t req[MB_RTU_REQ_LEN], rx[EXC_LEN];
    build_req(req);
    size_t len = make_exc(rx, ADDR, fc_write_exc(), EXC_ILLEGAL_VALUE);
    uint8_t exc = 0;
    TEST_ASSERT_EQUAL_INT(MB_RTU_EXCEPTION, mb_rtu_parse_fc06_reply(req, rx, len, &exc));
    TEST_ASSERT_EQUAL_UINT8(EXC_ILLEGAL_VALUE, exc);
    TEST_ASSERT_EQUAL_INT(MB_RTU_EXCEPTION, mb_rtu_parse_fc06_reply(req, rx, len, NULL));
    /* an independent vector: 01 86 02 -> C3 A1 */
    const uint8_t v[] = { 0x01, 0x86, 0x02, 0xC3, 0xA1 };
    TEST_ASSERT_EQUAL_INT(MB_RTU_EXCEPTION, mb_rtu_parse_fc06_reply(req, v, sizeof(v), &exc));
    TEST_ASSERT_EQUAL_UINT8(2, exc);
}

TEST_CASE("mb_rtu_write parse: exception code 0 is BAD_FRAME", "[mb_rtu_write]")
{
    uint8_t req[MB_RTU_REQ_LEN], rx[EXC_LEN];
    build_req(req);
    size_t len = make_exc(rx, ADDR, fc_write_exc(), 0);
    expect_bad(req, rx, len);
}

TEST_CASE("mb_rtu_write parse: len 0 is TIMEOUT, even for NULL rx", "[mb_rtu_write]")
{
    uint8_t req[MB_RTU_REQ_LEN];
    build_req(req);
    uint8_t exc = PAT_U8;
    TEST_ASSERT_EQUAL_INT(MB_RTU_TIMEOUT, mb_rtu_parse_fc06_reply(req, req, 0, &exc));
    TEST_ASSERT_EQUAL_UINT8(0, exc);
    TEST_ASSERT_EQUAL_INT(MB_RTU_TIMEOUT, mb_rtu_parse_fc06_reply(req, NULL, 0, NULL));
}

TEST_CASE("mb_rtu_write parse: echo with a wrong field is BAD_FRAME", "[mb_rtu_write]")
{
    uint8_t req[MB_RTU_REQ_LEN], rx[MB_RTU_REQ_LEN];
    build_req(req);
    for (size_t field = 0; field < 6; field++) {   /* addr, fc, reg hi/lo, value hi/lo, each with a valid CRC */
        memcpy(rx, req, sizeof(rx));
        rx[field] ^= 0x01u;
        seal(rx, 6);
        expect_bad(req, rx, sizeof(rx));
    }
    memcpy(rx, req, sizeof(rx));   /* only the CRC is wrong */
    rx[6] ^= 0x01u;
    expect_bad(req, rx, sizeof(rx));
    memcpy(rx, req, sizeof(rx));
    rx[7] ^= 0x80u;
    expect_bad(req, rx, sizeof(rx));
}

TEST_CASE("mb_rtu_write parse: exception with wrong addr, fc or CRC is BAD_FRAME", "[mb_rtu_write]")
{
    uint8_t req[MB_RTU_REQ_LEN], rx[EXC_LEN];
    build_req(req);
    size_t len = make_exc(rx, ADDR + 1u, fc_write_exc(), EXC_ILLEGAL_VALUE);
    expect_bad(req, rx, len);
    len = make_exc(rx, ADDR, 0x83u, EXC_ILLEGAL_VALUE);   /* read-exception function byte */
    expect_bad(req, rx, len);
    len = make_exc(rx, ADDR, fc_write_exc(), EXC_ILLEGAL_VALUE);
    rx[4] ^= 0x01u;
    expect_bad(req, rx, len);
}

TEST_CASE("mb_rtu_write parse: wrong lengths, NULL and a foreign request", "[mb_rtu_write]")
{
    uint8_t req[MB_RTU_REQ_LEN], rx[MB_RTU_REQ_LEN + 2];
    build_req(req);
    for (size_t len = 1; len < ECHO_LEN; len++) {   /* truncated echoes, incl. 5 bytes (not an exception frame) */
        expect_bad(req, req, len);
    }
    memcpy(rx, req, ECHO_LEN);
    rx[8] = 0;
    rx[9] = 0;
    expect_bad(req, rx, ECHO_LEN + 2u);   /* too long */
    TEST_ASSERT_EQUAL_INT(MB_RTU_BAD_FRAME, mb_rtu_parse_fc06_reply(req, NULL, ECHO_LEN, NULL));
    TEST_ASSERT_EQUAL_INT(MB_RTU_BAD_FRAME, mb_rtu_parse_fc06_reply(NULL, req, ECHO_LEN, NULL));
    uint8_t read_req[MB_RTU_REQ_LEN];   /* an FC03 request is no FC06 request, even if echoed */
    TEST_ASSERT_EQUAL_INT(8, mb_rtu_build_fc03(ADDR, 231, 1, read_req));
    TEST_ASSERT_EQUAL_INT(MB_RTU_BAD_FRAME, mb_rtu_parse_fc06_reply(read_req, read_req, ECHO_LEN, NULL));
}
