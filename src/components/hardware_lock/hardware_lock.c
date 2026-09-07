#include "hardware_lock.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"

static const char *TAG = "HARDWARE_LOCK";
static SemaphoreHandle_t hw_mutex = NULL;

esp_err_t hardware_lock_init(void) {
    if (hw_mutex != NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    hw_mutex = xSemaphoreCreateRecursiveMutex();
    if (hw_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Hardware lock initialized");
    return ESP_OK;
}

esp_err_t hardware_lock_acquire(void) {
    if (hw_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(hw_mutex, portMAX_DELAY) == pdTRUE) {
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t hardware_lock_release(void) {
    if (hw_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreGive(hw_mutex) == pdTRUE) {
        return ESP_OK;
    }
    return ESP_FAIL;
}
