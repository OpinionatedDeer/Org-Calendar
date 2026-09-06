#include "m274-encoder.h"
#include "encoder.h"

#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "esp_log.h"

#define PIN_A  CONFIG_PIN_A
#define PIN_B  CONFIG_PIN_B
#define PIN_SW CONFIG_PIN_SW

#define EV_QUEUE_LEN 10
#define ENCODER_TASK_STACK_SIZE 4096
#define ENCODER_TASK_PRIORITY 5

static const char *TAG = "m274-encoder";

/*
 * Debug logging
 *
 * When CONFIG_DEBUG_MODE is disabled, DEBUG_LOGI()
 * compiles to nothing.
 */
#if CONFIG_DEBUG_MODE
#define DEBUG_LOGI(...) ESP_LOGI(TAG, __VA_ARGS__)
#else
#define DEBUG_LOGI(...) do { } while (0)
#endif

static QueueHandle_t event_queue = NULL;
static rotary_encoder_handle_t encoder = NULL;
static TaskHandle_t encoder_task_handle = NULL;

static int32_t encoder_value = 0;
static m274_encoder_callback_t encoder_callback = NULL;

static void encoder_event_handler(
    const rotary_encoder_event_t *event,
    void *ctx)
{
    QueueHandle_t queue = (QueueHandle_t)ctx;

    if (queue != NULL) {
        xQueueSendToBack(queue, event, 0);
    }
}

static void encoder_task(void *arg)
{
    rotary_encoder_event_t event;

    while (1) {
        if (xQueueReceive(event_queue, &event, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch (event.type) {

        case RE_ET_BTN_PRESSED:
            DEBUG_LOGI("Button pressed");
            break;

        case RE_ET_BTN_RELEASED:
            DEBUG_LOGI("Button released");
            break;

        case RE_ET_BTN_CLICKED:
            DEBUG_LOGI("Button clicked");

            if (encoder != NULL) {
                rotary_encoder_enable_acceleration(
                    encoder,
                    100
                );
            }
            break;

        case RE_ET_BTN_LONG_PRESSED:
            DEBUG_LOGI("Button long pressed");

            if (encoder != NULL) {
                rotary_encoder_disable_acceleration(encoder);
            }
            break;

        case RE_ET_CHANGED:
            encoder_value += event.diff;

            DEBUG_LOGI(
                "Value = %" PRId32,
                encoder_value
            );

            if (encoder_callback != NULL) {
                encoder_callback(encoder_value);
            }
            break;

        default:
            break;
        }
    }
}

esp_err_t m274_encoder_init(void)
{
    if (event_queue != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    event_queue = xQueueCreate(
        EV_QUEUE_LEN,
        sizeof(rotary_encoder_event_t)
    );

    if (event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create event queue");
        return ESP_ERR_NO_MEM;
    }

    DEBUG_LOGI("Encoder queue initialized");

    return ESP_OK;
}

esp_err_t m274_encoder_start(void)
{
    if (event_queue == NULL) {
        ESP_LOGE(TAG, "Call m274_encoder_init() first");
        return ESP_ERR_INVALID_STATE;
    }

    if (encoder != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    rotary_encoder_config_t config =
        ROTARY_ENCODER_DEFAULT_CONFIG();

    config.pin_a = PIN_A;
    config.pin_b = PIN_B;
    config.pin_btn = PIN_SW;

    config.btn_pressed_level = 0;
    config.enable_internal_pullup = true;
    config.btn_dead_time_us = 50000;
    config.btn_long_press_time_us = 1000 * 1000;
    config.acceleration_threshold_ms = 1;
    config.acceleration_cap_ms = 100;
    config.polling_interval_us = 1000;

    config.callback = encoder_event_handler;
    config.callback_ctx = event_queue;

    esp_err_t err = rotary_encoder_create(
        &config,
        &encoder
    );

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed to create encoder: %s",
            esp_err_to_name(err)
        );
        return err;
    }

    BaseType_t task_result = xTaskCreate(
        encoder_task,
        "m274_encoder",
        ENCODER_TASK_STACK_SIZE,
        NULL,
        ENCODER_TASK_PRIORITY,
        &encoder_task_handle
    );

    if (task_result != pdPASS) {
        rotary_encoder_delete(encoder);
        encoder = NULL;

        ESP_LOGE(TAG, "Failed to create encoder task");

        return ESP_ERR_NO_MEM;
    }

    DEBUG_LOGI("Encoder started");

    return ESP_OK;
}

void m274_encoder_set_callback(m274_encoder_callback_t callback)
{
    encoder_callback = callback;
}

int32_t m274_encoder_get_value(void)
{
    return encoder_value;
}

void m274_encoder_set_value(int32_t value)
{
    encoder_value = value;
}

