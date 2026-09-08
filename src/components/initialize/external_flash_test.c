#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define PIN_CS 21
static const char *TAG = "EXTERNAL_FLASH_TEST";
esp_err_t external_flash_test_rw(spi_host_device_t host);

esp_err_t external_flash_test_jedec(spi_host_device_t host)
{
    esp_err_t ret;

    spi_device_handle_t dev = NULL;

    // Configure EXTERNAL_FLASH as a normal SPI device.
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 1000000,   // 1 MHz
        .mode = 0,                   // SPI mode 0
        .spics_io_num = PIN_CS,
        .queue_size = 1,
    };

ESP_LOGI("TAG", "Sending JEDEC ID command 0x9F");

    ret = spi_bus_add_device(host, &devcfg, &dev);
    if (ret != ESP_OK) {
        ESP_LOGE("TAG", "spi_bus_add_device failed: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    // 0x9F + 3 dummy bytes to generate 24 clock cycles
    uint8_t tx[4] = {
        0x9F,
        0x00,
        0x00,
        0x00
    };

    uint8_t rx[4] = {
        0x00,
        0x00,
        0x00,
        0x00
    };

    spi_transaction_t transaction = {
        .length = 32,
        .rxlength = 32,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };

    ret = spi_device_transmit(dev, &transaction);

    if (ret != ESP_OK) {
        ESP_LOGE("TAG", "spi_device_transmit failed: %s",
                 esp_err_to_name(ret));

        spi_bus_remove_device(dev);
        return ret;
    }

    ESP_LOGI("TAG", "RX: %02X %02X %02X %02X",
             rx[0], rx[1], rx[2], rx[3]);

    ESP_LOGI("TAG", "JEDEC ID: %02X %02X %02X",
             rx[1], rx[2], rx[3]);

    spi_bus_remove_device(dev);
	ret = external_flash_test_rw(host);

if (ret != ESP_OK) {
    ESP_LOGE("TAG", "EXTERNAL_FLASH R/W test FAILED");
} else {
    ESP_LOGI("TAG", "EXTERNAL_FLASH R/W test PASSED");
}


    return ESP_OK;
}


// ============================================================
// EXTERNAL_FLASH STATUS + WRITE/READ TEST
// ============================================================

#define EXTERNAL_FLASH_CMD_WRITE_ENABLE   0x06
#define EXTERNAL_FLASH_CMD_READ_STATUS1   0x05
#define EXTERNAL_FLASH_CMD_SECTOR_ERASE   0x20
#define EXTERNAL_FLASH_CMD_PAGE_PROGRAM   0x02
#define EXTERNAL_FLASH_CMD_READ_DATA      0x03

static esp_err_t external_flash_read_status1(spi_device_handle_t dev,
                                     uint8_t *status)
{
    uint8_t tx[2] = {
        EXTERNAL_FLASH_CMD_READ_STATUS1,
        0x00
    };

    uint8_t rx[2] = {0};

    spi_transaction_t t = {
        .length = 16,
        .rxlength = 16,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };

    esp_err_t ret = spi_device_transmit(dev, &t);
    if (ret != ESP_OK) {
        return ret;
    }

    *status = rx[1];

    return ESP_OK;
}


static esp_err_t external_flash_write_enable(spi_device_handle_t dev)
{
    uint8_t cmd = EXTERNAL_FLASH_CMD_WRITE_ENABLE;

    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
    };

    return spi_device_transmit(dev, &t);
}


