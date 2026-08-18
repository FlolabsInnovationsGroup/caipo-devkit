idf_component_register(SRCS "spk_max98357.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES esp_driver_gpio log freertos esp_driver_i2s caipo_bsp)