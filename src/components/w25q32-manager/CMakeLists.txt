idf_component_register(SRCS "w25q32-manager.c"
                    INCLUDE_DIRS "include"
					REQUIRES spi_flash nvs_flash
				)
