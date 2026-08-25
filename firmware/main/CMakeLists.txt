idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "include"
    REQUIRES tape ui leds driver esp_timer
)
