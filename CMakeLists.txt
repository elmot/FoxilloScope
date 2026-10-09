cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME FoxilloScope)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Generated sources
set_source_files_properties(${CMAKE_SOURCE_DIR}/version/version.h PROPERTIES GENERATED TRUE)

# Build-time generation of version.h
add_custom_target(generate_version ALL
    COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DBINARY_DIR=${CMAKE_BINARY_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/version.cmake
    BYPRODUCTS ${CMAKE_BINARY_DIR}/version.h ${CMAKE_CURRENT_SOURCE_DIR}/html/wifi.html
    COMMENT "Generating version.h..."
)
add_dependencies(${CMAKE_PROJECT_NAME} generate_version)

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        # Add user sources here
        Core/Src/oscilloscope.cpp
        Core/Src/oscilloscope_channel.cpp
        Core/Src/transmit.cpp
        Core/Src/data.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/version
    # Add user defined include paths
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)

# Generate .bin file post build
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${CMAKE_PROJECT_NAME}> $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/${CMAKE_PROJECT_NAME}.bin
    COMMENT "Generating ${CMAKE_PROJECT_NAME}.bin"
)

# ==============================================================================
# Pin Check Firmware Target
# ==============================================================================
add_executable(pin_check
    pin_check/main.c
    pin_check/pin_defs.c
    Core/Src/sysmem.c
    startup_stm32g474xx.s
)

target_include_directories(pin_check PRIVATE
    Core/Inc
)

# stm32cubemx is an INTERFACE library (include paths and defines only):
# no HAL driver objects, FreeRTOS or generated init code are linked.
target_link_libraries(pin_check
    stm32cubemx
    STM32_Drivers
    m
)

# Console output goes through semihosting (needs a debugger attached)
target_link_options(pin_check PRIVATE --specs=rdimon.specs)

set_target_properties(pin_check PROPERTIES ADDITIONAL_CLEAN_FILES pin_check.map)

add_custom_command(TARGET pin_check POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:pin_check> $<TARGET_FILE_DIR:pin_check>/pin_check.bin
    COMMENT "Generating pin_check.bin"
)

# ==============================================================================
# Run Pin Check via ST-LINK GDB Server Target
# ==============================================================================
add_custom_target(run_pin_check
    COMMAND ${CMAKE_COMMAND}
        -DOUTPUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/pin_check_output.log
        -DTIMEOUT_SEC=6
        -P ${CMAKE_CURRENT_SOURCE_DIR}/pin_check/run.cmake
    DEPENDS pin_check
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running ST-LINK GDB Server and capturing output to pin_check_output.log..."
    USES_TERMINAL
)



