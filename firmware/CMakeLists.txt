cmake_minimum_required(VERSION 3.22)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(daikin_matter_thermostat)

# Build variants (spec 019): the write image only in build_write/, the read image anywhere else, so an
# image cannot be built into the other one's directory.
get_filename_component(dmt_build_dir_name ${CMAKE_BINARY_DIR} NAME)
if(CONFIG_DMT_BUS_WRITE AND NOT dmt_build_dir_name STREQUAL "build_write")
    message(FATAL_ERROR "CONFIG_DMT_BUS_WRITE=y (write image) must be built in a directory named build_write, not '${dmt_build_dir_name}'")
endif()
if(NOT CONFIG_DMT_BUS_WRITE AND dmt_build_dir_name STREQUAL "build_write")
    message(FATAL_ERROR "build_write is the write image directory: CONFIG_DMT_BUS_WRITE must be y (sdkconfig.defaults.write)")
endif()

# Bus-safety lint: self-test first, then the tree scan. Either failing fails the build.
add_custom_target(bus_access_lint
    COMMAND ${PYTHON} tools/check_bus_access.py --self-test
    COMMAND ${PYTHON} tools/check_bus_access.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Bus-access lint"
    VERBATIM)
add_dependencies(${CMAKE_PROJECT_NAME}.elf bus_access_lint)

# Symbols the image must contain (the write image's own are added by the check's write mode).
set(DMT_ELF_REQUIRED_SYMBOLS mb_link_read,mb_rtu_build_fc03)

# ELF symbol check on the linked image: self-test first, then the check. Either failing fails the build.
# The mode follows Kconfig: the read image must define no write symbol, the write image must define its own.
if(CONFIG_DMT_BUS_WRITE)
    set(dmt_elf_mode write)
else()
    set(dmt_elf_mode read)
endif()
set(dmt_elf_check_args --nm ${CMAKE_NM} --mode ${dmt_elf_mode})
if(DMT_ELF_REQUIRED_SYMBOLS)
    list(APPEND dmt_elf_check_args --require ${DMT_ELF_REQUIRED_SYMBOLS})
endif()
add_custom_command(TARGET ${CMAKE_PROJECT_NAME}.elf POST_BUILD
    COMMAND ${PYTHON} tools/check_elf_symbols.py --self-test
    COMMAND ${PYTHON} tools/check_elf_symbols.py ${dmt_elf_check_args} $<TARGET_FILE:${CMAKE_PROJECT_NAME}.elf>
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "ELF symbol check"
    VERBATIM)
