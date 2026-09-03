// Program, ki vključuje PID regulacijo za nadziranje temperature paltierovega elementa.

#include <iostream>
#include <string>
#include <array>
#include <cassert>
#include <sstream>
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

bool write_DAC(double DAC_voltage)
{
    std::uint16_t Dn = static_cast<std::uint16_t>((DAC_voltage * 4095.0) / VDD);
    std::array<std::uint8_t, 2> buf;
    buf[0] = (Dn >> 8) & 0x0F;
    buf[1] = Dn & 0xFF;

    int ret = i2c_write_blocking(i2c0, DAC_ADDRESS, buf.data(), buf.size(), false);
    if (ret != 2) {
        std::cout << "I2C error: DAC write failed (ret=" << ret << ").\n";
        return false;
    }
    return true;
}

bool config_ina(std::uint8_t reg, std::uint16_t value)
{
    std::array<std::uint8_t, 3> buf = {reg, static_cast<std::uint8_t>(value >> 8), static_cast<std::uint8_t>(value & 0xFF)};
    int ret = i2c_write_blocking(i2c0, ina219::ADDRESS, buf.data(), buf.size(), false);
    if (ret != 3) {
        std::cout << "I2C error: INA219 config failed (ret=" << ret << ").\n";
        return false;
    }
    return true;
}

uint16_t read_ina(std::uint8_t reg)
{
    int ret = i2c_write_blocking(i2c0, ina219::ADDRESS, &reg, 1, true);
    if (ret != 1) {
        std::cout << "I2C error: INA219 register select failed.\n";
        return 0; // sentinel - klicatelj naj ve da je to lahko neveljavno
    }

    std::array<std::uint8_t, 2> buf;
    ret = i2c_read_blocking(i2c0, ina219::ADDRESS, buf.data(), buf.size(), false);
    if (ret != 2) {
        std::cout << "I2C error: INA219 read failed.\n";
        return 0;
    }

    return static_cast<std::uint16_t>(buf[0] << 8) | buf[1];
}

