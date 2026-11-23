// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <map>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "core/core_host.h"
#include "sys/systems.h"

namespace Satronho {

class Ini;

// Códigos de mando: 0..SDL_GAMEPAD_BUTTON_COUNT-1 son botones; los gatillos van aparte.
constexpr int kPadNone = -1;
constexpr int kPadLT = 200;
constexpr int kPadRT = 201;

// Asignación de una consola: tecla y botón de mando para cada botón del RetroPad.
struct Mapping {
    std::array<SDL_Scancode, 16> keys{};
    std::array<int, 16> pad{};
    Mapping() {
        keys.fill(SDL_SCANCODE_UNKNOWN);
        pad.fill(kPadNone);
    }
};

struct PlayerConfig {
    bool keyboard = false;
    int pad = -1; // posición del mando entre los conectados (0 = primero), -1 ninguno
};

enum class Hotkey { Menu, SaveState, LoadState, PrevSlot, NextSlot, FastForward, Rewind, Pause, Reset, Meter, Screenshot, Fullscreen, Count };

struct HotkeyDef {
    const char* name;
    bool hold; // se mantiene presionado (avance rápido, rebobinar)
};
const HotkeyDef& HotkeyInfo(Hotkey h);

enum class PadFamily { Xbox, PlayStation, Switch, Generic };

class Input {
public:
    static constexpr int kPlayers = 2;
    static constexpr const char* kDefaultProfile = "Predeterminado";

    void Init();
    void Shutdown();
    void Load(const Ini& ini);
    void Save(Ini& ini) const;

    void HandleEvent(const SDL_Event& e);
    void Update(CoreHost& core, const SystemDef& sys, bool keyboard_enabled);

    // Perfiles por consola.
    std::vector<std::string> Profiles(const std::string& sys) const;
    const std::string& PlayerProfile(int player, const SystemDef& sys);
    void SetPlayerProfile(int player, const SystemDef& sys, const std::string& name);
    Mapping& Edit(int player, const SystemDef& sys);
    bool NewProfile(const SystemDef& sys, const std::string& name, int copy_from_player);
    bool RenameProfile(const SystemDef& sys, const std::string& from, const std::string& to);
    void DeleteProfile(const SystemDef& sys, const std::string& name);
    static Mapping DefaultMapping(const SystemDef& sys, bool keyboard_keys);
    void AutoMapPad(int player, const SystemDef& sys);
    void ResetProfile(int player, const SystemDef& sys);
    void ClearProfile(int player, const SystemDef& sys);

    PlayerConfig players[kPlayers];

    // Vista en vivo: botones del RetroPad presionados ahora mismo por el jugador.
    uint16_t LiveButtons(int player, const SystemDef& sys);
    std::string last_input;    // «Mando 1 · View» — el último botón que llegó, para la vista en vivo
    double last_input_time = 0;

    // Asignación interactiva.
    struct Capture {
        int player = -1;
        std::string sys;
        int button = -1;   // id del RetroPad, o el atajo si hotkey
        bool pad = false;
        bool hotkey = false;
        bool any = false;       // acepta tecla o botón de mando, lo que llegue primero
        std::vector<int> queue; // «Asignar uno por uno»: botones que faltan
    } capture;
    bool Capturing() const { return capture.button >= 0; }
    void StartCapture(int player, const SystemDef& sys, int button, bool pad, bool any = false);
    void StartWizard(int player, const SystemDef& sys, bool pad);
    void StartHotkeyCapture(Hotkey h, bool pad);
    void CancelCapture();

    // Atajos.
    std::array<SDL_Scancode, static_cast<int>(Hotkey::Count)> hk_keys{};
    std::array<int, static_cast<int>(Hotkey::Count)> hk_pad{};
    void ResetHotkeys();
    bool HotkeyHeld(Hotkey h) const;
    std::vector<Hotkey> TakeHotkeyPresses();
    bool MenuComboPressed(); // Select+Start en cualquier mando

    // Mandos conectados.
    struct PadInfo {
        SDL_JoystickID id;
        std::string name;
        PadFamily family;
        std::string family_name;
        int slot;
    };
    std::vector<PadInfo> Gamepads() const;
    PadFamily FamilyOfSlot(int slot) const;
    static std::string PadCodeLabel(int code, PadFamily family);
    static std::string KeyLabel(SDL_Scancode sc);

    // Inclinación (juegos con sensor en el cartucho, como Kirby Tilt 'n' Tumble).
    int tilt_source = 0; // 0 stick derecho, 1 sensor de movimiento del mando
    bool tilt_invert_x = false, tilt_invert_y = false;
    std::array<SDL_Scancode, 4> tilt_keys{SDL_SCANCODE_I, SDL_SCANCODE_K, SDL_SCANCODE_J, SDL_SCANCODE_L};
    float Sensor(unsigned id);           // lo pide el núcleo (RETRO_SENSOR_*)
    void TiltNow(float* x, float* y, float* gyro); // -1..1, también para la vista en vivo
    bool PadHasMotion(int slot) const;

    float deadzone = 0.25f;
    bool stick_dpad = true;
    bool log_buttons = false;

private:
    struct Open {
        SDL_Gamepad* pad;
        SDL_JoystickID id;
        bool trig[2];
    };
    SDL_Gamepad* PadAt(int slot) const;
    bool PadCodeDown(SDL_Gamepad* pad, int code) const;
    std::map<std::string, Mapping>& ProfilesOf(const std::string& sys);
    void AssignCaptured(SDL_Scancode key, int pad_code);
    void Note(const std::string& what);

    std::map<std::string, std::map<std::string, Mapping>> profiles_;
    std::map<std::string, std::string> player_profile_[kPlayers];
    std::vector<Open> pads_;
    std::vector<Hotkey> presses_;
    bool combo_latched_ = false;
    uint16_t logged_[kPlayers] = {};
};

} // namespace Satronho
