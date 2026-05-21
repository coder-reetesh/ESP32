idf_component_register(SRCS "project.c" "wifi_program.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_adc esp_driver_gpio esp_timer esp_wifi nvs_flash esp_http_client)
target_compile_options(${COMPONENT_LIB} PRIVATE 
    -Wno-unused-variable 
    -DESP_VERSION_6_0_0=1
)