#include "audio/SyncTrack.h"

#include "core/Log.h"

#include <libopenmpt/libopenmpt.hpp>

#include <algorithm>

namespace demo {

namespace {

constexpr int kScanRate = 8000;       // lowest rate libopenmpt accepts; we only need timing, not audio
constexpr size_t kScanChunk = 16;     // 2 ms timing resolution
constexpr double kMaxScanSeconds = 30.0 * 60.0;

// Seconds since the last timestamp <= t in a sorted list.
float sinceLast(const std::vector<double>& times, double t) {
    const auto it = std::upper_bound(times.begin(), times.end(), t);
    return it == times.begin() ? kNeverHit : static_cast<float>(t - *(it - 1));
}

}  // namespace

void SyncTrack::clear() {
    m_rows.clear();
    for (auto& hits : m_channelHits) hits.clear();
    for (auto& hits : m_instrumentHits) hits.clear();
}

void SyncTrack::build(const std::vector<std::uint8_t>& moduleData) {
    clear();
    openmpt::module module(moduleData.data(), moduleData.size());
    module.set_repeat_count(0);

    const int channels = std::min(module.get_num_channels(), kSyncSlots);
    std::array<int, kSyncSlots> lastInstrument{};
    std::array<float, kScanChunk> scratch{};
    size_t frames = 0;
    int lastOrder = -1, lastRow = -1;

    while (true) {
        const int order = module.get_current_order();
        const int row = module.get_current_row();
        if (order != lastOrder || row != lastRow) {
            lastOrder = order;
            lastRow = row;
            const double time = static_cast<double>(frames) / kScanRate;
            m_rows.push_back({time, order, row});

            const int pattern = module.get_current_pattern();
            for (int channel = 0; channel < channels; ++channel) {
                const int note = module.get_pattern_row_channel_command(pattern, row, channel, openmpt::module::command_note);
                const int instrument =
                    module.get_pattern_row_channel_command(pattern, row, channel, openmpt::module::command_instrument);
                if (instrument > 0) lastInstrument[channel] = instrument;
                if (note < 1 || note > 120) continue;  // empty, note-off, note-cut...
                m_channelHits[channel].push_back(time);
                if (lastInstrument[channel] > 0 && lastInstrument[channel] < kSyncSlots)
                    m_instrumentHits[lastInstrument[channel]].push_back(time);
            }
        }

        const size_t rendered = module.read(kScanRate, kScanChunk, scratch.data());
        if (rendered == 0 || frames > static_cast<size_t>(kMaxScanSeconds * kScanRate)) break;
        frames += rendered;
    }

    size_t notes = 0;
    for (const auto& hits : m_channelHits) notes += hits.size();
    log::info("Sync track: {} rows, {} note-ons, {} channels", m_rows.size(), notes, channels);
}

void SyncTrack::evaluate(double seconds, SyncState& state) const {
    if (m_rows.empty()) return;
    const auto it = std::upper_bound(m_rows.begin(), m_rows.end(), seconds,
                                     [](double t, const RowEvent& e) { return t < e.time; });
    if (it != m_rows.begin()) {
        state.order = (it - 1)->order;
        state.row = (it - 1)->row;
    }
    for (int i = 0; i < kSyncSlots; ++i) {
        state.sinceChannel[i] = sinceLast(m_channelHits[i], seconds);
        state.sinceInstrument[i] = sinceLast(m_instrumentHits[i], seconds);
    }
}

}  // namespace demo
