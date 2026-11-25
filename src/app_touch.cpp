// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Pantallas táctiles (Android): control en pantalla con el estilo de cada consola, desplazar con el
// dedo, modo compacto para pantallas chicas y el explorador de carpetas para elegir los juegos.
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <functional>

#include "app.h"
#include "util/i18n.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "libretro.h"
#include "sys/android.h"
#include "ui/controllers.h"
#include "util/archive.h"
#include "util/log.h"

namespace Satronho {

using namespace Ui;

namespace {
void Muted(const char* text, float size) {
    text = Tr(text);
    FontScope f(GetFonts().regular, size);
    ImGui::PushStyleColor(ImGuiCol_Text, V4(Color::Muted));
    ImGui::TextWrapped("%s", Tr(text));
    ImGui::PopStyleColor();
}
std::string Lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
constexpr float kPi = 3.14159265f;

const char* ButtonName(const SystemDef& sys, int id) {
    for (const auto& [b, name] : sys.buttons)
        if (b == id) return name;
    return "";
}

// ---------------------------------------------------------------- dibujo del control en pantalla

// Color 0xRRGGBB aclarado (k > 1) u oscurecido (k < 1), con transparencia.
ImU32 Shade(unsigned rgb, float k, float alpha) {
    auto ch = [&](int shift) {
        const float v = static_cast<float>((rgb >> shift) & 0xFF);
        const float out = k >= 1.0f ? v + (255.0f - v) * (k - 1.0f) : v * k;
        return static_cast<int>(std::clamp(out, 0.0f, 255.0f));
    };
    return IM_COL32(ch(16), ch(8), ch(0), static_cast<int>(std::clamp(alpha, 0.0f, 1.0f) * 255));
}

// Lo que va dibujado en cada botón de acción según la consola: su color y su letra o figura.
struct FaceLook {
    unsigned base = 0x2A2B35;  // color del botón
    unsigned glyph = 0xF4F2FA; // color de la letra o figura
    enum Kind { Letter, Triangle, Circle, Cross, Square, Arrow } kind = Letter;
    float dx = 0, dy = 0;      // flecha de los botones C (64)
};

FaceLook LookFor(const std::string& sys, const std::string& name) {
    FaceLook l;
    // PlayStation y PSP: botones grafito con las cuatro figuras de colores.
    if (sys == "cd" || sys == "portatil") {
        l.base = 0x2B2C36;
        if (name == "Triángulo") { l.kind = FaceLook::Triangle; l.glyph = 0x3FD9A2; }
        else if (name == "Círculo") { l.kind = FaceLook::Circle; l.glyph = 0xFF5D70; }
        else if (name == "Cruz") { l.kind = FaceLook::Cross; l.glyph = 0x79A6FF; }
        else if (name == "Cuadrado") { l.kind = FaceLook::Square; l.glyph = 0xF78AD2; }
        return l;
    }
    auto color4 = [&](unsigned a, unsigned b, unsigned x, unsigned y) {
        if (name == "A") l.base = a;
        else if (name == "B") l.base = b;
        else if (name == "X") l.base = x;
        else if (name == "Y") l.base = y;
    };
    if (sys == "hexa") color4(0xD3313B, 0xF0BE2C, 0x3B52C4, 0x2C9A50);       // Super: rojo, amarillo, azul, verde
    else if (sys == "sueno") color4(0xDC3838, 0x3A6EDC, 0xF0BE2C, 0x33B055); // Dreamcast: rojo, azul, amarillo, verde
    else if (sys == "octa") l.base = 0xC8212B;                              // NES: botones rojos
    else if (sys == "bolsillo") l.base = 0x9A1E58;                          // Game Boy: magenta
    else if (sys == "bolsillo_plus") l.base = 0x4C4F6E;                     // GBA: pizarra
    else if (sys == "dual") { l.base = 0xD8D9E0; l.glyph = 0x2B2B33; }      // DS: grises claros
    else if (sys == "mega") { l.base = 0x232329; l.glyph = 0xD9D9E2; }      // Genesis: negros
    else if (sys == "anillo") { l.base = 0x34343F; l.glyph = 0xE6E6EF; }    // Saturn
    else if (sys == "n64") {
        if (name == "A") l.base = 0x2D53D4;
        else if (name == "B") l.base = 0x2CA14F;
        else if (name.rfind("C ", 0) == 0) {
            l.base = 0xF0BE2C;
            l.glyph = 0x4A3500;
            l.kind = FaceLook::Arrow;
            if (name == "C arriba") l.dy = -1;
            else if (name == "C abajo") l.dy = 1;
            else if (name == "C izquierda") l.dx = -1;
            else l.dx = 1;
        }
    }
    // Sobre amarillo, la letra va oscura para que se lea.
    if (l.base == 0xF0BE2C && l.kind == FaceLook::Letter) l.glyph = 0x4A3500;
    return l;
}

// Botón redondo con relieve: sombra, cuerpo, brillo arriba y borde. Al apretarlo se hunde y brilla.
void DrawDisc(ImDrawList* dl, ImVec2 c, float r, unsigned base, float a, bool on, unsigned glow) {
    const float body = on ? 1.0f : a;
    if (!on) dl->AddCircleFilled({c.x, c.y + r * 0.10f}, r * 1.02f, IM_COL32(0, 0, 0, static_cast<int>(90 * a)));
    dl->AddCircleFilled(c, r, Shade(base, on ? 0.95f : 0.62f, 0.94f * body));
    dl->AddCircleFilled({c.x, c.y - r * 0.06f}, r * 0.90f, Shade(base, on ? 1.35f : 1.0f, 0.94f * body));
    dl->AddEllipseFilled({c.x, c.y - r * 0.50f}, {r * 0.46f, r * 0.20f}, IM_COL32(255, 255, 255, static_cast<int>(20 * body)));
    dl->AddCircle(c, r, IM_COL32(255, 255, 255, static_cast<int>(46 * body)), 0, std::max(1.0f, r * 0.035f));
    if (on) dl->AddCircle(c, r + r * 0.12f, Shade(glow, 1.2f, 0.75f), 0, std::max(2.0f, r * 0.09f));
}

// Píldora (gatillos, Select, Start) con el mismo relieve.
void DrawPill(ImDrawList* dl, ImVec2 c, float hw, float hh, unsigned base, float a, bool on, unsigned glow) {
    const float body = on ? 1.0f : a;
    const ImVec2 p0(c.x - hw, c.y - hh), p1(c.x + hw, c.y + hh);
    if (!on) dl->AddRectFilled({p0.x, p0.y + hh * 0.22f}, {p1.x, p1.y + hh * 0.22f}, IM_COL32(0, 0, 0, static_cast<int>(90 * a)), hh);
    dl->AddRectFilled(p0, p1, Shade(base, on ? 0.95f : 0.62f, 0.94f * body), hh);
    dl->AddRectFilled({p0.x + hh * 0.12f, p0.y}, {p1.x - hh * 0.12f, p1.y - hh * 0.22f}, Shade(base, on ? 1.35f : 1.0f, 0.94f * body), hh * 0.9f);
    dl->AddRectFilled({p0.x + hh * 0.7f, p0.y + hh * 0.18f}, {p1.x - hh * 0.7f, p0.y + hh * 0.62f}, IM_COL32(255, 255, 255, static_cast<int>(16 * body)), hh * 0.3f);
    dl->AddRect(p0, p1, on ? Shade(glow, 1.2f, 0.85f) : IM_COL32(255, 255, 255, static_cast<int>(46 * body)), hh, std::max(1.0f, hh * (on ? 0.12f : 0.06f)));
}

void DrawLabel(ImDrawList* dl, ImVec2 c, float size, float maxw, ImU32 col, ImU32 shadow, const char* text) {
    ImFont* f = GetFonts().bold;
    ImVec2 ts = f->CalcTextSizeA(size, FLT_MAX, 0, text);
    if (ts.x > maxw) {
        size *= maxw / ts.x;
        ts = f->CalcTextSizeA(size, FLT_MAX, 0, text);
    }
    const ImVec2 p(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f);
    dl->AddText(f, size, {p.x, p.y + size * 0.06f}, shadow, text);
    dl->AddText(f, size, p, col, text);
}

// La figura o letra de un botón de acción.
void DrawGlyph(ImDrawList* dl, ImVec2 c, float r, const FaceLook& l, const char* label, float a, bool on) {
    const float vis = on ? 1.0f : std::max(a, 0.6f);
    const ImU32 col = Shade(l.glyph, on ? 1.25f : 1.0f, vis);
    const ImU32 shadow = IM_COL32(0, 0, 0, static_cast<int>(70 * vis));
    const float t = r * 0.15f; // grosor de las figuras
    const float q = r * 0.42f;
    switch (l.kind) {
    case FaceLook::Triangle: {
        const ImVec2 p1(c.x, c.y - q * 1.02f), p2(c.x + q * 1.0f, c.y + q * 0.72f), p3(c.x - q * 1.0f, c.y + q * 0.72f);
        dl->AddTriangle({p1.x, p1.y + t * 0.4f}, {p2.x, p2.y + t * 0.4f}, {p3.x, p3.y + t * 0.4f}, shadow, t);
        dl->AddTriangle(p1, p2, p3, col, t);
        break;
    }
    case FaceLook::Circle:
        dl->AddCircle({c.x, c.y + t * 0.4f}, q * 0.86f, shadow, 0, t);
        dl->AddCircle(c, q * 0.86f, col, 0, t);
        break;
    case FaceLook::Cross:
        for (int s = 0; s < 2; ++s) {
            const ImU32 cc = s == 0 ? shadow : col;
            const float oy = s == 0 ? t * 0.4f : 0;
            dl->AddLine({c.x - q * 0.78f, c.y - q * 0.78f + oy}, {c.x + q * 0.78f, c.y + q * 0.78f + oy}, cc, t);
            dl->AddLine({c.x + q * 0.78f, c.y - q * 0.78f + oy}, {c.x - q * 0.78f, c.y + q * 0.78f + oy}, cc, t);
        }
        break;
    case FaceLook::Square:
        dl->AddRect({c.x - q * 0.74f, c.y - q * 0.74f + t * 0.4f}, {c.x + q * 0.74f, c.y + q * 0.74f + t * 0.4f}, shadow, 0, t);
        dl->AddRect({c.x - q * 0.74f, c.y - q * 0.74f}, {c.x + q * 0.74f, c.y + q * 0.74f}, col, 0, t);
        break;
    case FaceLook::Arrow: {
        // Triángulo relleno que apunta hacia donde mira el botón C.
        const float dx = l.dx, dy = l.dy, k = q * 0.95f;
        const ImVec2 tip(c.x + dx * k, c.y + dy * k);
        const ImVec2 b1(c.x - dx * k * 0.55f - dy * k * 0.85f, c.y - dy * k * 0.55f + dx * k * 0.85f);
        const ImVec2 b2(c.x - dx * k * 0.55f + dy * k * 0.85f, c.y - dy * k * 0.55f - dx * k * 0.85f);
        dl->AddTriangleFilled(tip, b1, b2, col);
        break;
    }
    default:
        DrawLabel(dl, c, std::min(r * 0.95f, S(30)), r * 1.5f, col, shadow, Tr(label));
        break;
    }
}
} // namespace

// ---------------------------------------------------------------- modo táctil y compacto

void App::UpdateTouchMode() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float k = std::max(0.5f, Ui::Scale());
    compact_ = vp->Size.x / k < 1040.0f || vp->Size.y / k < 600.0f;
#ifdef __ANDROID__
    touch_mode_ = true;
#endif
    // Doble toque: con el dedo los dos toques nunca caen en el mismo punto. ImGui pide que estén a
    // menos de 6 píxeles (en un celular eso es como 1 mm); con el dedo se aceptan ~6 mm y un poco
    // más de tiempo.
    ImGuiIO& io = ImGui::GetIO();
    io.MouseDoubleClickMaxDist = touch_mode_ ? S(36) : 6.0f;
    io.MouseDoubleClickTime = touch_mode_ ? 0.40f : 0.30f;
}

