idf_component_register(SRCS "external_flash_manager.c"
                    INCLUDE_DIRS "include"
					REQUIRES
					spi_flash
					nvs_flash
					esp_driver_spi
					esp_driver_gpio
					PRIV_REQUIRES
					hardware_lock
				)
