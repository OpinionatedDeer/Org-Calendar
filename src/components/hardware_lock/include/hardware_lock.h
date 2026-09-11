#ifndef HARDWARE_LOCK_H
#define HARDWARE_LOCK_H

#include "esp_err.h"

/**
 * @brief Hardware states for pin multiplexing
 */
typedef enum {
    HW_STATE_NONE,
    HW_STATE_EINK,
    HW_STATE_FLASH
} hw_state_t;

/**
 * @brief Initializes the hardware lock mutex.
 * 
 * @return ESP_OK on success, or an error code.
 */
esp_err_t hardware_lock_init(void);

/**
 * @brief Acquires the hardware lock and reconfigures pins for the target state.
 * 
 * @param target_state The required hardware configuration.
 * @return ESP_OK on success, or an error code.
 */
esp_err_t hardware_lock_acquire(hw_state_t target_state);

/**
 * @brief Releases the hardware lock.
 * 
 * @return ESP_OK on success, or an error code.
 */
esp_err_t hardware_lock_release(void);

#endif // HARDWARE_LOCK_H
