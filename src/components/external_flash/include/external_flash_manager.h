#pragma once

#include "esp_err.h"
#include "driver/spi_common.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t initialize(void);
esp_err_t external_flash_manager_init(spi_host_device_t spi_host_device);
#ifdef __cplusplus
}
#endif

