# FoxilloScope Build-Time Version Generator

if(NOT DEFINED SOURCE_DIR)
    set(SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}")
endif()

if(NOT DEFINED BINARY_DIR)
    set(BINARY_DIR "${CMAKE_BINARY_DIR}")
endif()

string(TIMESTAMP CURRENT_DATE "%Y%m%d")

execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY "${SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE GIT_RES
    ERROR_QUIET
)

if(NOT GIT_RES EQUAL 0 OR NOT GIT_HASH)
    set(BUILD_VERSION "unknown")
else()
    set(BUILD_VERSION "${CURRENT_DATE}-${GIT_HASH}")
endif()

message(STATUS "FoxilloScope Build Version: ${BUILD_VERSION}")

file(WRITE "${SOURCE_DIR}/version/version.h" "#ifndef BUILD_VERSION\n#define BUILD_VERSION \"${BUILD_VERSION}\"\n#endif\n")

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/html")
file(WRITE "${CMAKE_CURRENT_LIST_DIR}/html/version.txt" "${BUILD_VERSION}")
endif ()


