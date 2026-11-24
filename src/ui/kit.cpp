// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/kit.h"
#include "util/i18n.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>

#include <SDL3/SDL.h>

#include "imgui_internal.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "util/paths.h"

namespace Satronho::Ui {

namespace {
float g_scale = 1.0f;
Fonts g_fonts;
constexpr float kPi = 3.14159265f;

ImVec2 Add(ImVec2 a, ImVec2 b) { return {a.x + b.x, a.y + b.y}; }
float Anim(ImGuiID id, bool target, float speed = 12.0f) {
    ImGuiStorage* st = ImGui::GetStateStorage();
    float v = st->GetFloat(id, target ? 1.0f : 0.0f);
    const float dt = ImGui::GetIO().DeltaTime;
    v += (target ? 1.0f : -1.0f) * dt * speed;
    v = std::clamp(v, 0.0f, 1.0f);
    st->SetFloat(id, v);
    return v;
}
unsigned Mix(unsigned a, unsigned b, float t) {
    auto ch = [&](int s) {
        const float x = ((a >> s) & 0xFF) * (1 - t) + ((b >> s) & 0xFF) * t;
        return static_cast<unsigned>(x + 0.5f) << s;
    };
    return ch(16) | ch(8) | ch(0);
}
} // namespace

// ---------------------------------------------------------------- colores

ImU32 U32(unsigned rgb, float a) {
    return IM_COL32((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF, static_cast<int>(std::clamp(a, 0.0f, 1.0f) * 255));
}
ImVec4 V4(unsigned rgb, float a) {
    return {((rgb >> 16) & 0xFF) / 255.0f, ((rgb >> 8) & 0xFF) / 255.0f, (rgb & 0xFF) / 255.0f, a};
}
ImU32 Hsv(float h, float s, float v, float a) {
    float r, g, b;
    ImGui::ColorConvertHSVtoRGB(std::fmod(h, 360.0f) / 360.0f, s, v, r, g, b);
    return ImGui::ColorConvertFloat4ToU32({r, g, b, a});
}

// ---------------------------------------------------------------- temas

const std::vector<Theme>& Themes() {
    static const std::vector<Theme> t = {
        {"Noche Satronho", "El morado de siempre, como Satronho NX.",
         0x0F0D17, 0x13111D, 0x1B1829, 0x241F38, 0x2E2848, 0x2E2848, 0xEBE8F5, 0xA9A4C2, 0x6E6A86, 0x9B8CFF, 0xA78BFA, false},
        {"Medianoche", "Azul profundo, tranquilo para jugar de noche.",
         0x0A0F1C, 0x0E1426, 0x141C33, 0x1B2542, 0x253153, 0x253153, 0xE6ECF8, 0x9DABC8, 0x65728F, 0x6EA8FF, 0x8DBBFF, false},
        {"Atardecer", "Cálido, con naranjas de arcade.",
         0x160E0C, 0x1C1210, 0x261915, 0x32211C, 0x422C25, 0x422C25, 0xF6EBE6, 0xC7ABA0, 0x8A7068, 0xFF9F5A, 0xFFB37A, false},
        {"Bosque", "Verdes suaves, descansa la vista.",
         0x0B1310, 0x0F1915, 0x15231D, 0x1C2E26, 0x263D32, 0x263D32, 0xE6F3EC, 0x9EBDAE, 0x667F72, 0x5FD39A, 0x7FE0B0, false},
        {"Claro", "Fondo claro para jugar de día.",
         0xF2F0F7, 0xE8E5F1, 0xFFFFFF, 0xEDE9F7, 0xDDD7EF, 0xD5CFE6, 0x1E1A2E, 0x575170, 0x878199, 0x6B55F0, 0x7E6AF2, true},
    };
    return t;
}

namespace {
int g_theme = 0;
}

void ApplyTheme(int index) {
    const auto& all = Themes();
    g_theme = std::clamp(index, 0, static_cast<int>(all.size()) - 1);
    const Theme& t = all[g_theme];
    Color::Bg = t.bg;
    Color::Side = t.side;
    Color::Surface = t.surface;
    Color::Surface2 = t.surface2;
    Color::Surface3 = t.surface3;
    Color::Border = t.border;
    Color::Text = t.text;
    Color::Muted = t.muted;
    Color::Dim = t.dim;
    Color::Accent = t.accent;
    Color::Accent2 = t.accent2;
}
int CurrentTheme() { return g_theme; }
bool LightTheme() { return Themes()[g_theme].light; }

// ---------------------------------------------------------------- tipografía

Fonts& GetFonts() { return g_fonts; }
float S(float px) { return px * g_scale; }
void SetScale(float s) { g_scale = s; }
float Scale() { return g_scale; }

void LoadFonts(const std::string& dir) {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    auto load = [&](const char* file) -> ImFont* {
        const std::string path = dir + "/" + file;
        ImFontConfig cfg;
        cfg.OversampleH = 2;
        cfg.PixelSnapH = false;
        ImFont* f = io.Fonts->AddFontFromFileTTF(path.c_str(), 16.0f, &cfg);
        return f ? f : io.Fonts->AddFontDefault();
    };
    g_fonts.regular = load("Poppins-Regular.ttf");
    g_fonts.medium = load("Poppins-Medium.ttf");
    g_fonts.semibold = load("Poppins-SemiBold.ttf");
    g_fonts.bold = load("Poppins-Bold.ttf");
    g_fonts.mono = load("JetBrainsMono-Medium.ttf");
    io.FontDefault = g_fonts.regular;
}

void ApplyStyle() {
    ImGuiStyle style;
    ImGui::StyleColorsDark(&style);
    style.WindowPadding = {0, 0};
    style.FramePadding = {12, 8};
    style.ItemSpacing = {10, 10};
    style.ItemInnerSpacing = {8, 6};
    style.ScrollbarSize = 10;
    style.GrabMinSize = 14;
    style.WindowRounding = 0;
    style.ChildRounding = 14;
    style.FrameRounding = 10;
    style.PopupRounding = 12;
    style.ScrollbarRounding = 8;
    style.GrabRounding = 10;
    style.TabRounding = 8;
    style.WindowBorderSize = 0;
    style.ChildBorderSize = 0;
    style.PopupBorderSize = 1;
    style.FrameBorderSize = 0;
    style.SelectableTextAlign = {0, 0.5f};
    style.FontSizeBase = 16.0f;

    ImVec4* c = style.Colors;
    c[ImGuiCol_Text] = V4(Color::Text);
    c[ImGuiCol_TextDisabled] = V4(Color::Dim);
    c[ImGuiCol_WindowBg] = V4(Color::Bg);
    c[ImGuiCol_ChildBg] = V4(0, 0);
    c[ImGuiCol_PopupBg] = V4(Color::Surface, 0.98f);
    c[ImGuiCol_Border] = V4(Color::Border);
    c[ImGuiCol_FrameBg] = V4(Color::Surface2);
    c[ImGuiCol_FrameBgHovered] = V4(Color::Surface3);
    c[ImGuiCol_FrameBgActive] = V4(Color::Surface3);
    c[ImGuiCol_ScrollbarBg] = V4(0, 0);
    c[ImGuiCol_ScrollbarGrab] = V4(Color::Surface3);
    c[ImGuiCol_ScrollbarGrabHovered] = V4(Color::Dim);
    c[ImGuiCol_ScrollbarGrabActive] = V4(Color::Accent);
    c[ImGuiCol_CheckMark] = V4(Color::Accent);
    c[ImGuiCol_SliderGrab] = V4(Color::Accent);
    c[ImGuiCol_SliderGrabActive] = V4(Color::Accent2);
    c[ImGuiCol_Button] = V4(Color::Surface2);
    c[ImGuiCol_ButtonHovered] = V4(Color::Surface3);
    c[ImGuiCol_ButtonActive] = V4(Color::Accent, 0.6f);
    c[ImGuiCol_Header] = V4(Color::Accent, 0.22f);
    c[ImGuiCol_HeaderHovered] = V4(Color::Accent, 0.30f);
    c[ImGuiCol_HeaderActive] = V4(Color::Accent, 0.40f);
    c[ImGuiCol_Separator] = V4(Color::Border);
    c[ImGuiCol_NavCursor] = V4(Color::Gold);
    c[ImGuiCol_TextSelectedBg] = V4(Color::Accent, 0.35f);
    c[ImGuiCol_ModalWindowDimBg] = V4(0x05040A, 0.70f);
    style.ScaleAllSizes(g_scale);
    style.FontSizeBase = 16.0f * g_scale;
    ImGui::GetStyle() = style;
}

FontScope::FontScope(ImFont* f, float px) { ImGui::PushFont(f, S(px)); }
FontScope::~FontScope() { ImGui::PopFont(); }

// ---------------------------------------------------------------- imágenes

ImTextureRef Image::Ref() const { return ImTextureRef(tex); }

namespace {
TextureBackend g_tex_backend;
}

void SetTextureBackend(const TextureBackend& b) { g_tex_backend = b; }

Image ImageFromRGBA(const unsigned char* rgba, int w, int h, bool smooth) {
    Image img;
    if (!g_tex_backend.create || !rgba || w <= 0 || h <= 0) return img;
    img.tex = g_tex_backend.create(rgba, w, h, smooth);
    if (img.tex != ImTextureID_Invalid) {
        img.w = w;
        img.h = h;
    }
    return img;
}

bool DecodeImage(const std::string& file, std::vector<unsigned char>& rgba, int* w, int* h) {
    size_t size = 0;
    void* data = SDL_LoadFile(file.c_str(), &size);
    if (!data) return false;
    int n;
    unsigned char* px = stbi_load_from_memory(static_cast<const stbi_uc*>(data), static_cast<int>(size), w, h, &n, 4);
    SDL_free(data);
    if (!px) return false;
    rgba.assign(px, px + static_cast<size_t>(*w) * *h * 4);
    stbi_image_free(px);
    return true;
}

Image LoadImageFile(const std::string& file) {
    std::vector<unsigned char> px;
    int w = 0, h = 0;
    if (!DecodeImage(file, px, &w, &h)) return {};
    return ImageFromRGBA(px.data(), w, h);
}

SDL_Surface* LoadSurface(const std::string& file) {
    size_t size = 0;
    void* data = SDL_LoadFile(file.c_str(), &size);
    if (!data) return nullptr;
    int w, h, n;
    unsigned char* px = stbi_load_from_memory(static_cast<const stbi_uc*>(data), static_cast<int>(size), &w, &h, &n, 4);
    SDL_free(data);
    if (!px) return nullptr;
    SDL_Surface* s = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
    if (s) {
        for (int y = 0; y < h; ++y) std::memcpy(static_cast<unsigned char*>(s->pixels) + y * s->pitch, px + y * w * 4, w * 4);
    }
    stbi_image_free(px);
    return s;
}

void FreeImage(Image& img) {
    if (img.tex != ImTextureID_Invalid && g_tex_backend.destroy) g_tex_backend.destroy(img.tex);
    img = {};
}

static void WriteToIo(void* ctx, void* data, int size) {
    SDL_WriteIO(static_cast<SDL_IOStream*>(ctx), data, static_cast<size_t>(size));
}

bool SavePng(const std::string& file, const unsigned char* rgba, int w, int h) {
    SDL_IOStream* io = SDL_IOFromFile(file.c_str(), "wb");
    if (!io) return false;
    const int ok = stbi_write_png_to_func(&WriteToIo, io, w, h, 4, rgba, w * 4);
    SDL_CloseIO(io);
    return ok != 0;
}

// ---------------------------------------------------------------- íconos

void DrawIcon(ImDrawList* dl, Icon icon, ImVec2 c, float sz, ImU32 col) {
    const float h = sz * 0.5f;
    const float t = std::max(1.5f, sz * 0.095f);
    auto P = [&](float x, float y) { return ImVec2(c.x + x * h, c.y + y * h); };
    switch (icon) {
    case Icon::None: break;
    case Icon::Library:
    case Icon::Grid: {
        const float g = 0.16f, s = 0.68f;
        for (int i = 0; i < 4; ++i) {
            const float x = (i % 2) ? g : -g - s, y = (i / 2) ? g : -g - s;
            dl->AddRectFilled(P(x, y), P(x + s, y + s), col, sz * 0.08f);
        }
        break;
    }
    case Icon::Carousel: {
        // Una carátula grande al centro y dos más chicas a los lados.
        dl->AddRectFilled(P(-0.34f, -0.72f), P(0.34f, 0.72f), col, sz * 0.08f);
        dl->AddRectFilled(P(-0.95f, -0.46f), P(-0.52f, 0.46f), col, sz * 0.06f);
        dl->AddRectFilled(P(0.52f, -0.46f), P(0.95f, 0.46f), col, sz * 0.06f);
        break;
    }
    case Icon::List: {
        // Tres filas: cuadrito (carátula) y raya (nombre).
        for (int i = -1; i <= 1; ++i) {
            dl->AddRectFilled(P(-0.9f, i * 0.62f - 0.22f), P(-0.46f, i * 0.62f + 0.22f), col, sz * 0.05f);
            dl->AddRectFilled(P(-0.3f, i * 0.62f - 0.1f), P(0.92f, i * 0.62f + 0.1f), col, sz * 0.05f);
        }
        break;
    }
    case Icon::ChevronDown: {
        dl->AddLine(P(-0.6f, -0.25f), P(0.0f, 0.35f), col, sz * 0.16f);
        dl->AddLine(P(0.0f, 0.35f), P(0.6f, -0.25f), col, sz * 0.16f);
        break;
    }
    case Icon::Tools: {
        // Llave inglesa cruzada con un destornillador.
        dl->AddLine(P(-0.75f, 0.75f), P(0.35f, -0.35f), col, sz * 0.2f);
        dl->AddCircle(P(0.5f, -0.5f), sz * 0.24f, col, 0, sz * 0.14f);
        dl->AddLine(P(-0.7f, -0.7f), P(0.7f, 0.7f), col, sz * 0.12f);
        dl->AddRectFilled(P(0.45f, 0.45f), P(0.85f, 0.85f), col, sz * 0.04f);
        break;
    }
    case Icon::Chat: {
        dl->AddRectFilled(P(-0.9f, -0.7f), P(0.9f, 0.45f), col, sz * 0.22f);
        dl->AddTriangleFilled(P(-0.5f, 0.4f), P(-0.1f, 0.4f), P(-0.55f, 0.85f), col);
        const ImU32 bg = U32(Color::Bg);
        for (int i = -1; i <= 1; ++i) dl->AddCircleFilled(P(i * 0.42f, -0.12f), sz * 0.07f, bg);
        break;
    }
    case Icon::Send: {
        dl->AddTriangleFilled(P(-0.85f, -0.75f), P(0.9f, 0.0f), P(-0.85f, 0.75f), col);
        break;
    }
    case Icon::Gamepad: {
        dl->AddRectFilled(P(-0.95f, -0.45f), P(0.95f, 0.55f), col, sz * 0.25f);
        const ImU32 bg = U32(Color::Bg);
        dl->AddRectFilled(P(-0.68f, -0.06f), P(-0.28f, 0.10f), bg, 1);
        dl->AddRectFilled(P(-0.56f, -0.20f), P(-0.40f, 0.24f), bg, 1);
        dl->AddCircleFilled(P(0.40f, -0.05f), h * 0.11f, bg);
        dl->AddCircleFilled(P(0.62f, 0.15f), h * 0.11f, bg);
        break;
    }
    case Icon::Gear: {
        for (int i = 0; i < 8; ++i) {
            const float a = i * kPi / 4;
            const ImVec2 d(std::cos(a), std::sin(a));
            const ImVec2 n(-d.y, d.x);
            const float r0 = 0.62f, r1 = 0.95f, w = 0.17f;
            ImVec2 q[4] = {P(d.x * r0 + n.x * w, d.y * r0 + n.y * w), P(d.x * r1 + n.x * w, d.y * r1 + n.y * w),
                           P(d.x * r1 - n.x * w, d.y * r1 - n.y * w), P(d.x * r0 - n.x * w, d.y * r0 - n.y * w)};
            dl->AddConvexPolyFilled(q, 4, col);
        }
        dl->AddCircle(c, h * 0.55f, col, 0, t * 1.6f);
        break;
    }
    case Icon::Sparkle: {
        auto star = [&](ImVec2 o, float r) {
            ImVec2 pts[8];
            for (int i = 0; i < 8; ++i) {
                const float a = i * kPi / 4 - kPi / 2;
                const float rr = (i % 2) ? r * 0.28f : r;
                pts[i] = {o.x + std::cos(a) * rr, o.y + std::sin(a) * rr};
            }
            for (int i = 0; i < 8; i += 2) {
                ImVec2 tri[3] = {o, pts[i], pts[(i + 1) % 8]};
                dl->AddTriangleFilled(tri[0], tri[1], tri[2], col);
                ImVec2 tri2[3] = {o, pts[i], pts[(i + 7) % 8]};
                dl->AddTriangleFilled(tri2[0], tri2[1], tri2[2], col);
            }
        };
        star(P(-0.15f, 0.1f), h * 0.85f);
        star(P(0.62f, -0.6f), h * 0.35f);
        break;
    }
    case Icon::Bell: {
        dl->PathArcTo(P(0, -0.2f), h * 0.55f, kPi, 2 * kPi);
        dl->PathLineTo(P(0.55f, 0.45f));
        dl->PathLineTo(P(0.8f, 0.6f));
        dl->PathLineTo(P(-0.8f, 0.6f));
        dl->PathLineTo(P(-0.55f, 0.45f));
        dl->PathFillConvex(col);
        dl->AddCircleFilled(P(0, 0.78f), h * 0.17f, col);
        break;
    }
    case Icon::Info:
        dl->AddCircle(c, h * 0.88f, col, 0, t);
        dl->AddLine(P(0, -0.1f), P(0, 0.48f), col, t * 1.2f);
        dl->AddCircleFilled(P(0, -0.42f), t * 0.75f, col);
        break;
    case Icon::Search:
        dl->AddCircle(P(-0.15f, -0.15f), h * 0.55f, col, 0, t * 1.2f);
        dl->AddLine(P(0.28f, 0.28f), P(0.8f, 0.8f), col, t * 1.5f);
        break;
    case Icon::Play:
        dl->AddTriangleFilled(P(-0.5f, -0.75f), P(0.8f, 0), P(-0.5f, 0.75f), col);
        break;
    case Icon::Plus:
        dl->AddLine(P(0, -0.75f), P(0, 0.75f), col, t * 1.3f);
        dl->AddLine(P(-0.75f, 0), P(0.75f, 0), col, t * 1.3f);
        break;
    case Icon::Folder:
        dl->AddRectFilled(P(-0.9f, -0.6f), P(-0.1f, -0.2f), col, sz * 0.06f);
        dl->AddRectFilled(P(-0.9f, -0.4f), P(0.9f, 0.7f), col, sz * 0.08f);
        break;
    case Icon::Star:
    case Icon::StarFill: {
        ImVec2 pts[10];
        for (int i = 0; i < 10; ++i) {
            const float a = i * kPi / 5 - kPi / 2;
            const float r = (i % 2) ? 0.42f : 0.95f;
            pts[i] = P(std::cos(a) * r, std::sin(a) * r + 0.06f);
        }
        if (icon == Icon::StarFill) {
            for (int i = 0; i < 10; ++i) dl->AddTriangleFilled(c, pts[i], pts[(i + 1) % 10], col);
        } else {
            dl->AddPolyline(pts, 10, col, t, ImDrawFlags_Closed);
        }
        break;
    }
    case Icon::Clock:
        dl->AddCircle(c, h * 0.85f, col, 0, t);
        dl->AddLine(c, P(0, -0.5f), col, t);
        dl->AddLine(c, P(0.38f, 0.18f), col, t);
        break;
    case Icon::ChevronRight:
        dl->AddLine(P(-0.25f, -0.6f), P(0.35f, 0), col, t * 1.3f);
        dl->AddLine(P(0.35f, 0), P(-0.25f, 0.6f), col, t * 1.3f);
        break;
    case Icon::ChevronLeft:
        dl->AddLine(P(0.25f, -0.6f), P(-0.35f, 0), col, t * 1.3f);
        dl->AddLine(P(-0.35f, 0), P(0.25f, 0.6f), col, t * 1.3f);
        break;
    case Icon::Close:
        dl->AddLine(P(-0.6f, -0.6f), P(0.6f, 0.6f), col, t * 1.3f);
        dl->AddLine(P(0.6f, -0.6f), P(-0.6f, 0.6f), col, t * 1.3f);
        break;
    case Icon::Pause:
        dl->AddRectFilled(P(-0.6f, -0.7f), P(-0.15f, 0.7f), col, 2);
        dl->AddRectFilled(P(0.15f, -0.7f), P(0.6f, 0.7f), col, 2);
        break;
    case Icon::Save:
        dl->AddRectFilled(P(-0.8f, -0.8f), P(0.8f, 0.8f), col, sz * 0.1f);
        dl->AddRectFilled(P(-0.45f, -0.8f), P(0.35f, -0.25f), U32(Color::Bg));
        dl->AddRectFilled(P(-0.5f, 0.15f), P(0.5f, 0.8f), U32(Color::Bg), 2);
        break;
    case Icon::Load:
        dl->AddLine(P(0, 0.35f), P(0, -0.8f), col, t * 1.3f);
        dl->AddLine(P(-0.45f, -0.35f), P(0, -0.8f), col, t * 1.3f);
        dl->AddLine(P(0.45f, -0.35f), P(0, -0.8f), col, t * 1.3f);
        dl->AddLine(P(-0.8f, 0.3f), P(-0.8f, 0.8f), col, t * 1.2f);
        dl->AddLine(P(-0.8f, 0.8f), P(0.8f, 0.8f), col, t * 1.2f);
        dl->AddLine(P(0.8f, 0.8f), P(0.8f, 0.3f), col, t * 1.2f);
        break;
    case Icon::Restart:
    case Icon::Refresh: {
        dl->PathArcTo(c, h * 0.7f, -kPi * 0.35f, kPi * 1.35f, 24);
        dl->PathStroke(col, t * 1.2f);
        const ImVec2 e = P(std::cos(-kPi * 0.35f) * 0.7f, std::sin(-kPi * 0.35f) * 0.7f);
        dl->AddTriangleFilled(Add(e, {h * 0.32f, -h * 0.12f}), Add(e, {-h * 0.12f, -h * 0.42f}), Add(e, {-h * 0.05f, h * 0.22f}), col);
        break;
    }
    case Icon::Camera:
        dl->AddRectFilled(P(-0.9f, -0.45f), P(0.9f, 0.75f), col, sz * 0.12f);
        dl->AddRectFilled(P(-0.35f, -0.7f), P(0.35f, -0.4f), col, 2);
        dl->AddCircleFilled(P(0, 0.15f), h * 0.36f, U32(Color::Bg));
        dl->AddCircleFilled(P(0, 0.15f), h * 0.2f, col);
        break;
    case Icon::Exit:
        dl->AddLine(P(-0.2f, -0.8f), P(-0.8f, -0.8f), col, t);
        dl->AddLine(P(-0.8f, -0.8f), P(-0.8f, 0.8f), col, t);
        dl->AddLine(P(-0.8f, 0.8f), P(-0.2f, 0.8f), col, t);
        dl->AddLine(P(-0.3f, 0), P(0.85f, 0), col, t * 1.2f);
        dl->AddLine(P(0.45f, -0.4f), P(0.85f, 0), col, t * 1.2f);
        dl->AddLine(P(0.45f, 0.4f), P(0.85f, 0), col, t * 1.2f);
        break;
    case Icon::Meter:
        dl->AddRectFilled(P(-0.8f, 0.2f), P(-0.4f, 0.8f), col, 2);
        dl->AddRectFilled(P(-0.2f, -0.25f), P(0.2f, 0.8f), col, 2);
        dl->AddRectFilled(P(0.4f, -0.8f), P(0.8f, 0.8f), col, 2);
        break;
    case Icon::Volume:
        dl->AddRectFilled(P(-0.85f, -0.3f), P(-0.45f, 0.3f), col, 1);
        dl->AddTriangleFilled(P(-0.5f, -0.3f), P(0.05f, -0.8f), P(0.05f, 0.8f), col);
        dl->AddTriangleFilled(P(-0.5f, -0.3f), P(0.05f, 0.8f), P(-0.5f, 0.3f), col);
        dl->PathArcTo(P(0.15f, 0), h * 0.45f, -kPi / 3, kPi / 3);
        dl->PathStroke(col, t);
        dl->PathArcTo(P(0.15f, 0), h * 0.75f, -kPi / 3, kPi / 3);
        dl->PathStroke(col, t);
        break;
    case Icon::Rewind:
        dl->AddTriangleFilled(P(0.0f, -0.6f), P(-0.85f, 0), P(0.0f, 0.6f), col);
        dl->AddTriangleFilled(P(0.85f, -0.6f), P(0.0f, 0), P(0.85f, 0.6f), col);
        break;
    case Icon::Forward:
        dl->AddTriangleFilled(P(0.0f, -0.6f), P(0.85f, 0), P(0.0f, 0.6f), col);
        dl->AddTriangleFilled(P(-0.85f, -0.6f), P(0.0f, 0), P(-0.85f, 0.6f), col);
        break;
    case Icon::Check:
        dl->AddLine(P(-0.7f, 0.05f), P(-0.2f, 0.55f), col, t * 1.4f);
        dl->AddLine(P(-0.2f, 0.55f), P(0.75f, -0.5f), col, t * 1.4f);
        break;
    case Icon::Cpu:
    case Icon::Chip:
        dl->AddRect(P(-0.55f, -0.55f), P(0.55f, 0.55f), col, 3, t);
        dl->AddRectFilled(P(-0.22f, -0.22f), P(0.22f, 0.22f), col, 2);
        for (int i = -1; i <= 1; ++i) {
            const float o = i * 0.3f;
            dl->AddLine(P(o, -0.55f), P(o, -0.85f), col, t);
            dl->AddLine(P(o, 0.55f), P(o, 0.85f), col, t);
            dl->AddLine(P(-0.55f, o), P(-0.85f, o), col, t);
            dl->AddLine(P(0.55f, o), P(0.85f, o), col, t);
        }
        break;
    case Icon::Memory:
        dl->AddRect(P(-0.9f, -0.45f), P(0.9f, 0.35f), col, 2, t);
        for (int i = 0; i < 3; ++i) dl->AddRectFilled(P(-0.6f + i * 0.45f, -0.2f), P(-0.35f + i * 0.45f, 0.1f), col);
        for (int i = 0; i < 6; ++i) dl->AddLine(P(-0.75f + i * 0.3f, 0.35f), P(-0.75f + i * 0.3f, 0.65f), col, t);
        break;
    case Icon::Monitor:
        dl->AddRect(P(-0.9f, -0.7f), P(0.9f, 0.4f), col, 3, t);
        dl->AddLine(P(0, 0.4f), P(0, 0.75f), col, t);
        dl->AddLine(P(-0.45f, 0.78f), P(0.45f, 0.78f), col, t * 1.2f);
        break;
    case Icon::Keyboard:
        dl->AddRect(P(-0.95f, -0.55f), P(0.95f, 0.55f), col, 3, t);
        for (int r = 0; r < 2; ++r)
            for (int i = 0; i < 5; ++i) dl->AddRectFilled(P(-0.7f + i * 0.33f, -0.3f + r * 0.3f), P(-0.55f + i * 0.33f, -0.17f + r * 0.3f), col);
        dl->AddLine(P(-0.45f, 0.32f), P(0.45f, 0.32f), col, t);
        break;
    case Icon::Paw: {
        // Huella de perrito: almohadilla grande y cuatro deditos.
        dl->AddEllipseFilled(P(0, 0.4f), {sz * 0.27f, sz * 0.22f}, col);
        dl->AddEllipseFilled(P(-0.72f, -0.08f), {sz * 0.11f, sz * 0.14f}, col);
        dl->AddEllipseFilled(P(-0.27f, -0.62f), {sz * 0.11f, sz * 0.15f}, col);
        dl->AddEllipseFilled(P(0.27f, -0.62f), {sz * 0.11f, sz * 0.15f}, col);
        dl->AddEllipseFilled(P(0.72f, -0.08f), {sz * 0.11f, sz * 0.14f}, col);
        break;
    }
    case Icon::Heart:
        dl->AddCircleFilled(P(-0.35f, -0.2f), h * 0.42f, col);
        dl->AddCircleFilled(P(0.35f, -0.2f), h * 0.42f, col);
        dl->AddTriangleFilled(P(-0.76f, -0.05f), P(0.76f, -0.05f), P(0, 0.8f), col);
        break;
    case Icon::File:
        dl->PathLineTo(P(-0.6f, -0.85f));
        dl->PathLineTo(P(0.25f, -0.85f));
        dl->PathLineTo(P(0.65f, -0.45f));
        dl->PathLineTo(P(0.65f, 0.85f));
        dl->PathLineTo(P(-0.6f, 0.85f));
        dl->PathStroke(col, t, ImDrawFlags_Closed);
        dl->AddLine(P(-0.3f, 0.0f), P(0.35f, 0.0f), col, t);
        dl->AddLine(P(-0.3f, 0.35f), P(0.35f, 0.35f), col, t);
        break;
    case Icon::Palette:
        dl->AddCircle(c, h * 0.85f, col, 0, t);
        dl->AddCircleFilled(P(-0.35f, -0.25f), h * 0.14f, col);
        dl->AddCircleFilled(P(0.1f, -0.45f), h * 0.14f, col);
        dl->AddCircleFilled(P(0.45f, -0.1f), h * 0.14f, col);
        dl->AddCircleFilled(P(-0.25f, 0.3f), h * 0.18f, col);
        break;
    }
}

void IconText(Icon icon, const char* text, ImU32 color) {
    text = Tr(text);
    const float sz = ImGui::GetFontSize();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    DrawIcon(ImGui::GetWindowDrawList(), icon, {p.x + sz * 0.5f, p.y + sz * 0.55f}, sz * 0.85f, color);
    ImGui::Dummy({sz, sz});
    ImGui::SameLine(0, S(8));
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
}

// ---------------------------------------------------------------- controles

bool Button(const char* label, ButtonKind kind, ImVec2 size, Icon icon, bool with_icon) {
    label = Tr(label);
    const char* end = ImGui::FindRenderedTextEnd(label);
    const ImVec2 ts = ImGui::CalcTextSize(label, end);
    const float icon_w = with_icon ? ImGui::GetFontSize() + S(8) : 0;
    const ImVec2 pad(S(18), S(10));
    if (size.x <= 0) size.x = ts.x + icon_w + pad.x * 2;
    if (size.y <= 0) size.y = ts.y + pad.y * 2;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton(label, size);
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();
    const bool disabled = ImGui::GetItemFlags() & ImGuiItemFlags_Disabled;
    const float hv = Anim(ImGui::GetItemID(), hovered, 14.0f);

    unsigned bg = Color::Surface2, fg = Color::Text, border = 0;
    float bg_a = 1.0f;
    switch (kind) {
    case ButtonKind::Primary: bg = Mix(Color::Accent, 0xB7ACFF, hv); fg = 0x14101F; break;
    case ButtonKind::Secondary: bg = Mix(Color::Surface2, Color::Surface3, hv); border = Color::Border; break;
    case ButtonKind::Ghost: bg = Color::Surface2; bg_a = hv * 0.9f; break;
    case ButtonKind::Danger: bg = Mix(0x3A1F2A, 0x4A2433, hv); fg = Color::Red; break;
    }
    if (held) bg = Mix(bg, 0xFFFFFF, 0.06f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p1(p.x + size.x, p.y + size.y);
    const float rr = S(12);
    if (kind == ButtonKind::Primary && hv > 0) dl->AddRectFilled({p.x - 2, p.y - 1}, {p1.x + 2, p1.y + 3}, U32(Color::Accent, 0.18f * hv), rr + 2);
    dl->AddRectFilled(p, p1, U32(bg, (disabled ? 0.45f : 1.0f) * bg_a), rr);
    if (border) dl->AddRect(p, p1, U32(border), rr, 1.0f);
    const float x0 = p.x + (size.x - ts.x - icon_w) * 0.5f;
    const float y0 = p.y + (size.y - ts.y) * 0.5f;
    const ImU32 fgc = U32(fg, disabled ? 0.5f : 1.0f);
    if (with_icon) DrawIcon(dl, icon, {x0 + ImGui::GetFontSize() * 0.45f, y0 + ts.y * 0.52f}, ImGui::GetFontSize() * 0.8f, fgc);
    dl->AddText({x0 + icon_w, y0}, fgc, label, end);
    return pressed && !disabled;
}

bool IconButton(const char* id, Icon icon, float size, ImU32 color, const char* tooltip, bool active) {
    tooltip = Tr(tooltip);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton(id, {size, size});
    const bool hovered = ImGui::IsItemHovered();
    const float hv = Anim(ImGui::GetItemID(), hovered || active, 14.0f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (hv > 0) dl->AddRectFilled(p, {p.x + size, p.y + size}, U32(active ? Color::Accent : Color::Surface3, (active ? 0.25f : 0.9f) * hv), size * 0.3f);
    DrawIcon(dl, icon, {p.x + size * 0.5f, p.y + size * 0.5f}, size * 0.48f, color);
    if (tooltip && hovered) HelpTooltip(tooltip);
    return pressed;
}

bool NavItem(const char* label, Icon icon, bool selected, const char* badge, bool disabled, unsigned accent, const char* hint) {
    label = Tr(label);
    badge = Tr(badge);
    hint = Tr(hint);
    const float w = ImGui::GetContentRegionAvail().x;
    const float hgt = S(42);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::PushID(label);
    const bool pressed = ImGui::InvisibleButton("##nav", {w, hgt});
    ImGui::PopID();
    const bool hovered = ImGui::IsItemHovered() && !disabled;
    const float hv = Anim(ImGui::GetItemID(), hovered || selected, 16.0f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (selected) {
        dl->AddRectFilled(p, {p.x + w, p.y + hgt}, U32(accent, 0.16f), S(12));
        dl->AddRectFilled({p.x, p.y + S(10)}, {p.x + S(3.5f), p.y + hgt - S(10)}, U32(accent), S(2));
    } else if (hv > 0) {
        dl->AddRectFilled(p, {p.x + w, p.y + hgt}, U32(Color::Surface2, 0.8f * hv), S(12));
    }
    const unsigned fg = disabled ? Color::Dim : (selected ? Color::Text : Mix(Color::Muted, Color::Text, hv));
    if (icon != Icon::None) DrawIcon(dl, icon, {p.x + S(24), p.y + hgt * 0.5f}, S(17), U32(selected ? accent : fg));
    {
        FontScope f(selected ? GetFonts().semibold : GetFonts().medium, 15);
        const float th = ImGui::GetFontSize();
        dl->AddText({p.x + S(44), p.y + (hgt - th) * 0.5f - S(1)}, U32(fg), label);
        if (hint && *hint) {
            const float lw = ImGui::CalcTextSize(label).x;
            FontScope f2(GetFonts().regular, 12);
            dl->AddText({p.x + S(44) + lw + S(6), p.y + (hgt - ImGui::GetFontSize()) * 0.5f}, U32(Color::Dim), hint);
        }
    }
    if (badge && *badge) {
        FontScope f(GetFonts().semibold, 11.5f);
        const ImVec2 bs = ImGui::CalcTextSize(badge);
        const ImVec2 b1(p.x + w - S(12), p.y + hgt * 0.5f + bs.y * 0.5f + S(3));
        const ImVec2 b0(b1.x - bs.x - S(14), b1.y - bs.y - S(6));
        const bool soon = disabled;
        dl->AddRectFilled(b0, b1, U32(soon ? Color::Surface2 : accent, soon ? 1.0f : 0.22f), S(20));
        dl->AddText({b0.x + S(7), b0.y + S(2.5f)}, U32(soon ? Color::Dim : accent), badge);
    }
    return pressed && !disabled;
}

bool Toggle(const char* id, bool* value) {
    const float w = S(46), h = S(26);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton(id, {w, h});
    if (pressed) *value = !*value;
    const float t = Anim(ImGui::GetItemID(), *value, 10.0f);
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, {p.x + w, p.y + h}, U32(Mix(hovered ? Color::Dim : Color::Surface3, Color::Accent, t)), h * 0.5f);
    const float r = h * 0.5f - S(3);
    const ImVec2 knob(p.x + h * 0.5f + (w - h) * t, p.y + h * 0.5f);
    dl->AddCircleFilled({knob.x, knob.y + 1}, r, IM_COL32(0, 0, 0, 60));
    dl->AddCircleFilled(knob, r, U32(Mix(0xCFCBE0, 0xFFFFFF, t)));
    return pressed;
}

bool Segmented(const char* id, int* value, const std::vector<const char*>& items, float width) {
    ImGui::PushID(id);
    const float h = S(36);
    float total = width;
    // Nunca más ancho que el espacio que hay (columnas angostas con la interfaz agrandada).
    const float avail = ImGui::GetContentRegionAvail().x;
    if (total > 0 && avail > S(60)) total = std::min(total, avail);
    std::vector<float> ws;
    for (auto* it : items) ws.push_back(ImGui::CalcTextSize(it).x + S(26));
    if (total <= 0) {
        total = S(8);
        for (float w : ws) total += w;
    } else {
        for (auto& w : ws) w = (total - S(8)) / items.size();
    }
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, {p.x + total, p.y + h}, U32(Color::Surface2), S(12));
    bool changed = false;
    float x = p.x + S(4);
    for (size_t i = 0; i < items.size(); ++i) {
        ImGui::SetCursorScreenPos({x, p.y + S(4)});
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::InvisibleButton("##seg", {ws[i], h - S(8)})) {
            *value = static_cast<int>(i);
            changed = true;
        }
        const bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();
        const bool sel = *value == static_cast<int>(i);
        if (sel) dl->AddRectFilled({x, p.y + S(4)}, {x + ws[i], p.y + h - S(4)}, U32(Color::Surface3), S(9));
        else if (hovered) dl->AddRectFilled({x, p.y + S(4)}, {x + ws[i], p.y + h - S(4)}, U32(Color::Surface3, 0.45f), S(9));
        ImFont* font = sel ? GetFonts().semibold : GetFonts().medium;
        float fs = S(13.5f);
        ImVec2 ts = font->CalcTextSizeA(fs, FLT_MAX, 0, Tr(items[i]));
        // Si el texto no cabe en su casilla, se achica un poco (hasta 75 %).
        if (ts.x > ws[i] - S(10)) {
            fs *= std::max(0.75f, (ws[i] - S(10)) / ts.x);
            ts = font->CalcTextSizeA(fs, FLT_MAX, 0, Tr(items[i]));
        }
        dl->AddText(font, fs, {x + (ws[i] - ts.x) * 0.5f, p.y + (h - ts.y) * 0.5f}, U32(sel ? Color::Text : Color::Muted), Tr(items[i]));
        x += ws[i];
    }
    ImGui::SetCursorScreenPos(p);
    ImGui::Dummy({total, h});
    ImGui::PopID();
    return changed;
}

static bool SliderCore(const char* id, float* t01, float width, const char* text) {
    const float h = S(36);
    const float label_w = S(64);
    const float track_w = width - label_w;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(id, {track_w, h});
    const bool active = ImGui::IsItemActive();
    const bool hovered = ImGui::IsItemHovered();
    bool changed = false;
    const float x0 = p.x + S(10), x1 = p.x + track_w - S(10);
    if (active) {
        const float v = std::clamp((ImGui::GetIO().MousePos.x - x0) / (x1 - x0), 0.0f, 1.0f);
        if (v != *t01) {
            *t01 = v;
            changed = true;
        }
    }
    if (ImGui::IsItemFocused()) {
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft)) { *t01 = std::max(0.0f, *t01 - 0.05f); changed = true; }
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight)) { *t01 = std::min(1.0f, *t01 + 0.05f); changed = true; }
    }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float cy = p.y + h * 0.5f;
    const float th = S(6);
    dl->AddRectFilled({x0, cy - th * 0.5f}, {x1, cy + th * 0.5f}, U32(Color::Surface3), th);
    const float kx = x0 + (x1 - x0) * *t01;
    dl->AddRectFilled({x0, cy - th * 0.5f}, {kx, cy + th * 0.5f}, U32(Color::Accent), th);
    const float kr = S(active || hovered ? 10.0f : 9.0f);
    dl->AddCircleFilled({kx, cy + 1}, kr, IM_COL32(0, 0, 0, 70));
    dl->AddCircleFilled({kx, cy}, kr, U32(0xFFFFFF));
    dl->AddCircleFilled({kx, cy}, kr * 0.45f, U32(Color::Accent));
    FontScope f(GetFonts().semibold, 14);
    const ImVec2 ts = ImGui::CalcTextSize(text);
    dl->AddRectFilled({p.x + track_w + S(6), p.y + S(4)}, {p.x + width, p.y + h - S(4)}, U32(Color::Surface2), S(9));
    dl->AddText({p.x + track_w + S(6) + (label_w - S(6) - ts.x) * 0.5f, cy - ts.y * 0.5f}, U32(Color::Text), text);
    ImGui::SetCursorScreenPos(p);
    ImGui::Dummy({width, h});
    return changed;
}

