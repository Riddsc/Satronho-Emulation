// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/input.h"

#include <algorithm>
#include <cmath>

#include "util/ini.h"
#include "util/log.h"

namespace Satronho {

const std::vector<PadButton>& NesButtons() {
    static const std::vector<PadButton> buttons{
        {RETRO_DEVICE_ID_JOYPAD_UP, "Arriba"},
        {RETRO_DEVICE_ID_JOYPAD_DOWN, "Abajo"},
        {RETRO_DEVICE_ID_JOYPAD_LEFT, "Izquierda"},
        {RETRO_DEVICE_ID_JOYPAD_RIGHT, "Derecha"},
        {RETRO_DEVICE_ID_JOYPAD_A, "A"},
        {RETRO_DEVICE_ID_JOYPAD_B, "B"},
        {RETRO_DEVICE_ID_JOYPAD_X, "A turbo"},
        {RETRO_DEVICE_ID_JOYPAD_Y, "B turbo"},
        {RETRO_DEVICE_ID_JOYPAD_START, "Start"},
        {RETRO_DEVICE_ID_JOYPAD_SELECT, "Select"},
    };
    return buttons;
}

void Input::Init() {
    ResetDefaults();
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    for (int i = 0; i < count; ++i) {
        if (SDL_Gamepad* pad = SDL_OpenGamepad(ids[i])) pads_.push_back({pad, ids[i]});
    }
    SDL_free(ids);
}

void Input::Shutdown() {
    for (auto& p : pads_) SDL_CloseGamepad(p.pad);
    pads_.clear();
}

void Input::ResetDefaults() {
    keys.fill(SDL_SCANCODE_UNKNOWN);
    pad_buttons.fill(SDL_GAMEPAD_BUTTON_INVALID);
    keys[RETRO_DEVICE_ID_JOYPAD_UP] = SDL_SCANCODE_UP;
    keys[RETRO_DEVICE_ID_JOYPAD_DOWN] = SDL_SCANCODE_DOWN;
    keys[RETRO_DEVICE_ID_JOYPAD_LEFT] = SDL_SCANCODE_LEFT;
    keys[RETRO_DEVICE_ID_JOYPAD_RIGHT] = SDL_SCANCODE_RIGHT;
    keys[RETRO_DEVICE_ID_JOYPAD_A] = SDL_SCANCODE_X;
    keys[RETRO_DEVICE_ID_JOYPAD_B] = SDL_SCANCODE_Z;
    keys[RETRO_DEVICE_ID_JOYPAD_X] = SDL_SCANCODE_S;
    keys[RETRO_DEVICE_ID_JOYPAD_Y] = SDL_SCANCODE_A;
    keys[RETRO_DEVICE_ID_JOYPAD_START] = SDL_SCANCODE_RETURN;
    keys[RETRO_DEVICE_ID_JOYPAD_SELECT] = SDL_SCANCODE_RSHIFT;

    pad_buttons[RETRO_DEVICE_ID_JOYPAD_UP] = SDL_GAMEPAD_BUTTON_DPAD_UP;
    pad_buttons[RETRO_DEVICE_ID_JOYPAD_DOWN] = SDL_GAMEPAD_BUTTON_DPAD_DOWN;
    pad_buttons[RETRO_DEVICE_ID_JOYPAD_LEFT] = SDL_GAMEPAD_BUTTON_DPAD_LEFT;
    pad_buttons[RETRO_DEVICE_ID_JOYPAD_RIGHT] = SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
    // Igual que en la NES: B a la izquierda, A a la derecha.
    pad_buttons[RETRO_DEVICE_ID_JOYPAD_A] = SDL_GAMEPAD_BUTTON_EAST;
    pad_buttons[RETRO_DEVICE_ID_JOYPAD_B] = SDL_GAMEPAD_BUTTON_SOUTH;
    pad_buttons[RETRO_DEVICE_ID_JOYPAD_X] = SDL_GAMEPAD_BUTTON_NORTH;
    pad_buttons[RETRO_DEVICE_ID_JOYPAD_Y] = SDL_GAMEPAD_BUTTON_WEST;
    pad_buttons[RETRO_DEVICE_ID_JOYPAD_START] = SDL_GAMEPAD_BUTTON_START;
    pad_buttons[RETRO_DEVICE_ID_JOYPAD_SELECT] = SDL_GAMEPAD_BUTTON_BACK;
}

void Input::Load(const Ini& ini) {
    for (const auto& b : NesButtons()) {
        const std::string k = "controles.teclado." + std::to_string(b.id);
        const std::string kv = ini.Get(k);
        if (!kv.empty()) {
            SDL_Scancode sc = SDL_GetScancodeFromName(kv.c_str());
            keys[b.id] = sc;
        }
        const std::string pv = ini.Get("controles.mando." + std::to_string(b.id));
        if (!pv.empty()) pad_buttons[b.id] = SDL_GetGamepadButtonFromString(pv.c_str());
    }
    deadzone = std::clamp(ini.GetFloat("controles.zona_muerta", 0.25f), 0.05f, 0.9f);
}

void Input::Save(Ini& ini) const {
    for (const auto& b : NesButtons()) {
        const char* kn = keys[b.id] == SDL_SCANCODE_UNKNOWN ? "" : SDL_GetScancodeName(keys[b.id]);
        ini.Set("controles.teclado." + std::to_string(b.id), kn ? kn : "");
        const char* pn = SDL_GetGamepadStringForButton(pad_buttons[b.id]);
        ini.Set("controles.mando." + std::to_string(b.id), pn ? pn : "");
    }
    ini.SetFloat("controles.zona_muerta", deadzone);
}

void Input::HandleEvent(const SDL_Event& e) {
    switch (e.type) {
    case SDL_EVENT_GAMEPAD_ADDED:
        if (std::none_of(pads_.begin(), pads_.end(), [&](const Open& o) { return o.id == e.gdevice.which; })) {
            if (SDL_Gamepad* pad = SDL_OpenGamepad(e.gdevice.which)) {
                pads_.push_back({pad, e.gdevice.which});
                Log::Info("Mando conectado: %s", SDL_GetGamepadName(pad));
            }
        }
        break;
    case SDL_EVENT_GAMEPAD_REMOVED:
        for (auto it = pads_.begin(); it != pads_.end(); ++it) {
            if (it->id == e.gdevice.which) {
                SDL_CloseGamepad(it->pad);
                pads_.erase(it);
                break;
            }
        }
        break;
    case SDL_EVENT_KEY_DOWN:
        if (capture_button >= 0 && !capture_gamepad) {
            if (e.key.scancode == SDL_SCANCODE_ESCAPE) {
                capture_button = -1;
            } else {
                // Si la tecla ya estaba en otro botón, se quita de ahí.
                for (auto& k : keys)
                    if (k == e.key.scancode) k = SDL_SCANCODE_UNKNOWN;
                keys[capture_button] = e.key.scancode;
                capture_button = -1;
            }
        }
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        if (capture_button >= 0 && capture_gamepad) {
            const auto b = static_cast<SDL_GamepadButton>(e.gbutton.button);
            for (auto& p : pad_buttons)
                if (p == b) p = SDL_GAMEPAD_BUTTON_INVALID;
            pad_buttons[capture_button] = b;
            capture_button = -1;
        } else if (e.gbutton.button == SDL_GAMEPAD_BUTTON_GUIDE) {
            guide_pressed_ = true;
        }
        break;
    default:
        break;
    }
}

void Input::Update(CoreHost& core, bool keyboard_enabled) {
    for (auto& p : core.pads) p = {};
    if (capture_button >= 0) return;

    const bool* ks = SDL_GetKeyboardState(nullptr);
    if (keyboard_enabled) {
        for (const auto& b : NesButtons()) {
            const SDL_Scancode sc = keys[b.id];
            if (sc != SDL_SCANCODE_UNKNOWN && ks[sc]) core.pads[0].buttons |= static_cast<uint16_t>(1u << b.id);
        }
    }

    // Mando 1 se suma al teclado en el jugador 1; el mando 2 es el jugador 2.
    for (size_t i = 0; i < pads_.size() && i < 4; ++i) {
        SDL_Gamepad* pad = pads_[i].pad;
        PadState& st = core.pads[i];
        for (const auto& b : NesButtons()) {
            const SDL_GamepadButton gb = pad_buttons[b.id];
            if (gb != SDL_GAMEPAD_BUTTON_INVALID && SDL_GetGamepadButton(pad, gb))
                st.buttons |= static_cast<uint16_t>(1u << b.id);
        }
        // El stick izquierdo también mueve la cruceta.
        const float lx = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f;
        const float ly = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;
        if (lx < -deadzone) st.buttons |= 1u << RETRO_DEVICE_ID_JOYPAD_LEFT;
        if (lx > deadzone) st.buttons |= 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT;
        if (ly < -deadzone) st.buttons |= 1u << RETRO_DEVICE_ID_JOYPAD_UP;
        if (ly > deadzone) st.buttons |= 1u << RETRO_DEVICE_ID_JOYPAD_DOWN;
        st.analog[0][0] = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX);
        st.analog[0][1] = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY);
    }

    // Turbo: los botones X/Y del RetroPad se convierten en A/B que se repiten 15 veces por segundo.
    const bool phase = (core.FrameSerial() / 2) % 2 == 0;
    for (auto& st : core.pads) {
        if (st.buttons & (1u << RETRO_DEVICE_ID_JOYPAD_X)) {
            st.buttons &= ~(1u << RETRO_DEVICE_ID_JOYPAD_X);
            if (phase) st.buttons |= 1u << RETRO_DEVICE_ID_JOYPAD_A;
        }
        if (st.buttons & (1u << RETRO_DEVICE_ID_JOYPAD_Y)) {
            st.buttons &= ~(1u << RETRO_DEVICE_ID_JOYPAD_Y);
            if (phase) st.buttons |= 1u << RETRO_DEVICE_ID_JOYPAD_B;
        }
    }
}

bool Input::MenuComboPressed() {
    if (guide_pressed_) {
        guide_pressed_ = false;
        return true;
    }
    bool combo = false;
    for (const auto& p : pads_)
        if (SDL_GetGamepadButton(p.pad, SDL_GAMEPAD_BUTTON_BACK) && SDL_GetGamepadButton(p.pad, SDL_GAMEPAD_BUTTON_START))
            combo = true;
    if (combo && !combo_latched_) {
        combo_latched_ = true;
        return true;
    }
    if (!combo) combo_latched_ = false;
    return false;
}

bool Input::FastForwardHeld() const {
    for (const auto& p : pads_)
        if (SDL_GetGamepadAxis(p.pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 16000) return true;
    return false;
}

bool Input::RewindHeld() const {
    for (const auto& p : pads_)
        if (SDL_GetGamepadAxis(p.pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 16000) return true;
    return false;
}

std::vector<Input::PadInfo> Input::Gamepads() const {
    std::vector<PadInfo> out;
    for (size_t i = 0; i < pads_.size(); ++i) {
        const char* n = SDL_GetGamepadName(pads_[i].pad);
        out.push_back({pads_[i].id, n ? n : "Mando", static_cast<int>(i) + 1});
    }
    return out;
}

} // namespace Satronho
