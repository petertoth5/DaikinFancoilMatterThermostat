# The FC06 write path exists only in the write image (spec 019).
set(mb_link_srcs src/mb_link.c)
if(CONFIG_DMT_BUS_WRITE)
    list(APPEND mb_link_srcs src/mb_link_write.c)
endif()

idf_component_register(SRCS ${mb_link_srcs}
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES rs485_port daikin_regs mb_rtu)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
