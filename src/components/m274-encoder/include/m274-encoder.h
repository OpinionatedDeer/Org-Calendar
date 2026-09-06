#pragma once

#include <stdint.h>
#include "esp_err.h"

typedef void (*m274_encoder_callback_t)(int32_t value);

esp_err_t m274_encoder_init(void);
esp_err_t m274_encoder_start(void);

void m274_encoder_set_callback(m274_encoder_callback_t callback);

int32_t m274_encoder_get_value(void);
void m274_encoder_set_value(int32_t value);

