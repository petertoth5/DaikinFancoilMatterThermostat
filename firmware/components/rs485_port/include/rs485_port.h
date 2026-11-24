/* rs485_port.h - bus UART, DE signal, silence check and one blocking request/reply transaction
 * (spec 014). Modbus-agnostic: no function codes, no CRC. Used only by mb_link.
 * DE (MAX485 DE and /RE, tied) is a plain GPIO output at level 0 whenever the UART driver is not
 * installed; while it is installed, DE is the UART RTS signal, low except while the 8 request bytes
 * are shifted out. */
#ifndef RS485_PORT_H
#define RS485_PORT_H

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

typedef struct {
    int uart_port, tx_gpio, rx_gpio, de_gpio;
    uint32_t baud;
} rs485_port_cfg_t;

#define RS485_LEN_MORE    ((size_t)0)   /* reply length not known yet */
#define RS485_LEN_UNKNOWN SIZE_MAX      /* not determinable: end at the first silence window */

/* Expected reply length from the n bytes received so far (n <= rx_cap given to the transaction). */
typedef size_t (*rs485_frame_len_fn)(const uint8_t *rx, size_t n, void *ctx);

/* Since boot, saturating. */
typedef struct {
    uint32_t gap_bytes;             /* bytes that arrived between two transactions; a lower bound: only bytes
                                       already in the ring buffer at count time are seen, bytes still in the
                                       HW FIFO or arriving between count and flush are missed */
    uint32_t faults;                /* port faults (port closed afterwards) */
    uint32_t max_byte_interval_us;  /* longest interval between two bytes of one reply; approximate: stamped
                                       when the task reads, so back-to-back reads of buffered bytes under-report
                                       and late wakes over-report */
} rs485_port_counters_t;

/* Cfg checks shared by hold_receive, silence_check and open: ESP_ERR_INVALID_ARG (no UART call) if cfg is
   NULL, uart_port is not 1..2 (UART0 is the console), a GPIO is negative, the three GPIOs are not
   distinct or baud is 0. */

/* DE: level 0, then output enable, pull-up off. ESP_ERR_INVALID_STATE while the port is open. */
esp_err_t rs485_port_hold_receive(const rs485_port_cfg_t *c);

/* ms == 0 or activity NULL: ESP_ERR_INVALID_ARG. Precondition: port not open (else ESP_ERR_INVALID_STATE). Plain RX-only UART (UART_MODE_UART, TX/RTS/CTS
   not routed, TX buffer 0, event queue); counts received bytes plus frame/parity/break/FIFO-overflow/
   buffer-full events for ms; deletes the driver; holds receive. DE stays a plain GPIO at 0 throughout.
   Any failure: driver deleted, receive held, error returned. */
esp_err_t rs485_port_silence_check(const rs485_port_cfg_t *c, uint32_t ms, uint32_t *activity);

/* Hold receive, then param config (8N1, baud, no flow control), driver install (RX 256, TX 0, no queue),
   RS485 half-duplex mode, pins (DE as RTS), RX FIFO threshold 1, RX pull-up. Any failure: close path,
   error returned. ESP_ERR_INVALID_STATE if already open. */
esp_err_t rs485_port_open(const rs485_port_cfg_t *c);

/* If installed: driver delete; then always hold receive. Safe if not open. */
esp_err_t rs485_port_close(void);

/* Blocking transaction: exactly 8 TX bytes, then collect the reply (end-of-frame rule in the spec).
   ESP_OK with *rx_len (0 = nothing received before the deadline); ESP_ERR_INVALID_STATE (not open) /
   ESP_ERR_INVALID_ARG (bad args, tx_len != 8, resp_timeout_ms not 1..2000) without bus access;
   ESP_FAIL / ESP_ERR_TIMEOUT on a port fault, after which the port is closed and held.
   Bytes beyond rx_cap are counted and discarded; *rx_len saturates at rx_cap. */
esp_err_t rs485_port_transact(const uint8_t *tx, size_t tx_len, uint8_t *rx, size_t rx_cap, size_t *rx_len,
                              rs485_frame_len_fn len_fn, void *len_ctx,
                              uint32_t resp_timeout_ms, uint32_t silence_us);

void rs485_port_get_counters(rs485_port_counters_t *out);

#endif /* RS485_PORT_H */
