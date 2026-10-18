#pragma once

#include <SDL3/SDL.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace openmpt {
class module;
}

namespace demo {

// Tracker module playback (libopenmpt) on an SDL3 audio stream.
// The demo clock is the master: every frame follow() is told where the music should be, and the player
// pauses, resumes or re-seeks to match. That keeps pause, scrubbing, looping and speed changes trivial.
class MusicPlayer {
public:
    static constexpr int kSampleRate = 48000;

    MusicPlayer();
    ~MusicPlayer();
    MusicPlayer(const MusicPlayer&) = delete;
    MusicPlayer& operator=(const MusicPlayer&) = delete;

    bool open();  // opens the default playback device; false (and silent operation) if there is none

    // Replaces the current song. Throws std::exception if the data is not a supported module.
    void load(const std::vector<std::uint8_t>& data, const std::string& name);
    void unload();

    // Call once per frame. `seconds` is where the song should be (wrapped by the song length);
    // `playing` is false while the demo is paused or running at a speed other than 1x.
    void follow(double seconds, bool playing);

    bool loaded() const { return m_duration > 0.0; }
    bool deviceOpen() const { return m_stream != nullptr; }
    double duration() const { return m_duration; }
    const std::string& title() const { return m_title; }
    double deviceLatency() const { return m_deviceLatency; }  // rough estimate of render-to-speaker delay

    float volume() const { return m_volume; }
    void setVolume(float volume) { m_volume = volume; }
    bool muted() const { return m_muted; }
    void setMuted(bool muted) { m_muted = muted; }

private:
    static void SDLCALL onAudio(void* userdata, SDL_AudioStream* stream, int additionalAmount, int totalAmount);
    void fill(SDL_AudioStream* stream, int bytes);  // audio thread, called with the stream locked
    void seek(double seconds);

    SDL_AudioStream* m_stream = nullptr;
    std::unique_ptr<openmpt::module> m_module;  // guarded by the stream lock (SDL_LockAudioStream)
    std::vector<float> m_buffer;                // audio thread only
    double m_duration = 0.0;
    std::string m_title;
    double m_deviceLatency = 0.0;
    bool m_playing = false;
    std::atomic<float> m_volume{0.8f};
    std::atomic<bool> m_muted{false};
};

}  // namespace demo
