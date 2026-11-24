// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>

struct SDL_AudioStream;

namespace Satronho {

// Salida de sonido con control dinámico de velocidad: ajusta la velocidad de reproducción
// en ±1 % según lo que haya en el búfer, así el sonido no se corta ni se atrasa aunque la
// pantalla no vaya exactamente a la velocidad de la consola.
class AudioOut {
public:
    bool Open();
    void Close();
    // Cierra y vuelve a abrir la salida de sonido (al empezar cada juego): si Android o el sistema
    // dejaron el dispositivo en pausa o cambiaron de salida, el juego nuevo suena igual.
    void Reopen();
    void Configure(double sample_rate);  // frecuencia de muestreo del núcleo
    void Push(const int16_t* frames, size_t count);
    void Update();                       // una vez por cuadro: control de velocidad
    void Clear();
    void SetPaused(bool paused);
    void SetVolume(float volume);        // 0..1
    float Volume() const { return volume_; }
    bool DevicePaused() const;
    void SetLatencyMs(int ms) { latency_ms_ = ms < 16 ? 16 : ms; }

    bool IsOpen() const { return stream_ != nullptr; }
    double QueuedMs() const;
    double Ratio() const { return ratio_; }
    int Underruns() const { return underruns_; }
    int Drops() const { return drops_; }
    int LatencyMs() const { return latency_ms_; }
    // Lo que falta (+) o sobra (-) en el búfer respecto a la latencia elegida, en cuadros de la consola.
    double FramesShort(double fps) const { return (latency_ms_ - QueuedMs()) * fps / 1000.0; }

private:
    SDL_AudioStream* stream_ = nullptr;
    double in_rate_ = 48000.0;
    double ratio_ = 1.0;
    double integ_ = 0.0;   // parte integral del control (corrige diferencias fijas de reloj)
    int latency_ms_ = 64;
    int underruns_ = 0;
    int drops_ = 0;
    bool starved_ = false;
    bool paused_ = false;
    float volume_ = 1.0f;
};

} // namespace Satronho
