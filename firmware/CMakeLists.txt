cmake_minimum_required(VERSION 3.22)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(daikin_matter_thermostat)

# Bus-safety lint: self-test first, then the tree scan. Either failing fails the build.
add_custom_target(bus_access_lint
    COMMAND ${PYTHON} tools/check_bus_access.py --self-test
    COMMAND ${PYTHON} tools/check_bus_access.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Bus-access lint"
    VERBATIM)
add_dependencies(${CMAKE_PROJECT_NAME}.elf bus_access_lint)

# Symbols the image must contain. Empty until 006 links the read path (006 sets: mb_link_read,mb_rtu_build_fc03).
set(DMT_ELF_REQUIRED_SYMBOLS "")

# ELF symbol check on the linked image: self-test first, then the check. Either failing fails the build.
set(dmt_elf_check_args --nm ${CMAKE_NM})
if(DMT_ELF_REQUIRED_SYMBOLS)
    list(APPEND dmt_elf_check_args --require ${DMT_ELF_REQUIRED_SYMBOLS})
endif()
add_custom_command(TARGET ${CMAKE_PROJECT_NAME}.elf POST_BUILD
    COMMAND ${PYTHON} tools/check_elf_symbols.py --self-test
    COMMAND ${PYTHON} tools/check_elf_symbols.py ${dmt_elf_check_args} $<TARGET_FILE:${CMAKE_PROJECT_NAME}.elf>
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "ELF symbol check"
    VERBATIM)
