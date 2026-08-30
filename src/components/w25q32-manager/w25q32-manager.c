#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "w25q32-manager.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_littlefs.h"
#include "sdkconfig.h"


static const char *TAG = "w25q32-manager";
static size_t total = 0, used = 0;

void init_w25q32_manager(void){
	esp_vfs_littlefs_conf_t conf = {
		.base_path ="/littlefs",
		.partition_label = "storage",
		.format_if_mount_failed = false,
		.dont_mount = false,
	};

	esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find LittleFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

	ret = esp_littlefs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get LittleFS partition information (%s)", esp_err_to_name(ret));
        esp_littlefs_format(conf.partition_label);
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
}
