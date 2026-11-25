// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// «Captura Satronho» (build 26, como en Satronho NX): la captura lleva abajo una franja con el logo,
// el nombre del juego y su consola, y la fecha.
#include "util/strip.h"

#include <algorithm>
#include <cmath>
#include <fstream>

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "imstb_truetype.h"

#include "ui/kit.h"

namespace Satronho::Strip {

namespace {

std::vector<unsigned char> ReadFile(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::vector<unsigned char>(std::istreambuf_iterator<char>(in), {});
}

void Blend(std::vector<uint8_t>& img, unsigned w, int x, int y, uint32_t rgb, float a) {
    if (x < 0 || y < 0 || x >= static_cast<int>(w) || a <= 0) return;
    uint8_t* p = &img[(static_cast<size_t>(y) * w + x) * 4];
    a = std::min(a, 1.0f);
    p[0] = static_cast<uint8_t>(p[0] + ((rgb >> 16 & 255) - p[0]) * a);
    p[1] = static_cast<uint8_t>(p[1] + ((rgb >> 8 & 255) - p[1]) * a);
    p[2] = static_cast<uint8_t>(p[2] + ((rgb & 255) - p[2]) * a);
    p[3] = 255;
}

// Texto UTF-8 en una línea; devuelve el ancho dibujado.
int Text(std::vector<uint8_t>& img, unsigned w, unsigned h, const stbtt_fontinfo& f, float px, int x, int baseline, const std::string& s,
         uint32_t rgb, bool measure_only = false) {
    const float scale = stbtt_ScaleForPixelHeight(&f, px);
    float pen = 0;
    size_t i = 0;
    while (i < s.size()) {
        unsigned c = static_cast<unsigned char>(s[i]);
        int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
        if (extra) c &= (0x3F >> extra);
        for (int k = 1; k <= extra && i + k < s.size(); ++k) c = (c << 6) | (static_cast<unsigned char>(s[i + k]) & 0x3F);
        i += 1 + extra;
        int adv = 0, lsb = 0;
        stbtt_GetCodepointHMetrics(&f, static_cast<int>(c), &adv, &lsb);
        if (!measure_only) {
            int gw = 0, gh = 0, xo = 0, yo = 0;
            unsigned char* bmp = stbtt_GetCodepointBitmap(&f, scale, scale, static_cast<int>(c), &gw, &gh, &xo, &yo);
            for (int yy = 0; yy < gh; ++yy)
                for (int xx = 0; xx < gw; ++xx) {
                    const int X = x + static_cast<int>(pen) + xo + xx, Y = baseline + yo + yy;
                    if (Y >= 0 && Y < static_cast<int>(h)) Blend(img, w, X, Y, rgb, bmp[yy * gw + xx] / 255.0f);
                }
            stbtt_FreeBitmap(bmp, nullptr);
        }
        pen += adv * scale;
    }
    return static_cast<int>(pen);
}

} // namespace

bool Add(std::vector<uint8_t>& rgba, unsigned w, unsigned& h, const std::string& title, const std::string& subtitle, const std::string& right,
         uint32_t accent, const fs::path& assets) {
    if (w < 64 || h < 32) return false;
    const unsigned band = std::max(40u, static_cast<unsigned>(std::lround(h * 0.1)));
    const unsigned H = h + band;
    std::vector<uint8_t> out(static_cast<size_t>(w) * H * 4);
    std::copy(rgba.begin(), rgba.end(), out.begin());
    // Fondo: violeta muy oscuro con un brillo suave del color de la consola a la izquierda.
    for (unsigned y = h; y < H; ++y)
        for (unsigned x = 0; x < w; ++x) {
            uint8_t* p = &out[(static_cast<size_t>(y) * w + x) * 4];
            const float k = 1.0f - static_cast<float>(x) / w;
            p[0] = static_cast<uint8_t>(16 + ((accent >> 16 & 255) - 16) * 0.22f * k * k);
            p[1] = static_cast<uint8_t>(13 + ((accent >> 8 & 255) - 13) * 0.22f * k * k);
            p[2] = static_cast<uint8_t>(26 + ((accent & 255) - 26) * 0.22f * k * k);
            p[3] = 255;
        }
    // Línea del color de la consola arriba de la franja.
    const unsigned line = std::max(2u, band / 20);
    for (unsigned y = h; y < h + line; ++y)
        for (unsigned x = 0; x < w; ++x) Blend(out, w, static_cast<int>(x), static_cast<int>(y), accent, 0.9f);
    // Logo.
    int x = static_cast<int>(band * 0.22f);
    std::vector<unsigned char> px;
    int lw = 0, lh = 0;
    if (Ui::DecodeImage(Paths::ToUtf8(assets / "logo.png"), px, &lw, &lh) && lw > 0 && lh > 0) {
        const int size = static_cast<int>(band * 0.74f);
        const int top = static_cast<int>(h + (band - size) / 2 + line / 2);
        for (int yy = 0; yy < size; ++yy)
            for (int xx = 0; xx < size; ++xx) {
                const int sx = xx * lw / size, sy = yy * lh / size;
                const unsigned char* s = &px[(static_cast<size_t>(sy) * lw + sx) * 4];
                Blend(out, w, x + xx, top + yy, static_cast<uint32_t>(s[0]) << 16 | static_cast<uint32_t>(s[1]) << 8 | s[2], s[3] / 255.0f);
            }
        x += size + static_cast<int>(band * 0.2f);
    }
    const auto bold = ReadFile(assets / "fonts" / "Poppins-SemiBold.ttf");
    const auto regular = ReadFile(assets / "fonts" / "Poppins-Regular.ttf");
    stbtt_fontinfo fb{}, fr{};
    const bool okb = !bold.empty() && stbtt_InitFont(&fb, bold.data(), stbtt_GetFontOffsetForIndex(bold.data(), 0));
    const bool okr = !regular.empty() && stbtt_InitFont(&fr, regular.data(), stbtt_GetFontOffsetForIndex(regular.data(), 0));
    if (okb && okr) {
        const float big = band * 0.34f, small = band * 0.24f;
        const int right_w = Text(out, w, H, fr, small, 0, 0, right, 0, true);
        const int right_x = static_cast<int>(w) - right_w - static_cast<int>(band * 0.3f);
        // El título se corta si choca con lo de la derecha.
        std::string t = title;
        while (t.size() > 4 && x + Text(out, w, H, fb, big, 0, 0, t + "…", 0, true) > right_x - static_cast<int>(band * 0.3f)) {
            t.pop_back();
            while (!t.empty() && (static_cast<unsigned char>(t.back()) & 0xC0) == 0x80) t.pop_back();
            if (!t.empty() && static_cast<unsigned char>(t.back()) >= 0xC0) t.pop_back();
        }
        if (t != title) t += "…";
        Text(out, w, H, fb, big, x, static_cast<int>(h + band * 0.5f), t, 0xF4F1FF);
        Text(out, w, H, fr, small, x, static_cast<int>(h + band * 0.82f), subtitle, accent);
        Text(out, w, H, fr, small, right_x, static_cast<int>(h + band * 0.66f), right, 0xB9B2CC);
    }
    rgba.swap(out);
    h = H;
    return true;
}

} // namespace Satronho::Strip
