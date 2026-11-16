cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico2_w)

include(pico_sdk_import.cmake)

project(Motor_Controller C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(Motor_Controller
    Motor_Controller.c
)

target_link_libraries(Motor_Controller
    pico_stdlib
    hardware_i2c
    hardware_pwm
    hardware_timer
    hardware_pio
    hardware_watchdog
    hardware_clocks
    m
)

pico_enable_stdio_usb(Motor_Controller 1)
pico_enable_stdio_uart(Motor_Controller 0)

pico_add_extra_outputs(Motor_Controller)
