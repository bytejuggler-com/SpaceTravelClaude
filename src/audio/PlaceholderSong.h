#pragma once

#include <cstdint>
#include <vector>

namespace demo {

// Builds a small 4-channel ProTracker module (M.K.) in memory with synthesized samples, used until a real
// track is chosen. 125 BPM, 6 patterns of 16 beats (46.08 s). Sample numbers, for sync:
//   1 kick, 2 snare, 3 hi-hat, 4 bass, 5 lead
// Channels: 0 kick, 1 snare/hats, 2 bass, 3 lead arpeggios (patterns 3-6).
std::vector<std::uint8_t> buildPlaceholderSong();

}  // namespace demo
