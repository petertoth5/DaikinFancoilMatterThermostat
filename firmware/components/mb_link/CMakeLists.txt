idf_component_register(SRCS "src/mb_link.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES rs485_port daikin_regs mb_rtu)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
