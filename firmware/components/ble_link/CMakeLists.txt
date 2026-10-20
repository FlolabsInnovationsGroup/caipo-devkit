idf_component_register(SRCS "ble_link.c"
                       INCLUDE_DIRS "include"
                       REQUIRES bt
                       PRIV_REQUIRES nvs_flash log r03_transfer)
