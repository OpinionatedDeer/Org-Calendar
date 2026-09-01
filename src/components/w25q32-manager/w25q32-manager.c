#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "w25q32-manager.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "sdkconfig.h"

#include "esp_flash_spi_init.h"
#include "esp_partition.h"
#include "esp_littlefs.h"

#define PIN_CS CONFIG_FLASH_CS
#define	SPI_FREQUENCY CONFIG_FLASH_SPI_FREQUENCY


static const char *TAG = "w25q32-manager";
#define W25Q32_SIZE    (4 * 1024 * 1024)


esp_err_t w25q32_manager_init(spi_host_device_t spi_host_device){

    esp_err_t ret;
    esp_flash_t *chip = NULL;

    esp_flash_spi_device_config_t config = {
        .host_id = spi_host_device,
        .cs_io_num = PIN_CS,
        .io_mode = SPI_FLASH_FASTRD,
        .input_delay_ns = 0,
        .freq_mhz = SPI_FREQUENCY,
    };

    ret = spi_bus_add_flash_device(&chip, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add W25Q32: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    ret = esp_flash_init(chip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize W25Q32: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    uint32_t flash_size = 0;

    ret = esp_flash_get_size(chip, &flash_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get flash size: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "W25Q32 flash size: %lu bytes",
             (unsigned long)flash_size);

    if (flash_size < W25Q32_SIZE) {
        ESP_LOGE(TAG, "Unexpected flash size");
        return ESP_ERR_INVALID_SIZE;
    }

    /*
     * Register the existing partition.
     *
     * This does NOT erase or format the flash.
     */
    ret = esp_partition_register_external(
        chip,
        0,
        W25Q32_SIZE,
        "data",
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_LITTLEFS,
        NULL
    );

    ret = esp_partition_register_external(
        chip,
        0,
        W25Q32_SIZE,
        "settings",
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_NVS,
        NULL
    );

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register external partition: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    // Mount existing LittleFS filesystem.
    // IMPORTANT: format_if_mount_failed = false
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/data",
        .partition_label = "data",
        .format_if_mount_failed = false,
        .dont_mount = false,
    };

    ret = esp_vfs_littlefs_register(&conf);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LittleFS is not valid or could not be mounted: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    size_t total = 0;
    size_t used = 0;

    ret = esp_littlefs_info("storage", &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read LittleFS info: %s",
                 esp_err_to_name(ret));

        esp_vfs_littlefs_unregister("storage");
        return ret;
    }

    ESP_LOGI(TAG,
             "W25Q32 LittleFS mounted: total=%u, used=%u, free=%u",
             (unsigned)total,
             (unsigned)used,
             (unsigned)(total - used));

    return ESP_OK;
}

