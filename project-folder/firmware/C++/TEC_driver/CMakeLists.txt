cmake_minimum_required(VERSION 3.13)

# mora biti nastavljeno pred include(pico_sdk_import.cmake)
set(PICO_PLATFORM rp2350-arm-s)
set(PICO_BOARD TEC_driver)
set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/boards)

include(pico_sdk_import.cmake)

project(TEC_driver C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(TEC_driver
        TEC_driver.cpp
)
add_subdirectory(onewire_library)

target_link_libraries(TEC_driver
        pico_stdlib
        hardware_i2c
        hardware_pio
        hardware_pwm
        onewire_library
)

pico_add_extra_outputs(TEC_driver)
pico_enable_stdio_usb(TEC_driver 1)
