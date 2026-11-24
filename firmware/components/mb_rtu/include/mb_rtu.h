/* mb_rtu.h - pure Modbus RTU FC03 frame codec (spec 012). No I/O, no ESP-IDF.
 * The only request builder the node has; the function code is fixed inside it.
 * Frame layout, CRC and exception frames: Modbus Application Protocol and
 * Modbus over Serial Line specifications (external, not from the wiki). */
#ifndef MB_RTU_H
#define MB_RTU_H

#include <stdint.h>
#include <stddef.h>

#define MB_RTU_MAX_QTY        4u
#define MB_RTU_REQ_LEN        8u                               /* addr, fc, reg hi/lo, qty hi/lo, crc lo/hi */
#define MB_RTU_MAX_REPLY_LEN  (5u + 2u * MB_RTU_MAX_QTY)       /* 13 */
#define MB_RTU_LEN_MORE       ((size_t)0)                      /* length not known yet: need more bytes */
#define MB_RTU_LEN_UNKNOWN    SIZE_MAX                         /* not determinable (unknown function byte) */

typedef enum { MB_RTU_OK, MB_RTU_EXCEPTION, MB_RTU_BAD_FRAME, MB_RTU_TIMEOUT } mb_rtu_res_t;

/* CRC-16/MODBUS: poly A001 reflected, init FFFF; transmitted low byte first. */
uint16_t mb_rtu_crc16(const uint8_t *buf, size_t len);

/* 8 on success; -1 (out untouched) if addr not 1..247, qty not 1..MB_RTU_MAX_QTY, or out == NULL. */
int mb_rtu_build_fc03(uint8_t addr, uint16_t reg, uint16_t qty, uint8_t out[MB_RTU_REQ_LEN]);

/* Expected length of the reply to an FC03 request of qty registers, from the bytes received so far:
   n < 2 -> MB_RTU_LEN_MORE; read-reply function byte -> 5 + 2*qty (from the request, not from rx[2]);
   its exception variant (bit 7 set) -> 5; any other function byte -> MB_RTU_LEN_UNKNOWN.
   Does not look at the address. */
size_t mb_rtu_fc03_reply_len(uint16_t qty, const uint8_t *rx, size_t n);

/* Classify a complete received byte sequence (everything the port collected for this transaction).
   *exc_code is set to 0 on entry (if non-NULL) and to the code only for MB_RTU_EXCEPTION;
   val[0..qty-1] (host order) is written only for MB_RTU_OK; val must be non-NULL (NULL -> MB_RTU_BAD_FRAME).
   len == 0 -> MB_RTU_TIMEOUT is checked first. */
mb_rtu_res_t mb_rtu_parse_fc03_reply(uint8_t addr, uint16_t qty, const uint8_t *rx, size_t len,
                                     uint16_t *val, uint8_t *exc_code);

/* t3.5 in microseconds: baud <= 19200 -> ceil(3.5 * 11 * 1e6 / baud); baud > 19200 -> 1750 (external). 0 baud -> 0. */
uint32_t mb_rtu_t35_us(uint32_t baud);

#endif /* MB_RTU_H */
