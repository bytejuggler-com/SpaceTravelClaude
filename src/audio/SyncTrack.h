#pragma once

#include "audio/SyncState.h"

#include <array>
#include <cstdint>
#include <vector>

namespace demo {

// Timestamps of every row and note-on in a module, found by playing it through once at load time
// (so tempo changes, pattern breaks and jumps are all honored).
class SyncTrack {
public:
    void build(const std::vector<std::uint8_t>& moduleData);  // throws std::exception on unsupported data
    void clear();
    bool empty() const { return m_rows.empty(); }

    // Fills order/row and the since* arrays of `state` for song position `seconds`.
    void evaluate(double seconds, SyncState& state) const;

private:
    struct RowEvent {
        double time;
        int order;
        int row;
    };

    std::vector<RowEvent> m_rows;
    std::array<std::vector<double>, kSyncSlots> m_channelHits;
    std::array<std::vector<double>, kSyncSlots> m_instrumentHits;
};

}  // namespace demo
