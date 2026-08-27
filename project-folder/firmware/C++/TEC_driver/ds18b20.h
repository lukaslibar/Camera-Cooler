#pragma once

#include <cstdint>

using uint = unsigned int;

// Function commands for DS18B20 1-Wire temperature sensor
// https://www.analog.com/en/products/ds18b20.html

namespace ds18b20 {

    constexpr uint PIN1 = 7;
    constexpr uint PIN2 = 8;

    constexpr std::uint8_t CONVERT_T         = 0x44;
    constexpr std::uint8_t WRITE_SCRATCHPAD  = 0x4E;
    constexpr std::uint8_t READ_SCRATCHPAD   = 0xBE;
    constexpr std::uint8_t COPY_SCRATCHPAD   = 0x48;
    constexpr std::uint8_t RECALL_EE         = 0xB8;
    constexpr std::uint8_t READ_POWER_SUPPLY = 0xB4;

}
