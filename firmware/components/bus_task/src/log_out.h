#pragma once

/* log_out.h - non-blocking output path of bus_task (spec 006): message buffer drained by a
 * low-priority task that prints to the console stdout. */

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#define LOG_OUT_MAX_LINE 160    /* longest line incl. NUL */

esp_err_t log_out_start(void);

/* Never blocks. false: line dropped (buffer full or line too long); the caller counts it. */
bool log_out_emit(const char *line, size_t len);