void App::TouchEvent(const SDL_Event& e) {
    if (e.type != SDL_EVENT_FINGER_DOWN && e.type != SDL_EVENT_FINGER_MOTION && e.type != SDL_EVENT_FINGER_UP && e.type != SDL_EVENT_FINGER_CANCELED)
        return;
    touch_mode_ = true;
    int w, h;
    SDL_GetWindowSizeInPixels(window_, &w, &h);
    const SDL_FingerID id = e.tfinger.fingerID;
    if (e.type == SDL_EVENT_FINGER_UP || e.type == SDL_EVENT_FINGER_CANCELED) {
        fingers_.erase(id);
        return;
    }
    Finger& f = fingers_[id];
    f.x = e.tfinger.x * w;
    f.y = e.tfinger.y * h;
    if (e.type == SDL_EVENT_FINGER_DOWN) {
        f.sx = f.x;
        f.sy = f.y;
        f.owner = -1;
        f.born = SDL_GetTicks() / 1000.0;
    }
}

// Desplazar listas con el dedo (con inercia), como en el celular. Si el dedo se movió, el toque
// no cuenta como clic.
void App::TouchScroll() {
    if (!touch_mode_) return;
    ImGuiContext& g = *GImGui;
    ImGuiIO& io = ImGui::GetIO();
    // La ventana que se puede desplazar en ese sentido (la del dedo o la que la contiene).
    auto scrollable = [](ImGuiWindow* w, int axis) {
        while (w && !(w->Flags & ImGuiWindowFlags_Popup) && (axis == 0 ? w->ScrollMax.x : w->ScrollMax.y) <= 0.0f) w = w->ParentWindow;
        return w && (axis == 0 ? w->ScrollMax.x : w->ScrollMax.y) > 0.0f ? w : nullptr;
    };
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        drag_start_win_ = g.HoveredWindow;
        drag_win_ = nullptr;
        drag_dx_ = drag_dy_ = 0;
        dragging_ = false;
        drag_vel_ = 0;
    }
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && drag_start_win_) {
        const ImVec2 d = io.MouseDelta;
        if (!dragging_) {
            drag_dx_ += d.x;
            drag_dy_ += d.y;
            if (std::fabs(drag_dx_) + std::fabs(drag_dy_) > S(12)) {
                // El sentido lo decide el primer movimiento: de lado o de arriba a abajo.
                drag_axis_ = std::fabs(drag_dx_) > std::fabs(drag_dy_) * 1.2f ? 0 : 1;
                // De lado sobre el carrusel: lo mueve el carrusel, no la página.
                const ImVec2 c = io.MouseClickedPos[0];
                const bool on_carousel = c.x >= carousel_rect_.x && c.x <= carousel_rect_.z && c.y >= carousel_rect_.y && c.y <= carousel_rect_.w;
                if (drag_axis_ == 0 && on_carousel) {
                    drag_start_win_ = nullptr;
                    return;
                }
                ImGuiWindow* w = scrollable(static_cast<ImGuiWindow*>(drag_start_win_), drag_axis_);
                if (!w && drag_axis_ == 0) { // nada que mover de lado: se mueve la página
                    drag_axis_ = 1;
                    w = scrollable(static_cast<ImGuiWindow*>(drag_start_win_), 1);
                }
                drag_win_ = w;
                dragging_ = true;
                ImGui::ClearActiveID(); // ya no es un toque: es desplazar
            }
        }
        auto* win = static_cast<ImGuiWindow*>(drag_win_);
        if (dragging_ && win) {
            const float delta = drag_axis_ == 0 ? d.x : d.y;
            if (drag_axis_ == 0) ImGui::SetScrollX(win, std::clamp(win->Scroll.x - delta, 0.0f, win->ScrollMax.x));
            else ImGui::SetScrollY(win, std::clamp(win->Scroll.y - delta, 0.0f, win->ScrollMax.y));
            if (io.DeltaTime > 0) drag_vel_ = drag_vel_ * 0.6f + (-delta / io.DeltaTime) * 0.4f;
        }
    } else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        auto* win = static_cast<ImGuiWindow*>(drag_win_);
        // Inercia al soltar.
        if (win && dragging_ && std::fabs(drag_vel_) > 20.0f) {
            if (drag_axis_ == 0) ImGui::SetScrollX(win, std::clamp(win->Scroll.x + drag_vel_ * io.DeltaTime, 0.0f, win->ScrollMax.x));
            else ImGui::SetScrollY(win, std::clamp(win->Scroll.y + drag_vel_ * io.DeltaTime, 0.0f, win->ScrollMax.y));
            drag_vel_ *= std::pow(0.05f, io.DeltaTime);
        } else {
            drag_win_ = nullptr;
            drag_start_win_ = nullptr;
            dragging_ = false;
        }
    }
}