bool SliderPercent(const char* id, float* value, float width) {
    char text[16];
    std::snprintf(text, sizeof text, "%d %%", static_cast<int>(std::lround(*value * 100)));
    float t = *value;
    if (SliderCore(id, &t, width, text)) {
        *value = std::round(t * 100.0f) / 100.0f;
        return true;
    }
    return false;
}

bool SliderIntStyled(const char* id, int* value, int min, int max, const char* fmt, float width) {
    char text[32];
    std::snprintf(text, sizeof text, fmt, *value);
    float t = max > min ? static_cast<float>(*value - min) / (max - min) : 0.0f;
    if (SliderCore(id, &t, width, text)) {
        const int v = min + static_cast<int>(std::lround(t * (max - min)));
        if (v != *value) {
            *value = v;
            return true;
        }
    }
    return false;
}

bool ComboStyled(const char* id, int* current, const std::vector<std::string>& items, float width) {
    ImGui::SetNextItemWidth(width);
    bool changed = false;
    std::string preview = *current >= 0 && *current < static_cast<int>(items.size()) ? Tr(items[*current]) : "";
    // El texto no pisa la flecha: si no cabe, se corta con «…».
    const float max_w = width - S(12) - S(40);
    if (ImGui::CalcTextSize(preview.c_str()).x > max_w) {
        while (!preview.empty() && ImGui::CalcTextSize((preview + "…").c_str()).x > max_w) {
            preview.pop_back();
            while (!preview.empty() && (static_cast<unsigned char>(preview.back()) & 0xC0) == 0x80) preview.pop_back();
        }
        preview += "…";
    }
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {S(12), S(8)});
    // Flecha propia (un chevron dibujado) en vez del triángulo de ImGui.
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float fh = ImGui::GetFrameHeight();
    ImDrawList* host = ImGui::GetWindowDrawList();
    {
        const ImVec2 c(p.x + width - S(20), p.y + fh * 0.5f);
        const float a = S(5);
        const bool hov = ImGui::IsMouseHoveringRect(p, {p.x + width, p.y + fh});
        const ImVec2 pts[3] = {{c.x - a, c.y - a * 0.5f}, {c.x, c.y + a * 0.5f}, {c.x + a, c.y - a * 0.5f}};
        // Se dibuja después del marco: se encola en un canal de arriba.
        host->ChannelsSplit(2);
        host->ChannelsSetCurrent(1);
        host->AddPolyline(pts, 3, U32(hov ? Color::Text : Color::Muted), S(2), 0);
        host->ChannelsSetCurrent(0);
    }
    const bool open = ImGui::BeginCombo(id, preview.c_str(), ImGuiComboFlags_HeightLarge | ImGuiComboFlags_NoArrowButton);
    host->ChannelsMerge();
    if (open) {
        for (int i = 0; i < static_cast<int>(items.size()); ++i) {
            const bool sel = i == *current;
            if (ImGui::Selectable(Tr(items[i]).c_str(), sel, 0, {0, S(26)})) {
                *current = i;
                changed = true;
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::PopStyleVar();
    return changed;
}

namespace {
float g_row_y = 0, g_row_text_bottom = 0, g_row_x = 0, g_row_w = 0;
}

void RowBegin(const char* title, const char* help) {
    title = Tr(title);
    help = Tr(help);
    g_row_w = ImGui::GetContentRegionAvail().x;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    g_row_x = p.x;
    g_row_y = p.y;
    // En pantallas angostas (celular en vertical) la fila se apila: el texto arriba y el control
    // debajo, a todo el ancho. Así nada queda con una letra por línea.
    const bool stacked = g_row_w < S(560);
    const float control_w = stacked ? g_row_w : std::min(S(340), g_row_w * 0.45f);
    const float text_w = stacked ? g_row_w : g_row_w - control_w - S(24);
    ImGui::PushTextWrapPos(p.x + text_w - ImGui::GetWindowPos().x + ImGui::GetScrollX());
    {
        FontScope f(GetFonts().semibold, 15);
        ImGui::TextUnformatted(title);
    }
    if (help && *help) {
        FontScope f(GetFonts().regular, 13.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, V4(Color::Muted));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - S(6));
        ImGui::TextWrapped("%s", help);
        ImGui::PopStyleColor();
    }
    ImGui::PopTextWrapPos();
    g_row_text_bottom = ImGui::GetCursorScreenPos().y;
    if (stacked) {
        ImGui::SetCursorScreenPos({p.x, g_row_text_bottom + S(6)});
        g_row_text_bottom = p.y; // la altura la da el control
    } else {
        ImGui::SetCursorScreenPos({p.x + g_row_w - control_w, p.y + S(2)});
    }
    ImGui::BeginGroup();
}

void RowEnd() {
    ImGui::EndGroup();
    const float bottom = std::max(g_row_text_bottom, ImGui::GetItemRectMax().y + S(10));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddLine({g_row_x, bottom + S(6)}, {g_row_x + g_row_w, bottom + S(6)}, U32(Color::Border, 0.7f));
    ImGui::SetCursorScreenPos({g_row_x, bottom + S(18)});
    ImGui::Dummy({0, 0});
}

void Chip(const char* text, unsigned color, bool filled) {
    text = Tr(text);
    FontScope f(GetFonts().semibold, 11.5f);
    const ImVec2 ts = ImGui::CalcTextSize(text);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const ImVec2 sz(ts.x + S(16), ts.y + S(6));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, {p.x + sz.x, p.y + sz.y}, U32(color, filled ? 1.0f : 0.18f), sz.y * 0.5f);
    dl->AddText({p.x + S(8), p.y + S(3)}, filled ? U32(0x14101F) : U32(color), text);
    ImGui::Dummy(sz);
}

