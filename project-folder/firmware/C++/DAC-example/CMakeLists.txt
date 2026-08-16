cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(FenEN C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(DAC
        DAC.cpp
)

# pull in common dependencies
target_link_libraries(DAC
        pico_stdlib
        hardware_i2c
)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(DAC)
pico_enable_stdio_usb(DAC 1)
