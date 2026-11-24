// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <SDL3/SDL.h>

#include <vector>

#include "core/core_host.h"
#include "gfx/gl.h"
#include "sys/settings.h"

namespace Satronho {

class HwRender;

// Imagen del juego con OpenGL: escala, filtros y efectos de pantalla hechos con sombreadores
// (nítido suave, líneas del televisor, CRT con máscara de fósforo, rejilla de LCD y el fondo
// ambiente). Muestra tanto los cuadros por software como los que dibuja la tarjeta gráfica.
class VideoOutGl {
public:
    bool Init();
    void Shutdown();
    void Upload(const CoreHost& core);
    void Draw(const CoreHost& core, const HwRender* hw, const Settings& s, int win_w, int win_h, bool handheld);
    SDL_FRect LastRect() const { return dest_; }
    void Invalidate() { uploaded_serial_ = 0; }

private:
    void EnsureSmall();

    GLuint prog_ = 0, amb_prog_ = 0, vao_ = 0;
    GLuint tex_ = 0;       // cuadro por software
    int tex_w_ = 0, tex_h_ = 0;
    PixelFormat tex_fmt_ = PixelFormat::XRGB1555;
    uint64_t uploaded_serial_ = 0;
    std::vector<uint16_t> conv_; // 1555 → 565 en OpenGL ES
    // Fondo ambiente: el cuadro reducido a 16×15 en un framebuffer pequeño.
    GLuint small_tex_ = 0, small_fbo_ = 0;
    uint64_t ambient_serial_ = 0;
    SDL_FRect dest_{};

    struct Uniforms {
        GLint rect, uv, tex, src_size, out_size, mode, effect, strength, lines, tex_size;
    } u_{};
    struct AmbUniforms {
        GLint rect, uv, tex, tint, vignette;
    } a_{};
};

} // namespace Satronho
