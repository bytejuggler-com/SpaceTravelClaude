#include "audio/PlaceholderSong.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <string>

namespace demo {

namespace {

// Playback rate of period 214 (C-3) on a PAL Amiga: drums triggered at C-3 play back at their synthesized pitch.
constexpr double kSynthRate = 7093789.2 / (2.0 * 214.0);

// ProTracker periods, finetune 0, octaves 1-3.
constexpr int kPeriods[36] = {856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
                              428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,
                              214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113};

enum Semitone { C = 0, Cs, D, Ds, E, F, Fs, G, Gs, A, As, B };
constexpr int note(int semitone, int octave) { return (octave - 1) * 12 + semitone; }  // MOD octaves 1..3

enum SampleId { kKick = 1, kSnare, kHat, kBass, kLead };

constexpr int kRows = 64;
constexpr int kChannels = 4;

struct Sample {
    std::string name;
    std::vector<std::int8_t> data;
    int volume = 64;
    bool loop = false;
};

struct Cell {
    int note = -1;  // index into kPeriods, -1 = empty
    int sample = 0;
    int effect = 0;
    int param = 0;
};

using Pattern = std::array<std::array<Cell, kChannels>, kRows>;

// Deterministic white noise in [-1, 1].
class Noise {
public:
    float next() {
        m_state = m_state * 1664525u + 1013904223u;
        return static_cast<float>(m_state >> 8) / 8388607.5f - 1.0f;
    }

private:
    std::uint32_t m_state = 12345u;
};

std::vector<std::int8_t> toPcm(const std::vector<float>& samples) {
    std::vector<std::int8_t> pcm(samples.size() + (samples.size() & 1), 0);  // MOD lengths are in words
    for (size_t i = 0; i < samples.size(); ++i)
        pcm[i] = static_cast<std::int8_t>(std::lround(std::clamp(samples[i], -1.0f, 1.0f) * 127.0f));
    return pcm;
}

std::vector<float> render(double seconds, auto&& generator) {
    std::vector<float> out(static_cast<size_t>(seconds * kSynthRate));
    for (size_t i = 0; i < out.size(); ++i) out[i] = generator(static_cast<double>(i) / kSynthRate);
    return out;
}

Sample makeKick() {
    double phase = 0.0;
    Noise noise;
    auto samples = render(0.3, [&](double t) {
        const double frequency = 45.0 + 120.0 * std::exp(-t * 28.0);  // pitch sweep down
        phase += 2.0 * std::numbers::pi * frequency / kSynthRate;
        const double click = t < 0.003 ? noise.next() * 0.4 * (1.0 - t / 0.003) : 0.0;
        return static_cast<float>(std::sin(phase) * std::exp(-t * 7.0) + click);
    });
    return {"kick", toPcm(samples), 64, false};
}

Sample makeSnare() {
    Noise noise;
    auto samples = render(0.22, [&](double t) {
        const double body = std::sin(2.0 * std::numbers::pi * 190.0 * t) * std::exp(-t * 30.0);
        return static_cast<float>(0.7 * noise.next() * std::exp(-t * 16.0) + 0.5 * body);
    });
    return {"snare", toPcm(samples), 50, false};
}

Sample makeHat() {
    Noise noise;
    float previous = 0.0f;
    auto samples = render(0.07, [&](double t) {
        const float white = noise.next();
        const float highpassed = white - previous;  // crude high-pass for a thin, bright hat
        previous = white;
        return static_cast<float>(0.5 * highpassed * std::exp(-t * 55.0));
    });
    return {"hihat", toPcm(samples), 30, false};
}

Sample makeBass() {
    std::vector<float> cycle(64);  // single-cycle saw; C-1 plays at ~65 Hz
    for (size_t i = 0; i < cycle.size(); ++i) cycle[i] = 0.8f * (1.0f - 2.0f * (static_cast<float>(i) + 0.5f) / 64.0f);
    return {"bass", toPcm(cycle), 46, true};
}

Sample makeLead() {
    std::vector<float> cycle(32);  // single-cycle 25% pulse; C-2 plays at ~259 Hz
    for (size_t i = 0; i < cycle.size(); ++i) cycle[i] = i < 8 ? 0.6f : -0.6f;
    return {"lead", toPcm(cycle), 30, true};
}

// Cm - Ab - Eb - Bb, one chord per bar (16 rows), four bars per pattern.
Pattern makePattern(bool snare, bool lead, bool fill) {
    static constexpr int kRoots[4] = {C, Gs, Ds, As};
    static constexpr int kArpeggios[4] = {0x37, 0x47, 0x47, 0x47};  // minor, major, major, major

    Pattern pattern{};
    for (int row = 0; row < kRows; ++row) {
        const int bar = row / 16;
        const int inBar = row % 16;
        auto& cells = pattern[row];

        if (inBar % 4 == 0) cells[0] = {note(C, 3), kKick};  // four on the floor
        if (inBar % 4 == 2) cells[1] = {note(C, 3), kHat};   // off-beat hats
        if (snare && (inBar == 4 || inBar == 12)) cells[1] = {note(C, 3), kSnare};
        if (fill && row >= 56 && (row % 2 == 0 || row >= 60)) cells[1] = {note(C, 3), kSnare};

        if (inBar % 2 == 0) cells[2] = {note(kRoots[bar], (inBar / 2) % 2 == 0 ? 1 : 2), kBass};  // octave bounce

        if (lead) {
            cells[3].effect = 0x0;  // arpeggio, on every row so the chord keeps cycling
            cells[3].param = kArpeggios[bar];
            if (inBar % 4 == 0) {
                cells[3].note = note(kRoots[bar], 2);
                cells[3].sample = kLead;
            }
        }
    }
    return pattern;
}

void put16(std::vector<std::uint8_t>& out, int value) {  // big-endian word
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
}

void putString(std::vector<std::uint8_t>& out, const std::string& text, size_t width) {
    for (size_t i = 0; i < width; ++i) out.push_back(i < text.size() ? static_cast<std::uint8_t>(text[i]) : 0);
}

}  // namespace

std::vector<std::uint8_t> buildPlaceholderSong() {
    const std::vector<Sample> samples = {makeKick(), makeSnare(), makeHat(), makeBass(), makeLead()};
    const std::vector<Pattern> patterns = {
        makePattern(false, false, false),  // 0: kick, hats, bass
        makePattern(true, false, true),    // 1: + snare, fill
        makePattern(true, true, false),    // 2: + lead arpeggios
        makePattern(true, true, true),     // 3: + fill
    };
    const std::vector<int> orders = {0, 1, 2, 3, 2, 3};

    std::vector<std::uint8_t> mod;
    putString(mod, "space travel demo", 20);

    for (int i = 0; i < 31; ++i) {
        if (i < static_cast<int>(samples.size())) {
            const Sample& sample = samples[i];
            const int words = static_cast<int>(sample.data.size() / 2);
            putString(mod, sample.name, 22);
            put16(mod, words);
            mod.push_back(0);  // finetune
            mod.push_back(static_cast<std::uint8_t>(sample.volume));
            put16(mod, 0);                         // loop start (words)
            put16(mod, sample.loop ? words : 1);   // loop length (words), 1 = no loop
        } else {
            putString(mod, "", 22);
            put16(mod, 0);
            mod.push_back(0);
            mod.push_back(0);
            put16(mod, 0);
            put16(mod, 1);
        }
    }

    mod.push_back(static_cast<std::uint8_t>(orders.size()));
    mod.push_back(0);  // restart position
    for (int i = 0; i < 128; ++i)
        mod.push_back(static_cast<std::uint8_t>(i < static_cast<int>(orders.size()) ? orders[i] : 0));
    putString(mod, "M.K.", 4);

    for (const Pattern& pattern : patterns) {
        for (const auto& row : pattern) {
            for (const Cell& cell : row) {
                const int period = cell.note >= 0 ? kPeriods[cell.note] : 0;
                mod.push_back(static_cast<std::uint8_t>((cell.sample & 0xF0) | ((period >> 8) & 0x0F)));
                mod.push_back(static_cast<std::uint8_t>(period & 0xFF));
                mod.push_back(static_cast<std::uint8_t>(((cell.sample & 0x0F) << 4) | (cell.effect & 0x0F)));
                mod.push_back(static_cast<std::uint8_t>(cell.param));
            }
        }
    }

    for (const Sample& sample : samples)
        for (std::int8_t value : sample.data) mod.push_back(static_cast<std::uint8_t>(value));

    return mod;
}

}  // namespace demo
