
cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Raspberry Pi Pico VS Code configuration
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()

set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)

set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)

if(EXISTS ${picoVscode})
    include(${picoVscode})
endif()

# Target board
set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Import Pico SDK
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/pico_sdk_import.cmake")
    include(pico_sdk_import.cmake)
else()
    include("$ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake")
endif()

project(Motor_Controller C CXX ASM)

pico_sdk_init()

# Project source files
add_executable(Motor_Controller

    main.c

    motor_control.c
    motor_pwm.c
    motor_oled.c
    gpio_buttons.c
    motor_encoder.c

    motor_http.cpp
    AsyncHttpClient.cpp

    motor_rtc.cpp

)

# Header files
target_include_directories(Motor_Controller PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

# Required libraries
target_link_libraries(Motor_Controller
    pico_stdlib
    pico_multicore
    pico_util
    pico_aon_timer
    hardware_i2c
    hardware_pwm
    hardware_timer
    hardware_clocks
    hardware_sync
    pico_cyw43_arch_lwip_threadsafe_background
    pico_lwip_http
    pico_lwip_mbedtls
    pico_mbedtls
    m
)

# Serial communication
pico_enable_stdio_usb(Motor_Controller 1)
pico_enable_stdio_uart(Motor_Controller 0)

# Generate firmware files
pico_add_extra_outputs(Motor_Controller)
