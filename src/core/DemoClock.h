#pragma once

#include <algorithm>

namespace demo {

// Demo time, decoupled from wall time so it can be paused, scrubbed and sped up.
// From Phase 2 the audio playback position becomes the master clock.
class DemoClock {
public:
    void advance(double wallDt) {
        m_dt = m_paused ? 0.0 : wallDt * m_speed;
        m_time += m_dt;
    }

    void seek(double t) { m_time = std::max(0.0, t); }
    void setPaused(bool paused) { m_paused = paused; }
    void setSpeed(double speed) { m_speed = std::max(0.0, speed); }

    double time() const { return m_time; }
    double dt() const { return m_dt; }
    double speed() const { return m_speed; }
    bool paused() const { return m_paused; }

private:
    double m_time = 0.0;
    double m_dt = 0.0;
    double m_speed = 1.0;
    bool m_paused = false;
};

}  // namespace demo
