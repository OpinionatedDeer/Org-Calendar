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

// #define PIN_CS CONFIG_FLASH_CS
// #define	SPI_FREQUENCY CONFIG_FLASH_SPI_FREQUENCY
#define	SPI_FREQUENCY 02
#define PIN_CS 21
#define FORMAT_PARTITION true


static const char *TAG = "w25q32-manager";
// #define W25Q32_SIZE    4
// #define NVS_OFFSET 0
// #define DATA_OFFSET 1
// #define NVS_SIZE 1
// #define DATA_SIZE  3

#define W25Q32_SIZE    (4 * 1024 * 1024)  // 4 MiB = 4,194,304 bytes

#define NVS_OFFSET     0x00000
#define NVS_SIZE       0x10000            // 64 KiB

#define DATA_OFFSET    (NVS_OFFSET + NVS_SIZE)
#define DATA_SIZE      (W25Q32_SIZE - DATA_OFFSET)


esp_err_t w25q32_manager_init(spi_host_device_t spi_host_device){
	esp_err_t ret;


	ESP_LOGI(TAG, "initialize W25q32");

    esp_flash_t *chip = NULL;

	const esp_flash_spi_device_config_t config = {
        .host_id = spi_host_device,
        .cs_io_num = PIN_CS,
        .io_mode = SPI_FLASH_SLOWRD,
        .input_delay_ns = 0,
        .freq_mhz = SPI_FREQUENCY,
    };



	ESP_LOGI(TAG, "Before spi_bus_add_flash_device");
    ret = spi_bus_add_flash_device(&chip, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add W25Q32: %s",
                 esp_err_to_name(ret));
        return ret;
    }


	ESP_LOGI(TAG, "After spi_bus_add_flash_device: %s",
         esp_err_to_name(ret));

uint32_t flash_size = 0;

ESP_LOGI(TAG, "Testing esp_flash_get_size BEFORE init");

ret = esp_flash_get_size(chip, &flash_size);

ESP_LOGI(TAG, "get_size result: %s, size=%lu",
         esp_err_to_name(ret),
         (unsigned long)flash_size);



	ESP_LOGI(TAG, "Before esp_flash_init");
    ret = esp_flash_init(chip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize W25Q32: %s",
                 esp_err_to_name(ret));
        return ret;
    }



	ESP_LOGI(TAG, "After esp_flash_init: %s",
         esp_err_to_name(ret));

    ret = esp_flash_get_size(chip, &flash_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get flash size: %s",
                 esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "W25Q32 flash size: %lu bytes",
             (unsigned long)flash_size);
    ret = esp_flash_get_physical_size(chip, &flash_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get flash physical size: %s",
                 esp_err_to_name(ret));
        return ret;
    }


    ESP_LOGI(TAG, "W25Q32 physical flash size: %lu bytes",
             (unsigned long)flash_size);

	const esp_partition_t * nvs_partition = NULL, * littlefs_partition = NULL;

	ESP_LOGI(TAG, "Before NVS partition register");
    ret = esp_partition_register_external(
        chip,
        0,
        NVS_SIZE,
        "settings",
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_NVS,
		&nvs_partition
    );

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register external partition: %s",
                 esp_err_to_name(ret));
        return ret;
    }
	ESP_LOGI(TAG, "After NVS register and before littlefs partition register");

    ret = esp_partition_register_external(
        chip,
        NVS_SIZE,
        DATA_SIZE,
        "data",
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_LITTLEFS,
        &littlefs_partition
    );

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register external partition: %s",
                 esp_err_to_name(ret));
        return ret;
    }
	ESP_LOGI(TAG, "After NVS register and after littlefs partition register");

	if(!FORMAT_PARTITION){
		esp_flash_erase_region(chip, 0, NVS_SIZE);
		esp_littlefs_format_partition(littlefs_partition);
	}

	ESP_LOGI(TAG, "After Format");


	ret = nvs_flash_init_partition_ptr(nvs_partition);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register or mount NVS: %s",
                 esp_err_to_name(ret));
        return ret;
    }

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

    return ESP_OK;
}

