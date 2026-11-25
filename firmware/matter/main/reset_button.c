#include "reset_button.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define RESET_BUTTON_GPIO GPIO_NUM_0  /* BOOT button */
#define RESET_BUTTON_TASK_STACK 4096
#define RESET_BUTTON_TASK_PRIORITY 1

static reset_button_action_t s_action;

bool reset_button_step(uint32_t *held_ms, bool pressed, uint32_t poll_ms, uint32_t hold_ms)
{
    if (!pressed) {
        *held_ms = 0;
        return false;
    }
    bool was_below = *held_ms < hold_ms;
    *held_ms += poll_ms;
    return was_below && *held_ms >= hold_ms;
}

static void reset_button_task(void *arg)
{
    (void)arg;
    uint32_t held_ms = 0;
    for (;;) {
        bool pressed = gpio_get_level(RESET_BUTTON_GPIO) == 0;
        if (reset_button_step(&held_ms, pressed, RESET_BUTTON_POLL_MS, RESET_BUTTON_HOLD_MS)) {
            s_action();
        }
        vTaskDelay(pdMS_TO_TICKS(RESET_BUTTON_POLL_MS));
    }
}

esp_err_t reset_button_start(reset_button_action_t action)
{
    if (!action) {
        return ESP_ERR_INVALID_ARG;
    }
    s_action = action;
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << RESET_BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    esp_err_t err = gpio_config(&config);
    if (err != ESP_OK) {
        return err;
    }
    BaseType_t created = xTaskCreate(reset_button_task, "reset_button", RESET_BUTTON_TASK_STACK, NULL,
                                     RESET_BUTTON_TASK_PRIORITY, NULL);
    return created == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
