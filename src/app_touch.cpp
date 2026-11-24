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
} // namespace

// ---------------------------------------------------------------- modo táctil y compacto

void App::UpdateTouchMode() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float k = std::max(0.5f, Ui::Scale());
    compact_ = vp->Size.x / k < 1040.0f || vp->Size.y / k < 600.0f;
#ifdef __ANDROID__
    touch_mode_ = true;
#endif
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
    auto scrollable = [](ImGuiWindow* w) {
        while (w && w->ScrollMax.y <= 0.0f && w->ScrollMax.x <= 0.0f && !(w->Flags & ImGuiWindowFlags_Popup)) w = w->ParentWindow;
        return w && (w->ScrollMax.y > 0.0f || w->ScrollMax.x > 0.0f) ? w : nullptr;
    };
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        drag_win_ = scrollable(g.HoveredWindow);
        drag_total_ = 0;
        dragging_ = false;
        drag_vel_ = 0;
    }
    auto* win = static_cast<ImGuiWindow*>(drag_win_);
    if (win && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        const ImVec2 d = io.MouseDelta;
        drag_total_ += std::fabs(d.y) + std::fabs(d.x) * 0.5f;
        if (!dragging_ && drag_total_ > S(12)) {
            dragging_ = true;
            ImGui::ClearActiveID(); // ya no es un toque: es desplazar
        }
        if (dragging_) {
            ImGui::SetScrollY(win, std::clamp(win->Scroll.y - d.y, 0.0f, win->ScrollMax.y));
            if (io.DeltaTime > 0) drag_vel_ = drag_vel_ * 0.6f + (-d.y / io.DeltaTime) * 0.4f;
        }
    } else if (win && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        // Inercia al soltar.
        if (std::fabs(drag_vel_) > 20.0f && dragging_) {
            ImGui::SetScrollY(win, std::clamp(win->Scroll.y + drag_vel_ * io.DeltaTime, 0.0f, win->ScrollMax.y));
            drag_vel_ *= std::pow(0.05f, io.DeltaTime);
        } else {
            drag_win_ = nullptr;
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
        // En CD, el stick izquierdo va un poco más abajo, junto a la cruceta.
        if (sys.id == "cd" && sys.analog) out.push_back({{c.x + u * 2.3f, c.y + u * 1.05f}, u * 0.75f, -3, "", "izq"});
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
    // Centro abajo: Select y Start (o solo Start).
    {
        const ImVec2 o = off("centro");
        const int n = static_cast<int>(sys.center.size());
        for (int i = 0; i < n; ++i) {
            const float x = W * 0.5f + (i - (n - 1) * 0.5f) * u * 1.9f;
            out.push_back({{x + o.x, H - u * 0.75f + o.y}, u * 0.5f, sys.center[i], ButtonName(sys, sys.center[i]), "centro"});
        }
    }
    // Menú arriba al centro.
    out.push_back({{W * 0.5f, u * 0.62f}, u * 0.45f, -1, "", "menu"});
    return out;
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
            // Cruceta de 8 direcciones con una zona muerta pequeña al centro.
            const float d = std::sqrt(dx * dx + dy * dy);
            if (d > b.r * 0.18f) {
                const float a = std::atan2(dy, dx);
                const int sector = static_cast<int>(std::lround(a / (kPi / 4))) & 7; // 0 derecha, 2 abajo…
                static const uint16_t dirs[8] = {
                    1u << RETRO_DEVICE_ID_JOYPAD_RIGHT, (1u << RETRO_DEVICE_ID_JOYPAD_RIGHT) | (1u << RETRO_DEVICE_ID_JOYPAD_DOWN),
                    1u << RETRO_DEVICE_ID_JOYPAD_DOWN,  (1u << RETRO_DEVICE_ID_JOYPAD_DOWN) | (1u << RETRO_DEVICE_ID_JOYPAD_LEFT),
                    1u << RETRO_DEVICE_ID_JOYPAD_LEFT,  (1u << RETRO_DEVICE_ID_JOYPAD_LEFT) | (1u << RETRO_DEVICE_ID_JOYPAD_UP),
                    1u << RETRO_DEVICE_ID_JOYPAD_UP,    (1u << RETRO_DEVICE_ID_JOYPAD_UP) | (1u << RETRO_DEVICE_ID_JOYPAD_RIGHT)};
                touch_buttons_ |= dirs[sector];
            }
        } else if (b.id == -3) {
            touch_stick_[0] = std::clamp(dx / b.r, -1.0f, 1.0f);
            touch_stick_[1] = std::clamp(dy / b.r, -1.0f, 1.0f);
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
            // Cruceta: cruz con brazos redondeados y la dirección apretada iluminada.
            const float arm = r * 0.36f;
            dl->AddCircleFilled(c, r, IM_COL32(10, 8, 18, static_cast<int>(90 * a)));
            dl->AddCircle(c, r, U32(col, 0.35f * a), 0, S(1.5f));
            dl->AddRectFilled({c.x - arm, c.y - r * 0.92f}, {c.x + arm, c.y + r * 0.92f}, U32(0xEBE8F5, 0.28f * a), arm * 0.5f);
            dl->AddRectFilled({c.x - r * 0.92f, c.y - arm}, {c.x + r * 0.92f, c.y + arm}, U32(0xEBE8F5, 0.28f * a), arm * 0.5f);
            struct D {
                int bit;
                float dx, dy;
            } dirs[] = {{RETRO_DEVICE_ID_JOYPAD_UP, 0, -1}, {RETRO_DEVICE_ID_JOYPAD_DOWN, 0, 1}, {RETRO_DEVICE_ID_JOYPAD_LEFT, -1, 0}, {RETRO_DEVICE_ID_JOYPAD_RIGHT, 1, 0}};
            for (const auto& d : dirs) {
                const bool on = touch_buttons_ & (1u << d.bit);
                const ImVec2 t(c.x + d.dx * r * 0.62f, c.y + d.dy * r * 0.62f);
                if (on) dl->AddCircleFilled(t, arm * 1.05f, U32(col, 0.75f));
                // Flechita.
                const ImVec2 tip(t.x + d.dx * arm * 0.45f, t.y + d.dy * arm * 0.45f);
                const ImVec2 l(t.x - d.dy * arm * 0.4f - d.dx * arm * 0.15f, t.y + d.dx * arm * 0.4f - d.dy * arm * 0.15f);
                const ImVec2 rr(t.x + d.dy * arm * 0.4f - d.dx * arm * 0.15f, t.y - d.dx * arm * 0.4f - d.dy * arm * 0.15f);
                dl->AddTriangleFilled(tip, l, rr, U32(0xFFFFFF, (on ? 0.95f : 0.55f) * a));
            }
            continue;
        }
        if (b.id == -3) {
            // Stick: base y cabeza que sigue al dedo.
            dl->AddCircleFilled(c, r, IM_COL32(10, 8, 18, static_cast<int>(90 * a)));
            dl->AddCircle(c, r, U32(col, 0.4f * a), 0, S(1.5f));
            const ImVec2 h(c.x + touch_stick_[0] * r * 0.55f, c.y + touch_stick_[1] * r * 0.55f);
            const bool on = touch_stick_[0] != 0 || touch_stick_[1] != 0;
            dl->AddCircleFilled(h, r * 0.48f, U32(on ? col : 0xEBE8F5, (on ? 0.75f : 0.32f) * (on ? 1.0f : a)));
            dl->AddCircle(h, r * 0.48f, U32(0xFFFFFF, 0.4f * a), 0, S(1.5f));
            continue;
        }
        const bool on = touch_buttons_ & (1u << b.id);
        const bool pill = std::string(b.group).rfind("gat", 0) == 0 || std::string(b.group) == "centro";
        if (pill) {
            const ImVec2 p0(c.x - r * 1.45f, c.y - r * 0.62f), p1(c.x + r * 1.45f, c.y + r * 0.62f);
            dl->AddRectFilled(p0, p1, on ? U32(col, 0.8f) : IM_COL32(10, 8, 18, static_cast<int>(110 * a)), r);
            dl->AddRect(p0, p1, U32(on ? 0xFFFFFF : col, (on ? 0.8f : 0.45f) * a), r, S(1.5f));
        } else {
            dl->AddCircleFilled({c.x, c.y + S(2)}, r, IM_COL32(0, 0, 0, static_cast<int>(50 * a)));
            dl->AddCircleFilled(c, r, on ? U32(col, 0.85f) : IM_COL32(10, 8, 18, static_cast<int>(110 * a)));
            dl->AddCircle(c, r, U32(on ? 0xFFFFFF : col, (on ? 0.85f : 0.5f) * a), 0, S(2));
        }
        // En CD, los botones de acción llevan sus figuras (triángulo, cuadrado, círculo y cruz).
        if (sys_->id == "cd" && std::string(b.group) == "der") {
            const float q = r * 0.42f;
            const ImU32 fc = U32(0xFFFFFF, (on ? 1.0f : 0.85f) * std::max(a, 0.6f));
            const float t = S(2.6f);
            switch (b.id) {
            case RETRO_DEVICE_ID_JOYPAD_X:
                dl->AddTriangle({c.x, c.y - q}, {c.x + q * 0.95f, c.y + q * 0.68f}, {c.x - q * 0.95f, c.y + q * 0.68f}, on ? fc : U32(0x4FD1A5, a + 0.2f), t);
                break;
            case RETRO_DEVICE_ID_JOYPAD_Y:
                dl->AddRect({c.x - q * 0.75f, c.y - q * 0.75f}, {c.x + q * 0.75f, c.y + q * 0.75f}, on ? fc : U32(0xFF8FC8, a + 0.2f), 0, t);
                break;
            case RETRO_DEVICE_ID_JOYPAD_A:
                dl->AddCircle(c, q * 0.85f, on ? fc : U32(0xFF6B7A, a + 0.2f), 0, t);
                break;
            default:
                dl->AddLine({c.x - q * 0.75f, c.y - q * 0.75f}, {c.x + q * 0.75f, c.y + q * 0.75f}, on ? fc : U32(0x7FA8FF, a + 0.2f), t);
                dl->AddLine({c.x + q * 0.75f, c.y - q * 0.75f}, {c.x - q * 0.75f, c.y + q * 0.75f}, on ? fc : U32(0x7FA8FF, a + 0.2f), t);
                break;
            }
            continue;
        }
        if (*b.label) {
            ImFont* f = GetFonts().bold;
            float fs = std::min(r * (pill ? 0.62f : 0.72f), S(22));
            ImVec2 ts = f->CalcTextSizeA(fs, FLT_MAX, 0, Tr(b.label));
            const float maxw = r * (pill ? 2.5f : 1.6f);
            if (ts.x > maxw) {
                fs *= maxw / ts.x;
                ts = f->CalcTextSizeA(fs, FLT_MAX, 0, Tr(b.label));
            }
            dl->AddText(f, fs, {c.x - ts.x * 0.5f, c.y - ts.y * 0.5f}, U32(0xFFFFFF, (on ? 1.0f : 0.8f) * std::max(a, 0.55f)), Tr(b.label));
        }
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
            ImGui::TextUnformatted(Tr(browser_folders_ ? "Elige la carpeta de tus juegos" : "Abrir un juego"));
        }
        if (!browser_perm_) {
            ImGui::Dummy({0, S(4)});
            GuideSays(GuideId::Samy, "Para buscar tus juegos en cualquier carpeta, Android necesita que me des el permiso de «acceso a todos los archivos». "
                                     "Solo los leo; no toco nada más.", inner);
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
                std::lock_guard lock(pending_mutex_);
                pending_folders_.push_back(Paths::ToUtf8(browser_dir_));
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

} // namespace Satronho
