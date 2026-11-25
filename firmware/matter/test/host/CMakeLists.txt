cmake_minimum_required(VERSION 3.22)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Only the pure components: matter_bridge (esp_matter) is never built for the host.
get_filename_component(phase1_components "${CMAKE_CURRENT_LIST_DIR}/../../../components" ABSOLUTE)
get_filename_component(matter_components "${CMAKE_CURRENT_LIST_DIR}/../../components" ABSOLUTE)
list(APPEND EXTRA_COMPONENT_DIRS
     "${phase1_components}/daikin_regs" "${phase1_components}/reg_model"
     "${matter_components}/sim_source" "${matter_components}/mapping")
set(COMPONENTS main)
project(matter_host_tests)
