// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/video.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Satronho {

namespace {
void Destroy(SDL_Texture*& t) {
    if (t) SDL_DestroyTexture(t);
    t = nullptr;
}

SDL_PixelFormat ToSdl(PixelFormat f) {
    switch (f) {
    case PixelFormat::XRGB8888: return SDL_PIXELFORMAT_XRGB8888;
    case PixelFormat::RGB565: return SDL_PIXELFORMAT_RGB565;
    default: return SDL_PIXELFORMAT_XRGB1555;
    }
}
} // namespace

void VideoOut::Shutdown() {
    Destroy(tex_);
    Destroy(sharp_);
    Destroy(ambient_);
    Destroy(scan_);
    Destroy(mask_);
    Destroy(vignette_);
}

void VideoOut::Invalidate() {
    uploaded_serial_ = 0;
    ambient_serial_ = 0;
}

void VideoOut::EnsureTexture(const CoreHost& core) {
    const int w = static_cast<int>(core.FrameWidth()), h = static_cast<int>(core.FrameHeight());
    const SDL_PixelFormat fmt = ToSdl(core.Format());
    if (tex_ && w == tex_w_ && h == tex_h_ && fmt == tex_fmt_) return;
    Destroy(tex_);
    tex_ = SDL_CreateTexture(r_, fmt, SDL_TEXTUREACCESS_STREAMING, w, h);
    tex_w_ = w;
    tex_h_ = h;
    tex_fmt_ = fmt;
    uploaded_serial_ = 0;
}

void VideoOut::Upload(const CoreHost& core) {
    if (core.Frame().empty()) return;
    EnsureTexture(core);
    if (!tex_ || core.FrameSerial() == uploaded_serial_) return;
    SDL_UpdateTexture(tex_, nullptr, core.Frame().data(), static_cast<int>(core.FramePitch()));
    uploaded_serial_ = core.FrameSerial();
}

void VideoOut::UpdateAmbient(const CoreHost& core) {
    // Cada 6 cuadros se reduce la imagen a 16×15 y se estira borrosa detrás del juego.
    if (core.FrameSerial() - ambient_serial_ < 6 && ambient_) return;
    ambient_serial_ = core.FrameSerial();
    unsigned w, h;
    const std::vector<uint8_t> rgba = core.FrameRGBA(&w, &h);
    if (rgba.empty()) return;
    constexpr int AW = 16, AH = 15;
    std::vector<uint8_t> small(AW * AH * 4);
    for (int y = 0; y < AH; ++y) {
        for (int x = 0; x < AW; ++x) {
            const unsigned x0 = x * w / AW, x1 = std::max(x0 + 1, (x + 1) * w / AW);
            const unsigned y0 = y * h / AH, y1 = std::max(y0 + 1, (y + 1) * h / AH);
            unsigned r = 0, g = 0, b = 0, n = 0;
            for (unsigned yy = y0; yy < y1; yy += 2)
                for (unsigned xx = x0; xx < x1; xx += 2) {
                    const uint8_t* p = &rgba[(yy * w + xx) * 4];
                    r += p[0]; g += p[1]; b += p[2]; ++n;
                }
            uint8_t* d = &small[(y * AW + x) * 4];
            d[0] = static_cast<uint8_t>(r / std::max(1u, n));
            d[1] = static_cast<uint8_t>(g / std::max(1u, n));
            d[2] = static_cast<uint8_t>(b / std::max(1u, n));
            d[3] = 255;
        }
    }
    if (!ambient_) {
        ambient_ = SDL_CreateTexture(r_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, AW, AH);
        if (ambient_) SDL_SetTextureScaleMode(ambient_, SDL_SCALEMODE_LINEAR);
    }
    if (ambient_) SDL_UpdateTexture(ambient_, nullptr, small.data(), AW * 4);
}

