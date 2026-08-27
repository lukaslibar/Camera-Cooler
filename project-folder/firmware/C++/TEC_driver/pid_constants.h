#pragma once

namespace pid {
    // PID konstante
    constexpr double Kp = 0.05; // proporcijska konstanta
    constexpr double Ki = 0.05; // integracijska konstanta
    constexpr double Kd = 0.01; // diferencialna konstanta
    constexpr double dT = 0.5; // časovni interval za aproksimacijo I/d

}
