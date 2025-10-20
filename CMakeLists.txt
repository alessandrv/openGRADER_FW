cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Keyboard selection (can be overridden via -DKEYBOARD=onekey)
if(NOT DEFINED KEYBOARD)
    set(KEYBOARD "standard")
endif()

message(STATUS "Building for keyboard: ${KEYBOARD}")

# Set the project name
set(CMAKE_PROJECT_NAME TINYUSBTEST)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# TinyUSB core sources
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tinyusb/src ${CMAKE_CURRENT_BINARY_DIR}/tinyusb_build)

# Pull in TinyUSB sources and device controller driver
tinyusb_target_add(${CMAKE_PROJECT_NAME})
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/usb_app.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/usb_descriptors.c
    ${CMAKE_CURRENT_SOURCE_DIR}/tinyusb/src/portable/st/stm32_fsdev/dcd_stm32_fsdev.c
)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/input/matrix.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/input/encoder.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/input/key_state.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/input/board_layout.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/input/keymap.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/input/slider.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/input/magnetic_switch.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/midi_handler.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/i2c_manager.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/config_protocol.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/eeprom_emulation.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/ws2812.c
    # Add keyboard-specific keymap
    ${CMAKE_CURRENT_SOURCE_DIR}/keyboards/${KEYBOARD}/keymap.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc/input
    ${CMAKE_CURRENT_SOURCE_DIR}/tinyusb/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tinyusb/src/common
    ${CMAKE_CURRENT_SOURCE_DIR}/tinyusb/src/device
    ${CMAKE_CURRENT_SOURCE_DIR}/tinyusb/src/class
    ${CMAKE_CURRENT_SOURCE_DIR}/tinyusb/src/typec
    ${CMAKE_CURRENT_SOURCE_DIR}/tinyusb/src/portable/st/stm32_fsdev
    # Add keyboards directory for config includes
    ${CMAKE_CURRENT_SOURCE_DIR}/keyboards
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add keyboard config header - use angle brackets include format
    KEYBOARD_CONFIG_HEADER=<${KEYBOARD}/config.h>
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