void VideoOut::EnsureEffects(int src_h, float strength, int effect) {
    if (scan_ && src_h == scan_h_ && std::fabs(strength - scan_strength_) < 0.001f && effect == scan_effect_) return;
    Destroy(scan_);
    Destroy(mask_);
    scan_h_ = src_h;
    scan_strength_ = strength;
    scan_effect_ = effect;

    // Líneas de barrido: dos filas por línea del juego, la segunda oscura.
    {
        const int h = src_h * 2;
        std::vector<uint8_t> px(h * 4);
        const float dark = effect == static_cast<int>(Effect::Crt) ? 0.55f : 0.65f;
        for (int y = 0; y < h; ++y) {
            px[y * 4 + 0] = px[y * 4 + 1] = px[y * 4 + 2] = 0;
            px[y * 4 + 3] = (y % 2) ? static_cast<uint8_t>(255 * dark * strength) : 0;
        }
        scan_ = SDL_CreateTexture(r_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, 1, h);
        if (scan_) {
            SDL_UpdateTexture(scan_, nullptr, px.data(), 4);
            SDL_SetTextureBlendMode(scan_, SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(scan_, SDL_SCALEMODE_LINEAR);
        }
    }
    // Máscara de fósforo (rejilla de apertura): columnas roja, verde y azul.
    {
        const uint8_t k = static_cast<uint8_t>(255 * (1.0f - 0.32f * strength));
        const uint8_t pattern[3][3] = {{255, k, k}, {k, 255, k}, {k, k, 255}};
        std::vector<uint8_t> px(3 * 4);
        for (int x = 0; x < 3; ++x) {
            px[x * 4 + 0] = pattern[x][0];
            px[x * 4 + 1] = pattern[x][1];
            px[x * 4 + 2] = pattern[x][2];
            px[x * 4 + 3] = 255;
        }
        mask_ = SDL_CreateTexture(r_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, 3, 1);
        if (mask_) {
            SDL_UpdateTexture(mask_, nullptr, px.data(), 12);
            SDL_SetTextureBlendMode(mask_, SDL_BLENDMODE_MOD);
            SDL_SetTextureScaleMode(mask_, SDL_SCALEMODE_NEAREST);
        }
    }
    if (!vignette_) {
        constexpr int N = 128;
        std::vector<uint8_t> px(N * N * 4);
        for (int y = 0; y < N; ++y)
            for (int x = 0; x < N; ++x) {
                const float dx = (x + 0.5f) / N * 2 - 1, dy = (y + 0.5f) / N * 2 - 1;
                const float d = std::sqrt(dx * dx * 0.7f + dy * dy * 0.7f);
                const float a = std::clamp((d - 0.55f) / 0.6f, 0.0f, 1.0f);
                uint8_t* p = &px[(y * N + x) * 4];
                p[0] = p[1] = p[2] = 0;
                p[3] = static_cast<uint8_t>(a * a * 200);
            }
        vignette_ = SDL_CreateTexture(r_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, N, N);
        if (vignette_) {
            SDL_UpdateTexture(vignette_, nullptr, px.data(), N * 4);
            SDL_SetTextureBlendMode(vignette_, SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(vignette_, SDL_SCALEMODE_LINEAR);
        }
    }
}

void VideoOut::Draw(const CoreHost& core, const Settings& s, int win_w, int win_h) {
    if (!tex_ || !tex_w_ || !tex_h_) return;
    const float aspect = core.AspectRatio();
    const float W = static_cast<float>(win_w), H = static_cast<float>(win_h);

    // Rectángulo de destino según el modo de escala.
    float dw, dh;
    switch (static_cast<ScaleMode>(s.scale_mode)) {
    case ScaleMode::Stretch:
        dw = W;
        dh = H;
        break;
    case ScaleMode::Integer: {
        int k = static_cast<int>(std::floor(H / tex_h_));
        while (k > 1 && tex_h_ * k * aspect > W) --k;
        k = std::max(1, k);
        dh = static_cast<float>(tex_h_ * k);
        dw = std::round(dh * aspect);
        break;
    }
    default:
        if (W / H > aspect) {
            dh = H;
            dw = std::round(H * aspect);
        } else {
            dw = W;
            dh = std::round(W / aspect);
        }
    }
    dest_ = {std::floor((W - dw) * 0.5f), std::floor((H - dh) * 0.5f), dw, dh};

    // Fondo ambiente.
    if (s.ambient && s.scale_mode != static_cast<int>(ScaleMode::Stretch)) {
        UpdateAmbient(core);
        if (ambient_) {
            const float cover = std::max(W / dw, H / dh) * 1.15f;
            const SDL_FRect amb{(W - dw * cover) * 0.5f, (H - dh * cover) * 0.5f, dw * cover, dh * cover};
            SDL_SetTextureColorMod(ambient_, 120, 120, 135);
            SDL_RenderTexture(r_, ambient_, nullptr, &amb);
            SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(r_, 10, 8, 18, 150);
            SDL_RenderFillRect(r_, nullptr);
            if (vignette_) SDL_RenderTexture(r_, vignette_, nullptr, nullptr);
        }
    }

    // Imagen del juego con el filtro elegido.
    const auto filter = static_cast<Filter>(s.filter);
    if (filter == Filter::SharpSmooth) {
        // Nítido suave: primero se agranda sin suavizar a un múltiplo entero y luego se
        // ajusta con suavizado. Los píxeles quedan cuadrados y sin bordes temblorosos.
        const int k = std::clamp(static_cast<int>(std::ceil(dh / tex_h_)), 1, 8);
        const int sw = tex_w_ * k, sh = tex_h_ * k;
        if (!sharp_ || sw != sharp_w_ || sh != sharp_h_) {
            Destroy(sharp_);
            sharp_ = SDL_CreateTexture(r_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, sw, sh);
            sharp_w_ = sw;
            sharp_h_ = sh;
            if (sharp_) SDL_SetTextureScaleMode(sharp_, SDL_SCALEMODE_LINEAR);
        }
        if (sharp_) {
            SDL_Texture* prev = SDL_GetRenderTarget(r_);
            SDL_SetRenderTarget(r_, sharp_);
            SDL_SetTextureScaleMode(tex_, SDL_SCALEMODE_NEAREST);
            SDL_RenderTexture(r_, tex_, nullptr, nullptr);
            SDL_SetRenderTarget(r_, prev);
            SDL_RenderTexture(r_, sharp_, nullptr, &dest_);
        }
    } else {
        SDL_SetTextureScaleMode(tex_, filter == Filter::Smooth ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
        SDL_RenderTexture(r_, tex_, nullptr, &dest_);
    }

    // Efectos de pantalla.
    const int effect = s.effect;
    if (effect != static_cast<int>(Effect::None) && s.effect_strength > 0.01f) {
        EnsureEffects(tex_h_, s.effect_strength, effect);
        // Con menos de 2 píxeles por línea las líneas de barrido hacen moiré: se omiten.
        if (scan_ && dh / tex_h_ >= 2.0f) SDL_RenderTexture(r_, scan_, nullptr, &dest_);
        if (effect == static_cast<int>(Effect::Crt)) {
            if (mask_ && dw >= tex_w_ * 3) SDL_RenderTextureTiled(r_, mask_, nullptr, 1.0f, &dest_);
            if (vignette_) {
                SDL_SetTextureAlphaModFloat(vignette_, 0.55f * s.effect_strength + 0.2f);
                SDL_RenderTexture(r_, vignette_, nullptr, &dest_);
                SDL_SetTextureAlphaModFloat(vignette_, 1.0f);
            }
        }
    }
}

} // namespace Satronho
