set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/common_components/led_strip)    

idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS ".")
