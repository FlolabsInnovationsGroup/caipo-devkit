idf_component_register(SRCS "mic_inmp441.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES esp_driver_gpio log freertos esp_driver_i2s caipo_bsp)
