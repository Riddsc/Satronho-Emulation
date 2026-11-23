// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>

struct SDL_AudioStream;

namespace Satronho {

// Salida de sonido con control dinámico de velocidad: ajusta la velocidad de reproducción
// en ±0.5 % según lo que haya en el búfer, así el sonido no se corta ni se atrasa aunque la
// pantalla no vaya exactamente a la velocidad de la consola.
class AudioOut {
public:
    bool Open();
    void Close();
    void Configure(double sample_rate);  // frecuencia de muestreo del núcleo
    void Push(const int16_t* frames, size_t count);
    void Update();                       // una vez por cuadro: control de velocidad
    void Clear();
    void SetPaused(bool paused);
    void SetVolume(float volume);        // 0..1
    void SetLatencyMs(int ms) { latency_ms_ = ms < 16 ? 16 : ms; }

    bool IsOpen() const { return stream_ != nullptr; }
    double QueuedMs() const;
    double Ratio() const { return ratio_; }
    int Underruns() const { return underruns_; }

private:
    SDL_AudioStream* stream_ = nullptr;
    double in_rate_ = 48000.0;
    double ratio_ = 1.0;
    int latency_ms_ = 64;
    int underruns_ = 0;
    bool paused_ = false;
};

} // namespace Satronho
