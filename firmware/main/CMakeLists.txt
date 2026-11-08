if(CONFIG_DMT_BUS_WRITE)
    message(FATAL_ERROR "CONFIG_DMT_BUS_WRITE: phase 3 is not specified yet (v0.3)")
endif()

idf_component_register(SRCS "main.c" INCLUDE_DIRS ".")
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
