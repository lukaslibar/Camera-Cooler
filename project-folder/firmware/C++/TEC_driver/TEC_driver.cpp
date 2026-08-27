// Program, ki vključuje PID regulacijo za nadziranje temperature paltierovega elementa.

#include <iostream>
#include <string>
#include <array>
#include <cassert>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/pwm.h"
#include "ina219.h"
#include "pwm_fans.h"
#include "pid_constants.h"
#include "onewire_library.h"
#include "ow_rom.h"
#include "ds18b20.h"

using uint = unsigned int;

// I2C
constexpr uint SDA_PIN = 4;
constexpr uint SCL_PIN = 5;
// DAC
constexpr std::uint8_t DAC_ADDRESS = 0x60;
constexpr double VDD = 3.3;

void write_DAC(double DAC_voltage)
{
    // Pretvorba napetosti v 12-bitno vrednost
    std::uint16_t Dn = static_cast<std::uint16_t>((DAC_voltage * 4095.0) / VDD); // static_cast bolj pregledno razširi 12b v 16b
    std::array<std::uint8_t, 2> buf;
    buf[0] = (Dn >> 8) & 0x0F; // Fast Mode: C2=0, C1=0, PD1=0, PD0=0, D11-D8
    buf[1] = Dn & 0xFF; // D7-D0

    int ret = i2c_write_blocking(i2c0, DAC_ADDRESS, buf.data(), buf.size(), false); // 3. argument pričakuje kazalec .data() -> kazalec na 1. element
    assert(ret == 2);
}

void config_ina(std::uint8_t reg, std::uint16_t value)
    // konfigurura INA219 registre
{
    std::array<std::uint8_t, 3> buf = {reg, static_cast<std::uint8_t>(value >> 8), static_cast<std::uint8_t>(value & 0xFF)};
    int ret = i2c_write_blocking(i2c0, ina219::ADDRESS, buf.data(), buf.size(), false);
    assert(ret == 3);
}

uint16_t read_ina(std::uint8_t reg)
    // prebere vrednosti registrov in izračuna napetost, tok in moč ter jih zapiše v array
{
    int ret = i2c_write_blocking(i2c0, ina219::ADDRESS, &reg, 1, true); // true -> ohranim bus brez stop pogoja, da lahko takoj berem
    assert(ret == 1);
    std::array<std::uint8_t, 2> buf;
    ret = i2c_read_blocking(i2c0, ina219::ADDRESS, buf.data(), buf.size(), false);
    assert(ret == 2);

    return static_cast<std::uint16_t>(buf[0] << 8) | buf[1];
}

double calculate_ina(std::string measurement)
    /* argumenti: vin_v, vbus_v, vshunt_mv, ma, mw
    glede na željeno meritev jo izračuna in vrne */
{
    assert(measurement == vin_v||vbus_v||vshunt_mv||ma||mw);

    std::uint16_t bus_raw = read_ina(ina219::REG_BUSVOLT);

    if (bus_raw & ina219::BUSVOLT_OVF)
        std::cout << "Math overflow - out of range\n";

    if (measurement == "vin_v"){
        double vshunt_mv = static_cast<std::int16_t>(read_ina(ina219::REG_SHUNTVOLT)) * ina219::VSHUNT_LSB_MV;
        double vbus_v = (bus_raw >> 3) * ina219::VBUS_LSB_MV / 1000.0;
        double vin_v = vbus_v + vshunt_mv / 1000.0; // vin_v je + na strani napajanja
        return vin_v;
    }
    else if (measurement == "vshunt_mv"){
        double vshunt_mv = static_cast<std::int16_t>(read_ina(ina219::REG_SHUNTVOLT)) * ina219::VSHUNT_LSB_MV; //10 µV/LSB
        return vshunt_mv;
    }
    else if (measurement == "vbus_v"){
        std::uint16_t bus_raw = read_ina(ina219::REG_BUSVOLT); // vbus napetost je le pozitivna, biti so [15,3], 4 mV/LSB, bit 0 -> prekoračitev
        double vbus_v = (bus_raw >> 3) * ina219::VBUS_LSB_MV / 1000.0; // vbus_v je - na strani porabnika
        return vbus_v;
    }
    else if (measurement == "ma"){
        double ma = static_cast<std::int16_t>(read_ina(ina219::REG_CURRENT)) * ina219::CURRENT_LSB_MA; // CURRENT_LSB_MA mA/LSB
        return ma;
    }
    else if (measurement == "mw"){
        double mw = read_ina(ina219::REG_POWER) * ina219::POWER_LSB_MW; // le pozitivna, POWER_LSB_MW mW/LSB
        return mw;
    }

    return 0.0;
}

void fan_control(bool on, int pwm_pin, uint duty_cycle)
{
    gpio_set_function(pwm_pin, GPIO_FUNC_PWM);
    gpio_init(pwmfan::FEN_EN);

    uint slice_st = pwm_gpio_to_slice_num(pwm_pin);
    uint channel = pwm_gpio_to_channel(pwm_pin);
    std::uint32_t clock = clock_get_hz(clk_sys);

    std::uint8_t duty_cyclet = 100 - duty_cycle;
    uint wrap = clock / pwmfan::FREQ - 1;
    uint level = (duty_cyclet * (wrap + 1)) / 100;

    gpio_set_outover(pwm_pin, GPIO_OVERRIDE_NORMAL); // preda nadzor pwm

    pwm_set_clkdiv(slice_st, 1.0); // brez deljenja ure
    pwm_set_wrap(slice_st, wrap);
    pwm_set_chan_level(slice_st, channel, level);

    gpio_set_dir(pwmfan::FEN_EN, GPIO_IN); // GPIO mora biti nastavljen na vhod da lahko deluje v HiZ.

    if (on){
        pwm_set_enabled(slice_st, true);
        gpio_set_pulls(pwmfan::FEN_EN, false, false); // prižgan HiZ
    }
    else if (!on){
        gpio_set_outover(pwm_pin, GPIO_OVERRIDE_LOW); // prepiše pwm signal. zagotovi nizko stanje
        pwm_set_enabled(slice_st, false); // lahko da bo treba eksplicitno določiti stanje pinov
        gpio_set_pulls(pwmfan::FEN_EN, false, true); // vgasnen
    }
}

