# write_guard is the write-image safety layer (spec 019). Its sources are compiled only in the write
# image and in host tests; the read image sees the headers only, so no write_guard_ symbol can link.
if(CONFIG_DMT_BUS_WRITE OR IDF_TARGET STREQUAL "linux")
    idf_component_register(SRCS "src/write_guard.c" "src/wg_format.c"
                           INCLUDE_DIRS "include"
                           REQUIRES poll_sched reg_model daikin_regs)
    target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
else()
    idf_component_register(INCLUDE_DIRS "include"
                           REQUIRES poll_sched reg_model daikin_regs)
endif()
