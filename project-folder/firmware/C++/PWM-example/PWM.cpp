#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

constexpr uint PWM_PIN = 10;
constexpr uint8_t DUTY_CYCLE = 50;
constexpr uint FREQ = 25000;

int main() {

    gpio_set_function(PWM_PIN, GPIO_FUNC_PWM);

    uint slice_st = pwm_gpio_to_slice_num(PWM_PIN);
    uint kanal = pwm_gpio_to_channel(PWM_PIN);
    uint32_t ura = clock_get_hz(clk_sys);

    uint wrap = ura / FREQ - 1;
    uint level = (DUTY_CYCLE * (wrap + 1)) / 100;

    pwm_set_clkdiv(slice_st, 1.0f); // brez deljenja ure
    pwm_set_wrap(slice_st, wrap);
    pwm_set_chan_level(slice_st, kanal, level);

    pwm_set_enabled(slice_st, true);

    while (true)
    {
        tight_loop_contents();
    }
}