bool init_ds18b20(OW* ow, PIO pio, uint offset)
{
    if (!ow_init(ow, pio, offset, ds18b20::PIN2)){
        std::cout << "Initialization of OW on GPIO " << ds18b20::PIN2 << " failed!"; // za enkrat naj bo le en termometer
        return false;
    }

    if (!ow_reset(ow)){
        std::cout << "DS18B20 on GPIO " << ds18b20::PIN2 << " not found!";
        return false;
    }

    return true;
}

double read_ds18b20(OW* ow) // * -> pričakuje kazalec
    // prebere vrednost na izbranem termometru in vrne temperaturo v °C
{
    ow_reset(ow);
    ow_send(ow, ow::SKIP_ROM); // samo 1 naprava na enem busu
    ow_send(ow, ds18b20::CONVERT_T);

    while (ow_read(ow) == 0); // ko je pretvorba končna ds18b20 vrne 1 -> zaključi zanko

    ow_reset(ow);
    ow_send(ow, ow::SKIP_ROM);
    ow_send(ow, ds18b20::READ_SCRATCHPAD);

    std::uint8_t lsb = ow_read(ow);
    std::uint8_t msb = ow_read(ow);

    std::int16_t raw_temperature = static_cast<std::int16_t>(static_cast<std::uint16_t>(msb) << 8 | lsb);

    return raw_temperature / 16.0;
}

double pid_controller(double temp_current, double temp_desired)
    /* implimentacija PID regulacije, povratna zanka -> vhod: izmerjena temp
    (temp_current > temp_desired) izhod: napotost na DAC(0 V - 3,3 V) */
{
    static double previous_error = 0.0;
    static double integral = 0.0;
    static bool first_run = true;

    double error = temp_current - temp_desired;

    // na prvem klicu ni prejšnje napake
    // poskrbi, da je prvi odvod 0
    if (first_run){
        previous_error = error;
        assert(temp_current > temp_desired); // samo prepreči napako uporabnika
        first_run = false;
    }

    integral += error * pid::dT;
    double derivative = (error - previous_error) / pid::dT;
    double DAC_voltage = pid::Kp*error + pid::Ki*integral + pid::Kd*derivative;

    // omejitev izhoda DAC-a
    if (DAC_voltage > VDD)
        DAC_voltage = VDD;
    else if (DAC_voltage < 0)
        DAC_voltage = 0;

    previous_error = error;
    return 1.0 / DAC_voltage;
}

int main()
{
    stdio_init_all();
    sleep_ms(2000); // počakaj, da se odpre USB Serial
    i2c_init(i2c0, 100000);
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);

    // OneWire konfiguracija
    PIO pio = pio0;
    uint offset;
    if (!pio_can_add_program(pio, &onewire_program)){
        std::cout << "Could not add 1-Wire PIO program\n";
        return 1;
    }
    offset = pio_add_program(pio, &onewire_program);
    OW ow1;
    //OW ow2; za enkrat le en termometer
    if (!init_ds18b20(&ow1, pio, offset))
        return 2;

    // DAC konfiguracija
    double DAC_voltage = VDD; // privzeta napetost na DAC, ne hladi
    write_DAC(DAC_voltage);

    // INA219 konfiguracija
    config_ina(ina219::REG_CONFIG, ina219::CONFIG_VALUE);
    config_ina(ina219::REG_CALIBRATION, ina219::CALIBRATION_VALUE);

    // sprobaj ventilatorja
    fan_control(false, pwmfan::PIN1, 0);
    fan_control(false, pwmfan::PIN2, 0);
    sleep_ms(1000);
    fan_control(true, pwmfan::PIN1, 30);
    fan_control(true, pwmfan::PIN2, 30);
    sleep_ms(1000);
    fan_control(false, pwmfan::PIN1, 0);
    fan_control(false, pwmfan::PIN2, 0);
    sleep_ms(1000);

    double temp_desired = 10.0;

    // Kaj bi rad pošiljal nazaj: temperatura, napetost na DAC, INA219 napetost in tok.

    fan_control(true, pwmfan::PIN1, 20);
    fan_control(true, pwmfan::PIN2, 20);

    while (true){
        config_ina(ina219::REG_CALIBRATION, ina219::CALIBRATION_VALUE); // INA219 je občutljiva na tokovne sunke -> vsakič resetiraš kalibracijo

        double temp_current = read_ds18b20(&ow1);

        double DAC_voltage = pid_controller(temp_current, temp_desired);

        write_DAC(DAC_voltage);

        std::cout << "T: " << temp_current << "°C, " << " dT: " << temp_current - temp_desired << "°C" << std::endl;
        std::cout << "DAC voltage: " << DAC_voltage << "V" << std::endl;
        std::cout << "INA219 data: " << calculate_ina("ma") << "mA, " << calculate_ina("vin_v") << "V"<< std::endl;

        double dTm = pid::dT * 1000.0;
        sleep_ms(dTm);
    }

    return 0;
}
