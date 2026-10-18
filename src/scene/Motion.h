#pragma once

#include <algorithm>

// Helpers for time-driven motion. Scenes must be pure functions of time, so anything with a varying speed
// (accelerating starfields, camera paths) integrates its speed curve from the entry start instead of
// accumulating per frame.
namespace demo::motion {

inline double smoothstep(double edge0, double edge1, double x) {
    const double t = std::clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

// Integral of f over [0, t] (trapezoid rule). 256 steps is plenty for smooth speed curves.
template <class F>
double integrate(F&& f, double t, int steps = 256) {
    if (t <= 0.0) return 0.0;
    const double h = t / steps;
    double sum = 0.5 * (f(0.0) + f(t));
    for (int i = 1; i < steps; ++i) sum += f(i * h);
    return sum * h;
}

}  // namespace demo::motion
