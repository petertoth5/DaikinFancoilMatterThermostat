/* mb_link.h - the single seam between the Modbus link layer and everything above it (spec 014).
 * Own types only; nothing here depends on how the link frames, times or classifies. */
#ifndef MB_LINK_H
#define MB_LINK_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

typedef enum { MBL_OK, MBL_TIMEOUT, MBL_EXCEPTION, MBL_BAD_FRAME, MBL_ERROR, MBL_REFUSED } mb_link_res_t;

typedef struct {
    int uart_port, tx_gpio, rx_gpio, de_gpio;
    uint32_t baud;
} mb_link_port_cfg_t;

typedef struct {
    mb_link_port_cfg_t port;
    uint32_t resp_timeout_ms;
} mb_link_config_t;

/* measured = true: values come from the link. measured = false: not measurable, all values 0.
   gap_bytes: a lower bound; only bytes already in the ring buffer at count time are seen, bytes still in the
   HW FIFO or arriving between count and flush are missed.
   max_byte_interval_us: approximate; stamped when the task reads, so back-to-back reads of buffered bytes
   under-report and late wakes over-report. */
typedef struct {
    bool measured;
    uint32_t gap_bytes, faults, max_byte_interval_us;
} mb_link_counters_t;

/* First call in app_main: DE low, transceiver in receive. */
esp_err_t mb_link_hold_receive(const mb_link_config_t *cfg);

/* Listens for ms and reports bus activity (received bytes plus line errors). Not while initialised:
   ESP_ERR_INVALID_STATE. */
esp_err_t mb_link_silence_check(const mb_link_config_t *cfg, uint32_t ms, uint32_t *activity);

/* Opens the bus. resp_timeout_ms 200..2000 and baud 1200..115200, else ESP_ERR_INVALID_ARG;
   twice: ESP_ERR_INVALID_STATE. */
esp_err_t mb_link_init(const mb_link_config_t *cfg);

/* Closes the bus and holds receive. ESP_OK no-op if not initialised. */
esp_err_t mb_link_deinit(void);

/* Reads qty holding registers (1..4) from slave addr (1..247) into val[0..qty-1] (host order).
   Refused without touching the bus: addr outside 1..247 or a range the read whitelist rejects.
   MBL_ERROR without bus access: NULL val or exc_code, or not initialised. A port fault also returns
   MBL_ERROR and leaves the link not initialised until mb_link_init.
   Blocking, at most TX frame time + resp_timeout_ms + 2 silence windows + 1 tick.
   *exc_code is the exception code for MBL_EXCEPTION, else 0. */
mb_link_res_t mb_link_read(uint8_t addr, uint16_t reg, uint16_t qty, uint16_t *val, uint8_t *exc_code);

/* Copy of the link counters since boot (bus_task only). */
void mb_link_get_counters(mb_link_counters_t *out);

#endif /* MB_LINK_H */
