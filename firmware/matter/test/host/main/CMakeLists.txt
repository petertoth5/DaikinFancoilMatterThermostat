file(GLOB test_sources "${CMAKE_CURRENT_LIST_DIR}/test_*.c")

idf_component_register(SRCS test_main.c ${test_sources}
                       REQUIRES unity reg_model sim_source mapping
                       WHOLE_ARCHIVE)  # keep TEST_CASE registrations from being dropped by the linker

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
