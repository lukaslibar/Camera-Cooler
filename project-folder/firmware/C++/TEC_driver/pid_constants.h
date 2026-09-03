#pragma once

namespace pid {
    // PID konstante
    constexpr double Kp = 0.4; // proporcijska konstanta
    constexpr double Ki = 0.004; // integracijska konstanta
    constexpr double Kd = 0.0; // diferencialna konstanta
    constexpr double dT = 1; // časovni interval za aproksimacijo I in d

}
