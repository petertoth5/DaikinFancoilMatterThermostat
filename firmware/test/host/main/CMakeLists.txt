# Test sources: main/test_*.c plus every component's test_host/test_*.c.
file(GLOB test_sources
    "${CMAKE_CURRENT_LIST_DIR}/test_*.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../../components/*/test_host/test_*.c")

# Later tasks append their component to REQUIRES.
idf_component_register(SRCS test_main.c ${test_sources}
                       REQUIRES unity
                       WHOLE_ARCHIVE)  # keep TEST_CASE registrations from being dropped by the linker

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
# Directory of the probe-log fixtures (created by task 011).
get_filename_component(fixture_dir "${CMAKE_CURRENT_LIST_DIR}/../../fixtures" ABSOLUTE)
target_compile_definitions(${COMPONENT_LIB} PRIVATE FIXTURE_DIR="${fixture_dir}")
