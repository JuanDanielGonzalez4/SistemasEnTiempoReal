idf_component_register(SRCS "ntp.c" "rgb_led.c" "wifi_app.c" "http_server.c" "main.c" "adc.c"
                    INCLUDE_DIRS "."
                    EMBED_FILES webpage/app.css webpage/app.js webpage/favicon.ico webpage/index.html webpage/jquery-3.3.1.min.js)
