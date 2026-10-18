#pragma once

#include <array>
#include <cmath>

namespace demo {

inline constexpr int kSyncSlots = 32;
inline constexpr float kNeverHit = 1.0e6f;

// Music timing handed to scenes. Computed from demo time alone (beat grid from the timeline BPM, note-ons
// from the module's pattern data), so it is identical whether the demo plays, pauses or is scrubbed.
struct SyncState {
    bool hasMusic = false;
    double time = 0.0;        // song position (s) as heard, latency-compensated
    float bpm = 125.0f;
    double beat = 0.0;        // continuous beat counter; floor() is the beat index
    float sinceBeat = 0.0f;   // seconds since the last beat
    int order = -1;           // module order/row at `time` (-1 without music)
    int row = -1;
    std::array<float, kSyncSlots> sinceChannel;     // seconds since the last note-on, per channel (0-based)
    std::array<float, kSyncSlots> sinceInstrument;  // ... per instrument/sample number (1-based, [0] unused)

    SyncState() {
        sinceChannel.fill(kNeverHit);
        sinceInstrument.fill(kNeverHit);
    }

    // 1 at the hit, decaying exponentially (decay = 1/seconds).
    static float pulse(float since, float decay) { return std::exp(-since * decay); }

    float beatPulse(float decay = 8.0f) const { return pulse(sinceBeat, decay); }
    float channelPulse(int channel, float decay = 8.0f) const {
        return channel >= 0 && channel < kSyncSlots ? pulse(sinceChannel[channel], decay) : 0.0f;
    }
    float instrumentPulse(int instrument, float decay = 8.0f) const {
        return instrument > 0 && instrument < kSyncSlots ? pulse(sinceInstrument[instrument], decay) : 0.0f;
    }
};

}  // namespace demo
