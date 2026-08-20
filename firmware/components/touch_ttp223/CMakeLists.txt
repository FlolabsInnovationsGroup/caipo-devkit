idf_component_register(SRCS "touch_ttp223.c" "touch_gesture.c"
                       INCLUDE_DIRS "include"
                       REQUIRES caipo_bsp
                       PRIV_REQUIRES driver esp_timer)
