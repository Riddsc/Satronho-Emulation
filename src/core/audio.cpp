// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/audio.h"

#include <algorithm>
#include <cmath>

#include <SDL3/SDL.h>

#include "util/log.h"

namespace Satronho {

bool AudioOut::Open() {
    SDL_AudioSpec spec{SDL_AUDIO_S16, 2, 48000};
    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (!stream_) {
        Log::Warn("Sin salida de sonido: %s", SDL_GetError());
        return false;
    }
    SDL_ResumeAudioStreamDevice(stream_);
    return true;
}

void AudioOut::Close() {
    if (stream_) SDL_DestroyAudioStream(stream_);
    stream_ = nullptr;
}

void AudioOut::Configure(double sample_rate) {
    if (!stream_) return;
    in_rate_ = sample_rate;
    SDL_AudioSpec src{SDL_AUDIO_S16, 2, static_cast<int>(std::lround(sample_rate))};
    SDL_SetAudioStreamFormat(stream_, &src, nullptr);
    SDL_ClearAudioStream(stream_);
    ratio_ = 1.0;
    SDL_SetAudioStreamFrequencyRatio(stream_, 1.0f);
}

void AudioOut::Push(const int16_t* frames, size_t count) {
    if (!stream_ || paused_ || !count) return;
    SDL_PutAudioStreamData(stream_, frames, static_cast<int>(count * 4));
}

double AudioOut::QueuedMs() const {
    if (!stream_) return 0.0;
    const int bytes = SDL_GetAudioStreamQueued(stream_);
    return bytes / 4.0 / in_rate_ * 1000.0;
}

void AudioOut::Update() {
    if (!stream_ || paused_) return;
    const double queued = QueuedMs();
    const double target = latency_ms_;
    if (queued > target * 3.0) {
        // Demasiado atrasado (por ejemplo, tras una pausa larga): se descarta y se sigue.
        SDL_ClearAudioStream(stream_);
        return;
    }
    if (queued < 2.0) ++underruns_;
    // Si hay más sonido del esperado se reproduce un poquito más rápido, y viceversa.
    const double error = std::clamp((queued - target) / target, -1.0, 1.0);
    const double wanted = 1.0 + error * 0.005;
    ratio_ = ratio_ * 0.9 + wanted * 0.1;
    SDL_SetAudioStreamFrequencyRatio(stream_, static_cast<float>(ratio_));
}

void AudioOut::Clear() {
    if (stream_) SDL_ClearAudioStream(stream_);
}

void AudioOut::SetPaused(bool paused) {
    if (!stream_ || paused == paused_) return;
    paused_ = paused;
    if (paused) {
        SDL_PauseAudioStreamDevice(stream_);
    } else {
        SDL_ClearAudioStream(stream_);
        SDL_ResumeAudioStreamDevice(stream_);
    }
}

void AudioOut::SetVolume(float volume) {
    if (stream_) SDL_SetAudioStreamGain(stream_, std::clamp(volume, 0.0f, 1.0f));
}

} // namespace Satronho
