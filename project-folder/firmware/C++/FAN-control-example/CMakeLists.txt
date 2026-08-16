cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(FenEN C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(fen_kontrol
        fen_kontrol.cpp
)

# pull in common dependencies
target_link_libraries(fen_kontrol pico_stdlib hardware_pwm hardware_gpio hardware_clocks)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(fen_kontrol)