// ---------------------------------------------------------------- control en pantalla

namespace {
struct Pad {
    ImVec2 c;      // centro
    float r;       // radio
    int id;        // botón del RetroPad (-1 menú, -2 cruceta, -3 stick)
    const char* label;
    const char* group;
};
} // namespace

bool App::TouchPadVisible() const {
    if (!in_game_ || menu_open_ || confirm_ != 0) return false;
    if (settings_.touch_show == 2) return false;
    if (settings_.touch_show == 1) return true;
    // Automático: con pantalla táctil y sin un mando conectado.
    return touch_mode_ && input_.Gamepads().empty();
}

std::vector<App::TouchButton> App::TouchLayout(float W, float H) const {
    std::vector<TouchButton> out;
    const SystemDef& sys = *sys_;
    const float k = std::clamp(settings_.touch_scale, 0.6f, 1.6f);
    const float u = H * 0.115f * k; // unidad: radio de un botón grande
    auto off = [&](const char* group) {
        auto it = settings_.touch_offsets.find(sys.id + "." + group);
        return it == settings_.touch_offsets.end() ? ImVec2(0, 0) : ImVec2(it->second.first * W, it->second.second * H);
    };
    // Izquierda: cruceta o stick (64).
    {
        const ImVec2 o = off("izq");
        const ImVec2 c(W * 0.035f + u * 1.55f + o.x, H * 0.62f + o.y);
        if (sys.id == "n64") out.push_back({c, u * 1.35f, -3, "", "izq"});
        else out.push_back({c, u * 1.45f, -2, "", "izq"});
        // En CD, el stick izquierdo va abajo a la derecha de la cruceta (dentro de la columna, sin
        // tapar el juego).
        if (sys.analog && sys.id != "n64") out.push_back({{c.x + u * 1.15f, c.y + u * 2.05f}, u * 0.75f, -3, "", "izq"});
    }
    // Derecha: los botones de acción en las mismas filas que en Controles.
    {
        const ImVec2 o = off("der");
        const auto& rows = sys.face;
        const int nrows = static_cast<int>(rows.size());
        const float br = nrows >= 4 ? u * 0.62f : u * 0.78f;
        const float step_y = br * (nrows >= 4 ? 1.55f : 1.85f);
        const ImVec2 base(W - W * 0.035f - u * 1.7f + o.x, H * 0.62f + o.y - step_y * (nrows - 1) * 0.5f);
        for (int r = 0; r < nrows; ++r) {
            const int n = static_cast<int>(rows[r].size());
            const float step_x = br * 2.35f;
            for (int i = 0; i < n; ++i) {
                const float x = base.x + (i - (n - 1) * 0.5f) * step_x;
                out.push_back({{x, base.y + r * step_y}, br, rows[r][i], ButtonName(sys, rows[r][i]), "der"});
            }
        }
    }
    // Gatillos arriba: los «L…» a la izquierda y el resto a la derecha.
    {
        int li = 0, ri = 0;
        for (int b : sys.shoulders) {
            // L3/R3 son el clic de los sticks: en pantalla no van arriba.
            if (b == RETRO_DEVICE_ID_JOYPAD_L3 || b == RETRO_DEVICE_ID_JOYPAD_R3) continue;
            const char* name = ButtonName(sys, b);
            const bool left = name[0] == 'L' || name[0] == 'Z';
            const ImVec2 o = off(left ? "gat_izq" : "gat_der");
            const int idx = left ? li++ : ri++;
            const float x = left ? W * 0.035f + u * 0.95f + idx * u * 2.05f : W - W * 0.035f - u * 0.95f - idx * u * 2.05f;
            out.push_back({{x + o.x, H * 0.16f + o.y}, u * 0.62f, b, name, left ? "gat_izq" : "gat_der"});
        }
    }
    // Select y Start, y el botón de menú. Acostado (lo normal), van a los costados, entre los gatillos y
    // la cruceta: Select a la izquierda, Start a la derecha y el menú junto a Start. Así nada queda
    // encima del juego (antes iban al centro, sobre la imagen). En vertical, abajo al centro.
    const bool landscape = W > H * 1.25f;
    {
        const ImVec2 o = off("centro");
        const int n = static_cast<int>(sys.center.size());
        for (int i = 0; i < n; ++i) {
            ImVec2 c;
            if (landscape) {
                const bool left = n > 1 && i == 0; // con un solo botón (Start), va a la derecha
                c = left ? ImVec2(W * 0.035f + u * 1.55f, H * 0.33f) : ImVec2(W - W * 0.035f - u * 1.05f, H * 0.33f);
            } else {
                c = ImVec2(W * 0.5f + (i - (n - 1) * 0.5f) * u * 1.9f, H - u * 0.75f);
            }
            out.push_back({{c.x + o.x, c.y + o.y}, u * 0.5f, sys.center[i], ButtonName(sys, sys.center[i]), "centro"});
        }
    }
    if (landscape) out.push_back({{W - W * 0.035f - u * 2.75f, H * 0.33f}, u * 0.42f, -1, "", "menu"});
    else out.push_back({{W * 0.5f, u * 0.62f}, u * 0.45f, -1, "", "menu"});
    return out;
}

