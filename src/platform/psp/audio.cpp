#include "platform/psp/audio.h"

#include <pspaudio.h>
#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <pspmp3.h>

#include <cstring>

namespace osu {

namespace {

constexpr int kBlockFrames = 1024;      // stereo frames per audio block (~23 ms)
constexpr int kMp3BufferSize = 32 * 1024;
constexpr int kPcmBufferSize = 16 * 1024;

// The audio hardware and sceMp3 both want 64-byte aligned buffers.
alignas(64) unsigned char g_mp3_buffer[kMp3BufferSize];
alignas(64) unsigned char g_pcm_buffer[kPcmBufferSize];
alignas(64) int16_t g_block[kBlockFrames * 2];

AudioPlayer* g_active_player = nullptr;

}  // namespace

AudioPlayer::~AudioPlayer() { shutdown(); }

bool AudioPlayer::init() {
    if (initialised_) return true;
    if (sceMp3InitResource() < 0) return false;
    initialised_ = true;
    return true;
}

void AudioPlayer::shutdown() {
    stop();
    if (initialised_) {
        sceMp3TermResource();
        initialised_ = false;
    }
}

bool AudioPlayer::refill_stream() {
    if (file_ < 0 || handle_ < 0) return false;

    SceUChar8* destination = nullptr;
    SceInt32 to_write = 0;
    SceInt32 source_position = 0;
    if (sceMp3GetInfoToAddStreamData(handle_, &destination, &to_write, &source_position) < 0) {
        return false;
    }
    if (to_write <= 0) return false;

    const int read = sceIoRead(file_, destination, static_cast<unsigned int>(to_write));
    if (read <= 0) {
        eof_ = true;
        return false;
    }
    sceMp3NotifyAddStreamData(handle_, read);
    return true;
}

void AudioPlayer::push_samples(const int16_t* samples, int frames, int channels) {
    carry_.reserve(carry_.size() + static_cast<size_t>(frames) * 2u);
    if (channels == 2) {
        carry_.insert(carry_.end(), samples, samples + static_cast<size_t>(frames) * 2u);
        return;
    }
    // Mono: duplicate into both channels, the output is always stereo.
    for (int i = 0; i < frames; ++i) {
        carry_.push_back(samples[i]);
        carry_.push_back(samples[i]);
    }
}

bool AudioPlayer::decode_more() {
    for (int attempt = 0; attempt < 16; ++attempt) {
        if (!eof_ && handle_ >= 0 && sceMp3CheckStreamDataNeeded(handle_) > 0) {
            if (!refill_stream()) eof_ = true;
        }

        short* pcm = nullptr;
        const int bytes = sceMp3Decode(handle_, &pcm);
        if (bytes > 0 && pcm != nullptr) {
            const int frames = bytes / (2 * channels_);
            if (frames <= 0) return false;
            push_samples(reinterpret_cast<const int16_t*>(pcm), frames, channels_);
            return true;
        }
        if (bytes == 0) {
            eof_ = true;
            return false;
        }
        if (eof_ || !refill_stream()) {
            eof_ = true;
            return false;
        }
    }
    return false;
}

void AudioPlayer::run() {
    while (running_) {
        while (running_ && static_cast<int>(carry_.size()) < kBlockFrames * 2) {
            if (!decode_more()) {
                eof_ = true;
                break;
            }
        }
        if (static_cast<int>(carry_.size()) < kBlockFrames * 2) break;

        std::memcpy(g_block, carry_.data(), kBlockFrames * 2 * sizeof(int16_t));
        carry_.erase(carry_.begin(), carry_.begin() + kBlockFrames * 2);

        if (sceAudioOutputBlocking(channel_, PSP_AUDIO_VOLUME_MAX, g_block) < 0) break;

        position_ms_ += static_cast<double>(kBlockFrames) / static_cast<double>(sample_rate_) * 1000.0;
    }
    active_ = false;
}

int AudioPlayer::thread_entry(unsigned int, void*) {
    if (g_active_player) g_active_player->run();
    return 0;
}

bool AudioPlayer::play(const std::string& path) {
    stop();
    if (!initialised_) return false;

    file_ = sceIoOpen(path.c_str(), PSP_O_RDONLY, 0);
    if (file_ < 0) return false;
    file_size_ = static_cast<double>(sceIoLseek(file_, 0, PSP_SEEK_END));
    sceIoLseek(file_, 0, PSP_SEEK_SET);
    if (file_size_ < 1024.0) {
        release();
        return false;
    }

    SceMp3InitArg args{};
    args.mp3StreamStart = 0;
    args.mp3StreamEnd = static_cast<SceOff>(file_size_);
    args.mp3Buf = g_mp3_buffer;
    args.mp3BufSize = kMp3BufferSize;
    args.pcmBuf = g_pcm_buffer;
    args.pcmBufSize = kPcmBufferSize;

    handle_ = sceMp3ReserveMp3Handle(&args);
    if (handle_ < 0) {
        handle_ = -1;
        release();
        return false;
    }

    if (!refill_stream() || sceMp3Init(handle_) < 0) {
        release();
        return false;
    }

    sample_rate_ = sceMp3GetSamplingRate(handle_);
    channels_ = sceMp3GetMp3ChannelNum(handle_);
    if (sample_rate_ <= 0) sample_rate_ = 44100;
    if (channels_ <= 0) channels_ = 2;

    channel_ = sceAudioChReserve(0, kBlockFrames, PSP_AUDIO_FORMAT_STEREO);
    if (channel_ < 0) {
        channel_ = -1;
        release();
        return false;
    }

    carry_.clear();
    position_ms_ = 0.0;
    eof_ = false;
    running_ = true;
    active_ = true;

    g_active_player = this;
    thread_id_ = sceKernelCreateThread("osu_audio", thread_entry, 0x12, 0x8000, 0, nullptr);
    if (thread_id_ < 0) {
        thread_id_ = -1;
        running_ = false;
        active_ = false;
        release();
        return false;
    }
    sceKernelStartThread(thread_id_, 0, nullptr);
    return true;
}

void AudioPlayer::release() {
    if (channel_ >= 0) {
        sceAudioChRelease(channel_);
        channel_ = -1;
    }
    if (handle_ >= 0) {
        sceMp3ReleaseMp3Handle(handle_);
        handle_ = -1;
    }
    if (file_ >= 0) {
        sceIoClose(file_);
        file_ = -1;
    }
    carry_.clear();
}

void AudioPlayer::abandon() {
    running_ = false;
    active_ = false;
    thread_id_ = -1;
    channel_ = -1;
    handle_ = -1;
    file_ = -1;
    carry_.clear();
    if (g_active_player == this) g_active_player = nullptr;
}

void AudioPlayer::stop() {
    running_ = false;
    if (thread_id_ >= 0) {
        sceKernelWaitThreadEnd(thread_id_, nullptr);
        sceKernelDeleteThread(thread_id_);
        thread_id_ = -1;
    }
    release();
    active_ = false;
    if (g_active_player == this) g_active_player = nullptr;
}

}  // namespace osu