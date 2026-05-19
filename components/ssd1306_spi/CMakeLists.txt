idf_component_register(SRCS "ssd1306_spi.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_driver_spi esp_common
                    PRIV_REQUIRES esp_driver_gpio freertos log)