// Acostado y con el control en pantalla, el juego se dibuja entre las dos columnas del control (se
// achica un poco si hace falta) para que ningún botón quede encima de la imagen.
void App::TouchReserve(int W, int H, int* left, int* right) const {
    *left = *right = 0;
    if (!TouchPadVisible() || bar_open_ || touch_edit_ || W <= H * 1.25f || sys_->touch) return;
    const auto layout = TouchLayout(static_cast<float>(W), static_cast<float>(H));
    float l = 0, r = 0;
    for (const auto& b : layout) {
        const bool pill = std::string(b.group).rfind("gat", 0) == 0 || std::string(b.group) == "centro";
        const float ext = b.r * (pill ? 1.45f : 1.0f);
        if (b.c.x < W * 0.5f) l = std::max(l, b.c.x + ext);
        else r = std::max(r, W - (b.c.x - ext));
    }
    const float gap = W * 0.008f;
    // Nunca más de un tercio por lado (si alguien movió un grupo al centro, el juego no desaparece).
    *left = static_cast<int>(std::min(l + gap, W * 0.33f));
    *right = static_cast<int>(std::min(r + gap, W * 0.33f));
}

void App::ApplyTouchPad() {
    touch_buttons_ = 0;
    touch_stick_[0] = touch_stick_[1] = 0;
    if (!TouchPadVisible() || touch_edit_ || bar_open_) {
        touch_prev_ = 0;
        return;
    }
    int W, H;
    SDL_GetWindowSizeInPixels(window_, &W, &H);
    const auto layout = TouchLayout(static_cast<float>(W), static_cast<float>(H));
    bool menu = false;
    for (auto& [id, f] : fingers_) {
        // Un dedo que empezó en la cruceta o el stick se queda con ellos aunque se salga un poco.
        if (f.owner < 0) {
            float best = 1e9f;
            for (int i = 0; i < static_cast<int>(layout.size()); ++i) {
                const auto& b = layout[i];
                const float dx = f.sx - b.c.x, dy = f.sy - b.c.y;
                const float d = std::sqrt(dx * dx + dy * dy) / (b.r * (b.id <= -2 ? 1.25f : 1.35f));
                if (d < 1.0f && d < best) {
                    best = d;
                    f.owner = i;
                }
            }
            if (f.owner < 0) f.owner = 9999; // fuera de todo (en Dual, la pantalla táctil)
        }
        if (f.owner >= static_cast<int>(layout.size())) continue;
        const auto& b = layout[f.owner];
        const float dx = f.x - b.c.x, dy = f.y - b.c.y;
        if (b.id == -1) {
            menu = true;
        } else if (b.id == -2) {
            // Cruceta: arriba, abajo, izquierda y derecha tienen zonas anchas (60°) y las diagonales
            // angostas (30°), así tocar «arriba» no marca también un lado. Para cambiar de dirección
            // hay que pasarse un poco del borde (no tiembla entre dos vecinas).
            const float d = std::sqrt(dx * dx + dy * dy);
            if (d > b.r * 0.22f) {
                float deg = std::atan2(dy, dx) * 180.0f / kPi; // 0 derecha, 90 abajo
                if (deg < 0) deg += 360.0f;
                auto half = [](int dir) { return dir % 2 == 0 ? 30.0f : 15.0f; };
                auto dist = [](float a, float b2) {
                    float x = std::fabs(a - b2);
                    return x > 180.0f ? 360.0f - x : x;
                };
                int dir = -1;
                if (f.dir >= 0 && dist(deg, f.dir * 45.0f) <= half(f.dir) + 8.0f) dir = f.dir; // se queda
                for (int k = 0; dir < 0 && k < 8; ++k)
                    if (dist(deg, k * 45.0f) <= half(k)) dir = k;
                if (dir < 0) dir = static_cast<int>(std::lround(deg / 45.0f)) & 7;
                f.dir = dir;
                static const uint16_t dirs[8] = {
                    1u << RETRO_DEVICE_ID_JOYPAD_RIGHT, (1u << RETRO_DEVICE_ID_JOYPAD_RIGHT) | (1u << RETRO_DEVICE_ID_JOYPAD_DOWN),
                    1u << RETRO_DEVICE_ID_JOYPAD_DOWN,  (1u << RETRO_DEVICE_ID_JOYPAD_DOWN) | (1u << RETRO_DEVICE_ID_JOYPAD_LEFT),
                    1u << RETRO_DEVICE_ID_JOYPAD_LEFT,  (1u << RETRO_DEVICE_ID_JOYPAD_LEFT) | (1u << RETRO_DEVICE_ID_JOYPAD_UP),
                    1u << RETRO_DEVICE_ID_JOYPAD_UP,    (1u << RETRO_DEVICE_ID_JOYPAD_UP) | (1u << RETRO_DEVICE_ID_JOYPAD_RIGHT)};
                touch_buttons_ |= dirs[dir];
            } else {
                f.dir = -1;
            }
        } else if (b.id == -3) {
            // Palanca: zona muerta al centro y, cerca de arriba/abajo/izquierda/derecha, se endereza
            // (si vas hacia arriba no se cuela un poquito de lado).
            float x = std::clamp(dx / b.r, -1.0f, 1.0f), y = std::clamp(dy / b.r, -1.0f, 1.0f);
            const float mag = std::sqrt(x * x + y * y);
            if (mag < 0.15f) {
                x = y = 0;
            } else {
                if (std::fabs(x) < mag * 0.38f) x = 0;
                if (std::fabs(y) < mag * 0.38f) y = 0;
            }
            touch_stick_[0] = x;
            touch_stick_[1] = y;
        } else {
            touch_buttons_ |= static_cast<uint16_t>(1u << b.id);
        }
    }
    // Vibración corta al apretar algo nuevo.
    const uint16_t fresh = touch_buttons_ & ~touch_prev_;
    if (fresh && settings_.touch_haptics) Android::Vibrate(12, 90);
    touch_prev_ = touch_buttons_;
    core_.pads[0].buttons |= touch_buttons_;
    if (touch_stick_[0] != 0 || touch_stick_[1] != 0) {
        core_.pads[0].analog[0][0] = static_cast<int16_t>(touch_stick_[0] * 32767);
        core_.pads[0].analog[0][1] = static_cast<int16_t>(touch_stick_[1] * 32767);
    }
    if (menu && !touch_menu_latched_) ToggleGameBar(); // con el dedo, el botón de menú despliega la barra de arriba
    touch_menu_latched_ = menu;
}

