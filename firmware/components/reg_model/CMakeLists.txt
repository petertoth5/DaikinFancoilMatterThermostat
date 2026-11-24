idf_component_register(SRCS "src/reg_model.c"
                       INCLUDE_DIRS "include"
                       REQUIRES daikin_regs)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