double calculate_ina(std::string measurement)
    /* argumenti: vin_v, vbus_v, vshunt_mv, ma, mw
    glede na željeno meritev jo izračuna in vrne */
{
    std::uint16_t bus_raw = read_ina(ina219::REG_BUSVOLT);
    double vbus_v = (bus_raw >> 3) * ina219::VBUS_LSB_MV / 1000.0;

    if (bus_raw & ina219::BUSVOLT_OVF)
        std::cout << "Math overflow - out of range.\n";

    if (measurement == "vin_v"){
        double vshunt_mv = static_cast<std::int16_t>(read_ina(ina219::REG_SHUNTVOLT)) * ina219::VSHUNT_LSB_MV;
        double vin_v = vbus_v + vshunt_mv / 1000.0; // vin_v je + na strani porabnika
        return vin_v;
    }
    else if (measurement == "vshunt_mv"){
        double vshunt_mv = static_cast<std::int16_t>(read_ina(ina219::REG_SHUNTVOLT)) * ina219::VSHUNT_LSB_MV; //10 µV/LSB
        return vshunt_mv;
    }
    else if (measurement == "vbus_v"){
        return vbus_v; // vbus_v je na strani napajanja
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

namespace {
    bool fan1_on = false; // lahko v 2 funkcijah uporabljam
    bool fan2_on = false;
}

void update_fan_enable()
{
    if (fan1_on || fan2_on){
        gpio_set_dir(pwmfan::FEN_EN, GPIO_IN);
        gpio_set_pulls(pwmfan::FEN_EN, false, false); // HiZ -> omogoči
    }
    else{
        gpio_set_dir(pwmfan::FEN_EN, GPIO_IN);
        gpio_set_pulls(pwmfan::FEN_EN, false, true); // pull-down -> onemogoči
    }
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

    gpio_set_outover(pwm_pin, GPIO_OVERRIDE_NORMAL);
    pwm_set_clkdiv(slice_st, 1.0);
    pwm_set_wrap(slice_st, wrap);
    pwm_set_chan_level(slice_st, channel, level);

    // posodobi stanje samo za ventilator, ki ga ta klic zadeva
    if (pwm_pin == static_cast<int>(pwmfan::PIN1))
        fan1_on = on;
    else if (pwm_pin == static_cast<int>(pwmfan::PIN2))
        fan2_on = on;

    if (on)
        pwm_set_enabled(slice_st, true);
    else{
        gpio_set_outover(pwm_pin, GPIO_OVERRIDE_LOW);
        pwm_set_enabled(slice_st, false);
    }

    update_fan_enable(); // FEN_EN se zdaj postavi glede na oba ventilatorja
}

bool init_ds18b20(OW* ow, PIO pio, uint offset)
{
    if (!ow_init(ow, pio, offset, ds18b20::PIN2)){
        std::cout << "Initialization of OW on GPIO " << ds18b20::PIN2 << " failed!\n"; // za enkrat naj bo le en termometer
        return false;
    }

    if (!ow_reset(ow)){
        std::cout << "DS18B20 on GPIO " << ds18b20::PIN2 << " not found!\n";
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

    sleep_ms(750);
    //while (ow_read(ow) == 0) // ko je pretvorba končna ds18b20 vrne 1 -> zaključi zanko

    ow_reset(ow);
    ow_send(ow, ow::SKIP_ROM);
    ow_send(ow, ds18b20::READ_SCRATCHPAD);

    std::uint8_t lsb = ow_read(ow);
    std::uint8_t msb = ow_read(ow);

    std::int16_t raw_temperature = static_cast<std::int16_t>(static_cast<std::uint16_t>(msb) << 8 | lsb);

    return raw_temperature / 16.0;
}

bool is_valid_int(const std::string& s)
    // preverjanje če je pravilen vnos
{
    if (s.empty())
        return false;

    std::size_t start = (s[0] == '-' || s[0] == '+') ? 1 : 0; // if/else izniči predznak
    if (start == s.size()) // sam predznak brez številk
        return false;

    for (std::size_t i = start; i < s.size(); ++i){
        if (!std::isdigit(static_cast<unsigned char>(s[i])))
            return false;
    }
    return true;
}

bool is_valid_double(const std::string& s)
    // preverjanje če je pravilen vnos
{
    if (s.empty())
        return false;

    std::size_t i = 0;
    if (s[i] == '-' || s[i] == '+') // izpusti predznak
        ++i;

    bool has_digits = false;
    bool has_dot = false;

    for (; i < s.size(); ++i) {
        if (std::isdigit(static_cast<unsigned char>(s[i]))){
            has_digits = true;
        }
        else if (s[i] == '.' && !has_dot)
            has_dot = true;
        else
            return false;
    }
    return has_digits;
}

bool read_USB(std::array<std::string, 4>& usb_data)
{
    static std::string raw_data;
    int c;
    int chars_read = 0;
    constexpr int MAX_CHARS_PER_CALL = 512;
    constexpr std::size_t MAX_LINE_LEN = 128; // preprečim da se spomin zapolni

    while ((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT){
        if (c == '\n'){
            std::stringstream ss(raw_data);
            raw_data.clear();

            for (std::size_t i = 0; i < usb_data.size(); ++i){
                if (!std::getline(ss, usb_data[i], ','))
                    return false;
            }

            if (usb_data[0] != "SET")
                return false;

            if (!is_valid_int(usb_data[2])){
                std::cout << "Invalid fan number in USB data.\n";
                return false;
            }

            int fan = std::stoi(usb_data[2]); // varno, ker smo že preverili is_valid_int

            if (fan == 1)
                usb_data[2] = std::to_string(pwmfan::PIN1);
            else if (fan == 2)
                usb_data[2] = std::to_string(pwmfan::PIN2);
            else
                return false;

            std::cout << "ACK\n" << std::flush;
            return true;
        }

        raw_data += static_cast<char>(c);

        if (raw_data.size() > MAX_LINE_LEN){
            std::cout << "USB line too long, discarding.\n";
            raw_data.clear();
        }

        if (++chars_read > MAX_CHARS_PER_CALL){
            break;
        }
    }

    return false;
}

double pid_controller(double temp_current, double temp_desired, bool reset_integral)
{
    static double previous_error = 0.0;
    static double integral = 0.0;
    static bool first_run = true;

    if (reset_integral){
        integral = 0.0;
        first_run = true; // da se tudi odvod ne izračuna napačno na naslednjem klicu
    }

    double error = temp_current - temp_desired;

    if (first_run){
        previous_error = error;
        first_run = false;
    }

    double derivative = (error - previous_error) / pid::dT;

    // izračunaj izhod brez posodobitve integrala, da lahko preverimo nasičenost
    double DAC_voltage = VDD - (pid::Kp * error + pid::Ki * integral + pid::Kd * derivative);

    bool saturated_high = (DAC_voltage >= VDD);
    bool saturated_low  = (DAC_voltage <= 0.4); // omejitev

    /* integral posodobi samo, če izhod ni nasičen,
    ali če bi posodobitev integrala pomagala izhod premakniti stran od nasičenosti */
    bool integrating_would_help =
    (saturated_low  && error < 0.0) ? false :
    (saturated_high && error > 0.0) ? false :
    true;

    if (integrating_would_help){
        integral += error * pid::dT;
        DAC_voltage = VDD - (pid::Kp * error + pid::Ki * integral + pid::Kd * derivative); // ponovno izračunaj izhod s posodobljenim integralom
    }

    // omejitev izhoda DAC-a
    if (DAC_voltage > VDD)
        DAC_voltage = VDD;
    else if (DAC_voltage < 0.4)
        DAC_voltage = 0.4;

    previous_error = error;
    return DAC_voltage;
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
        std::cout << "Could not add 1-Wire PIO program.\n";
        return 1;
    }
    offset = pio_add_program(pio, &onewire_program);
    OW ow1;
    //OW ow2; za enkrat le en termometer
    if (!init_ds18b20(&ow1, pio, offset)){
        std::cout << "Could not initializate the DS18B20.\n";
        return 2;
    }

    // INA219 konfiguracija
    config_ina(ina219::REG_CONFIG, ina219::CONFIG_VALUE);
    config_ina(ina219::REG_CALIBRATION, ina219::CALIBRATION_VALUE);

    // sprobaj ventilatorja
    fan_control(false, pwmfan::PIN1, 0);
    fan_control(false, pwmfan::PIN2, 0);
    sleep_ms(1000);
    fan_control(true, pwmfan::PIN1, 20);
    fan_control(true, pwmfan::PIN2, 20);
    sleep_ms(1000);
    fan_control(false, pwmfan::PIN1, 0);
    fan_control(false, pwmfan::PIN2, 0);


    constexpr double STEP = 4.0;
    constexpr double STEP_TOLERANCE = 0.5;
    bool first_run = true;
    bool reset_integral_flag = false;
    double DAC_voltage = VDD; // privzeta napetost na DAC, ne hladi
    double temp_desired = 20.0; // itak se prepiše
    double temp_desired_step = 20.0; // itak se prepiše
    int timer = 0;

    while (true) {
        std::array<std::string, 4> new_usb_data;

        // za prvi krog ne nadaljuj z zanko dokler ne dobiš nastavitev
        if (first_run){
            // DAC konfiguracija
            write_DAC(DAC_voltage);

            while (true){
                while (!read_USB(new_usb_data)){

                    if (timer == 25){  // približno vsako sekundo
                        std::cout << "DATA,"
                        << read_ds18b20(&ow1) << "," // odstrani, če bo preveč blokirajoče
                        << calculate_ina("vin_v") << ","
                        << calculate_ina("ma") << ","
                        << calculate_ina("mw") << ","
                        << DAC_voltage
                        << "\n" << std::flush;

                        timer = 0;
                    }

                    ++timer;
                    sleep_ms(10); // da ni procesor popolnoma porabljen
                }

                if (is_valid_double(new_usb_data[1]) &&
                    is_valid_int(new_usb_data[2]) &&
                    is_valid_int(new_usb_data[3])){

                    temp_desired = std::stod(new_usb_data[1]);
                    uint fan_pin = static_cast<uint>(std::stoi(new_usb_data[2]));
                    uint fan_speed = static_cast<uint>(std::stoi(new_usb_data[3]));
                    fan_control(true, fan_pin, fan_speed);
                    break;
                }
                std::cout << "Invalid USB data, waiting for valid settings.\n";
            }
        }

        absolute_time_t cycle_start = get_absolute_time();

        if (!first_run){
            if (read_USB(new_usb_data)){
                if (!is_valid_double(new_usb_data[1]) ||
                    !is_valid_int(new_usb_data[2]) ||
                    !is_valid_int(new_usb_data[3])){
                    std::cout << "Invalid USB data, ignoring command.\n";
                }
                else{
                    double new_temp_desired = std::stod(new_usb_data[1]);

                    bool big_change = std::abs(new_temp_desired - temp_desired) > 10.0; // prag prilagodi po potrebi

                    temp_desired = new_temp_desired;
                    uint fan_pin = static_cast<uint>(std::stoi(new_usb_data[2]));
                    uint fan_speed = static_cast<uint>(std::stoi(new_usb_data[3]));
                    fan_control(true, fan_pin, fan_speed);

                    if (big_change)
                        reset_integral_flag = true;
                }
            }
        }

        config_ina(ina219::REG_CALIBRATION, ina219::CALIBRATION_VALUE);

        double temp_current = read_ds18b20(&ow1);

        // prepreči prehitro hlajenje tako, da haldi v korakih
        double diff_to_desired = temp_current - temp_desired;
        if (diff_to_desired < 0.0){
            // stopničasto hlajenje nima smisla
            temp_desired_step = temp_desired;
            first_run = false;
        }
        else if (first_run){
            if (diff_to_desired > STEP)
                temp_desired_step = temp_current - STEP;
            else
                temp_desired_step = temp_desired;

            first_run = false;
        }
        else{
            double diff_to_step = temp_current - temp_desired_step;

            if (diff_to_step <= STEP_TOLERANCE){
                if (diff_to_desired > STEP)
                    temp_desired_step = temp_current - STEP;
                else
                    temp_desired_step = temp_desired;
            }
            // sicer: temp_desired_step ostane nespremenjen, še nismo dosegli trenutnega koraka
        }

        DAC_voltage = pid_controller(temp_current, temp_desired_step, reset_integral_flag);
        reset_integral_flag = false;

        write_DAC(DAC_voltage);

        std::cout << "DATA,"
        << temp_current << ","
        << calculate_ina("vin_v") << ","
        << calculate_ina("ma") << ","
        << calculate_ina("mw") << ","
        << DAC_voltage
        << "\n" << std::flush;

        // predvidljiv cikel za izračune
        int64_t elapsed_us = absolute_time_diff_us(cycle_start, get_absolute_time());
        int64_t target_us = static_cast<int64_t>(pid::dT * 1000000.0);
        int64_t remaining_us = target_us - elapsed_us;

        if (remaining_us > 0) // ne čaka dodatno
            sleep_us(remaining_us);
    }

    return 0;
}