void App::DrawTouchPad() {
    if ((!TouchPadVisible() && !touch_edit_) || (bar_open_ && !touch_edit_)) return;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    // La interfaz va en puntos y los dedos en píxeles: se convierte.
    int W, H;
    SDL_GetWindowSizeInPixels(window_, &W, &H);
    const float sx = vp->Size.x / std::max(1, W), sy = vp->Size.y / std::max(1, H);
    const auto layout = TouchLayout(static_cast<float>(W), static_cast<float>(H));
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const float a = std::clamp(settings_.touch_opacity, 0.15f, 1.0f);
    const unsigned col = sys_->color;
    auto P = [&](ImVec2 p) { return ImVec2(vp->Pos.x + p.x * sx, vp->Pos.y + p.y * sy); };
    for (const auto& b : layout) {
        const ImVec2 c = P(b.c);
        const float r = b.r * sx;
        if (b.id == -1) {
            dl->AddCircleFilled(c, r, IM_COL32(10, 8, 18, static_cast<int>(150 * a)));
            dl->AddCircle(c, r, U32(0xFFFFFF, 0.35f * a), 0, S(1.5f));
            DrawIcon(dl, Icon::Pause, c, r * 0.9f, U32(0xFFFFFF, 0.85f * a));
            continue;
        }
        if (b.id == -2) {
            // Cruceta: una cruz sólida con relieve, como la de un mando; el brazo apretado se ilumina.
            const float arm = r * 0.34f, len = r * 0.96f, rad = arm * 0.32f;
            const unsigned body = 0x272833;
            dl->AddCircleFilled(c, r, IM_COL32(10, 8, 18, static_cast<int>(45 * a)));
            for (int pass = 0; pass < 3; ++pass) {
                // 0: sombra; 1: cuerpo; 2: brillo de arriba.
                const float oy = pass == 0 ? r * 0.07f : pass == 2 ? -r * 0.03f : 0;
                const float in = pass == 2 ? arm * 0.16f : 0;
                const ImU32 cc = pass == 0 ? IM_COL32(0, 0, 0, static_cast<int>(95 * a)) : pass == 1 ? Shade(body, 0.75f, 0.92f * a) : Shade(body, 1.08f, 0.92f * a);
                dl->AddRectFilled({c.x - arm + in, c.y - len + in + oy}, {c.x + arm - in, c.y + len - in + oy}, cc, rad);
                dl->AddRectFilled({c.x - len + in, c.y - arm + in + oy}, {c.x + len - in, c.y + arm - in + oy}, cc, rad);
            }
            struct D {
                int bit;
                float dx, dy;
            } dirs[] = {{RETRO_DEVICE_ID_JOYPAD_UP, 0, -1}, {RETRO_DEVICE_ID_JOYPAD_DOWN, 0, 1}, {RETRO_DEVICE_ID_JOYPAD_LEFT, -1, 0}, {RETRO_DEVICE_ID_JOYPAD_RIGHT, 1, 0}};
            for (const auto& d : dirs) {
                const bool on = touch_buttons_ & (1u << d.bit);
                if (on) {
                    // El brazo apretado: color de la consola y un poco hundido.
                    const ImVec2 q0(c.x + (d.dx != 0 ? (d.dx > 0 ? arm * 0.9f : -len) : -arm), c.y + (d.dy != 0 ? (d.dy > 0 ? arm * 0.9f : -len) : -arm));
                    const ImVec2 q1(c.x + (d.dx != 0 ? (d.dx > 0 ? len : -arm * 0.9f) : arm), c.y + (d.dy != 0 ? (d.dy > 0 ? len : -arm * 0.9f) : arm));
                    dl->AddRectFilled(q0, q1, Shade(col, 1.0f, 0.85f), rad);
                }
                // Flechita grabada en cada brazo.
                const ImVec2 t(c.x + d.dx * len * 0.66f, c.y + d.dy * len * 0.66f);
                const float k = arm * 0.42f;
                const ImVec2 tip(t.x + d.dx * k, t.y + d.dy * k);
                const ImVec2 l(t.x - d.dy * k - d.dx * k * 0.4f, t.y + d.dx * k - d.dy * k * 0.4f);
                const ImVec2 rr(t.x + d.dy * k - d.dx * k * 0.4f, t.y - d.dx * k - d.dy * k * 0.4f);
                dl->AddTriangleFilled(tip, l, rr, IM_COL32(255, 255, 255, static_cast<int>((on ? 240 : 90) * (on ? 1.0f : a))));
            }
            // Hundido del centro.
            dl->AddCircleFilled(c, arm * 0.62f, Shade(body, 0.55f, 0.9f * a));
            dl->AddCircle(c, arm * 0.62f, IM_COL32(255, 255, 255, static_cast<int>(28 * a)), 0, std::max(1.0f, arm * 0.06f));
            continue;
        }
        if (b.id == -3) {
            // Palanca: base hundida con su aro y una cabeza con relieve que sigue al dedo.
            const bool on = touch_stick_[0] != 0 || touch_stick_[1] != 0;
            dl->AddCircleFilled({c.x, c.y + r * 0.06f}, r * 1.02f, IM_COL32(0, 0, 0, static_cast<int>(70 * a)));
            dl->AddCircleFilled(c, r, Shade(0x1E1F28, 1.0f, 0.82f * a));
            dl->AddCircle(c, r, on ? Shade(col, 1.1f, 0.7f) : IM_COL32(255, 255, 255, static_cast<int>(40 * a)), 0, std::max(1.5f, r * 0.04f));
            dl->AddCircle(c, r * 0.72f, IM_COL32(0, 0, 0, static_cast<int>(70 * a)), 0, std::max(1.5f, r * 0.05f));
            const ImVec2 h(c.x + touch_stick_[0] * r * 0.55f, c.y + touch_stick_[1] * r * 0.55f);
            DrawDisc(dl, h, r * 0.5f, on ? col : 0x3B3C4A, a, false, col);
            dl->AddCircle({h.x, h.y - r * 0.02f}, r * 0.33f, IM_COL32(0, 0, 0, static_cast<int>(60 * (on ? 1.0f : a))), 0, std::max(1.0f, r * 0.04f));
            continue;
        }
        const bool on = touch_buttons_ & (1u << b.id);
        const std::string group = b.group;
        const bool pill = group.rfind("gat", 0) == 0 || group == "centro";
        if (pill) {
            const float hw = r * 1.45f, hh = r * 0.62f;
            const ImVec2 cc(c.x, c.y + (on ? hh * 0.08f : 0));
            DrawPill(dl, cc, hw, hh, 0x2C2D38, a, on, col);
            if (*b.label)
                DrawLabel(dl, cc, std::min(hh * 1.05f, S(22)), hw * 1.6f, IM_COL32(244, 242, 250, static_cast<int>(255 * (on ? 1.0f : std::max(a, 0.6f)))),
                          IM_COL32(0, 0, 0, static_cast<int>(70 * (on ? 1.0f : a))), Tr(b.label));
            continue;
        }
        // Botones de acción con el color y la figura de cada consola.
        const FaceLook look = LookFor(sys_->id, b.label);
        const ImVec2 cc(c.x, c.y + (on ? r * 0.06f : 0));
        // El brillo al apretar: el color del botón, o el de su figura/consola si el botón es oscuro.
        const unsigned lum = ((look.base >> 16) & 0xFF) + ((look.base >> 8) & 0xFF) + (look.base & 0xFF);
        const unsigned glow = look.kind != FaceLook::Letter ? look.glyph : lum < 200 ? col : look.base;
        DrawDisc(dl, cc, r, look.base, a, on, glow);
        DrawGlyph(dl, cc, r, look, b.label, a, on);
    }
    if (touch_edit_) DrawTouchEditor(layout, sx, sy);
}

