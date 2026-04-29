idf_component_register(SRCS "project.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_adc esp_driver_gpio)
target_compile_options(${COMPONENT_LIB} PRIVATE 
    -Wno-unused-variable 
    -DESP_VERSION_6_0_0=1
)