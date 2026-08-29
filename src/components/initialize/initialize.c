#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "hal/spi_types.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "eink-driver.h"

static const char *TAG = "initialize";
static spi_host_device_t spi_host = SPI2_HOST;
static bool bus_inited = false;

#define PIN_MOSI CONFIG_INIT_PIN_MOSI
#define PIN_MISO CONFIG_INIT_PIN_MISO
#define PIN_SCLK CONFIG_INIT_PIN_SCLK



esp_err_t initialize(void){

    esp_err_t ret;
    if (!bus_inited) {
        spi_bus_config_t buscfg = {
            .mosi_io_num = PIN_MOSI,
            .miso_io_num = PIN_MISO,
            .sclk_io_num = PIN_SCLK,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = 4096,
        };
        ret = spi_bus_initialize(spi_host, &buscfg, SPI_DMA_CH_AUTO);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(ret));
            return ret;
        }
        bus_inited = true;
		eink_init(spi_host);
    }
	return ESP_OK;
}