void SectionLabel(const char* text) {
    text = Tr(text);
    FontScope f(GetFonts().semibold, 11.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, V4(Color::Dim));
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
}

void Title(const char* title, const char* subtitle) {
    title = Tr(title);
    subtitle = Tr(subtitle);
    {
        FontScope f(GetFonts().bold, 27);
        ImGui::TextUnformatted(title);
    }
    if (subtitle) {
        FontScope f(GetFonts().regular, 14.5f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - S(8));
        ImGui::PushStyleColor(ImGuiCol_Text, V4(Color::Muted));
        ImGui::TextUnformatted(subtitle);
        ImGui::PopStyleColor();
    }
}

void Spinner(float radius, unsigned color) {
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const ImVec2 c(p.x + radius, p.y + radius);
    ImGui::Dummy({radius * 2, radius * 2});
    const float t = static_cast<float>(ImGui::GetTime()) * 6.0f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PathArcTo(c, radius * 0.8f, t, t + kPi * 1.4f, 24);
    dl->PathStroke(U32(color), radius * 0.25f);
}

void HelpTooltip(const char* text) {
    text = Tr(text);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {S(12), S(8)});
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(S(320));
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
    ImGui::PopStyleVar();
}

bool PanelBegin(const char* id, ImVec2 size, unsigned bg, float rounding, ImVec2 padding) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, V4(bg));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, S(rounding));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {S(padding.x), S(padding.y)});
    const bool open = ImGui::BeginChild(id, size, ImGuiChildFlags_AlwaysUseWindowPadding | (size.y == 0 ? ImGuiChildFlags_AutoResizeY : 0));
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    return open;
}

