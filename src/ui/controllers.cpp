// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <cstdio>
#include "ui/controllers.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "libretro.h"
#include "ui/kit.h"

namespace Satronho::Ui {

namespace {

float g_sticks[4] = {};

struct Ctx {
    ImDrawList* dl;
    ImVec2 o;      // origen
    float k;       // escala de unidades de diseño a píxeles
    uint16_t pressed;
    int capturing;
    ImVec2 mouse;
    int hovered = -1;
    float pulse;
    bool mini = false;

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
        if (mini) return;
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
        // Brazo completo, desde el centro hacia afuera (antes arriba e izquierda medían cero y no se encendían).
        const ImVec2 r0 = c.P(arm.dx ? (arm.dx < 0 ? cx - a : cx + w) : cx - w, arm.dy ? (arm.dy < 0 ? cy - a : cy + w) : cy - w);
        const ImVec2 r1 = c.P(arm.dx ? (arm.dx < 0 ? cx - w : cx + a) : cx + w, arm.dy ? (arm.dy < 0 ? cy - w : cy + a) : cy + w);
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

// Letra impresa sobre un botón redondo.
void ButtonLetter(Ctx& c, int id, const char* t, float x, float y, float size, ImU32 col) {
    c.Text(t, x, y, size, c.Down(id) ? IM_COL32(70, 50, 0, 255) : col);
}

// ---------------------------------------------------------------- Mega Drive / Genesis, mando de 6 botones (diseño 640 × 340)
void DrawGenesis(Ctx& c) {
    ImDrawList* dl = c.dl;
    const ImU32 shell = IM_COL32(36, 35, 41, 255), rim = IM_COL32(58, 57, 66, 255);
    auto body = [&](float dx, float dy, ImU32 col) {
        // Forma de riñón: dos lóbulos redondos unidos por el centro.
        dl->AddCircleFilled(c.P(170 + dx, 182 + dy), c.L(150), col, 64);
        dl->AddCircleFilled(c.P(470 + dx, 182 + dy), c.L(150), col, 64);
        dl->AddRectFilled(c.P(170 + dx, 58 + dy), c.P(470 + dx, 318 + dy), col, c.L(40));
    };
    // Botón Mode en el borde de arriba, a la derecha.
    {
        const int id = RETRO_DEVICE_ID_JOYPAD_SELECT;
        c.Hit(id, 520, 30, 30);
        dl->AddRectFilled(c.P(496, 18), c.P(544, 44), c.Down(id) ? U32(Color::Gold) : IM_COL32(70, 69, 78, 255), c.L(8));
        if (c.hovered == id || c.capturing == id)
            dl->AddRect(c.P(490, 12), c.P(550, 50), U32(Color::Accent, c.capturing == id ? 0.4f + 0.6f * c.pulse : 0.9f), c.L(10), c.L(2.5f));
    }
    body(5, 10, IM_COL32(0, 0, 0, 90));
    body(0, 0, rim);
    // Borde un poco más claro y el frente.
    dl->AddCircleFilled(c.P(170, 182), c.L(142), shell, 64);
    dl->AddCircleFilled(c.P(470, 182), c.L(142), shell, 64);
    dl->AddRectFilled(c.P(170, 66), c.P(470, 310), shell, c.L(36));
    c.Text("MODE", 520, 66, 12, IM_COL32(150, 148, 160, 255));
    // Cruceta sobre su disco.
    dl->AddCircleFilled(c.P(170, 186), c.L(82), IM_COL32(26, 25, 30, 255), 48);
    dl->AddCircle(c.P(170, 186), c.L(82), IM_COL32(70, 69, 78, 255), 48, c.L(2));
    DPad(c, 170, 186, 132, IM_COL32(52, 51, 59, 255), IM_COL32(80, 79, 90, 255));
    // Start al centro.
    {
        const int id = RETRO_DEVICE_ID_JOYPAD_START;
        c.Hit(id, 320, 150, 34);
        dl->AddRectFilled(c.P(296 + 2, 140 + 3), c.P(344 + 2, 160 + 3), IM_COL32(0, 0, 0, 80), c.L(10));
        dl->AddRectFilled(c.P(296, 140), c.P(344, 160), c.Down(id) ? U32(Color::Gold) : IM_COL32(64, 63, 72, 255), c.L(10));
        if (c.hovered == id || c.capturing == id)
            dl->AddRect(c.P(288, 132), c.P(352, 168), U32(Color::Accent, c.capturing == id ? 0.4f + 0.6f * c.pulse : 0.9f), c.L(14), c.L(2.5f));
        c.Text("START", 320, 182, 12, IM_COL32(150, 148, 160, 255));
    }
    // X Y Z arriba (chicos) y A B C abajo (grandes), en diagonal hacia arriba.
    const ImU32 btn = IM_COL32(62, 61, 70, 255), btn_hi = IM_COL32(92, 91, 104, 255), letter = IM_COL32(190, 188, 200, 255);
    struct Btn {
        int id;
        const char* t;
        float x, y, r;
    } row[] = {{RETRO_DEVICE_ID_JOYPAD_Y, "X", 404, 132, 19}, {RETRO_DEVICE_ID_JOYPAD_X, "Y", 462, 116, 19},
               {RETRO_DEVICE_ID_JOYPAD_L, "Z", 520, 100, 19}, {RETRO_DEVICE_ID_JOYPAD_B, "A", 398, 214, 28},
               {RETRO_DEVICE_ID_JOYPAD_A, "B", 466, 196, 28}, {RETRO_DEVICE_ID_JOYPAD_R, "C", 534, 178, 28}};
    for (const auto& b : row) {
        RoundButton(c, b.id, b.x, b.y, b.r, btn, btn_hi);
        ButtonLetter(c, b.id, b.t, b.x, b.y, b.r * 0.85f, letter);
    }
}

// ---------------------------------------------------------------- SNES / Super Famicom (diseño 640 × 316)
void DrawSnes(Ctx& c) {
    ImDrawList* dl = c.dl;
    const ImU32 shell = IM_COL32(200, 199, 208, 255), shell_dark = IM_COL32(176, 175, 186, 255);
    auto shoulder = [&](int id, bool right) {
        const float x0 = right ? 404 : 56, x1 = right ? 584 : 236;
        c.Hit(id, (x0 + x1) * 0.5f, 18, 46);
        dl->AddRectFilled(c.P(x0, -8), c.P(x1, 60), c.Down(id) ? U32(Color::Gold) : IM_COL32(150, 149, 160, 255), c.L(24),
                          right ? ImDrawFlags_RoundTopRight : ImDrawFlags_RoundTopLeft);
        if (c.hovered == id || c.capturing == id)
            dl->AddRect(c.P(x0 - 3, -11), c.P(x1 + 3, 40), U32(Color::Accent, c.capturing == id ? 0.4f + 0.6f * c.pulse : 0.9f), c.L(22), c.L(2.5f));
        c.Text(right ? "R" : "L", (x0 + x1) * 0.5f, 10, 16, c.Down(id) ? IM_COL32(60, 40, 0, 255) : IM_COL32(90, 89, 100, 255));
    };
    shoulder(RETRO_DEVICE_ID_JOYPAD_L, false);
    shoulder(RETRO_DEVICE_ID_JOYPAD_R, true);
    auto body = [&](float dx, float dy, ImU32 col) {
        // Forma de hueso: dos extremos redondos y el centro un poco más angosto.
        dl->AddCircleFilled(c.P(160 + dx, 168 + dy), c.L(140), col, 64);
        dl->AddCircleFilled(c.P(480 + dx, 168 + dy), c.L(140), col, 64);
        dl->AddRectFilled(c.P(160 + dx, 50 + dy), c.P(480 + dx, 286 + dy), col);
    };
    body(5, 10, IM_COL32(0, 0, 0, 80));
    body(0, 0, shell);
    // Cruceta en su hueco.
    dl->AddCircleFilled(c.P(160, 168), c.L(76), shell_dark, 48);
    DPad(c, 160, 168, 120, IM_COL32(64, 64, 72, 255), IM_COL32(92, 92, 102, 255));
    // Select y Start inclinados al centro.
    PillButton(c, RETRO_DEVICE_ID_JOYPAD_SELECT, 278, 192, 42, 14, -28, IM_COL32(118, 117, 128, 255));
    PillButton(c, RETRO_DEVICE_ID_JOYPAD_START, 340, 192, 42, 14, -28, IM_COL32(118, 117, 128, 255));
    c.Text("SELECT", 274, 228, 11, IM_COL32(110, 109, 122, 255));
    c.Text("START", 338, 228, 11, IM_COL32(110, 109, 122, 255));
    // Los cuatro botones en rombo sobre su hueco más oscuro.
    dl->AddCircleFilled(c.P(482, 168), c.L(100), shell_dark, 64);
    struct Btn {
        int id;
        const char* t;
        float x, y;
        ImU32 col, hi;
    } btns[] = {{RETRO_DEVICE_ID_JOYPAD_X, "X", 480, 104, IM_COL32(70, 98, 196, 255), IM_COL32(110, 136, 226, 255)},
                {RETRO_DEVICE_ID_JOYPAD_Y, "Y", 416, 168, IM_COL32(48, 150, 92, 255), IM_COL32(90, 186, 128, 255)},
                {RETRO_DEVICE_ID_JOYPAD_A, "A", 544, 168, IM_COL32(204, 52, 60, 255), IM_COL32(232, 98, 104, 255)},
                {RETRO_DEVICE_ID_JOYPAD_B, "B", 480, 232, IM_COL32(222, 178, 40, 255), IM_COL32(244, 210, 96, 255)}};
    for (const auto& b : btns) {
        RoundButton(c, b.id, b.x, b.y, 28, b.col, b.hi);
        ButtonLetter(c, b.id, b.t, b.x, b.y, 22, IM_COL32(255, 255, 255, 220));
    }
}

// ---------------------------------------------------------------- PlayStation, mando con dos sticks (diseño 660 × 380)
void DrawPlayStation(Ctx& c) {
    ImDrawList* dl = c.dl;
    const ImU32 shell = IM_COL32(190, 192, 201, 255), shell_dark = IM_COL32(160, 162, 173, 255), ink = IM_COL32(110, 112, 124, 255);
    auto pulse_ring = [&](int id, ImVec2 a, ImVec2 b, float r) {
        if (c.hovered == id || c.capturing == id)
            dl->AddRect({a.x - c.L(3), a.y - c.L(3)}, {b.x + c.L(3), b.y + c.L(3)},
                        U32(Color::Accent, c.capturing == id ? 0.4f + 0.6f * c.pulse : 0.9f), c.L(r), c.L(2.5f));
    };
    // Gatillos: L2/R2 atrás (más arriba) y L1/R1 adelante.
    auto trigger = [&](int id, float x0, float x1, float y0, float y1, const char* t, ImU32 col) {
        c.Hit(id, (x0 + x1) * 0.5f, (y0 + y1) * 0.5f - 4, 36);
        const ImVec2 a = c.P(x0, y0), b = c.P(x1, y1);
        dl->AddRectFilled(a, b, c.Down(id) ? U32(Color::Gold) : col, c.L(12), ImDrawFlags_RoundTop);
        pulse_ring(id, a, b, 12);
        c.Text(t, (x0 + x1) * 0.5f, y0 + 13, 13, c.Down(id) ? IM_COL32(60, 40, 0, 255) : IM_COL32(80, 82, 94, 255));
    };
    trigger(RETRO_DEVICE_ID_JOYPAD_L2, 100, 230, 0, 52, "L2", IM_COL32(140, 142, 154, 255));
    trigger(RETRO_DEVICE_ID_JOYPAD_R2, 430, 560, 0, 52, "R2", IM_COL32(140, 142, 154, 255));
    trigger(RETRO_DEVICE_ID_JOYPAD_L, 104, 226, 30, 80, "L1", IM_COL32(165, 167, 178, 255));
    trigger(RETRO_DEVICE_ID_JOYPAD_R, 434, 556, 30, 80, "R1", IM_COL32(165, 167, 178, 255));
    // Cuerpo: centro y dos agarraderas que bajan.
    auto body = [&](float dx, float dy, ImU32 col) {
        dl->AddRectFilled(c.P(120 + dx, 62 + dy), c.P(540 + dx, 250 + dy), col, c.L(60));
        for (int side = 0; side < 2; ++side) {
            const float x0 = side ? 512 : 148, x1 = side ? 560 : 100;
            for (int i = 0; i <= 10; ++i) {
                const float t = i / 10.0f;
                dl->AddCircleFilled(c.P(x0 + (x1 - x0) * t + dx, 165 + 150 * t + dy), c.L(108 - 44 * t), col, 40);
            }
        }
    };
    body(5, 10, IM_COL32(0, 0, 0, 80));
    body(0, 0, shell);
    // Cruceta en su hueco redondo.
    dl->AddCircleFilled(c.P(150, 160), c.L(66), shell_dark, 48);
    DPad(c, 150, 160, 104, IM_COL32(205, 207, 215, 255), IM_COL32(150, 152, 163, 255));
    // Select y Start.
    auto small = [&](int id, float x, const char* t, bool tri) {
        c.Hit(id, x, 150, 24);
        const ImU32 col = c.Down(id) ? U32(Color::Gold) : IM_COL32(96, 98, 110, 255);
        if (tri) dl->AddTriangleFilled(c.P(x - 16, 141), c.P(x - 16, 159), c.P(x + 16, 150), col);
        else dl->AddRectFilled(c.P(x - 16, 143), c.P(x + 16, 157), col, c.L(3));
        if (c.hovered == id || c.capturing == id)
            dl->AddRect(c.P(x - 24, 134), c.P(x + 24, 166), U32(Color::Accent, c.capturing == id ? 0.4f + 0.6f * c.pulse : 0.9f), c.L(8), c.L(2.5f));
        c.Text(t, x, 176, 11, ink);
    };
    small(RETRO_DEVICE_ID_JOYPAD_SELECT, 290, "SELECT", false);
    small(RETRO_DEVICE_ID_JOYPAD_START, 370, "START", true);
    // Botones de acción con sus figuras de colores.
    dl->AddCircleFilled(c.P(510, 160), c.L(84), shell_dark, 48);
    const ImU32 btn = IM_COL32(58, 58, 66, 255), btn_hi = IM_COL32(84, 84, 94, 255);
    struct Face {
        int id;
        float x, y;
    } faces[] = {{RETRO_DEVICE_ID_JOYPAD_X, 510, 110}, {RETRO_DEVICE_ID_JOYPAD_Y, 460, 160},
                 {RETRO_DEVICE_ID_JOYPAD_A, 560, 160}, {RETRO_DEVICE_ID_JOYPAD_B, 510, 210}};
    for (const auto& f : faces) {
        RoundButton(c, f.id, f.x, f.y, 24, btn, btn_hi);
        const bool down = c.Down(f.id);
        const float t = c.L(2.5f);
        const ImVec2 m = c.P(f.x, f.y);
        const float r = c.L(10);
        switch (f.id) {
        case RETRO_DEVICE_ID_JOYPAD_X: // triángulo
            dl->AddTriangle({m.x, m.y - r}, {m.x - r * 1.05f, m.y + r * 0.75f}, {m.x + r * 1.05f, m.y + r * 0.75f},
                            down ? IM_COL32(60, 40, 0, 255) : IM_COL32(64, 200, 150, 255), t);
            break;
        case RETRO_DEVICE_ID_JOYPAD_A: // círculo
            dl->AddCircle(m, r, down ? IM_COL32(60, 40, 0, 255) : IM_COL32(240, 90, 96, 255), 24, t);
            break;
        case RETRO_DEVICE_ID_JOYPAD_B: // cruz
            dl->AddLine({m.x - r, m.y - r}, {m.x + r, m.y + r}, down ? IM_COL32(60, 40, 0, 255) : IM_COL32(110, 150, 240, 255), t);
            dl->AddLine({m.x + r, m.y - r}, {m.x - r, m.y + r}, down ? IM_COL32(60, 40, 0, 255) : IM_COL32(110, 150, 240, 255), t);
            break;
        default: // cuadrado
            dl->AddRect({m.x - r * 0.85f, m.y - r * 0.85f}, {m.x + r * 0.85f, m.y + r * 0.85f},
                        down ? IM_COL32(60, 40, 0, 255) : IM_COL32(236, 130, 200, 255), 0, t);
            break;
        }
    }
    // Sticks (L3 y R3 al hundirlos).
    for (int side = 0; side < 2; ++side) {
        const int id = side ? RETRO_DEVICE_ID_JOYPAD_R3 : RETRO_DEVICE_ID_JOYPAD_L3;
        const float x = side ? 420 : 240, y = 250;
        c.Hit(id, x, y, 40);
        dl->AddCircleFilled(c.P(x, y), c.L(44), IM_COL32(70, 72, 82, 255), 40);
        // La palanca se mueve con la del mando real.
        const float sx = c.mini ? 0 : g_sticks[side * 2] * 11, sy = c.mini ? 0 : g_sticks[side * 2 + 1] * 11;
        const bool moved = sx * sx + sy * sy > 9;
        dl->AddCircleFilled(c.P(x + sx + 2, y + sy + 4), c.L(31), IM_COL32(0, 0, 0, 80), 32);
        dl->AddCircleFilled(c.P(x + sx, y + sy), c.L(31), c.Down(id) ? U32(Color::Gold) : moved ? U32(Color::Accent) : IM_COL32(44, 45, 52, 255), 32);
        dl->AddCircle(c.P(x + sx, y + sy), c.L(24), IM_COL32(70, 72, 82, 255), 32, c.L(2));
        c.Ring(id, c.P(x + sx, y + sy), c.L(31));
    }
}

// ---------------------------------------------------------------- Nintendo 64, mando de tres agarraderas (diseño 640 × 420)
void DrawN64(Ctx& c) {
    ImDrawList* dl = c.dl;
    const ImU32 shell = IM_COL32(150, 152, 162, 255), shell_dark = IM_COL32(120, 122, 134, 255);
    auto pulse_ring = [&](int id, ImVec2 a, ImVec2 b, float r) {
        if (c.hovered == id || c.capturing == id)
            dl->AddRect({a.x - c.L(3), a.y - c.L(3)}, {b.x + c.L(3), b.y + c.L(3)},
                        U32(Color::Accent, c.capturing == id ? 0.4f + 0.6f * c.pulse : 0.9f), c.L(r), c.L(2.5f));
    };
    // Gatillos L y R arriba, en las esquinas.
    auto shoulder = [&](int id, float x0, float x1, const char* t) {
        c.Hit(id, (x0 + x1) * 0.5f, 26, 36);
        const ImVec2 a = c.P(x0, 6), b = c.P(x1, 52);
        dl->AddRectFilled(a, b, c.Down(id) ? U32(Color::Gold) : IM_COL32(118, 120, 132, 255), c.L(16), ImDrawFlags_RoundTop);
        pulse_ring(id, a, b, 16);
        c.Text(t, (x0 + x1) * 0.5f, 22, 14, c.Down(id) ? IM_COL32(60, 40, 0, 255) : IM_COL32(60, 62, 74, 255));
    };
    shoulder(RETRO_DEVICE_ID_JOYPAD_SELECT, 70, 200, "L");
    shoulder(RETRO_DEVICE_ID_JOYPAD_R2, 440, 570, "R");
    auto capsule = [&](float x0, float y0, float x1, float y1, float r0, float r1, float dx, float dy, ImU32 col) {
        for (int i = 0; i <= 12; ++i) {
            const float t = i / 12.0f;
            dl->AddCircleFilled(c.P(x0 + (x1 - x0) * t + dx, y0 + (y1 - y0) * t + dy), c.L(r0 + (r1 - r0) * t), col, 36);
        }
    };
    auto body = [&](float dx, float dy, ImU32 col) {
        dl->AddRectFilled(c.P(40 + dx, 40 + dy), c.P(600 + dx, 190 + dy), col, c.L(70));
        capsule(130, 150, 116, 340, 88, 44, dx, dy, col);
        capsule(320, 170, 320, 380, 62, 40, dx, dy, col);
        capsule(510, 150, 524, 340, 88, 44, dx, dy, col);
    };
    body(5, 10, IM_COL32(0, 0, 0, 80));
    body(0, 0, shell);
    // Cruceta a la izquierda.
    dl->AddCircleFilled(c.P(130, 120), c.L(60), shell_dark, 40);
    DPad(c, 130, 120, 96, IM_COL32(58, 58, 66, 255), IM_COL32(88, 88, 98, 255));
    // Start rojo al centro.
    RoundButton(c, RETRO_DEVICE_ID_JOYPAD_START, 320, 100, 17, IM_COL32(206, 48, 54, 255), IM_COL32(236, 100, 104, 255));
    c.Text("START", 320, 136, 11, IM_COL32(70, 72, 84, 255));
    // Stick al centro y el gatillo Z debajo (se ve como una pestaña en la agarradera).
    dl->AddCircleFilled(c.P(320, 214), c.L(42), shell_dark, 40);
    {
        const float sx = c.mini ? 0 : g_sticks[0] * 10, sy = c.mini ? 0 : g_sticks[1] * 10;
        const bool moved = sx * sx + sy * sy > 9;
        dl->AddCircleFilled(c.P(320 + sx, 214 + sy), c.L(26), IM_COL32(80, 82, 94, 255), 32);
        dl->AddCircleFilled(c.P(320 + sx, 210 + sy), c.L(20), moved ? U32(Color::Accent) : IM_COL32(104, 106, 118, 255), 32);
    }
    {
        const int id = RETRO_DEVICE_ID_JOYPAD_L2;
        c.Hit(id, 320, 330, 34);
        const ImVec2 a = c.P(296, 304), b = c.P(344, 356);
        dl->AddRectFilled(a, b, c.Down(id) ? U32(Color::Gold) : IM_COL32(74, 76, 88, 255), c.L(12));
        pulse_ring(id, a, b, 12);
        c.Text("Z", 320, 330, 16, c.Down(id) ? IM_COL32(60, 40, 0, 255) : IM_COL32(190, 192, 202, 255));
    }
    // A (azul) y B (verde) a la derecha, y los cuatro C amarillos arriba.
    RoundButton(c, RETRO_DEVICE_ID_JOYPAD_B, 470, 178, 24, IM_COL32(58, 98, 210, 255), IM_COL32(104, 140, 236, 255));
    ButtonLetter(c, RETRO_DEVICE_ID_JOYPAD_B, "A", 470, 178, 18, IM_COL32(255, 255, 255, 220));
    RoundButton(c, RETRO_DEVICE_ID_JOYPAD_Y, 424, 140, 22, IM_COL32(52, 160, 90, 255), IM_COL32(96, 196, 130, 255));
    ButtonLetter(c, RETRO_DEVICE_ID_JOYPAD_Y, "B", 424, 140, 17, IM_COL32(255, 255, 255, 220));
    struct Cb {
        int id;
        float x, y;
        float ax, ay;
    } cbs[] = {{RETRO_DEVICE_ID_JOYPAD_X, 540, 74, 0, -1}, {RETRO_DEVICE_ID_JOYPAD_A, 540, 140, 0, 1},
               {RETRO_DEVICE_ID_JOYPAD_L, 507, 107, -1, 0}, {RETRO_DEVICE_ID_JOYPAD_R, 573, 107, 1, 0}};
    for (const auto& cb : cbs) {
        RoundButton(c, cb.id, cb.x, cb.y, 15, IM_COL32(236, 190, 40, 255), IM_COL32(250, 220, 110, 255));
        const ImVec2 m = c.P(cb.x, cb.y);
        const float r = c.L(6);
        const ImU32 col = c.Down(cb.id) ? IM_COL32(60, 40, 0, 255) : IM_COL32(120, 84, 0, 255);
        // Flechita hacia donde apunta.
        dl->AddTriangleFilled({m.x + cb.ax * r, m.y + cb.ay * r}, {m.x - cb.ax * r * 0.4f + cb.ay * r * 0.8f, m.y - cb.ay * r * 0.4f + cb.ax * r * 0.8f},
                              {m.x - cb.ax * r * 0.4f - cb.ay * r * 0.8f, m.y - cb.ay * r * 0.4f - cb.ax * r * 0.8f}, col);
    }
}

// ---------------------------------------------------------------- Nintendo DS abierta (diseño 460 × 560)
void DrawDs(Ctx& c) {
    ImDrawList* dl = c.dl;
    const ImU32 shell = IM_COL32(64, 70, 86, 255), shell_hi = IM_COL32(82, 90, 108, 255);
    // Gatillos en las esquinas de la bisagra.
    auto shoulder = [&](int id, bool right) {
        const float x0 = right ? 340 : 20, x1 = right ? 440 : 120;
        c.Hit(id, (x0 + x1) * 0.5f, 276, 34);
        const ImU32 col = c.Down(id) ? U32(Color::Gold) : IM_COL32(110, 118, 138, 255);
        dl->AddRectFilled(c.P(x0, 262), c.P(x1, 292), col, c.L(10));
        if (c.hovered == id || c.capturing == id)
            dl->AddRect(c.P(x0 - 3, 259), c.P(x1 + 3, 295), U32(Color::Accent, c.capturing == id ? 0.4f + 0.6f * c.pulse : 0.9f), c.L(12), c.L(2.5f));
        c.Text(right ? "R" : "L", (x0 + x1) * 0.5f, 277, 14, c.Down(id) ? IM_COL32(60, 40, 0, 255) : IM_COL32(220, 224, 236, 255));
    };
    // Tapa con la pantalla de arriba.
    dl->AddRectFilled(c.P(36, 16), c.P(430, 266), IM_COL32(0, 0, 0, 80), c.L(22));
    dl->AddRectFilled(c.P(30, 6), c.P(424, 256), shell_hi, c.L(22));
    dl->AddRectFilled(c.P(110, 40), c.P(344, 222), IM_COL32(30, 32, 42, 255), c.L(8));
    dl->AddRectFilledMultiColor(c.P(124, 52), c.P(330, 208), IM_COL32(60, 78, 100, 255), IM_COL32(50, 66, 88, 255),
                                IM_COL32(34, 44, 60, 255), IM_COL32(42, 54, 72, 255));
    shoulder(RETRO_DEVICE_ID_JOYPAD_L, false);
    shoulder(RETRO_DEVICE_ID_JOYPAD_R, true);
    // Base con la pantalla táctil.
    dl->AddRectFilled(c.P(26, 300), c.P(436, 556), IM_COL32(0, 0, 0, 80), c.L(22));
    dl->AddRectFilled(c.P(20, 290), c.P(430, 546), shell, c.L(22));
    dl->AddRectFilled(c.P(118, 312), c.P(336, 492), IM_COL32(26, 28, 36, 255), c.L(6));
    dl->AddRectFilledMultiColor(c.P(128, 322), c.P(326, 482), IM_COL32(70, 92, 116, 255), IM_COL32(58, 78, 102, 255),
                                IM_COL32(40, 52, 70, 255), IM_COL32(48, 62, 82, 255));
    c.Text("TÁCTIL", 227, 402, 12, IM_COL32(150, 170, 196, 160));
    // Cruceta y botones.
    DPad(c, 68, 390, 78, IM_COL32(32, 34, 42, 255), IM_COL32(62, 66, 78, 255));
    struct Face {
        int id;
        const char* t;
        float x, y;
    } faces[] = {{RETRO_DEVICE_ID_JOYPAD_X, "X", 388, 354}, {RETRO_DEVICE_ID_JOYPAD_Y, "Y", 358, 388},
                 {RETRO_DEVICE_ID_JOYPAD_A, "A", 418, 388}, {RETRO_DEVICE_ID_JOYPAD_B, "B", 388, 422}};
    for (const auto& f : faces) {
        RoundButton(c, f.id, f.x, f.y, 15, IM_COL32(34, 36, 44, 255), IM_COL32(70, 74, 88, 255));
        ButtonLetter(c, f.id, f.t, f.x, f.y, 12, IM_COL32(200, 206, 222, 255));
    }
    // Start y Select: botoncitos redondos abajo a la derecha.
    RoundButton(c, RETRO_DEVICE_ID_JOYPAD_START, 372, 478, 9, IM_COL32(34, 36, 44, 255), IM_COL32(70, 74, 88, 255));
    RoundButton(c, RETRO_DEVICE_ID_JOYPAD_SELECT, 372, 512, 9, IM_COL32(34, 36, 44, 255), IM_COL32(70, 74, 88, 255));
    c.Text("START", 388, 478, 10, IM_COL32(190, 196, 212, 255), false);
    c.Text("SELECT", 388, 512, 10, IM_COL32(190, 196, 212, 255), false);
}

// ---------------------------------------------------------------- Sega Saturn (diseño 640 × 340)
void DrawSaturn(Ctx& c) {
    ImDrawList* dl = c.dl;
    const ImU32 shell = IM_COL32(52, 52, 60, 255), rim = IM_COL32(72, 72, 82, 255);
    auto shoulder = [&](int id, bool right) {
        const float x0 = right ? 430 : 70, x1 = right ? 570 : 210;
        c.Hit(id, (x0 + x1) * 0.5f, 30, 40);
        const ImU32 col = c.Down(id) ? U32(Color::Gold) : IM_COL32(96, 96, 108, 255);
        dl->AddRectFilled(c.P(x0, 8), c.P(x1, 70), col, c.L(30), right ? ImDrawFlags_RoundTopRight : ImDrawFlags_RoundTopLeft);
        if (c.hovered == id || c.capturing == id)
            dl->AddRect(c.P(x0 - 3, 5), c.P(x1 + 3, 50), U32(Color::Accent, c.capturing == id ? 0.4f + 0.6f * c.pulse : 0.9f), c.L(26), c.L(2.5f));
        c.Text(right ? "R" : "L", (x0 + x1) * 0.5f, 28, 15, c.Down(id) ? IM_COL32(60, 40, 0, 255) : IM_COL32(200, 200, 212, 255));
    };
    shoulder(RETRO_DEVICE_ID_JOYPAD_L2, false);
    shoulder(RETRO_DEVICE_ID_JOYPAD_R2, true);
    // Cuerpo ovalado.
    dl->AddRectFilled(c.P(26, 56), c.P(626, 334), IM_COL32(0, 0, 0, 90), c.L(135));
    dl->AddRectFilled(c.P(20, 46), c.P(620, 324), rim, c.L(135));
    dl->AddRectFilled(c.P(28, 54), c.P(612, 316), shell, c.L(128));
    // Cruceta sobre su disco.
    dl->AddCircleFilled(c.P(160, 186), c.L(76), IM_COL32(36, 36, 42, 255), 48);
    DPad(c, 160, 186, 120, IM_COL32(84, 84, 96, 255), IM_COL32(110, 110, 124, 255));
    // Start al centro.
    RoundButton(c, RETRO_DEVICE_ID_JOYPAD_START, 320, 214, 15, IM_COL32(90, 90, 102, 255), IM_COL32(120, 120, 134, 255));
    c.Text("START", 320, 248, 11, IM_COL32(150, 150, 164, 255));
    // X Y Z arriba (chicos) y A B C abajo (grandes).
    const ImU32 small = IM_COL32(110, 110, 124, 255), small_hi = IM_COL32(140, 140, 156, 255);
    struct Btn {
        int id;
        const char* t;
        float x, y, r;
        ImU32 col, hi;
    } btns[] = {{RETRO_DEVICE_ID_JOYPAD_Y, "X", 418, 140, 18, small, small_hi},
                {RETRO_DEVICE_ID_JOYPAD_X, "Y", 474, 124, 18, small, small_hi},
                {RETRO_DEVICE_ID_JOYPAD_L, "Z", 530, 108, 18, small, small_hi},
                {RETRO_DEVICE_ID_JOYPAD_B, "A", 414, 214, 27, IM_COL32(88, 104, 190, 255), IM_COL32(120, 136, 220, 255)},
                {RETRO_DEVICE_ID_JOYPAD_A, "B", 478, 196, 27, IM_COL32(64, 150, 100, 255), IM_COL32(100, 186, 136, 255)},
                {RETRO_DEVICE_ID_JOYPAD_R, "C", 542, 178, 27, IM_COL32(200, 160, 50, 255), IM_COL32(230, 196, 100, 255)}};
    for (const auto& b : btns) {
        RoundButton(c, b.id, b.x, b.y, b.r, b.col, b.hi);
        ButtonLetter(c, b.id, b.t, b.x, b.y, b.r * 0.8f, IM_COL32(255, 255, 255, 220));
    }
}

struct Design {
    float w, h;
};
Design DesignOf(Layout l) {
    switch (l) {
    case Layout::GameBoy: return {300, 490};
    case Layout::GameBoyAdvance: return {572, 336};
    case Layout::Genesis: return {650, 350};
    case Layout::Snes: return {650, 336};
    case Layout::PlayStation: return {660, 390};
    case Layout::N64: return {640, 420};
    case Layout::DS: return {460, 560};
    case Layout::Saturn: return {640, 340};
    default: return {692, 318};
    }
}

} // namespace

void SetLiveSticks(const float sticks[4]) {
    for (int i = 0; i < 4; ++i) g_sticks[i] = sticks[i];
}

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
    if (layout == Layout::Snes) c.o.y += 12 * k;
    // Cada botón revisa el mouse justo antes de dibujarse, así su resaltado sale en la misma pasada.
    switch (layout) {
    case Layout::GameBoy: DrawGameBoy(c); break;
    case Layout::GameBoyAdvance: DrawGba(c); break;
    case Layout::Genesis: DrawGenesis(c); break;
    case Layout::Snes: DrawSnes(c); break;
    case Layout::PlayStation: DrawPlayStation(c); break;
    case Layout::N64: DrawN64(c); break;
    case Layout::DS: DrawDs(c); break;
    case Layout::Saturn: DrawSaturn(c); break;
    default: DrawNes(c); break;
    }
    return c.hovered;
}

void DrawControllerMini(ImDrawList* dl, Layout layout, ImVec2 center, float width, float alpha) {
    const Design d = DesignOf(layout);
    const float k = std::min(width / d.w, (d.h > d.w ? width * 1.1f : width * 0.62f) / d.h);
    Ctx c{dl, {center.x - d.w * k * 0.5f, center.y - d.h * k * 0.5f}, k, 0, -1, {-1e5f, -1e5f}};
    c.pulse = 0;
    c.mini = true;
    if (layout == Layout::GameBoyAdvance) c.o.y += 10 * k;
    if (layout == Layout::Snes) c.o.y += 12 * k;
    const int start = dl->VtxBuffer.Size;
    switch (layout) {
    case Layout::GameBoy: DrawGameBoy(c); break;
    case Layout::GameBoyAdvance: DrawGba(c); break;
    case Layout::Genesis: DrawGenesis(c); break;
    case Layout::Snes: DrawSnes(c); break;
    case Layout::PlayStation: DrawPlayStation(c); break;
    case Layout::N64: DrawN64(c); break;
    case Layout::DS: DrawDs(c); break;
    case Layout::Saturn: DrawSaturn(c); break;
    default: DrawNes(c); break;
    }
    if (alpha < 1.0f) {
        // Atenuado (sin seleccionar): se baja la opacidad de todo lo que se acaba de dibujar.
        for (int i = start; i < dl->VtxBuffer.Size; ++i) {
            ImU32& col = dl->VtxBuffer[i].col;
            const unsigned a = (col >> IM_COL32_A_SHIFT) & 0xFF;
            col = (col & ~IM_COL32_A_MASK) | (static_cast<ImU32>(a * alpha) << IM_COL32_A_SHIFT);
        }
    }
}

} // namespace Satronho::Ui
