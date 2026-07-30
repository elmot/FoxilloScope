# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

list(PREPEND SDKCONFIG_DEFAULTS
        "sdkconfig.secrets"
        "sdkconfig.defaults")

idf_build_set_property(MINIMAL_BUILD ON)
project(esp32-gateway)

set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/version/version.h PROPERTIES GENERATED TRUE)

add_custom_target(generate_version ALL
    COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${CMAKE_CURRENT_LIST_DIR} -DBINARY_DIR=${CMAKE_BINARY_DIR} -P ${CMAKE_CURRENT_LIST_DIR}/../version.cmake
    BYPRODUCTS ${CMAKE_CURRENT_LIST_DIR}/version/version.h
    COMMENT "Generating version.h..."
)

add_dependencies(__idf_main generate_version)

