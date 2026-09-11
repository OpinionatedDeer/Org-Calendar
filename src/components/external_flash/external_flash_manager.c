#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include "external_flash_manager.h"
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

#ifndef CONFIG_FORMAT_PARTITION
#define CONFIG_FORMAT_PARTITION false
#endif


#ifndef CONFIG_DEBUG_MODE
#define CONFIG_DEBUG_MODE false
#endif


#define EXTERNAL_FLASH_SIZE    (CONFIG_FLASH_SIZE * 1024 * 1024)

#define DATA_OFFSET    0x0000
#define DATA_SIZE      EXTERNAL_FLASH_SIZE

#define EXTERNAL_FLASH_BASE_PATH "/data"

static const char *TAG = "external_flash_manager";

static void build_full_path(char *dest, size_t dest_size, const char *rel_path) {
    if (rel_path == NULL || rel_path[0] == '\0') {
        snprintf(dest, dest_size, "%s", EXTERNAL_FLASH_BASE_PATH);
    } else {
        const char *p = rel_path;
        if (*p == '/') {
            p++;
        }
        snprintf(dest, dest_size, "%s/%s", EXTERNAL_FLASH_BASE_PATH, p);
    }
}

esp_err_t external_flash_manager_init(spi_host_device_t spi_host_device){
	esp_err_t ret = ESP_OK;
 	hardware_lock_acquire(HW_STATE_FLASH);

	ESP_LOGI(TAG, "initialize EXTERNAL_FLASH");

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
        ESP_LOGE(TAG, "Failed to add EXTERNAL_FLASH: %s",
                 esp_err_to_name(ret));
        goto cleanup;
    }


	ESP_LOGI(TAG, "After spi_bus_add_flash_device: %s",
         esp_err_to_name(ret));

	uint32_t flash_size = 0;


	ESP_LOGI(TAG, "Before esp_flash_init");
    ret = esp_flash_init(chip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize EXTERNAL_FLASH: %s",
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
    ESP_LOGI(TAG, "EXTERNAL_FLASH flash size: %lu bytes",
             (unsigned long)flash_size);
    ret = esp_flash_get_physical_size(chip, &flash_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get flash physical size: %s",
                 esp_err_to_name(ret));
        goto cleanup;
    }


    ESP_LOGI(TAG, "EXTERNAL_FLASH physical flash size: %lu bytes",
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

const esp_partition_t *found =
    esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_ANY,
        "data");