void PanelEnd() { ImGui::EndChild(); }

float GuideBob(const char* seed) {
    // Cada guía se mueve un poquito, a su ritmo (sale de su nombre), para que se sientan vivos.
    unsigned h = 2166136261u;
    for (const char* c = seed; c && *c; ++c) h = (h ^ static_cast<unsigned char>(*c)) * 16777619u;
    const float t = static_cast<float>(ImGui::GetTime());
    const float phase = (h % 628) / 100.0f, speed = 1.6f + (h % 7) * 0.12f;
    // Saltito de vez en cuando (cada ~5 s) sobre un vaivén lento.
    const float hop = std::max(0.0f, std::sin(t * 1.25f + phase) - 0.92f) * 12.0f;
    return S(1.6f) * std::sin(t * speed + phase) - S(3.0f) * hop;
}

void GuideBanner(const Image& avatar, const char* name, const char* role, unsigned color, const char* text, float width) {
    role = Tr(role);
    text = Tr(text);
    if (width <= 0) width = ImGui::GetContentRegionAvail().x;
    const float av = S(58);
    const float pad = S(16);
    const float text_w = width - av - pad * 3;
    float text_h;
    {
        FontScope f(GetFonts().regular, 14.5f);
        text_h = ImGui::CalcTextSize(text, nullptr, false, text_w).y;
    }
    const float head_h = S(22);
    const float h = std::max(av + pad * 2, head_h + text_h + pad * 2 + S(4));
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, {p.x + width, p.y + h}, U32(Color::Surface), S(16));
    dl->AddRectFilled(p, {p.x + width, p.y + h}, U32(color, 0.06f), S(16));
    dl->AddRect(p, {p.x + width, p.y + h}, U32(color, 0.35f), S(16), 1.0f);
    const ImVec2 ac(p.x + pad + av * 0.5f, p.y + pad + av * 0.5f + GuideBob(name));
    if (avatar) {
        // Un brillo suave detrás, como si el guía respirara.
        const float glow = 0.5f + 0.5f * std::sin(static_cast<float>(ImGui::GetTime()) * 1.7f + name[0]);
        dl->AddCircleFilled(ac, av * 0.5f + S(3) + glow * S(2), U32(color, 0.10f + 0.08f * glow));
        dl->AddImageRounded(avatar.Ref(), {ac.x - av * 0.5f, ac.y - av * 0.5f}, {ac.x + av * 0.5f, ac.y + av * 0.5f}, {0, 0}, {1, 1}, IM_COL32_WHITE, av * 0.5f);
    }
    else dl->AddCircleFilled(ac, av * 0.5f, U32(color));
    const float tx = p.x + pad * 2 + av;
    {
        FontScope f(GetFonts().bold, 15);
        dl->AddText({tx, p.y + pad - S(2)}, U32(color), name);
        const float nw = ImGui::CalcTextSize(name).x;
        FontScope f2(GetFonts().medium, 12.5f);
        dl->AddText({tx + nw + S(10), p.y + pad + S(1)}, U32(Color::Dim), role);
    }
    {
        FontScope f(GetFonts().regular, 14.5f);
        dl->AddText(nullptr, 0.0f, {tx, p.y + pad + head_h}, U32(Color::Text), text, nullptr, text_w);
    }
    ImGui::Dummy({width, h});
}

