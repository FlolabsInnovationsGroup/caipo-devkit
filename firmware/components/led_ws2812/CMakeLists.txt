idf_component_register(SRCS "led_ws2812.c"
                    INCLUDE_DIRS "include"
                    REQUIRES led_strip
                    PRIV_REQUIRES esp_driver_gpio log caipo_bsp)
