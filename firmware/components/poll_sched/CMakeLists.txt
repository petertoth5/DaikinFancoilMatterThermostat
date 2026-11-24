idf_component_register(SRCS "src/poll_sched.c" "src/poll_step.c" "src/txn_log.c"
                       INCLUDE_DIRS "include"
                       REQUIRES daikin_regs reg_model)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
