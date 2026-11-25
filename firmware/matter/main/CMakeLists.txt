idf_component_register(SRCS "app_main.cpp" "thermostat_ep.cpp" "reset_button.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_matter nvs_flash esp_driver_gpio)

# Our sources only; managed_components and IDF keep their own warning settings.
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
