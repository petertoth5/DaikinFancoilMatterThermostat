# The write glue and its console parser exist only in the write image (spec 019).
set(bus_task_srcs src/bus_task.c src/mb_console.c src/log_out.c)
if(CONFIG_DMT_BUS_WRITE)
    list(APPEND bus_task_srcs src/bus_write.c src/mb_console_write.c)
endif()

idf_component_register(SRCS ${bus_task_srcs}
                       INCLUDE_DIRS "include"
                       REQUIRES mb_link poll_sched reg_model daikin_regs write_guard console esp_timer freertos esp_app_format)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