// Modo edición: se arrastra cada grupo (cruceta, botones, gatillos, centro) a donde guste.
void App::DrawTouchEditor(const std::vector<TouchButton>& layout, float sx, float sy) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    int W, H;
    SDL_GetWindowSizeInPixels(window_, &W, &H);
    // Caja de cada grupo.
    std::map<std::string, ImRect> boxes;
    for (const auto& b : layout) {
        if (b.id == -1) continue;
        const ImVec2 c(vp->Pos.x + b.c.x * sx, vp->Pos.y + b.c.y * sy);
        const float r = b.r * sx * 1.5f;
        ImRect box(c.x - r, c.y - r, c.x + r, c.y + r);
        auto it = boxes.find(b.group);
        if (it == boxes.end()) boxes[b.group] = box;
        else it->second.Add(box);
    }
    const ImVec2 m = ImGui::GetIO().MousePos;
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        touch_drag_group_.clear();
        for (const auto& [g, box] : boxes)
            if (box.Contains(m)) touch_drag_group_ = g;
    }
    if (!touch_drag_group_.empty() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        auto& o = settings_.touch_offsets[sys_->id + "." + touch_drag_group_];
        o.first += ImGui::GetIO().MouseDelta.x / sx / W;
        o.second += ImGui::GetIO().MouseDelta.y / sy / H;
        o.first = std::clamp(o.first, -0.45f, 0.45f);
        o.second = std::clamp(o.second, -0.45f, 0.45f);
    }
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !touch_drag_group_.empty()) {
        touch_drag_group_.clear();
        SaveConfig();
    }
    for (const auto& [g, box] : boxes) {
        const bool sel = g == touch_drag_group_;
        dl->AddRect(box.Min, box.Max, U32(sel ? Color::Gold : 0xFFFFFF, sel ? 0.9f : 0.45f), S(14), S(2));
    }
    // Barra de arriba: tamaño, opacidad, restablecer y listo.
    ImGui::SetNextWindowPos({vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + S(70)}, ImGuiCond_Always, {0.5f, 0.0f});
    ImGui::SetNextWindowBgAlpha(0.94f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {S(18), S(14)});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, S(18));
    ImGui::Begin("##editar_tactil", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
    {
        FontScope f(GetFonts().semibold, 14);
        ImGui::TextUnformatted(Tr(("Control en pantalla · " + sys_->full).c_str()));
    }
    {
        FontScope f(GetFonts().regular, 12.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, V4(Color::Muted));
        ImGui::TextUnformatted(Tr("Arrastra cada grupo a donde te quede cómodo."));
        ImGui::PopStyleColor();
    }
    int size = static_cast<int>(std::lround(settings_.touch_scale * 100));
    if (SliderIntStyled("##tam_tactil", &size, 60, 160, "Tamaño %d %%", S(260))) settings_.touch_scale = size / 100.0f;
    int op = static_cast<int>(std::lround(settings_.touch_opacity * 100));
    if (SliderIntStyled("##op_tactil", &op, 15, 100, "Opacidad %d %%", S(260))) settings_.touch_opacity = op / 100.0f;
    if (Button("Como venía##tactil", ButtonKind::Secondary, {S(125), S(40)})) {
        for (auto it = settings_.touch_offsets.begin(); it != settings_.touch_offsets.end();)
            it = it->first.rfind(sys_->id + ".", 0) == 0 ? settings_.touch_offsets.erase(it) : std::next(it);
    }
    ImGui::SameLine(0, S(10));
    if (Button("Listo##tactil", ButtonKind::Primary, {S(125), S(40)}, Icon::Check, true)) {
        touch_edit_ = false;
        SaveConfig();
        SetMenuOpen(false);
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

// ---------------------------------------------------------------- explorador de carpetas

void App::OpenBrowser(bool folders) {
    browser_for_root_ = false;
    browser_open_ = true;
    browser_folders_ = folders;
    if (browser_dir_.empty()) {
        const auto roots = Android::Roots();
        browser_dir_ = roots.empty() ? Paths::User() : Paths::FromUtf8(roots.front());
    }
    browser_items_dirty_ = true;
}

void App::DrawBrowser() {
    if (!browser_open_) return;
    // ¿Hay permiso para ver todas las carpetas? (se revisa cada segundo, por si lo acaban de dar)
    const double now = ImGui::GetTime();
    if (now - browser_perm_t_ > 1.0) {
        browser_perm_t_ = now;
        const bool had = browser_perm_;
        browser_perm_ = Android::HasFileAccess();
        if (browser_perm_ && !had) browser_items_dirty_ = true;
    }
    if (browser_items_dirty_) {
        browser_items_dirty_ = false;
        browser_items_.clear();
        std::error_code ec;
        std::vector<std::string> exts = AllExtensions();
        exts.push_back("zip");
        exts.push_back("7z");
        for (fs::directory_iterator it(browser_dir_, fs::directory_options::skip_permission_denied, ec), end; it != end && !ec; it.increment(ec)) {
            const std::string name = Paths::ToUtf8(it->path().filename());
            if (name.empty() || name[0] == '.') continue;
            const bool dir = it->is_directory(ec);
            if (!dir) {
                if (browser_folders_) continue;
                std::string e = Lower(Paths::ToUtf8(it->path().extension()));
                if (!e.empty()) e.erase(0, 1);
                if (std::find(exts.begin(), exts.end(), e) == exts.end()) continue;
            }
            browser_items_.push_back({name, dir});
        }
        std::sort(browser_items_.begin(), browser_items_.end(), [](const auto& a, const auto& b) {
            if (a.second != b.second) return a.second > b.second;
            return Lower(a.first) < Lower(b.first);
        });
    }

    ImGui::OpenPopup("##explorador");
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float w = std::min(S(760), vp->Size.x - S(24)), h = std::min(S(640), vp->Size.y - S(24));
    ImGui::SetNextWindowPos({vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f}, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({w, h});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {S(20), S(18)});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, S(22));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, V4(Color::Bg));
    ImGui::PushStyleColor(ImGuiCol_Border, V4(Color::Amber, 0.4f));
    if (ImGui::BeginPopupModal("##explorador", nullptr, ImGuiWindowFlags_NoDecoration)) {
        const float inner = ImGui::GetContentRegionAvail().x;
        {
            FontScope f(GetFonts().bold, 19);
            ImGui::TextUnformatted(Tr(browser_for_root_ ? "Elige dónde va «Satronho Games»" : browser_folders_ ? "Elige la carpeta de tus juegos" : "Abrir un juego"));
        }
        if (!browser_perm_) {
            ImGui::Dummy({0, S(4)});
            GuideSays(GuideId::Samy, "Para buscar tus juegos en cualquier carpeta, Android necesita que me des el permiso de «acceso a todos los archivos». "
                                     "Los leo y guardo tus juegos, partidas y estados en la carpeta «Satronho Games»; no toco nada más.", inner);
            ImGui::Dummy({0, S(6)});
            if (Button("Dar permiso", ButtonKind::Primary, {inner, S(48)}, Icon::Check, true)) Android::RequestFileAccess();
        }
        ImGui::Dummy({0, S(4)});
        // Accesos rápidos: memoria interna, tarjetas SD y Descargas.
        {
            const auto roots = Android::Roots();
            FontScope f(GetFonts().medium, 13);
            int i = 0;
            for (const auto& r : roots) {
                if (i) ImGui::SameLine(0, S(6));
                const std::string label = i == 0 ? std::string("Memoria interna") : "Tarjeta SD " + std::to_string(i);
                ImGui::PushID(i++);
                if (Button(label.c_str(), ButtonKind::Secondary, {0, S(36)}, Icon::Folder, true)) {
                    browser_dir_ = Paths::FromUtf8(r);
                    browser_items_dirty_ = true;
                }
                ImGui::PopID();
            }
            if (!roots.empty()) {
                ImGui::SameLine(0, S(6));
                if (Button("Descargas", ButtonKind::Secondary, {0, S(36)}, Icon::Load, true)) {
                    browser_dir_ = Paths::FromUtf8(roots.front()) / "Download";
                    browser_items_dirty_ = true;
                }
            }
        }
        // Ruta actual y subir.
        {
            if (IconButton("##arriba", Icon::ChevronLeft, S(40), U32(Color::Text), "Subir") && browser_dir_.has_parent_path() &&
                browser_dir_.parent_path() != browser_dir_) {
                browser_dir_ = browser_dir_.parent_path();
                browser_items_dirty_ = true;
            }
            ImGui::SameLine(0, S(8));
            const ImVec2 p = ImGui::GetCursorScreenPos();
            Ellipsize(ImGui::GetWindowDrawList(), GetFonts().medium, S(14), {p.x, p.y + S(11)}, U32(Color::Muted), Paths::ToUtf8(browser_dir_),
                      inner - S(60), 1);
            ImGui::NewLine();
        }
        const float foot = S(60);
        ImGui::BeginChild("##lista_carpetas", {0, ImGui::GetContentRegionAvail().y - foot}, 0);
        if (browser_items_.empty()) Muted(browser_folders_ ? "No hay carpetas aquí." : "No hay juegos aquí.", 14);
        fs::path go;
        for (size_t i = 0; i < browser_items_.size(); ++i) {
            const auto& [name, dir] = browser_items_[i];
            ImGui::PushID(static_cast<int>(i));
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const float rw = ImGui::GetContentRegionAvail().x;
            const bool clicked = ImGui::InvisibleButton("##it", {rw, S(52)});
            const bool hov = ImGui::IsItemHovered();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(p, {p.x + rw, p.y + S(52)}, U32(hov ? Color::Surface3 : Color::Surface2, hov ? 1.0f : 0.7f), S(12));
            DrawIcon(dl, dir ? Icon::Folder : Icon::Play, {p.x + S(26), p.y + S(26)}, S(20), U32(dir ? Color::Amber : Color::Accent));
            Ellipsize(dl, GetFonts().medium, S(15), {p.x + S(52), p.y + S(15)}, U32(Color::Text), name, rw - S(70), 1);
            if (clicked) go = browser_dir_ / Paths::FromUtf8(name);
            if (clicked && !dir) {
                browser_open_ = false;
                RequestStart(go);
                go.clear();
            }
            ImGui::PopID();
            ImGui::Dummy({0, S(1)});
        }
        ImGui::EndChild();
        if (!go.empty()) {
            browser_dir_ = go;
            browser_items_dirty_ = true;
            ImGui::SetScrollY(0);
        }
        const float bw = browser_folders_ ? (inner - S(10)) * 0.5f : inner;
        {
        FontScope f(GetFonts().semibold, 15);
        if (Button("Cancelar##exp", ButtonKind::Secondary, {bw, S(48)})) browser_open_ = false;
        if (browser_folders_) {
            ImGui::SameLine(0, S(10));
            if (Button("Usar esta carpeta", ButtonKind::Primary, {bw, S(48)}, Icon::Check, true)) {
                browser_open_ = false;
                if (browser_for_root_) {
                    // Nueva casa de «Satronho Games»: adentro de la carpeta elegida (o ella misma).
                    fs::path root = browser_dir_;
                    if (root.filename() != "Satronho Games") root /= "Satronho Games";
                    if (root != Paths::PublicRoot()) {
                        settings_.games_root = Paths::ToUtf8(root);
                        SaveConfig();
                        BeginDataMove(root);
                    }
                } else {
                    std::lock_guard lock(pending_mutex_);
                    pending_folders_.push_back(Paths::ToUtf8(browser_dir_));
                }
            }
        }
        }
        if (!browser_open_) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

} // namespace Satronho

namespace Satronho {

// ---------------------------------------------------------------- barra lateral angosta

void App::DrawSidebarRail(float width) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, V4(Color::Side));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {S(10), S(12)});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {S(4), S(4)});
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, S(3));
    ImGui::BeginChild("##riel", {width, 0}, ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float w = ImGui::GetContentRegionAvail().x;
    // Logo.
    {
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float ls = S(46);
        if (logo_) dl->AddImageRounded(logo_.Ref(), {p.x + (w - ls) * 0.5f, p.y}, {p.x + (w + ls) * 0.5f, p.y + ls}, {0, 0}, {1, 1}, IM_COL32_WHITE, S(12));
        ImGui::Dummy({w, ls + S(10)});
    }
    auto item = [&](const char* id, bool sel, unsigned color, const std::function<void(ImVec2, float)>& icon, const char* badge = nullptr) {
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float h = S(52);
        const bool click = ImGui::InvisibleButton(id, {w, h});
        const bool hov = ImGui::IsItemHovered();
        if (sel || hov) dl->AddRectFilled(p, {p.x + w, p.y + h}, U32(sel ? color : Color::Surface2, sel ? 0.22f : 0.6f), S(14));
        if (sel) dl->AddRectFilled({p.x - S(10), p.y + S(12)}, {p.x - S(6), p.y + h - S(12)}, U32(color), S(2));
        icon({p.x + w * 0.5f, p.y + h * 0.5f}, sel || hov ? 1.0f : 0.75f);
        if (badge && *badge) {
            FontScope f(GetFonts().semibold, 10);
            const ImVec2 ts = ImGui::CalcTextSize(Tr(badge));
            const ImVec2 b1(p.x + w - S(2), p.y + S(4) + ts.y + S(4));
            dl->AddRectFilled({b1.x - ts.x - S(8), p.y + S(4)}, b1, U32(color, 0.9f), S(8));
            dl->AddText({b1.x - ts.x - S(4), p.y + S(6)}, U32(0x14101F), Tr(badge));
        }
        return click;
    };
    size_t visible = 0;
    for (const auto& g : games_) visible += !g.hidden;
    const std::string total = visible ? std::to_string(visible) : std::string();
    if (item("##r_bib", page_ == Page::Library && lib_system_.empty(), Color::Accent,
             [&](ImVec2 c, float a) { DrawIcon(dl, Icon::Library, c, S(20), U32(Color::Text, a)); }, total.c_str())) {
        page_ = Page::Library;
        lib_system_.clear();
    }
    dl->AddRectFilled({ImGui::GetCursorScreenPos().x + S(14), ImGui::GetCursorScreenPos().y + S(2)},
                      {ImGui::GetCursorScreenPos().x + w - S(14), ImGui::GetCursorScreenPos().y + S(3)}, U32(Color::Border));
    ImGui::Dummy({w, S(6)});
    for (const auto& sys : Systems()) {
        const bool sel = page_ == Page::Library && lib_system_ == sys.id;
        ImGui::PushID(sys.id.c_str());
        if (item("##r_sys", sel, sys.color, [&](ImVec2 c, float a) { DrawControllerMini(dl, sys.layout, c, sys.handheld ? S(24) : S(34), a); })) {
            page_ = Page::Library;
            lib_system_ = sys.id;
        }
        ImGui::PopID();
    }
    ImGui::Dummy({w, S(6)});
    struct Nav {
        const char* id;
        Page page;
        Icon icon;
        unsigned color;
    } navs[] = {{"##r_guias", Page::Guides, Icon::Paw, Color::Green},     {"##r_sug", Page::Suggested, Icon::Sparkle, Color::Teal},
                {"##r_nov", Page::News, Icon::Bell, Color::Pink},          {"##r_ctrl", Page::Controls, Icon::Gamepad, Color::Amber},
                {"##r_herr", Page::Tools, Icon::Tools, Color::Orange},
                {"##r_aj", Page::Settings, Icon::Gear, Color::Accent},     {"##r_ac", Page::About, Icon::Info, Color::Accent}};
    for (const auto& n : navs)
        if (item(n.id, page_ == n.page, n.color, [&](ImVec2 c, float a) { DrawIcon(dl, n.icon, c, S(20), U32(Color::Text, a)); },
                 n.page == Page::News && updater_.GetState() == Updater::State::Available ? "!" : nullptr))
            page_ = n.page;
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
}

