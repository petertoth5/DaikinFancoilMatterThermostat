/* mb_rtu_write.h - pure Modbus RTU FC06 (write single register) frame codec (spec 019a).
 * No I/O, no ESP-IDF. Only the write path (the write link) uses it; the function code is fixed
 * inside it. Frame layout, echo reply and exception frames: Modbus Application Protocol
 * specification (external, not from the wiki). */
#ifndef MB_RTU_WRITE_H
#define MB_RTU_WRITE_H

#include <stdint.h>
#include <stddef.h>
#include "mb_rtu.h"

/* 8 on success (addr, fc, reg hi/lo, value hi/lo, crc lo/hi);
   -1 (out untouched) if addr not 1..247 (broadcast 0 refused) or out == NULL. */
int mb_rtu_build_fc06(uint8_t addr, uint16_t reg, uint16_t value, uint8_t out[MB_RTU_REQ_LEN]);

/* Expected length of the reply to an FC06 request, from the bytes received so far:
   n < 2 -> MB_RTU_LEN_MORE; write function byte (echo) -> 8; its exception variant -> 5;
   any other function byte -> MB_RTU_LEN_UNKNOWN. Does not look at the address. */
size_t mb_rtu_fc06_reply_len(const uint8_t *rx, size_t n);

/* Classify the bytes received for the request req (a frame built by mb_rtu_build_fc06).
   *exc_code is set to 0 on entry (if non-NULL) and to the code only for MB_RTU_EXCEPTION.
   len == 0 -> MB_RTU_TIMEOUT. OK only for an exact echo of req with a valid CRC.
   EXCEPTION for addr, exception function byte, code != 0, valid CRC. Anything else
   (code 0, wrong addr/reg/value/CRC, wrong length, NULL req or rx) -> MB_RTU_BAD_FRAME. */
mb_rtu_res_t mb_rtu_parse_fc06_reply(const uint8_t req[MB_RTU_REQ_LEN], const uint8_t *rx, size_t len,
                                     uint8_t *exc_code);

#endif /* MB_RTU_WRITE_H */
