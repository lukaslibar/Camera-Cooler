#pragma once

#include <cstdint>

/* INA219 stare definicije
constexpr uint8_t INA_ADDRESS = 0x40;
constexpr uint REG_CONFIG = 0x00;
constexpr uint REG_CALIBRATION = 0x05;
constexpr uint REG_BUSVOLT = 0x02;
constexpr uint REG_SHUNTVOLT = 0x01;
constexpr uint REG_CURRENT = 0x04;
constexpr uint REG_POWER = 0x03;
/* CONFIG: 16V bus range, gain ÷1 (±40mV shunt), 12-bit 32-sample averaging, continuous bus+shunt
 bit13=0 (16V), bits[12:11]=00 (÷1)*, bits[10:7]=0xD (32S), bits[6:3]=0xD (32S), bits[2:0]=7
constexpr uint CONFIG_VALUE 0x06EF;
constexpr uint CALIBRATION_VALUE = 4096;
constexpr double CURRENT_LSB_MA = 0.05; // 0.04096 / (4096 × 0.2 Ω) × 1000 mA/LSB
constexpr double POWER_LSB_MW = 2.0; // 20 × CURRENT_LSB_MA mW/LSB
/* Shunt voltage: signed 16-bit, 10 µV/LSB = 0.01 mV/LSB
 Bus voltage: bits [15:3], 4 mV/LSB*; bit 1 = conversion ready, bit 0 = math overflow
constexpr double VSHUNT_LSB_MV = 0.01;
constexpr double VBUS_LSB_MV = 4.0;
constexpr uint BUSVOLT_OVF = 0x01; */

namespace ina219 {
    // INA219 I2C naslov
    constexpr std::uint8_t ADDRESS = 0x40;

    constexpr std::uint8_t REG_CONFIG      = 0x00;
    constexpr std::uint8_t REG_SHUNTVOLT   = 0x01;
    constexpr std::uint8_t REG_BUSVOLT     = 0x02;
    constexpr std::uint8_t REG_POWER       = 0x03;
    constexpr std::uint8_t REG_CURRENT     = 0x04;
    constexpr std::uint8_t REG_CALIBRATION = 0x05;
    // CONFIG register:
    // 16 V bus range
    // ±40 mV shunt range
    // 32-sample averaging
    // continuous bus + shunt conversion
    constexpr std::uint16_t CONFIG_VALUE = 0x06EF;
    constexpr std::uint16_t CALIBRATION_VALUE = 4096;

    constexpr double CURRENT_LSB_MA = 0.05;
    constexpr double POWER_LSB_MW = 2.0;
    constexpr double VSHUNT_LSB_MV = 0.01;
    constexpr double VBUS_LSB_MV = 4.0;

    constexpr std::uint16_t BUSVOLT_OVF = 0x01;

}
