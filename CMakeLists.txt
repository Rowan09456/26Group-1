
cmake_minimum_required(VERSION 3.13)

# Select Raspberry Pi Pico 2W
set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Locate Pico SDK using PICO_SDK_PATH
if(NOT DEFINED ENV{PICO_SDK_PATH})
    message(FATAL_ERROR "PICO_SDK_PATH is not set")
endif()

include("$ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake")

project(Motor_Controller C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# Motor Controller modules
add_executable(Motor_Controller
    main.c
    motor_control.c
    motor_pwm.c
    motor_oled.c
    gpio_buttons.c
    motor_encoder.c
)

target_link_libraries(Motor_Controller
    pico_stdlib
    hardware_i2c
    hardware_pwm
    hardware_timer
    hardware_clocks
    hardware_sync
    m
)

# Enable USB serial
pico_enable_stdio_usb(Motor_Controller 1)
pico_enable_stdio_uart(Motor_Controller 0)

# Generate UF2, HEX and BIN files
pico_add_extra_outputs(Motor_Controller)
