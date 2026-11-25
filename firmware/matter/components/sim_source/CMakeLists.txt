idf_component_register(SRCS "src/sim_source.c"
                       INCLUDE_DIRS "include"
                       REQUIRES reg_model daikin_regs)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
