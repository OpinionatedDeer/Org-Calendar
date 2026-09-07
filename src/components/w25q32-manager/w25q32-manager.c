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
#include "driver/gpio.h"

#include "esp_flash_spi_init.h"
#include "esp_partition.h"
#include "esp_littlefs.h"
#include "nvs_flash.h"
#include "hardware_lock.h"

#include "esp_flash.h"


#define	SPI_FREQUENCY CONFIG_FLASH_SPI_FREQUENCY
#define PIN_CS CONFIG_FLASH_CS
#define FORMAT_PARTITION CONFIG_FORMAT_PARTITION

#if CONFIG_DEBUG_MODE
#define DEBUG_LOGI(...) ESP_LOGI(TAG, __VA_ARGS__)
#else
#define DEBUG_LOGI(...) do { } while (0)
#endif

#define W25Q32_SIZE    (CONFIG_FLASH_SIZE * 1024 * 1024)

#define DATA_OFFSET    0x0000
#define DATA_SIZE      W25Q32_SIZE

static const char *TAG = "w25q32-manager";

esp_err_t w25q32_manager_init(spi_host_device_t spi_host_device){
	esp_err_t ret = ESP_OK;
	hardware_lock_acquire();

	ESP_LOGI(TAG, "initialize W25q32");

    esp_flash_t *chip = NULL;

	const esp_flash_spi_device_config_t config = {
        .host_id = spi_host_device,
        .cs_io_num = PIN_CS,
        .io_mode = SPI_FLASH_FASTRD,
        .input_delay_ns = 0,
        .freq_mhz = SPI_FREQUENCY,
    };



	ESP_LOGI(TAG, "Before spi_bus_add_flash_device");
    ret = spi_bus_add_flash_device(&chip, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add W25Q32: %s",
                 esp_err_to_name(ret));
        goto cleanup;
    }


	ESP_LOGI(TAG, "After spi_bus_add_flash_device: %s",
         esp_err_to_name(ret));

	uint32_t flash_size = 0;


	ESP_LOGI(TAG, "Before esp_flash_init");
    ret = esp_flash_init(chip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize W25Q32: %s",
                 esp_err_to_name(ret));
        goto cleanup;
    }



	ESP_LOGI(TAG, "After esp_flash_init: %s",
         esp_err_to_name(ret));

    ret = esp_flash_get_size(chip, &flash_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get flash size: %s",
                 esp_err_to_name(ret));
        goto cleanup;
    }
    ESP_LOGI(TAG, "W25Q32 flash size: %lu bytes",
             (unsigned long)flash_size);
    ret = esp_flash_get_physical_size(chip, &flash_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get flash physical size: %s",
                 esp_err_to_name(ret));
        goto cleanup;
    }


    ESP_LOGI(TAG, "W25Q32 physical flash size: %lu bytes",
             (unsigned long)flash_size);

	const esp_partition_t * littlefs_partition = NULL;

	ESP_LOGI(TAG, "Registering littlefs as external Partition");

    ret = esp_partition_register_external(
        chip,
        DATA_OFFSET,
        DATA_SIZE,
        "data",
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_LITTLEFS,
        &littlefs_partition
    );

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register external partition: %s",
                 esp_err_to_name(ret));
        goto cleanup;
    }
	ESP_LOGI(TAG, "Partition Registered");

	if (FORMAT_PARTITION) {
		ESP_LOGI(TAG, "Formatting littlefs partition");

		ret = esp_littlefs_format_partition(littlefs_partition);

		if (ret != ESP_OK) {
			ESP_LOGE(TAG, "Failed to format littlefs partition: %s",
					 esp_err_to_name(ret));
			goto cleanup;
		}
	}


	ESP_LOGI(TAG, "Mounting partition");

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
        goto cleanup;
    }

	ESP_LOGI(TAG, "Successfully mounted partition");

cleanup:
    hardware_lock_release();

    return ret;
}

