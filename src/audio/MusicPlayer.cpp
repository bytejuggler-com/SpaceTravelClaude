#include "audio/MusicPlayer.h"

#include "core/Log.h"

#include <libopenmpt/libopenmpt.hpp>

#include <algorithm>
#include <cmath>

namespace demo {

namespace {

// The renderer runs ahead of what is heard by the buffered audio; only re-seek on real jumps (seek, loop, hitch).
constexpr double kResyncSeconds = 0.25;
constexpr int kStereoSeparationPercent = 40;  // Amiga hard-panning is tiring on headphones

// Holds the stream lock: the SDL audio callback runs under the same lock, so this excludes it.
class StreamLock {
public:
    explicit StreamLock(SDL_AudioStream* stream) : m_stream(stream) {
        if (m_stream) SDL_LockAudioStream(m_stream);
    }
    ~StreamLock() {
        if (m_stream) SDL_UnlockAudioStream(m_stream);
    }
    StreamLock(const StreamLock&) = delete;
    StreamLock& operator=(const StreamLock&) = delete;

private:
    SDL_AudioStream* m_stream;
};

}  // namespace

MusicPlayer::MusicPlayer() = default;

MusicPlayer::~MusicPlayer() {
    if (m_stream) SDL_DestroyAudioStream(m_stream);  // stops the callback before the module goes away
}

bool MusicPlayer::open() {
    if (!SDL_WasInit(SDL_INIT_AUDIO)) {
        log::warn("Audio subsystem not available; music disabled");
        return false;
    }
    const SDL_AudioSpec spec{SDL_AUDIO_F32, 2, kSampleRate};
    m_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &MusicPlayer::onAudio, this);
    if (!m_stream) {
        log::warn("No audio device ({}); music disabled", SDL_GetError());
        return false;
    }

    SDL_AudioSpec deviceSpec{};
    int deviceFrames = 0;
    if (SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(m_stream), &deviceSpec, &deviceFrames) && deviceSpec.freq > 0)
        m_deviceLatency = 2.0 * deviceFrames / deviceSpec.freq;  // device buffer + one callback request
    log::info("Audio: {} Hz, ~{:.0f} ms device latency", deviceSpec.freq, m_deviceLatency * 1000.0);
    return true;  // the device starts paused; follow() resumes it
}

void MusicPlayer::load(const std::vector<std::uint8_t>& data, const std::string& name) {
    auto module = std::make_unique<openmpt::module>(data.data(), data.size());
    module->set_repeat_count(-1);  // loop forever; the demo decides where we are
    module->set_render_param(openmpt::module::RENDER_STEREOSEPARATION_PERCENT, kStereoSeparationPercent);

    const double duration = module->get_duration_seconds();
    const std::string title = module->get_metadata("title");
    {
        StreamLock lock(m_stream);
        if (m_stream) SDL_ClearAudioStream(m_stream);
        m_module = std::move(module);
    }
    m_duration = duration;
    m_title = title.empty() ? name : title;
    m_playing = false;
    if (m_stream) SDL_PauseAudioStreamDevice(m_stream);
    log::info("Music '{}' ({}): {:.1f} s", m_title, name, m_duration);
}

void MusicPlayer::unload() {
    if (m_stream) SDL_PauseAudioStreamDevice(m_stream);
    {
        StreamLock lock(m_stream);
        if (m_stream) SDL_ClearAudioStream(m_stream);
        m_module.reset();
    }
    m_duration = 0.0;
    m_title.clear();
    m_playing = false;
}

void MusicPlayer::follow(double seconds, bool playing) {
    if (!m_stream || !loaded()) return;
    seconds = std::fmod(std::max(0.0, seconds), m_duration);

    if (playing != m_playing) {
        m_playing = playing;
        if (playing) {
            seek(seconds);
            SDL_ResumeAudioStreamDevice(m_stream);
        } else {
            SDL_PauseAudioStreamDevice(m_stream);
        }
        return;
    }
    if (!playing) return;

    double position = 0.0;
    {
        StreamLock lock(m_stream);
        position = m_module->get_position_seconds();
    }
    const double drift = std::remainder(position - seconds, m_duration);  // shortest way around the loop point
    if (std::abs(drift) > kResyncSeconds) seek(seconds);
}

void MusicPlayer::seek(double seconds) {
    StreamLock lock(m_stream);
    SDL_ClearAudioStream(m_stream);  // drop audio already rendered for the old position
    m_module->set_position_seconds(seconds);
}

void SDLCALL MusicPlayer::onAudio(void* userdata, SDL_AudioStream* stream, int additionalAmount, int /*totalAmount*/) {
    if (additionalAmount > 0) static_cast<MusicPlayer*>(userdata)->fill(stream, additionalAmount);
}

void MusicPlayer::fill(SDL_AudioStream* stream, int bytes) {
    const size_t frames = static_cast<size_t>(bytes) / (2 * sizeof(float));
    m_buffer.assign(frames * 2, 0.0f);

    size_t rendered = 0;
    if (m_module) rendered = m_module->read_interleaved_stereo(kSampleRate, frames, m_buffer.data());
    std::fill(m_buffer.begin() + static_cast<std::ptrdiff_t>(rendered * 2), m_buffer.end(), 0.0f);

    const float gain = m_muted ? 0.0f : m_volume.load();
    if (gain != 1.0f)
        for (float& sample : m_buffer) sample *= gain;

    SDL_PutAudioStreamData(stream, m_buffer.data(), static_cast<int>(m_buffer.size() * sizeof(float)));
}

}  // namespace demo
