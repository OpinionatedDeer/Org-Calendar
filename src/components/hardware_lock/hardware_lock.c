#include "hardware_lock.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_rom_gpio.h"
#include "soc/gpio_sig_map.h"
#include "sdkconfig.h"

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

esp_err_t hardware_lock_acquire(hw_state_t target_state) {
    if (hw_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(hw_mutex, portMAX_DELAY) == pdTRUE) {
        if (target_state == HW_STATE_EINK) {
            ESP_LOGD(TAG, "Reconfiguring for E-Ink (GPIO mode)");
            gpio_reset_pin(CONFIG_EINK_PIN_DC);
            gpio_set_direction(CONFIG_EINK_PIN_DC, GPIO_MODE_OUTPUT);
            gpio_set_level(CONFIG_EINK_PIN_DC, 1);
        } else if (target_state == HW_STATE_FLASH) {
            ESP_LOGD(TAG, "Reconfiguring for Flash (SPI MISO mode)");
            // MISO is an input signal from the peripheral to the SoC.
            // First, set the pin as an input to allow it to receive the signal.
            gpio_set_direction(CONFIG_EINK_PIN_DC, GPIO_MODE_INPUT);
            esp_rom_gpio_connect_in_signal(CONFIG_EINK_PIN_DC, FSPIQ_IN_IDX, false);
        }
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
