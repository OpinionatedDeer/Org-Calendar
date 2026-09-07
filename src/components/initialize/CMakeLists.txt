idf_component_register(SRCS "initialize.c"
                    INCLUDE_DIRS "include"
				REQUIRES
					esp_driver_spi
					esp_driver_gpio
				PRIV_REQUIRES
					eink-driver
					w25q32-manager
					hardware_lock
				)
