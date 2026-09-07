#ifndef HARDWARE_LOCK_H
#define HARDWARE_LOCK_H

#include "esp_err.h"

/**
 * @brief Initializes the hardware lock mutex.
 * 
 * @return ESP_OK on success, or an error code.
 */
esp_err_t hardware_lock_init(void);

/**
 * @brief Acquires the hardware lock. Blocks until available.
 * 
 * @return ESP_OK on success, or an error code.
 */
esp_err_t hardware_lock_acquire(void);

/**
 * @brief Releases the hardware lock.
 * 
 * @return ESP_OK on success, or an error code.
 */
esp_err_t hardware_lock_release(void);

#endif // HARDWARE_LOCK_H
