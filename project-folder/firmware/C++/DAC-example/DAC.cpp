#include <iostream>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "pico/binary_info.h"

constexpr uint8_t DAC_ADDRESS = 0x60;
constexpr float VDD = 3.3f; // 3.3 je sprva double zato nakoncu f, da ne pride do pretvorbe
constexpr float DAC_VOLTAGE = 3.3f;
constexpr uint SDA_PIN = 4;
constexpr uint SCL_PIN = 5;

static void write_dac(float voltage)
{

    if (voltage < 0.0f)
        voltage = 0.0f;

    if (voltage > VDD)
        voltage = VDD;

    // Pretvorba napetosti v 12-bitno vrednost
    uint16_t Dn = static_cast<uint16_t>((voltage * 4095.0f) / VDD); // static_cast bolj pregledno pretvori 12b v 16b

    uint8_t buf[2];

    // Fast Mode:
    // C2=0, C1=0, PD1=0, PD0=0, D11-D8
    buf[0] = (Dn >> 8) & 0x0F;

    // D7-D0
    buf[1] = Dn & 0xFF;

    int ret = i2c_write_blocking(i2c0, DAC_ADDRESS, buf, 2, false);

    if (ret != 2)
    {
        std::cout << ("I2C napaka! ret = %d", ret) << std::endl;
    }
}

int main()
{
    stdio_init_all();

    sleep_ms(2000); // počakaj, da se odpre USB Serial

    std::cout << ("MCP4725 test") << std::endl;

    bi_decl(bi_2pins_with_func(SDA_PIN, SCL_PIN, GPIO_FUNC_I2C));
    bi_decl(bi_program_description("MCP4725 DAC Example"));

    i2c_init(i2c0, 100000);

    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);

    write_dac(DAC_VOLTAGE);

    std::cout << ("DAC nastavljen na %d V", DAC_VOLTAGE) << std::endl;

    while (true)
    {
        tight_loop_contents();
    }
}