// Celular en vertical: barra de secciones abajo, como en las apps del celular.
void App::DrawBottomNav(float height) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, V4(Color::Side));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {S(6), S(6)});
    ImGui::BeginChild("##barra_abajo", {0, height}, ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 w0 = ImGui::GetWindowPos();
    dl->AddLine(w0, {w0.x + ImGui::GetWindowWidth(), w0.y}, U32(Color::Border));
    struct Nav {
        const char* id;
        const char* label;
        Page page;
        Icon icon;
        unsigned color;
    } navs[] = {{"##b_bib", "Biblioteca", Page::Library, Icon::Library, Color::Accent}, {"##b_ctrl", "Controles", Page::Controls, Icon::Gamepad, Color::Amber},
                {"##b_guias", "Guías", Page::Guides, Icon::Paw, Color::Green},          {"##b_aj", "Ajustes", Page::Settings, Icon::Gear, Color::Accent},
                {"##b_mas", "Más", Page::Tools, Icon::Grid, Color::Teal}};
    const int n = static_cast<int>(std::size(navs));
    const float w = ImGui::GetContentRegionAvail().x / n, h = ImGui::GetContentRegionAvail().y;
    const bool more_sel = page_ == Page::Tools || page_ == Page::Suggested || page_ == Page::News || page_ == Page::About;
    for (int i = 0; i < n; ++i) {
        const auto& nv = navs[i];
        if (i) ImGui::SameLine(0, 0);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const bool is_more = i == n - 1;
        const bool sel = is_more ? more_sel : page_ == nv.page;
        if (ImGui::InvisibleButton(nv.id, {w, h})) {
            if (is_more) ImGui::OpenPopup("##mas_abajo");
            else {
                page_ = nv.page;
                if (nv.page == Page::Library) lib_system_.clear();
            }
        }
        if (sel) dl->AddRectFilled({p.x + S(8), p.y + S(2)}, {p.x + w - S(8), p.y + h - S(2)}, U32(nv.color, 0.18f), S(12));
        const unsigned col = sel ? Color::Text : Color::Muted;
        DrawIcon(dl, nv.icon, {p.x + w * 0.5f, p.y + h * 0.36f}, S(18), U32(col));
        FontScope f(GetFonts().semibold, 11.0f);
        const char* t = Tr(nv.label);
        const float tw = ImGui::CalcTextSize(t).x;
        dl->AddText({p.x + (w - tw) * 0.5f, p.y + h * 0.66f}, U32(col), t);
    }
    // «Más»: lo que no cabe abajo.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {S(14), S(12)});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {S(10), S(10)});
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, S(16));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, V4(Color::Surface, 0.98f));
    ImGui::SetNextWindowPos({w0.x + ImGui::GetWindowWidth() - S(8), w0.y - S(6)}, ImGuiCond_Always, {1.0f, 1.0f});
    if (ImGui::BeginPopup("##mas_abajo")) {
        const struct {
            const char* label;
            Page page;
            Icon icon;
        } more[] = {{"Configuración sugerida", Page::Suggested, Icon::Sparkle},
                    {"Novedades", Page::News, Icon::Bell},
                    {"Herramientas", Page::Tools, Icon::Tools},
                    {"Acerca de", Page::About, Icon::Info}};
        ImDrawList* pdl = ImGui::GetWindowDrawList();
        for (const auto& m : more) {
            FontScope f(GetFonts().medium, 15);
            const ImVec2 p = ImGui::GetCursorScreenPos();
            if (ImGui::Selectable((std::string("      ") + Tr(m.label)).c_str(), page_ == m.page, 0, {S(240), S(40)})) page_ = m.page;
            DrawIcon(pdl, m.icon, {p.x + S(12), p.y + S(20)}, S(16), U32(Color::Muted));
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
    ImGui::EndChild();
}

} // namespace Satronho
