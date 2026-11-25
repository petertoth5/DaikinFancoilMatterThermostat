idf_component_register(SRCS "src/matter_bridge.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_matter esp_timer reg_model mapping)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
