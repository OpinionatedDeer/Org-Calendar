#pragma once

#include "esp_err.h"
#include "driver/spi_common.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t initialize(void);
esp_err_t external_flash_manager_init(spi_host_device_t spi_host_device);
esp_err_t external_flash_file_exists(const char *path);
esp_err_t external_flash_file_remove(const char *path);
esp_err_t external_flash_file_list(const char *dir_path, void (*callback)(const char *name, bool is_dir));
esp_err_t external_flash_file_get_size(const char *path, uint32_t *size);
esp_err_t external_flash_file_mkdir(const char *path);
esp_err_t external_flash_file_rename(const char *old_path, const char *new_path);
#ifdef __cplusplus
}
#endif
