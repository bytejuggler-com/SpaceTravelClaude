#pragma once

#include "core/Gl.h"

namespace demo {

// Measures GPU time between begin() and end() with a ring of GL_TIME_ELAPSED queries,
// reading results a few frames late so the CPU never stalls waiting for the GPU.
class GpuTimer {
public:
    GpuTimer();
    ~GpuTimer();
    GpuTimer(const GpuTimer&) = delete;
    GpuTimer& operator=(const GpuTimer&) = delete;

    void begin();
    void end();
    double lastMs() const { return m_lastMs; }

private:
    static constexpr int kQueryCount = 4;
    GLuint m_queries[kQueryCount] = {};
    long long m_frame = 0;
    double m_lastMs = 0.0;
};

}  // namespace demo
