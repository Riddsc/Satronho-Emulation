// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <SDL3/SDL.h>

#include "core/core_host.h"
#include "sys/settings.h"

namespace Satronho {

// Dibuja la imagen del juego en la ventana: escala, filtro, fondo ambiente y efectos de
// pantalla (líneas de barrido y CRT).
class VideoOut {
public:
    void Init(SDL_Renderer* renderer) { r_ = renderer; }
    void Shutdown();
    void Upload(const CoreHost& core);  // sube el cuadro nuevo a la tarjeta gráfica
    void Draw(const CoreHost& core, const Settings& s, int win_w, int win_h);
    SDL_FRect LastRect() const { return dest_; }
    void Invalidate();

private:
    void EnsureTexture(const CoreHost& core);
    void EnsureEffects(int src_h, float strength, int effect);
    void UpdateAmbient(const CoreHost& core);

    SDL_Renderer* r_ = nullptr;
    SDL_Texture* tex_ = nullptr;
    SDL_Texture* sharp_ = nullptr;
    SDL_Texture* ambient_ = nullptr;
    SDL_Texture* scan_ = nullptr;
    SDL_Texture* mask_ = nullptr;
    SDL_Texture* vignette_ = nullptr;
    int tex_w_ = 0, tex_h_ = 0;
    SDL_PixelFormat tex_fmt_ = SDL_PIXELFORMAT_UNKNOWN;
    int sharp_w_ = 0, sharp_h_ = 0;
    int scan_h_ = 0;
    float scan_strength_ = -1;
    int scan_effect_ = -1;
    uint64_t uploaded_serial_ = 0;
    uint64_t ambient_serial_ = 0;
    SDL_FRect dest_{};
};

} // namespace Satronho