static esp_err_t external_flash_wait_busy(spi_device_handle_t dev)
{
    uint8_t status;

    while (1) {
        esp_err_t ret = external_flash_read_status1(dev, &status);

        if (ret != ESP_OK) {
            return ret;
        }

        if ((status & 0x01) == 0) {
            return ESP_OK;
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}


static esp_err_t external_flash_sector_erase(spi_device_handle_t dev,
                                     uint32_t addr)
{
    uint8_t tx[4] = {
        EXTERNAL_FLASH_CMD_SECTOR_ERASE,
        (addr >> 16) & 0xFF,
        (addr >> 8) & 0xFF,
        addr & 0xFF
    };

    spi_transaction_t t = {
        .length = 32,
        .tx_buffer = tx,
    };

    return spi_device_transmit(dev, &t);
}


static esp_err_t external_flash_page_program(spi_device_handle_t dev,
                                     uint32_t addr,
                                     const uint8_t *data,
                                     size_t len)
{
    if (len == 0 || len > 256) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t tx[4 + 256];

    tx[0] = EXTERNAL_FLASH_CMD_PAGE_PROGRAM;
    tx[1] = (addr >> 16) & 0xFF;
    tx[2] = (addr >> 8) & 0xFF;
    tx[3] = addr & 0xFF;

    memcpy(&tx[4], data, len);

    spi_transaction_t t = {
        .length = (4 + len) * 8,
        .tx_buffer = tx,
    };

    return spi_device_transmit(dev, &t);
}


static esp_err_t external_flash_read_data(spi_device_handle_t dev,
                                  uint32_t addr,
                                  uint8_t *data,
                                  size_t len)
{
    uint8_t buffer[4 + 256] = {0};

    buffer[0] = EXTERNAL_FLASH_CMD_READ_DATA;
    buffer[1] = (addr >> 16) & 0xFF;
    buffer[2] = (addr >> 8) & 0xFF;
    buffer[3] = addr & 0xFF;

    spi_transaction_t t = {
        .length = (4 + len) * 8,
        .rxlength = (4 + len) * 8,
        .tx_buffer = buffer,
        .rx_buffer = buffer,
    };

    esp_err_t ret = spi_device_transmit(dev, &t);

    if (ret != ESP_OK) {
        return ret;
    }

    memcpy(data, &buffer[4], len);

    return ESP_OK;
}


// ============================================================
// RUN EXTERNAL_FLASH TEST
// ============================================================

esp_err_t external_flash_test_rw(spi_host_device_t host)
{
    esp_err_t ret;
    spi_device_handle_t dev = NULL;

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 1000000,
        .mode = 0,
        .spics_io_num = PIN_CS,
        .queue_size = 1,
    };

    ret = spi_bus_add_device(host, &devcfg, &dev);

    if (ret != ESP_OK) {
        ESP_LOGE("TAG", "spi_bus_add_device failed: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    // --------------------------------------------------------
    // 1. READ STATUS REGISTER
    // --------------------------------------------------------

    uint8_t status = 0;

    ESP_LOGI("TAG", "Reading Status Register-1...");

    ret = external_flash_read_status1(dev, &status);

    if (ret != ESP_OK) {
        ESP_LOGE("TAG", "Read status failed: %s",
                 esp_err_to_name(ret));
        goto cleanup;
    }

    ESP_LOGI("TAG",
             "Status = 0x%02X  BUSY=%d  WEL=%d",
             status,
             status & 0x01,
             (status >> 1) & 0x01);


    // --------------------------------------------------------
    // TEST ADDRESS
    // IMPORTANT: Make sure this sector is unused!
    // --------------------------------------------------------

    const uint32_t test_addr = 0x000000;


    // --------------------------------------------------------
    // 2. WRITE ENABLE
    // --------------------------------------------------------

    ESP_LOGI("TAG", "Sending Write Enable...");

    ret = external_flash_write_enable(dev);

    if (ret != ESP_OK) {
        ESP_LOGE("TAG", "Write Enable failed: %s",
                 esp_err_to_name(ret));
        goto cleanup;
    }

    ret = external_flash_read_status1(dev, &status);

    if (ret != ESP_OK) {
        goto cleanup;
    }

    ESP_LOGI("TAG",
             "After WREN: Status=0x%02X WEL=%d",
             status,
             (status >> 1) & 0x01);

    if (!(status & 0x02)) {
        ESP_LOGE("TAG", "WEL bit was NOT set!");
        ret = ESP_FAIL;
        goto cleanup;
    }


    // --------------------------------------------------------
    // 3. ERASE 4KB SECTOR
    // --------------------------------------------------------

    ESP_LOGI("TAG",
             "Erasing sector at address 0x%06lX...",
             (unsigned long)test_addr);

    ret = external_flash_sector_erase(dev, test_addr);

    if (ret != ESP_OK) {
        ESP_LOGE("TAG", "Sector erase failed: %s",
                 esp_err_to_name(ret));
        goto cleanup;
    }

    ret = external_flash_wait_busy(dev);

    if (ret != ESP_OK) {
        ESP_LOGE("TAG", "Erase wait failed: %s",
                 esp_err_to_name(ret));
        goto cleanup;
    }

    ESP_LOGI("TAG", "Sector erase complete");


    // --------------------------------------------------------
    // 4. WRITE 4 BYTES
    // --------------------------------------------------------

    uint8_t write_data[4] = {
        0xDE,
        0xAD,
        0xBE,
        0xEF
    };

    ESP_LOGI("TAG", "Writing: DE AD BE EF");

    ret = external_flash_write_enable(dev);

    if (ret != ESP_OK) {
        ESP_LOGE("TAG", "Write Enable failed");
        goto cleanup;
    }

    ret = external_flash_page_program(dev,
                               test_addr,
                               write_data,
                               sizeof(write_data));

    if (ret != ESP_OK) {
        ESP_LOGE("TAG", "Page program failed: %s",
                 esp_err_to_name(ret));
        goto cleanup;
    }

    ret = external_flash_wait_busy(dev);

    if (ret != ESP_OK) {
        ESP_LOGE("TAG", "Program wait failed: %s",
                 esp_err_to_name(ret));
        goto cleanup;
    }

    ESP_LOGI("TAG", "Write complete");


    // --------------------------------------------------------
    // 5. READ BACK
    // --------------------------------------------------------

    uint8_t read_data[4] = {0};

    ESP_LOGI("TAG", "Reading back...");

    ret = external_flash_read_data(dev,
                           test_addr,
                           read_data,
                           sizeof(read_data));

    if (ret != ESP_OK) {
        ESP_LOGE("TAG", "Read failed: %s",
                 esp_err_to_name(ret));
        goto cleanup;
    }

    ESP_LOGI("TAG",
             "Read back: %02X %02X %02X %02X",
             read_data[0],
             read_data[1],
             read_data[2],
             read_data[3]);


    // --------------------------------------------------------
    // 6. VERIFY
    // --------------------------------------------------------

    if (memcmp(write_data,
               read_data,
               sizeof(write_data)) == 0) {

        ESP_LOGI("TAG", "================================");
        ESP_LOGI("TAG", "EXTERNAL_FLASH WRITE/READ TEST: PASS");
        ESP_LOGI("TAG", "================================");

        ret = ESP_OK;

    } else {

        ESP_LOGE("TAG", "================================");
        ESP_LOGE("TAG", "EXTERNAL_FLASH WRITE/READ TEST: FAIL");
        ESP_LOGE("TAG", "================================");

        ret = ESP_FAIL;
    }


cleanup:

    spi_bus_remove_device(dev);

    return ret;
}

