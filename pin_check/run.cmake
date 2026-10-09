# CMake script to run ST-LINK GDB server with semihosting and capture output to a file

if(NOT STLINK_GDBSERVER)
    find_program(STLINK_GDBSERVER NAMES ST-LINK_gdbserver ST-LINK_gdbserver.exe
        HINTS
            "$ENV{STM32CLT_PATH}/STLink-gdb-server/bin"
            "$ENV{STM32_CUBE_CLT_PATH}/STLink-gdb-server/bin"
    )
    if(NOT STLINK_GDBSERVER)
        set(STLINK_GDBSERVER "ST-LINK_gdbserver")
    endif()
endif()

if(NOT STM32_PROGRAMMER_DIR)
    find_path(STM32_PROGRAMMER_DIR NAMES STM32_Programmer_CLI STM32_Programmer_CLI.exe
        HINTS
            "$ENV{STM32_PRG_PATH}"
            "$ENV{STM32CLT_PATH}/STM32CubeProgrammer/bin"
            "$ENV{STM32_CUBE_CLT_PATH}/STM32CubeProgrammer/bin"
    )
endif()

if(NOT OUTPUT_FILE)
    set(OUTPUT_FILE "pin_check_output.log")
endif()

if(NOT TIMEOUT_SEC)
    set(TIMEOUT_SEC 16)
endif()

set(ARGS --frequency 8000 -d --semihosting all --semihost-console-port 45464)
if(STM32_PROGRAMMER_DIR)
    list(APPEND ARGS -cp "${STM32_PROGRAMMER_DIR}")
endif()

message(STATUS "Launching ST-LINK GDB Server for ${TIMEOUT_SEC} seconds...")
message(STATUS "GDB Server  : ${STLINK_GDBSERVER}")
message(STATUS "Programmer  : ${STM32_PROGRAMMER_DIR}")
message(STATUS "Output File : ${OUTPUT_FILE}")

execute_process(
    COMMAND "${STLINK_GDBSERVER}" ${ARGS}
    TIMEOUT ${TIMEOUT_SEC}
    OUTPUT_FILE "${OUTPUT_FILE}"
    ERROR_FILE "${OUTPUT_FILE}"
    RESULT_VARIABLE RES
)

message(STATUS "GDB server session ended. Output saved to: ${OUTPUT_FILE}")
