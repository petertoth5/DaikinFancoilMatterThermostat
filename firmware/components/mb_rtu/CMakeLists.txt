# The FC06 codec exists only in the write image and in host tests (spec 019).
set(mb_rtu_srcs src/mb_rtu.c)
if(CONFIG_DMT_BUS_WRITE OR IDF_TARGET STREQUAL "linux")
    list(APPEND mb_rtu_srcs src/mb_rtu_write.c)
endif()

idf_component_register(SRCS ${mb_rtu_srcs}
                       INCLUDE_DIRS "include")

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
