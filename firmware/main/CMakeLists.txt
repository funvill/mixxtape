idf_component_register(
    SRCS "main.c"
         "factory_main.c"
    INCLUDE_DIRS "include"
    REQUIRES tape ui leds factory driver esp_timer
)

# Factory test image for the jig:  idf.py -DMIXXTAPE_FACTORY_TEST=ON build
if(MIXXTAPE_FACTORY_TEST)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE MIXXTAPE_FACTORY_TEST=1)
    message(STATUS "mixxtape: building FACTORY TEST image")
endif()
