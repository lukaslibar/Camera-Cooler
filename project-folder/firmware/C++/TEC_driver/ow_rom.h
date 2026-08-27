#pragma once

#include <cstdint>

namespace ow {

    constexpr std::uint8_t READ_ROM    = 0x33;
    constexpr std::uint8_t MATCH_ROM    = 0x55;
    constexpr std::uint8_t SKIP_ROM     = 0xCC;
    constexpr std::uint8_t ALARM_SEARCH = 0xEC;
    constexpr std::uint8_t SEARCH_ROM   = 0xF0;

}
