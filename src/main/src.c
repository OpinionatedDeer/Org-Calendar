// #include "initialize.h"
//
//
// void app_main(void)
// {
// 	printf("Test");
// 	// initialize();
//
// }

#include <stdio.h>

#include "m274-encoder.h"
#include "esp_log.h"

static const char *TAG = "main";

static void encoder_changed(int32_t value)
{
    ESP_LOGI(TAG, "Encoder value: %ld", (long)value);
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
}

