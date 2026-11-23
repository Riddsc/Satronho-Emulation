// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/controllers.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "libretro.h"
#include "ui/kit.h"

namespace Satronho::Ui {

namespace {

struct Ctx {
    ImDrawList* dl;
    ImVec2 o;      // origen
    float k;       // escala de unidades de diseño a píxeles
    uint16_t pressed;
    int capturing;
    ImVec2 mouse;
    int hovered = -1;
    float pulse;

    ImVec2 P(float x, float y) const { return {o.x + x * k, o.y + y * k}; }
    float L(float v) const { return v * k; }
    bool Down(int id) const { return id >= 0 && (pressed >> id) & 1; }
    void Hit(int id, float x, float y, float r) {
        const ImVec2 c = P(x, y);
        const float dx = mouse.x - c.x, dy = mouse.y - c.y;
        if (dx * dx + dy * dy <= L(r) * L(r)) hovered = id;
    }
    // Aro de selección: dorado si está presionado, lila al pasar el mouse, parpadeo si espera asignación.
    void Ring(int id, ImVec2 c, float r) const {
        if (capturing == id) dl->AddCircle(c, r + L(6), U32(Color::Accent, 0.4f + 0.6f * pulse), 0, L(4));
        else if (hovered == id) dl->AddCircle(c, r + L(5), U32(Color::Accent, 0.8f), 0, L(3));
        if (Down(id)) {
            dl->AddCircleFilled(c, r + L(9), U32(Color::Gold, 0.18f));
            dl->AddCircle(c, r + L(5), U32(Color::Gold), 0, L(3.5f));
        }
    }
    void Text(const char* t, float x, float y, float size, ImU32 col, bool center = true) const {
        ImFont* f = GetFonts().bold;
        const ImVec2 ts = f->CalcTextSizeA(L(size), FLT_MAX, 0, t);
        dl->AddText(f, L(size), {P(x, y).x - (center ? ts.x * 0.5f : 0), P(x, y).y - ts.y * 0.5f}, col, t);
    }
};

// Cruceta en cruz. Cada brazo se ilumina por separado.
void DPad(Ctx& c, float cx, float cy, float size, ImU32 body, ImU32 shade) {
    const float a = size * 0.5f, w = size * 0.17f;
    c.dl->AddRectFilled(c.P(cx - a + 3, cy - w + 4), c.P(cx + a + 3, cy + w + 4), IM_COL32(0, 0, 0, 60), c.L(4));
    c.dl->AddRectFilled(c.P(cx - w + 3, cy - a + 4), c.P(cx + w + 3, cy + a + 4), IM_COL32(0, 0, 0, 60), c.L(4));
    c.dl->AddRectFilled(c.P(cx - a, cy - w), c.P(cx + a, cy + w), body, c.L(4));
    c.dl->AddRectFilled(c.P(cx - w, cy - a), c.P(cx + w, cy + a), body, c.L(4));
    c.dl->AddCircleFilled(c.P(cx, cy), c.L(w * 0.55f), shade);
    struct Arm {
        int id;
        float dx, dy;
    } arms[] = {{RETRO_DEVICE_ID_JOYPAD_UP, 0, -1}, {RETRO_DEVICE_ID_JOYPAD_DOWN, 0, 1}, {RETRO_DEVICE_ID_JOYPAD_LEFT, -1, 0}, {RETRO_DEVICE_ID_JOYPAD_RIGHT, 1, 0}};
    for (const auto& arm : arms) {
        const float mx = cx + arm.dx * (a - w * 1.1f), my = cy + arm.dy * (a - w * 1.1f);
        c.Hit(arm.id, mx, my, std::max(w * 1.3f, 18.0f));
        // Flechita en relieve.
        const ImVec2 tip = c.P(mx + arm.dx * w * 0.45f, my + arm.dy * w * 0.45f);
        const ImVec2 b1 = c.P(mx - arm.dx * w * 0.25f + arm.dy * w * 0.45f, my - arm.dy * w * 0.25f + arm.dx * w * 0.45f);
        const ImVec2 b2 = c.P(mx - arm.dx * w * 0.25f - arm.dy * w * 0.45f, my - arm.dy * w * 0.25f - arm.dx * w * 0.45f);
        c.dl->AddTriangleFilled(tip, b1, b2, shade);
        const ImVec2 r0 = c.P(arm.dx ? std::min(mx - w, mx + arm.dx * w) : cx - w, arm.dy ? std::min(my - w, my + arm.dy * w) : cy - w);
        const ImVec2 r1 = c.P(arm.dx ? std::max(mx - w, mx + arm.dx * w) + (arm.dx > 0 ? w * 0.1f : 0) : cx + w,
                              arm.dy ? std::max(my - w, my + arm.dy * w) : cy + w);
        if (c.Down(arm.id)) c.dl->AddRectFilled(r0, r1, U32(Color::Gold, 0.85f), c.L(3));
        if (c.hovered == arm.id || c.capturing == arm.id)
            c.dl->AddRect(r0, r1, U32(Color::Accent, c.capturing == arm.id ? 0.4f + 0.6f * c.pulse : 0.9f), c.L(3), c.L(2.5f));
    }
}

void RoundButton(Ctx& c, int id, float x, float y, float r, ImU32 col, ImU32 shade) {
    c.Hit(id, x, y, std::max(r * 1.3f, 22.0f)); // los botones chicos también se pueden tocar
    const ImVec2 p = c.P(x, y);
    c.dl->AddCircleFilled({p.x + c.L(2), p.y + c.L(4)}, c.L(r), IM_COL32(0, 0, 0, 70));
    c.dl->AddCircleFilled(p, c.L(r), c.Down(id) ? U32(Color::Gold) : col);
    c.dl->AddCircleFilled({p.x - c.L(r * 0.25f), p.y - c.L(r * 0.3f)}, c.L(r * 0.45f), shade);
    c.Ring(id, p, c.L(r));
}

// Botón alargado (Select/Start). `angle` en grados.
void PillButton(Ctx& c, int id, float x, float y, float len, float thick, float angle, ImU32 col) {
    c.Hit(id, x, y, len * 0.6f);
    const float ar = angle * 3.14159265f / 180.0f;
    const ImVec2 d(std::cos(ar) * len * 0.5f, std::sin(ar) * len * 0.5f);
    const ImVec2 a = c.P(x - d.x, y - d.y), b = c.P(x + d.x, y + d.y);
    c.dl->AddLine({a.x + c.L(1.5f), a.y + c.L(3)}, {b.x + c.L(1.5f), b.y + c.L(3)}, IM_COL32(0, 0, 0, 70), c.L(thick));
    c.dl->AddLine(a, b, c.Down(id) ? U32(Color::Gold) : col, c.L(thick));
    c.dl->AddCircleFilled(a, c.L(thick * 0.5f), c.Down(id) ? U32(Color::Gold) : col);
    c.dl->AddCircleFilled(b, c.L(thick * 0.5f), c.Down(id) ? U32(Color::Gold) : col);
    if (c.hovered == id || c.capturing == id) {
        const float pad = thick * 0.5f + 5;
        c.dl->AddRect(c.P(x - len * 0.5f - pad, y - pad), c.P(x + len * 0.5f + pad, y + pad),
                      U32(Color::Accent, c.capturing == id ? 0.4f + 0.6f * c.pulse : 0.9f), c.L(pad), c.L(2.5f));
    }
}

// ---------------------------------------------------------------- NES (diseño 680 × 300)
void DrawNes(Ctx& c) {
    ImDrawList* dl = c.dl;
    dl->AddRectFilled(c.P(6, 12), c.P(686, 312), IM_COL32(0, 0, 0, 80), c.L(16));
    dl->AddRectFilled(c.P(0, 0), c.P(680, 300), IM_COL32(203, 201, 198, 255), c.L(16));
    dl->AddRectFilled(c.P(0, 0), c.P(680, 22), IM_COL32(220, 218, 215, 255), c.L(16), ImDrawFlags_RoundTop);
    dl->AddRect(c.P(0, 0), c.P(680, 300), IM_COL32(150, 148, 146, 255), c.L(16), c.L(2));
    // Panel frontal oscuro.
    dl->AddRectFilled(c.P(22, 62), c.P(658, 280), IM_COL32(28, 27, 30, 255), c.L(10));
    // Franjas grises del centro.
    for (int i = 0; i < 4; ++i) {
        const float y = 86 + i * 24;
        dl->AddRectFilled(c.P(232, y), c.P(448, y + 14), IM_COL32(150, 148, 146, 255), c.L(3));
    }
    // Canal gris de Select/Start.
    dl->AddRectFilled(c.P(232, 196), c.P(448, 256), IM_COL32(176, 174, 171, 255), c.L(8));
    c.Text("SELECT", 290, 184, 13, IM_COL32(214, 40, 48, 255));
    c.Text("START", 390, 184, 13, IM_COL32(214, 40, 48, 255));
    PillButton(c, RETRO_DEVICE_ID_JOYPAD_SELECT, 290, 226, 44, 16, 0, IM_COL32(36, 35, 38, 255));
    PillButton(c, RETRO_DEVICE_ID_JOYPAD_START, 390, 226, 44, 16, 0, IM_COL32(36, 35, 38, 255));
    // Cruceta.
    dl->AddRectFilled(c.P(58, 108), c.P(208, 258), IM_COL32(40, 39, 42, 255), c.L(12));
    DPad(c, 133, 183, 128, IM_COL32(18, 18, 20, 255), IM_COL32(48, 47, 52, 255));
    // B y A en sus pozos grises.
    for (int i = 0; i < 2; ++i) {
        const float x = i ? 572 : 484;
        dl->AddRectFilled(c.P(x - 38, 168), c.P(x + 38, 244), IM_COL32(176, 174, 171, 255), c.L(10));
    }
    RoundButton(c, RETRO_DEVICE_ID_JOYPAD_B, 484, 206, 30, IM_COL32(206, 34, 44, 255), IM_COL32(230, 82, 90, 255));
    RoundButton(c, RETRO_DEVICE_ID_JOYPAD_A, 572, 206, 30, IM_COL32(206, 34, 44, 255), IM_COL32(230, 82, 90, 255));
    c.Text("B", 525, 262, 15, IM_COL32(214, 40, 48, 255));
    c.Text("A", 613, 262, 15, IM_COL32(214, 40, 48, 255));
}

// ---------------------------------------------------------------- Game Boy (diseño 300 × 490)
void DrawGameBoy(Ctx& c) {
    ImDrawList* dl = c.dl;
    auto body = [&](float dx, float dy, ImU32 col) {
        // Esquina inferior derecha bien redondeada, como la original.
        dl->PathClear();
        dl->PathArcTo(c.P(14 + dx, 14 + dy), c.L(14), 3.14159f, 3.14159f * 1.5f);
        dl->PathArcTo(c.P(286 + dx, 14 + dy), c.L(14), 3.14159f * 1.5f, 3.14159f * 2.0f);
        dl->PathArcTo(c.P(230 + dx, 434 + dy), c.L(56), 0, 3.14159f * 0.5f);
        dl->PathArcTo(c.P(14 + dx, 476 + dy), c.L(14), 3.14159f * 0.5f, 3.14159f);
        dl->PathFillConvex(col);
    };
    body(5, 10, IM_COL32(0, 0, 0, 80));
    body(0, 0, IM_COL32(200, 197, 189, 255));
    for (int i = 0; i < 2; ++i) dl->AddLine(c.P(16, 22 + i * 6), c.P(284, 22 + i * 6), IM_COL32(170, 167, 160, 255), c.L(2));
    // Marco de la pantalla (esquina inferior derecha redondeada) y la pantalla verde.
    dl->PathClear();
    dl->PathLineTo(c.P(28, 44));
    dl->PathLineTo(c.P(272, 44));
    dl->PathArcTo(c.P(250, 210), c.L(22), 0, 3.14159f * 0.5f);
    dl->PathLineTo(c.P(28, 232));
    dl->PathFillConvex(IM_COL32(88, 86, 102, 255));
    dl->AddRectFilled(c.P(72, 66), c.P(232, 210), IM_COL32(139, 172, 15, 255), c.L(2));
    dl->AddRectFilledMultiColor(c.P(72, 66), c.P(232, 210), IM_COL32(155, 188, 15, 255), IM_COL32(139, 172, 15, 255),
                                IM_COL32(110, 145, 12, 255), IM_COL32(120, 155, 14, 255));
    dl->AddCircleFilled(c.P(48, 122), c.L(5), IM_COL32(220, 40, 40, 255));
    // Cruceta.
    DPad(c, 76, 318, 78, IM_COL32(24, 24, 28, 255), IM_COL32(56, 56, 62, 255));
    // B y A inclinados.
    RoundButton(c, RETRO_DEVICE_ID_JOYPAD_B, 194, 334, 20, IM_COL32(156, 37, 89, 255), IM_COL32(186, 70, 120, 255));
    RoundButton(c, RETRO_DEVICE_ID_JOYPAD_A, 252, 306, 20, IM_COL32(156, 37, 89, 255), IM_COL32(186, 70, 120, 255));
    c.Text("B", 194, 366, 13, IM_COL32(60, 60, 140, 255));
    c.Text("A", 252, 338, 13, IM_COL32(60, 60, 140, 255));
    // Select y Start inclinados.
    PillButton(c, RETRO_DEVICE_ID_JOYPAD_SELECT, 118, 404, 30, 9, -25, IM_COL32(128, 126, 134, 255));
    PillButton(c, RETRO_DEVICE_ID_JOYPAD_START, 168, 404, 30, 9, -25, IM_COL32(128, 126, 134, 255));
    c.Text("SELECT", 116, 428, 9, IM_COL32(60, 60, 140, 255));
    c.Text("START", 168, 428, 9, IM_COL32(60, 60, 140, 255));
    // Parlante: ranuras en diagonal.
    for (int i = 0; i < 6; ++i) {
        const float x = 210 + i * 12;
        dl->AddLine(c.P(x, 470), c.P(x + 24, 410), IM_COL32(150, 147, 140, 255), c.L(5));
    }
}

// ---------------------------------------------------------------- Game Boy Advance (diseño 560 × 320)
void DrawGba(Ctx& c) {
    ImDrawList* dl = c.dl;
    const ImU32 shell = IM_COL32(90, 74, 176, 255), shell_dark = IM_COL32(66, 52, 140, 255);
    // Gatillos L y R.
    auto shoulder = [&](int id, bool right) {
        const float x0 = right ? 380 : 30, x1 = right ? 530 : 180;
        c.Hit(id, (x0 + x1) * 0.5f, 22, 50);
        const ImU32 col = c.Down(id) ? U32(Color::Gold) : IM_COL32(150, 140, 200, 255);
        dl->AddRectFilled(c.P(x0, -6), c.P(x1, 44), col, c.L(18), right ? ImDrawFlags_RoundTopRight : ImDrawFlags_RoundTopLeft);
        if (c.hovered == id || c.capturing == id)
            dl->AddRect(c.P(x0 - 3, -9), c.P(x1 + 3, 34), U32(Color::Accent, c.capturing == id ? 0.4f + 0.6f * c.pulse : 0.9f), c.L(18), c.L(2.5f));
    };
    shoulder(RETRO_DEVICE_ID_JOYPAD_L, false);
    shoulder(RETRO_DEVICE_ID_JOYPAD_R, true);
    // Cuerpo con los extremos redondos.
    dl->AddRectFilled(c.P(6, 40), c.P(566, 320), IM_COL32(0, 0, 0, 80), c.L(130));
    dl->AddRectFilled(c.P(0, 30), c.P(560, 310), shell, c.L(130));
    dl->AddRectFilled(c.P(150, 30), c.P(410, 310), shell_dark, c.L(28));
    c.Text("L", 105, 12, 18, c.Down(RETRO_DEVICE_ID_JOYPAD_L) ? IM_COL32(60, 40, 0, 255) : IM_COL32(50, 40, 100, 255));
    c.Text("R", 455, 12, 18, c.Down(RETRO_DEVICE_ID_JOYPAD_R) ? IM_COL32(60, 40, 0, 255) : IM_COL32(50, 40, 100, 255));
    // Pantalla.
    dl->AddRectFilled(c.P(166, 58), c.P(394, 236), IM_COL32(40, 38, 52, 255), c.L(14));
    dl->AddRectFilledMultiColor(c.P(190, 78), c.P(370, 198), IM_COL32(60, 70, 80, 255), IM_COL32(50, 58, 70, 255),
                                IM_COL32(32, 38, 48, 255), IM_COL32(40, 46, 58, 255));
    // Cruceta.
    DPad(c, 92, 140, 84, IM_COL32(40, 34, 70, 255), IM_COL32(70, 62, 110, 255));
    // A y B.
    RoundButton(c, RETRO_DEVICE_ID_JOYPAD_A, 482, 122, 22, IM_COL32(170, 160, 220, 255), IM_COL32(200, 192, 240, 255));
    RoundButton(c, RETRO_DEVICE_ID_JOYPAD_B, 436, 160, 22, IM_COL32(170, 160, 220, 255), IM_COL32(200, 192, 240, 255));
    c.Text("A", 482, 156, 13, IM_COL32(210, 205, 240, 255));
    c.Text("B", 436, 194, 13, IM_COL32(210, 205, 240, 255));
    // Start y Select: botones redondos pequeños abajo a la izquierda.
    RoundButton(c, RETRO_DEVICE_ID_JOYPAD_START, 108, 232, 12, IM_COL32(40, 34, 70, 255), IM_COL32(70, 62, 110, 255));
    RoundButton(c, RETRO_DEVICE_ID_JOYPAD_SELECT, 108, 272, 12, IM_COL32(40, 34, 70, 255), IM_COL32(70, 62, 110, 255));
    c.Text("START", 128, 232, 12, IM_COL32(210, 205, 240, 255), false);
    c.Text("SELECT", 128, 272, 12, IM_COL32(210, 205, 240, 255), false);
    // Parlante.
    for (int r = 0; r < 3; ++r)
        for (int i = 0; i < 3; ++i) dl->AddCircleFilled(c.P(462 + i * 14, 236 + r * 14), c.L(3), shell_dark);
}

struct Design {
    float w, h;
};
Design DesignOf(Layout l) {
    switch (l) {
    case Layout::GameBoy: return {300, 490};
    case Layout::GameBoyAdvance: return {572, 336};
    default: return {692, 318};
    }
}

} // namespace

float ControllerAspect(Layout layout) {
    const Design d = DesignOf(layout);
    return d.w / d.h;
}

int DrawController(ImDrawList* dl, Layout layout, ImVec2 pos, ImVec2 size, uint16_t pressed, int capturing, ImVec2 mouse) {
    const Design d = DesignOf(layout);
    const float k = std::min(size.x / d.w, size.y / d.h);
    Ctx c{dl, {pos.x + (size.x - d.w * k) * 0.5f, pos.y + (size.y - d.h * k) * 0.5f}, k, pressed, capturing, mouse};
    c.pulse = 0.5f + 0.5f * static_cast<float>(std::sin(ImGui::GetTime() * 7.0));
    if (layout == Layout::GameBoyAdvance) c.o.y += 10 * k; // los gatillos sobresalen por arriba
    // Cada botón revisa el mouse justo antes de dibujarse, así su resaltado sale en la misma pasada.
    switch (layout) {
    case Layout::GameBoy: DrawGameBoy(c); break;
    case Layout::GameBoyAdvance: DrawGba(c); break;
    default: DrawNes(c); break;
    }
    return c.hovered;
}

} // namespace Satronho::Ui
