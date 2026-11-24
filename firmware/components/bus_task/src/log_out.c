/* log_out.c - ring buffer between bus_task (sole producer) and the printing task. */

#include "log_out.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/message_buffer.h"
#include "freertos/task.h"

#define LOG_OUT_BUFFER_BYTES 2048
#define LOG_OUT_STACK_BYTES  3072
/* Equals the default REPL priority (task_priority = 2 in ESP_CONSOLE_REPL_CONFIG_DEFAULT,
 * components/console/esp_console.h:70, IDF v6.0.2) and is far below bus_task (10). */
#define LOG_OUT_PRIORITY     2
#define LOG_OUT_CORE         0      /* core 1 is bus_task's */

static MessageBufferHandle_t s_buffer;
static StaticMessageBuffer_t s_buffer_struct;
static uint8_t s_buffer_storage[LOG_OUT_BUFFER_BYTES + 1];   /* static message buffers need size + 1 */
static StaticTask_t s_task_tcb;
static StackType_t s_task_stack[LOG_OUT_STACK_BYTES / sizeof(StackType_t)];

static void log_out_task(void *arg)
{
    (void)arg;
    char line[LOG_OUT_MAX_LINE];
    for (;;) {
        size_t n = xMessageBufferReceive(s_buffer, line, sizeof(line) - 1, portMAX_DELAY);
        line[n] = '\0';
        printf("%s\n", line);
    }
}

esp_err_t log_out_start(void)
{
    s_buffer = xMessageBufferCreateStatic(LOG_OUT_BUFFER_BYTES, s_buffer_storage, &s_buffer_struct);
    if (s_buffer == NULL) {
        return ESP_ERR_NO_MEM;
    }
    TaskHandle_t task = xTaskCreateStaticPinnedToCore(log_out_task, "log_out",
                                                      sizeof(s_task_stack) / sizeof(StackType_t), NULL,
                                                      LOG_OUT_PRIORITY, s_task_stack, &s_task_tcb, LOG_OUT_CORE);
    return task != NULL ? ESP_OK : ESP_ERR_NO_MEM;
}

bool log_out_emit(const char *line, size_t len)
{
    if (len == 0 || len >= LOG_OUT_MAX_LINE) {
        return false;
    }
    return xMessageBufferSend(s_buffer, line, len, 0) == len;   /* timeout 0: never blocks */
}
