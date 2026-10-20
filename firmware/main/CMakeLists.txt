idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES ble_link r03_transfer touch_ttp223 mic_inmp441 spk_max98357
                                     nvs_flash esp_timer log)