std::string Initials(const std::string& title) {
    std::string out;
    bool start = true;
    for (unsigned char c : title) {
        if (std::isalnum(c)) {
            if (start && out.size() < 2) out += static_cast<char>(std::toupper(c));
            start = false;
        } else if (c == ' ' || c == '-' || c == ':' || c == '.') {
            start = true;
        }
    }
    if (out.empty()) out = "?";
    return out;
}

void DrawCartridge(ImDrawList* dl, ImVec2 p0, ImVec2 p1, unsigned hue, const std::string& title, float rounding, bool hovered, Layout layout,
                   const char* label) {
    const float w = p1.x - p0.x, h = p1.y - p0.y;
    const float hf = static_cast<float>(hue);
    // Fondo con brillo del color del juego.
    dl->AddRectFilled(p0, p1, Hsv(hf, 0.55f, 0.18f), rounding, ImDrawFlags_RoundTop);
    dl->PushClipRect(p0, p1, true);
    // Brillos suaves: varios círculos concéntricos que no llegan a las esquinas redondeadas.
    for (int i = 0; i < 6; ++i) {
        const float k = 1.0f - i * 0.15f;
        dl->AddCircleFilled({p0.x + w * 0.38f, p0.y + h * 0.36f}, w * 0.44f * k, Hsv(hf, 0.6f, 0.45f, 0.07f), 48);
        dl->AddCircleFilled({p0.x + w * 0.72f, p0.y + h * 0.80f}, w * 0.30f * k, Hsv(hf + 40, 0.6f, 0.45f, 0.06f), 48);
    }
    for (int i = 0; i < 9; ++i) {
        const float sx = std::fmod((hue * 7.3f + i * 53.1f), 100.0f) / 100.0f;
        const float sy = std::fmod((hue * 3.1f + i * 31.7f), 100.0f) / 100.0f;
        dl->AddCircleFilled({p0.x + sx * w, p0.y + sy * h}, 1.0f + (i % 3) * 0.6f, IM_COL32(255, 255, 255, 50 + (i % 3) * 30));
    }
    dl->PopClipRect();

    const float lift = hovered ? S(5) : 0;
    ImFont* bold = GetFonts().bold;
    ImFont* semi = GetFonts().semibold;
    const std::string ini = Initials(title);
    // Etiqueta común: degradado del color del juego, iniciales grandes y franja con el nombre Satronho.
    auto draw_label = [&](ImVec2 l0, ImVec2 l1, float band_k) {
        dl->AddRectFilledMultiColor(l0, l1, Hsv(hf, 0.62f, 0.95f), Hsv(hf + 25, 0.62f, 0.95f), Hsv(hf + 25, 0.72f, 0.62f), Hsv(hf, 0.72f, 0.62f));
        dl->AddRect(l0, l1, IM_COL32(0, 0, 0, 60), 0, 1.0f);
        const float lh = l1.y - l0.y, lw = l1.x - l0.x;
        const float fs = std::min(lh * 0.42f, lw * 0.36f);
        const ImVec2 ts = bold->CalcTextSizeA(fs, FLT_MAX, 0, ini.c_str());
        const ImVec2 tp(l0.x + (lw - ts.x) * 0.5f, l0.y + lh * 0.40f - ts.y * 0.5f);
        dl->AddText(bold, fs, {tp.x + 2, tp.y + 2}, IM_COL32(0, 0, 0, 90), ini.c_str());
        dl->AddText(bold, fs, tp, IM_COL32(255, 255, 255, 245), ini.c_str());
        const float band = lh * band_k;
        dl->AddRectFilled({l0.x, l1.y - band}, l1, IM_COL32(20, 16, 31, 200));
        float fs2 = band * 0.62f;
        ImVec2 ts2 = semi->CalcTextSizeA(fs2, FLT_MAX, 0, label);
        if (ts2.x > lw * 0.9f) {
            fs2 *= lw * 0.9f / ts2.x;
            ts2 = semi->CalcTextSizeA(fs2, FLT_MAX, 0, label);
        }
        dl->AddText(semi, fs2, {l0.x + (lw - ts2.x) * 0.5f, l1.y - band + (band - ts2.y) * 0.5f}, U32(Color::Gold), label);
    };

    if (layout == Layout::GameBoy) {
        // Cartucho de bolsillo: gris claro, esquina superior derecha cortada y estrías arriba.
        const float cw = w * 0.56f, ch = cw * 1.12f;
        const ImVec2 c0(p0.x + (w - cw) * 0.5f, p0.y + (h - ch) * 0.55f - lift), c1(c0.x + cw, c0.y + ch);
        const float cut = cw * 0.14f;
        ImVec2 shape[5] = {c0, {c1.x - cut, c0.y}, {c1.x, c0.y + cut}, c1, {c0.x, c1.y}};
        ImVec2 shadow[5];
        for (int i = 0; i < 5; ++i) shadow[i] = {shape[i].x + 3, shape[i].y + 6};
        dl->AddConvexPolyFilled(shadow, 5, IM_COL32(0, 0, 0, 70));
        dl->AddConvexPolyFilled(shape, 5, IM_COL32(168, 166, 172, 255));
        for (int i = 0; i < 4; ++i) {
            const float y = c0.y + ch * (0.05f + i * 0.03f);
            dl->AddLine({c0.x + cw * 0.08f, y}, {c1.x - cut - cw * 0.02f, y}, IM_COL32(130, 128, 136, 255), std::max(1.0f, ch * 0.012f));
        }
        draw_label({c0.x + cw * 0.1f, c0.y + ch * 0.2f}, {c1.x - cw * 0.1f, c0.y + ch * 0.86f}, 0.2f);
    } else if (layout == Layout::GameBoyAdvance) {
        // Cartucho ancho y oscuro, con la pestaña arriba.
        const float cw = w * 0.74f, ch = cw * 0.66f;
        const ImVec2 c0(p0.x + (w - cw) * 0.5f, p0.y + (h - ch) * 0.55f - lift), c1(c0.x + cw, c0.y + ch);
        dl->AddRectFilled({c0.x + 3, c0.y + 6}, {c1.x + 3, c1.y + 6}, IM_COL32(0, 0, 0, 70), cw * 0.05f);
        dl->AddRectFilled({c0.x + cw * 0.32f, c0.y - ch * 0.06f}, {c1.x - cw * 0.32f, c0.y + ch * 0.05f}, IM_COL32(70, 68, 78, 255), cw * 0.03f);
        dl->AddRectFilled(c0, c1, IM_COL32(86, 84, 96, 255), cw * 0.05f);
        draw_label({c0.x + cw * 0.08f, c0.y + ch * 0.12f}, {c1.x - cw * 0.08f, c0.y + ch * 0.80f}, 0.22f);
        dl->AddRectFilled({c0.x + cw * 0.3f, c1.y - ch * 0.12f}, {c1.x - cw * 0.3f, c1.y - ch * 0.05f}, IM_COL32(60, 58, 68, 255), 2);
    } else if (layout == Layout::Genesis) {
        // Cartucho negro, casi cuadrado, con agarre de estrías a los lados y etiqueta grande.
        const float cw = w * 0.64f, ch = std::min(h * 0.78f, cw * 1.08f);
        const ImVec2 c0(p0.x + (w - cw) * 0.5f, p0.y + (h - ch) * 0.55f - lift), c1(c0.x + cw, c0.y + ch);
        dl->AddRectFilled({c0.x + 3, c0.y + 6}, {c1.x + 3, c1.y + 6}, IM_COL32(0, 0, 0, 80), cw * 0.04f);
        dl->AddRectFilled(c0, c1, IM_COL32(34, 33, 38, 255), cw * 0.04f);
        dl->AddRectFilled(c0, {c1.x, c0.y + ch * 0.07f}, IM_COL32(48, 47, 54, 255), cw * 0.04f, ImDrawFlags_RoundTop);
        for (int i = 0; i < 6; ++i) {
            const float y = c0.y + ch * (0.18f + i * 0.1f);
            const float t = std::max(1.0f, ch * 0.014f);
            dl->AddLine({c0.x + cw * 0.025f, y}, {c0.x + cw * 0.07f, y}, IM_COL32(70, 68, 78, 255), t);
            dl->AddLine({c1.x - cw * 0.07f, y}, {c1.x - cw * 0.025f, y}, IM_COL32(70, 68, 78, 255), t);
        }
        draw_label({c0.x + cw * 0.11f, c0.y + ch * 0.12f}, {c1.x - cw * 0.11f, c0.y + ch * 0.86f}, 0.18f);
    } else if (layout == Layout::PlayStation || layout == Layout::Saturn || layout == Layout::Dreamcast || layout == Layout::Psp) {
        // Caja de CD: el disco asoma por arriba a la derecha y la portada va al frente.
        const float cw = w * 0.62f, ch = std::min(h * 0.74f, cw * 0.95f);
        const ImVec2 c0(p0.x + (w - cw) * 0.5f - w * 0.04f, p0.y + (h - ch) * 0.6f - lift), c1(c0.x + cw, c0.y + ch);
        const ImVec2 dc(c1.x - cw * 0.08f, c0.y + ch * 0.12f);
        const float dr = cw * 0.42f;
        dl->AddCircleFilled({dc.x + 3, dc.y + 5}, dr, IM_COL32(0, 0, 0, 60), 48);
        dl->AddCircleFilled(dc, dr, layout == Layout::Saturn ? IM_COL32(214, 206, 230, 255) : layout == Layout::Dreamcast ? IM_COL32(236, 220, 200, 255) : IM_COL32(196, 200, 214, 255), 48);
        dl->AddCircle(dc, dr * 0.72f, IM_COL32(170, 176, 196, 255), 48, std::max(1.0f, dr * 0.04f));
        dl->AddCircleFilled(dc, dr * 0.2f, IM_COL32(150, 156, 176, 255), 32);
        dl->AddCircleFilled(dc, dr * 0.08f, IM_COL32(30, 28, 40, 255), 16);
        dl->AddRectFilled({c0.x + 3, c0.y + 6}, {c1.x + 3, c1.y + 6}, IM_COL32(0, 0, 0, 80), cw * 0.03f);
        dl->AddRectFilled(c0, c1, IM_COL32(40, 40, 50, 255), cw * 0.03f);
        dl->AddRectFilled(c0, {c0.x + cw * 0.07f, c1.y}, IM_COL32(24, 24, 32, 255), cw * 0.03f, ImDrawFlags_RoundLeft);
        draw_label({c0.x + cw * 0.1f, c0.y + ch * 0.05f}, {c1.x - cw * 0.04f, c1.y - ch * 0.05f}, 0.16f);
        dl->AddRect(c0, c1, IM_COL32(255, 255, 255, 40), cw * 0.03f, 1.0f);
    } else if (layout == Layout::N64) {
        // Cartucho gris ancho, con el borde de arriba redondeado y estrías a los lados.
        const float cw = w * 0.70f, ch = cw * 0.80f;
        const ImVec2 c0(p0.x + (w - cw) * 0.5f, p0.y + (h - ch) * 0.55f - lift), c1(c0.x + cw, c0.y + ch);
        dl->AddRectFilled({c0.x + 3, c0.y + 6}, {c1.x + 3, c1.y + 6}, IM_COL32(0, 0, 0, 70), cw * 0.1f);
        dl->AddRectFilled(c0, c1, IM_COL32(120, 122, 132, 255), cw * 0.12f, ImDrawFlags_RoundTop);
        for (int i = 0; i < 5; ++i) {
            const float y = c0.y + ch * (0.2f + i * 0.12f);
            dl->AddLine({c0.x + cw * 0.03f, y}, {c0.x + cw * 0.08f, y}, IM_COL32(96, 98, 108, 255), std::max(1.0f, ch * 0.016f));
            dl->AddLine({c1.x - cw * 0.08f, y}, {c1.x - cw * 0.03f, y}, IM_COL32(96, 98, 108, 255), std::max(1.0f, ch * 0.016f));
        }
        draw_label({c0.x + cw * 0.12f, c0.y + ch * 0.1f}, {c1.x - cw * 0.12f, c0.y + ch * 0.82f}, 0.2f);
    } else if (layout == Layout::DS) {
        // Tarjeta chiquita con una muesca arriba a la derecha.
        const float cw = w * 0.5f, ch = cw * 1.1f;
        const ImVec2 c0(p0.x + (w - cw) * 0.5f, p0.y + (h - ch) * 0.55f - lift), c1(c0.x + cw, c0.y + ch);
        const float cut = cw * 0.12f;
        ImVec2 shape[5] = {c0, {c1.x - cut, c0.y}, {c1.x, c0.y + cut}, c1, {c0.x, c1.y}};
        ImVec2 shadow[5];
        for (int i = 0; i < 5; ++i) shadow[i] = {shape[i].x + 3, shape[i].y + 6};
        dl->AddConvexPolyFilled(shadow, 5, IM_COL32(0, 0, 0, 70));
        dl->AddConvexPolyFilled(shape, 5, IM_COL32(54, 56, 66, 255));
        draw_label({c0.x + cw * 0.1f, c0.y + ch * 0.12f}, {c1.x - cw * 0.1f, c0.y + ch * 0.84f}, 0.2f);
    } else if (layout == Layout::Snes) {
        // Cartucho gris claro y ancho, con los hombros redondeados arriba y estrías abajo.
        const float cw = w * 0.74f, ch = cw * 0.72f;
        const ImVec2 c0(p0.x + (w - cw) * 0.5f, p0.y + (h - ch) * 0.55f - lift), c1(c0.x + cw, c0.y + ch);
        dl->AddRectFilled({c0.x + 3, c0.y + 6}, {c1.x + 3, c1.y + 6}, IM_COL32(0, 0, 0, 70), cw * 0.08f);
        dl->AddRectFilled(c0, c1, IM_COL32(176, 175, 184, 255), cw * 0.08f, ImDrawFlags_RoundTop);
        // Las dos muescas de arriba.
        dl->AddRectFilled({c0.x + cw * 0.12f, c0.y}, {c0.x + cw * 0.2f, c0.y + ch * 0.07f}, IM_COL32(150, 149, 158, 255), 2);
        dl->AddRectFilled({c1.x - cw * 0.2f, c0.y}, {c1.x - cw * 0.12f, c0.y + ch * 0.07f}, IM_COL32(150, 149, 158, 255), 2);
        for (int i = 0; i < 3; ++i) {
            const float y = c1.y - ch * (0.06f + i * 0.045f);
            dl->AddLine({c0.x + cw * 0.06f, y}, {c1.x - cw * 0.06f, y}, IM_COL32(140, 139, 150, 255), std::max(1.0f, ch * 0.014f));
        }
        draw_label({c0.x + cw * 0.14f, c0.y + ch * 0.12f}, {c1.x - cw * 0.14f, c0.y + ch * 0.74f}, 0.22f);
    } else {
        // Cartucho de NES.
        const float cw = w * 0.60f, ch = h * 0.80f;
        const ImVec2 c0(p0.x + (w - cw) * 0.5f, p0.y + (h - ch) * 0.58f - lift);
        const ImVec2 c1(c0.x + cw, c0.y + ch);
        dl->AddRectFilled({c0.x + 3, c0.y + 6}, {c1.x + 3, c1.y + 6}, IM_COL32(0, 0, 0, 70), cw * 0.05f);
        dl->AddRectFilled(c0, c1, IM_COL32(150, 147, 158, 255), cw * 0.05f);
        dl->AddRectFilled(c0, {c1.x, c0.y + ch * 0.06f}, IM_COL32(172, 169, 180, 255), cw * 0.05f, ImDrawFlags_RoundTop);
        for (int i = 0; i < 5; ++i) {
            const float y = c0.y + ch * (0.80f + i * 0.035f);
            dl->AddLine({c0.x + cw * 0.12f, y}, {c1.x - cw * 0.12f, y}, IM_COL32(110, 107, 118, 255), std::max(1.0f, ch * 0.012f));
        }
        draw_label({c0.x + cw * 0.09f, c0.y + ch * 0.11f}, {c1.x - cw * 0.09f, c0.y + ch * 0.72f}, 0.2f);
    }
}

