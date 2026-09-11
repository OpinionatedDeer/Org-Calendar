#include <stdio.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#include "m274-encoder.h"
#include "esp_log.h"
#include "initialize.h"
#include "eink-driver.h"
#include "external_flash_manager.h"


static const char *TAG = "main";

static void encoder_changed(int32_t value)
{
    ESP_LOGI(TAG, "Encoder value: %ld", (long)value);
}

static void list_callback(const char *name, bool is_dir)
{
    ESP_LOGI("TEST", "  [%s] %s", is_dir ? "DIR" : "FILE", name);
}

static void test_external_flash_file_api(void)
{
    ESP_LOGI(TAG, "Starting External Flash File API Test...");

    const char *test_file = "test_file.txt";
    const char *test_dir = "test_dir";
    const char *test_subdir = "test_dir/sub_dir";
    const char *rename_file = "test_renamed.txt";
    const char *data_to_write = "Hello, External Flash!";
    char read_buffer[64] = {0};
    size_t bytes_read = 0;

    // 1. Test mkdir
    ESP_ERROR_CHECK(external_flash_file_mkdir(test_dir));
    ESP_LOGI(TAG, "Created directory: %s", test_dir);

    // 2. Test mkdir sub_dir
    ESP_ERROR_CHECK(external_flash_file_mkdir(test_subdir));
    ESP_LOGI(TAG, "Created sub-directory: %s", test_subdir);

    // 3. Test file_write
    ESP_ERROR_CHECK(external_flash_file_write(test_file, data_to_write, strlen(data_to_write)));
    ESP_LOGI(TAG, "Wrote file: %s", test_file);

    // 4. Test file_exists
    ESP_ERROR_CHECK(external_flash_file_exists(test_file));
    ESP_LOGI(TAG, "Verified file exists: %s", test_file);

    // 5. Test file_get_size
    uint32_t size = 0;
    ESP_ERROR_CHECK(external_flash_file_get_size(test_file, &size));
    ESP_LOGI(TAG, "File size: %lu", (unsigned long)size);
    ESP_ERROR_CHECK(size == strlen(data_to_write) ? ESP_OK : ESP_FAIL);

    // 6. Test file_read
    memset(read_buffer, 0, sizeof(read_buffer));
    ESP_ERROR_CHECK(external_flash_file_read(test_file, read_buffer, sizeof(read_buffer) - 1, &bytes_read));
    ESP_LOGI(TAG, "Read back: %s (bytes read: %zu)", read_buffer, bytes_read);
    ESP_ERROR_CHECK(strcmp(read_buffer, data_to_write) == 0 ? ESP_OK : ESP_FAIL);

    // 7. Test file_append
    const char *append_data = " Append more!";
    ESP_ERROR_CHECK(external_flash_file_append(test_file, append_data, strlen(append_data)));
    ESP_LOGI(TAG, "Appended data to: %s", test_file);

    // 8. Test file_read again to see appended content
    memset(read_buffer, 0, sizeof(read_buffer));
    ESP_ERROR_CHECK(external_flash_file_read(test_file, read_buffer, sizeof(read_buffer) - 1, &bytes_read));
    ESP_LOGI(TAG, "Read back after append: %s", read_buffer);
    ESP_ERROR_CHECK(strstr(read_buffer, append_data) != NULL ? ESP_OK : ESP_FAIL);

    // 9. Test file_rename
    ESP_ERROR_CHECK(external_flash_file_rename(test_file, rename_file));
    ESP_LOGI(TAG, "Renamed %s to %s", test_file, rename_file);

    // 10. Test file_exists for old name (should fail)
    if (external_flash_file_exists(test_file) != ESP_OK) {
        ESP_LOGI(TAG, "Old filename %s correctly does not exist", test_file);
    } else {
        ESP_LOGE(TAG, "Old filename %s still exists!", test_file);
        ESP_ERROR_CHECK(ESP_FAIL);
    }

    // 11. Test file_list
    ESP_LOGI(TAG, "Listing directory %s:", test_dir);
    external_flash_file_list(test_dir, list_callback);

    // 12. Cleanup: remove files and directories
    ESP_ERROR_CHECK(external_flash_file_remove(rename_file));
    ESP_ERROR_CHECK(external_flash_file_rmdir(test_subdir));
    ESP_ERROR_CHECK(external_flash_file_rmdir(test_dir));

    ESP_LOGI(TAG, "External Flash File API Test PASSED!");
}

void app_main(void)
{
    // 1. Create queue/resources
    ESP_ERROR_CHECK(m274_encoder_init());

    // 2. Tell encoder what to do when the value changes
    m274_encoder_set_callback(encoder_changed);

    // 3. Start the actual encoder
    ESP_ERROR_CHECK(m274_encoder_start());

    ESP_LOGI(TAG, "Application started");
	initialize();

    // Test the new File API
    test_external_flash_file_api();
	//
	eink_initialize();
	eink_clear_black();

   vTaskDelay(pdMS_TO_TICKS(10000));
   eink_clear();
   eink_sleep();

}
