idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES sensor my_mqtt esp_wifi nvs_flash
                     EMBED_TXTFILES "Cert/RootCA1.pem"
                                     "Cert/private.pem"
                                     "Cert/certificate.pem")
