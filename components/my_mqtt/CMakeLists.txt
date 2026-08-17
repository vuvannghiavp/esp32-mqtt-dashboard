idf_component_register(
    SRCS "src/my_mqtt.c"
    INCLUDE_DIRS "include"
    REQUIRES sensor mqtt json
    EMBED_TXTFILES      "src/Cert/RootCA1.pem"
                        "src/Cert/private.pem"
                        "src/Cert/certificate.pem"
)