void Ellipsize(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, ImU32 color, const std::string& text, float max_w, int max_lines) {
    std::vector<std::string> lines;
    std::string cur;
    size_t i = 0;
    auto width = [&](const std::string& s) { return font->CalcTextSizeA(size, FLT_MAX, 0, s.c_str()).x; };
    while (i < text.size()) {
        size_t sp = text.find(' ', i);
        std::string word = text.substr(i, sp == std::string::npos ? std::string::npos : sp - i);
        std::string trial = cur.empty() ? word : cur + " " + word;
        if (width(trial) <= max_w || cur.empty()) {
            cur = trial;
        } else {
            lines.push_back(cur);
            cur = word;
        }
        if (sp == std::string::npos) break;
        i = sp + 1;
    }
    if (!cur.empty()) lines.push_back(cur);
    const bool overflow = static_cast<int>(lines.size()) > max_lines;
    if (overflow) lines.resize(max_lines);
    for (size_t li = 0; li < lines.size(); ++li) {
        std::string s = lines[li];
        const bool last = li + 1 == lines.size();
        if ((last && overflow) || width(s) > max_w) {
            while (!s.empty() && width(s + "…") > max_w) {
                s.pop_back();
                while (!s.empty() && (static_cast<unsigned char>(s.back()) & 0xC0) == 0x80) s.pop_back();
            }
            s += "…";
        }
        dl->AddText(font, size, {pos.x, pos.y + li * size * 1.25f}, color, s.c_str());
    }
}

