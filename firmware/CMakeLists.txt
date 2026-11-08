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
