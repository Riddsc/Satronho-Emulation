// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "core/core_host.h"

namespace Satronho {

class Ini;

// Botones del RetroPad que usa la NES, en el orden en que se muestran en Ajustes.
struct PadButton {
    int id;            // RETRO_DEVICE_ID_JOYPAD_*
    const char* name;  // nombre para el usuario
};
const std::vector<PadButton>& NesButtons();

class Input {
public:
    void Init();
    void Shutdown();
    void Load(const Ini& ini);
    void Save(Ini& ini) const;
    void ResetDefaults();

    void HandleEvent(const SDL_Event& e);
    // Llena el estado de los mandos para el núcleo. `keyboard_enabled` se apaga cuando
    // la interfaz tiene el foco de un campo de texto.
    void Update(CoreHost& core, bool keyboard_enabled);

    // Atajos con mando: botón Guía o Select+Start abren el menú.
    bool MenuComboPressed();
    bool FastForwardHeld() const;   // gatillo derecho
    bool RewindHeld() const;        // gatillo izquierdo

    std::array<SDL_Scancode, 16> keys{};
    std::array<SDL_GamepadButton, 16> pad_buttons{};

    // Asignación interactiva: esperando a que el usuario presione algo.
    int capture_button = -1;   // índice en keys/pad_buttons
    bool capture_gamepad = false;

    struct PadInfo {
        SDL_JoystickID id;
        std::string name;
        int port;
    };
    std::vector<PadInfo> Gamepads() const;
    float deadzone = 0.25f;

private:
    struct Open {
        SDL_Gamepad* pad;
        SDL_JoystickID id;
    };
    std::vector<Open> pads_;
    bool guide_pressed_ = false;
    bool combo_latched_ = false;
};

} // namespace Satronho