KeepContentSize::KeepContentSize() {
    ImGuiWindow* w = ImGui::GetCurrentWindow();
    max_x_ = w->DC.CursorMaxPos.x;
    max_y_ = w->DC.CursorMaxPos.y;
    ideal_x_ = w->DC.IdealMaxPos.x;
    ideal_y_ = w->DC.IdealMaxPos.y;
}

KeepContentSize::~KeepContentSize() {
    ImGuiWindow* w = ImGui::GetCurrentWindow();
    w->DC.CursorMaxPos = {max_x_, max_y_};
    w->DC.IdealMaxPos = {ideal_x_, ideal_y_};
}

void BackToTop() {
    ImGuiWindow* w = ImGui::GetCurrentWindow();
    if (w->Scroll.y < w->Size.y * 0.8f) return;
    const float d = S(46);
    // El botón flota sobre la página, en la esquina de abajo a la derecha de lo que se ve. Es un
    // botón de verdad (así gana el clic a la tarjeta que tenga debajo), pero no cuenta como
    // contenido: se guarda hasta dónde llegaba la página y se devuelve igual. Si contara, la página
    // crecería con cada desplazamiento y se podría mover de lado y hacia abajo sin fin (pasaba
    // hasta la build 19).
    const ImVec2 keep = w->DC.CursorPos, keep_max = w->DC.CursorMaxPos, keep_ideal = w->DC.IdealMaxPos;
    const ImVec2 p(w->InnerRect.Max.x - d - S(18), w->InnerRect.Max.y - d - S(18));
    ImGui::SetCursorScreenPos(p);
    const bool click = ImGui::InvisibleButton("##arriba", {d, d});
    const bool hov = ImGui::IsItemHovered();
    w->DC.CursorPos = keep;
    w->DC.CursorMaxPos = keep_max;
    w->DC.IdealMaxPos = keep_ideal;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 c = {p.x + d * 0.5f, p.y + d * 0.5f};
    dl->AddCircleFilled({c.x, c.y + S(2)}, d * 0.5f, IM_COL32(0, 0, 0, 90));
    dl->AddCircleFilled(c, d * 0.5f, U32(Color::Accent, hov ? 1.0f : 0.9f));
    const float a = S(7);
    dl->AddTriangleFilled({c.x - a, c.y + a * 0.45f}, {c.x + a, c.y + a * 0.45f}, {c.x, c.y - a * 0.65f}, U32(0x15121F));
    if (hov) ImGui::SetTooltip("%s", Tr("Volver arriba"));
    if (click) ImGui::SetScrollY(0);
}

} // namespace Satronho::Ui
