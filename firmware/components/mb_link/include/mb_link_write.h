/* mb_link_write.h - one FC06 write on the link opened by mb_link (spec 019b, write image only).
 * Written without touching mb_link.c: the link parameters come from mb_link_write_configure. */
#ifndef MB_LINK_WRITE_H
#define MB_LINK_WRITE_H

#include <stdint.h>
#include "esp_err.h"
#include "mb_link.h"

/* Remembers the response timeout and baud for the t3.5 silence; call once, with the config given to
   mb_link_init, before the first mb_link_write. ESP_ERR_INVALID_ARG on NULL. */
esp_err_t mb_link_write_configure(const mb_link_config_t *cfg);

/* Writes one register (FC06) on slave addr and classifies the echo. No retry.
   MBL_REFUSED before any port access: addr outside 1..247, a register the write whitelist rejects
   (daikin_regs_write_mask == 0), or a value outside the register's fixed range (231 50..400, 233 in {0, 3, 5}).
   MBL_ERROR without bus access: NULL exc_code, not configured, or the port is not open. A port fault also
   returns MBL_ERROR and the port is closed, as in mb_link_read.
   Blocking, at most TX frame time + resp_timeout_ms + 2 silence windows + 1 tick.
   *exc_code is the exception code for MBL_EXCEPTION, else 0. */
mb_link_res_t mb_link_write(uint8_t addr, uint16_t reg, uint16_t value, uint8_t *exc_code);

#endif /* MB_LINK_WRITE_H */
