// MP3 playback for the PSP: streams the song from the Memory Stick with sceMp3,
// outputs it through sceAudio and exposes the playback position so gameplay can
// be clocked by the audio instead of by a wall clock.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace osu {

class AudioPlayer {
public:
    ~AudioPlayer();

    // Allocates the decoder resources. Safe to call once.
    bool init();
    void shutdown();

    // Starts streaming `path` from the beginning. Returns false when the file or
    // the decoder is unavailable, in which case the caller falls back to a clock.
    bool play(const std::string& path);
    void stop();

    // Gives up on a stalled stream without waiting for the decoder thread (which
    // may be blocked in the audio channel). Leaks the thread/handle on purpose.
    void abandon();

    bool active() const { return active_; }

    // Milliseconds of audio already played, or 0 when nothing is playing.
    double position_ms() const { return position_ms_; }

private:
    static int thread_entry(unsigned int args, void* argp);
    void run();
    bool decode_more();
    bool refill_stream();
    void push_samples(const int16_t* samples, int frames, int channels);
    void release();

    bool initialised_ = false;
    bool active_ = false;
    bool running_ = false;
    bool eof_ = false;

    int channel_ = -1;
    int handle_ = -1;
    int file_ = -1;
    int thread_id_ = -1;
    int sample_rate_ = 44100;
    int channels_ = 2;
    double file_size_ = 0.0;
    double position_ms_ = 0.0;

    std::vector<int16_t> carry_;  // decoded stereo frames waiting to be played
};

}  // namespace osu