if (!found) {
    ESP_LOGE(TAG, "Could NOT find registered external partition!");
} else {
    ESP_LOGI(TAG,
             "Found partition: label=%s address=0x%lx size=0x%lx type=%d subtype=%d",
             found->label,
             (unsigned long)found->address,
             (unsigned long)found->size,
             found->type,
             found->subtype);

    ESP_LOGI(TAG,
             "Original partition: address=0x%lx size=0x%lx",
             (unsigned long)littlefs_partition->address,
             (unsigned long)littlefs_partition->size);

    ESP_LOGI(TAG,
             "Same partition pointer? %s",
             found == littlefs_partition ? "YES" : "NO");
}


	if (CONFIG_FORMAT_PARTITION) {
		ESP_LOGI(TAG, "Formatting littlefs partition");

		ret = esp_littlefs_format_partition(littlefs_partition);

		if (ret != ESP_OK) {
			ESP_LOGE(TAG, "Failed to format littlefs partition: %s",
					 esp_err_to_name(ret));
			goto cleanup;
		}
		uint8_t buf0[64];
uint8_t buf1[64];

ESP_ERROR_CHECK(esp_partition_read(
    littlefs_partition,
    0x0000,
    buf0,
    sizeof(buf0)
));

ESP_ERROR_CHECK(esp_partition_read(
    littlefs_partition,
    0x1000,
    buf1,
    sizeof(buf1)
));

ESP_LOGI(TAG, "Partition block 0:");
ESP_LOG_BUFFER_HEX(TAG, buf0, sizeof(buf0));

ESP_LOGI(TAG, "Partition block 1:");
ESP_LOG_BUFFER_HEX(TAG, buf1, sizeof(buf1));

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

esp_err_t external_flash_file_exists(const char *path) {
    esp_err_t ret = ESP_OK;
    char full_path[512];
    build_full_path(full_path, sizeof(full_path), path);

     hardware_lock_acquire(HW_STATE_FLASH);
    ESP_LOGI(TAG, "Checking if file exists: %s", full_path);

    struct stat st;
    if (stat(full_path, &st) != 0) {
        ret = ESP_FAIL;
        goto cleanup;
    }

cleanup:
    hardware_lock_release();
    return ret;
}

esp_err_t external_flash_file_remove(const char *path) {
    esp_err_t ret = ESP_OK;
    char full_path[512];
    build_full_path(full_path, sizeof(full_path), path);

     hardware_lock_acquire(HW_STATE_FLASH);
    ESP_LOGI(TAG, "Removing file: %s", full_path);

    if (remove(full_path) != 0) {
        ESP_LOGE(TAG, "Failed to remove file %s", full_path);
        ret = ESP_FAIL;
        goto cleanup;
    }

cleanup:
    hardware_lock_release();
    return ret;
}

esp_err_t external_flash_file_list(const char *dir_path, void (*callback)(const char *name, bool is_dir)) {
    esp_err_t ret = ESP_OK;
    char full_dir_path[512];
    build_full_path(full_dir_path, sizeof(full_dir_path), dir_path);

     hardware_lock_acquire(HW_STATE_FLASH);
    ESP_LOGI(TAG, "Listing directory: %s", full_dir_path);

    DIR *dir = opendir(full_dir_path);
    if (dir == NULL) {
        ESP_LOGE(TAG, "Failed to open directory %s", full_dir_path);
        ret = ESP_FAIL;
        goto cleanup;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_file_path[512];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(full_file_path, sizeof(full_file_path), "%s/%s", full_dir_path, entry->d_name);
#pragma GCC diagnostic pop

        struct stat st;
        bool is_dir = false;
        if (stat(full_file_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                is_dir = true;
            }
        }

        if (callback) {
            callback(entry->d_name, is_dir);
        }
    }

    closedir(dir);

cleanup:
    hardware_lock_release();
    return ret;
}

esp_err_t external_flash_file_get_size(const char *path, uint32_t *size) {
    esp_err_t ret = ESP_OK;
    char full_path[512];
    build_full_path(full_path, sizeof(full_path), path);

     hardware_lock_acquire(HW_STATE_FLASH);
    ESP_LOGI(TAG, "Getting file size: %s", full_path);

    struct stat st;
    if (stat(full_path, &st) != 0) {
        ESP_LOGE(TAG, "Failed to get size for file %s", full_path);
        ret = ESP_FAIL;
        goto cleanup;
    }

    if (size != NULL) {
        *size = (uint32_t)st.st_size;
    }

cleanup:
    hardware_lock_release();
    return ret;
}

// esp_err_t external_flash_file_mkdir(const char *path) {
//     esp_err_t ret = ESP_OK;
//     char full_path[512];
//     build_full_path(full_path, sizeof(full_path), path);
//
//     hardware_lock_acquire();
//     ESP_LOGI(TAG, "Creating directory: %s", full_path);
//
//     if (mkdir(full_path, 0755) != 0) {
//         ESP_LOGE(TAG, "Failed to create directory %s", full_path);
//         ret = ESP_FAIL;
//         goto cleanup;
//     }
//
// cleanup:
//     hardware_lock_release();
//     return ret;
// }

esp_err_t external_flash_file_rename(const char *old_path, const char *new_path) {
    esp_err_t ret = ESP_OK;
    char full_old_path[512];
    char full_new_path[512];
    build_full_path(full_old_path, sizeof(full_old_path), old_path);
    build_full_path(full_new_path, sizeof(full_new_path), new_path);

     hardware_lock_acquire(HW_STATE_FLASH);
    ESP_LOGI(TAG, "Renaming file from %s to %s", full_old_path, full_new_path);

    if (rename(full_old_path, full_new_path) != 0) {
        ESP_LOGE(TAG, "Failed to rename file from %s to %s", full_old_path, full_new_path);
        ret = ESP_FAIL;
        goto cleanup;
    }

cleanup:
    hardware_lock_release();
    return ret;
}

esp_err_t external_flash_file_rmdir(const char *path) {
    esp_err_t ret = ESP_OK;
    char full_path[512];
    build_full_path(full_path, sizeof(full_path), path);

     hardware_lock_acquire(HW_STATE_FLASH);
    ESP_LOGI(TAG, "Removing directory: %s", full_path);

    if (rmdir(full_path) != 0) {
        ESP_LOGE(TAG, "Failed to remove directory %s", full_path);
        ret = ESP_FAIL;
        goto cleanup;
    }

cleanup:
    hardware_lock_release();
    return ret;
}

#include <errno.h>
#include <string.h>
#include <sys/stat.h>

esp_err_t external_flash_file_mkdir(const char *path)
{
    char full_path[512];

    build_full_path(full_path, sizeof(full_path), path);

     hardware_lock_acquire(HW_STATE_FLASH);

    ESP_LOGI(TAG, "Creating directory: %s", full_path);

    errno = 0;

    int result = mkdir(full_path, 0755);

    if (result != 0) {
        ESP_LOGE(TAG,
                 "mkdir(%s) failed: result=%d errno=%d (%s)",
                 full_path,
                 result,
                 errno,
                 strerror(errno));

        hardware_lock_release();
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Directory created successfully: %s", full_path);

    hardware_lock_release();
    return ESP_OK;
}


esp_err_t external_flash_file_write(const char *path, const void *data, size_t len) {
    esp_err_t ret = ESP_OK;
    char full_path[512];
    build_full_path(full_path, sizeof(full_path), path);

     hardware_lock_acquire(HW_STATE_FLASH);
    ESP_LOGI(TAG, "Writing to file: %s (%zu bytes)", full_path, len);

    FILE *f = fopen(full_path, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", full_path);
        ret = ESP_FAIL;
        goto cleanup;
    }

    if (fwrite(data, 1, len, f) != len) {
        ESP_LOGE(TAG, "Failed to write all data to file: %s", full_path);
        ret = ESP_FAIL;
        fclose(f);
        goto cleanup;
    }

    fclose(f);

cleanup:
    hardware_lock_release();
    return ret;
}

esp_err_t external_flash_file_read(const char *path, void *data, size_t len, size_t *bytes_read) {
    esp_err_t ret = ESP_OK;
    char full_path[512];
    build_full_path(full_path, sizeof(full_path), path);

     hardware_lock_acquire(HW_STATE_FLASH);
    ESP_LOGI(TAG, "Reading from file: %s (%zu bytes requested)", full_path, len);

    FILE *f = fopen(full_path, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading: %s", full_path);
        ret = ESP_FAIL;
        goto cleanup;
    }

    size_t read_len = fread(data, 1, len, f);
    if (bytes_read != NULL) {
        *bytes_read = read_len;
    }

    if (read_len < len) {
        // If we read less than requested, it might be EOF, which is fine for a read operation
        // but we might want to log it if it's not expected.
        // For now, we'll just treat it as success if we read SOMETHING or if len was 0.
        if (read_len == 0 && len > 0) {
             // Check if it's actually an error (e.g. file empty or error)
             // feof(f) is a good indicator
        }
    }

    fclose(f);

cleanup:
    hardware_lock_release();
    return ret;
}

esp_err_t external_flash_file_append(const char *path, const void *data, size_t len) {
    esp_err_t ret = ESP_OK;
    char full_path[512];
    build_full_path(full_path, sizeof(full_path), path);

     hardware_lock_acquire(HW_STATE_FLASH);
    ESP_LOGI(TAG, "Appending to file: %s (%zu bytes)", full_path, len);

    FILE *f = fopen(full_path, "ab");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for appending: %s", full_path);
        ret = ESP_FAIL;
        goto cleanup;
    }

    if (fwrite(data, 1, len, f) != len) {
        ESP_LOGE(TAG, "Failed to append data to file: %s", full_path);
        ret = ESP_FAIL;
        fclose(f);
        goto cleanup;
    }

    fclose(f);

cleanup:
    hardware_lock_release();
    return ret;
}
