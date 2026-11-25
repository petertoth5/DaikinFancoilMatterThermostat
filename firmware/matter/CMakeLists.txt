cmake_minimum_required(VERSION 3.22)

# Only the two pure phase-1 components; no bus code is built into this image (task 015).
set(EXTRA_COMPONENT_DIRS ../components/daikin_regs ../components/reg_model)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(daikin_matter_skeleton)

# Bus guard: self-test and source scan before the link, either failing fails the build.
add_custom_target(no_bus_sources
    COMMAND ${PYTHON} tools/check_no_bus.py --self-test
    COMMAND ${PYTHON} tools/check_no_bus.py --sources ${CMAKE_CURRENT_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Bus guard: sources"
    VERBATIM)
add_dependencies(${CMAKE_PROJECT_NAME}.elf no_bus_sources)

# Symbol check on the linked image: either a forbidden bus symbol or an unusable nm output fails the build.
add_custom_command(TARGET ${CMAKE_PROJECT_NAME}.elf POST_BUILD
    COMMAND ${PYTHON} tools/check_no_bus.py --nm ${CMAKE_NM} $<TARGET_FILE:${CMAKE_PROJECT_NAME}.elf>
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Bus guard: ELF symbols"
    VERBATIM)
