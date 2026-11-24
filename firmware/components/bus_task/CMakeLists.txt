idf_component_register(SRCS "src/bus_task.c" "src/mb_console.c" "src/log_out.c"
                       INCLUDE_DIRS "include"
                       REQUIRES mb_link poll_sched reg_model daikin_regs console esp_timer freertos esp_app_format)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
