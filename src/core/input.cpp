// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/input.h"
#include "util/i18n.h"

#include <algorithm>
#include <cmath>
#include <sstream>

#include "util/ini.h"
#include "util/log.h"

namespace Satronho {

namespace {
constexpr float kTrigger = 0.55f;

const char* kStickIds[] = {"leftstick_up", "leftstick_down", "leftstick_left", "leftstick_right",
                           "rightstick_up", "rightstick_down", "rightstick_left", "rightstick_right"};

const char* PadCodeId(int code) {
    if (code == kPadLT) return "lefttrigger";
    if (code == kPadRT) return "righttrigger";
    if (IsStickCode(code)) return kStickIds[code - kPadStick];
    if (code < 0) return "";
    const char* s = SDL_GetGamepadStringForButton(static_cast<SDL_GamepadButton>(code));
    return s ? s : "";
}

int PadCodeFromId(const std::string& s) {
    if (s.empty()) return kPadNone;
    if (s == "lefttrigger") return kPadLT;
    if (s == "righttrigger") return kPadRT;
    for (int i = 0; i < 8; ++i)
        if (s == kStickIds[i]) return kPadStick + i;
    const SDL_GamepadButton b = SDL_GetGamepadButtonFromString(s.c_str());
    return b == SDL_GAMEPAD_BUTTON_INVALID ? kPadNone : static_cast<int>(b);
}

PadFamily FamilyOf(SDL_GamepadType t) {
    switch (t) {
    case SDL_GAMEPAD_TYPE_XBOX360:
    case SDL_GAMEPAD_TYPE_XBOXONE: return PadFamily::Xbox;
    case SDL_GAMEPAD_TYPE_PS3:
    case SDL_GAMEPAD_TYPE_PS4:
    case SDL_GAMEPAD_TYPE_PS5: return PadFamily::PlayStation;
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR: return PadFamily::Switch;
    default: return PadFamily::Generic;
    }
}

const char* FamilyName(PadFamily f) {
    switch (f) {
    case PadFamily::Xbox: return "Xbox";
    case PadFamily::PlayStation: return "PlayStation";
    case PadFamily::Switch: return "Switch";
    default: return "Genérico";
    }
}

// El nombre va dentro de las claves de satronho.ini: sin «.», «|», «=» ni espacios en las puntas.
bool ValidProfileName(const std::string& n) {
    if (n.empty() || n.size() > 40) return false;
    if (n.front() == ' ' || n.back() == ' ') return false;
    for (char c : n)
        if (c == '.' || c == '|' || c == '=' || c == '#' || c == ';' || static_cast<unsigned char>(c) < 32) return false;
    return true;
}

std::vector<std::string> Split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, sep))
        if (!item.empty()) out.push_back(item);
    return out;
}
} // namespace

const HotkeyDef& HotkeyInfo(Hotkey h) {
    static const HotkeyDef defs[] = {
        {"Abrir el menú del juego", false},   {"Guardar estado", false},       {"Cargar estado", false},
        {"Ranura anterior", false},           {"Ranura siguiente", false},     {"Avance rápido (mantener)", true},
        {"Rebobinar (mantener)", true},       {"Pausa", false},                {"Reiniciar el juego", false},
        {"Mostrar u ocultar el medidor", false}, {"Captura de pantalla", false}, {"Pantalla completa", false},
        {"Cambiar pantallas (Satronho Dual)", false},
    };
    return defs[static_cast<int>(h)];
}

// ---------------------------------------------------------------- inicio y guardado

void Input::Init() {
    ResetHotkeys();
    players[0] = {};
    players[0].keyboard = true;
    players[0].want_pos = 0;
    for (int pl = 1; pl < kPlayers; ++pl) {
        players[pl] = {};
        players[pl].want_pos = pl;
    }
    int count = 0;
    // Para el registro: todo lo que SDL ve como mando, sea o no un mando que sabe leer.
    if (SDL_JoystickID* js = SDL_GetJoysticks(&count)) {
        for (int i = 0; i < count; ++i) {
            const char* n = SDL_GetJoystickNameForID(js[i]);
            Log::Info("SDL ve: %s (%s)", n ? n : "?", SDL_IsGamepad(js[i]) ? "mando" : "sin mapa");
        }
        SDL_free(js);
    }
    count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    for (int i = 0; i < count; ++i) {
        if (SDL_Gamepad* pad = SDL_OpenGamepad(ids[i])) {
            pads_.push_back({pad, ids[i], {false, false}, {}});
            Log::Info("Mando conectado: %s (%s)", SDL_GetGamepadName(pad), FamilyName(FamilyOf(SDL_GetGamepadType(pad))));
        }
    }
    SDL_free(ids);
    ResolvePads();
}

void Input::Shutdown() {
    for (auto& p : pads_) SDL_CloseGamepad(p.pad);
    pads_.clear();
}

void Input::ResetHotkeys() {
    hk_keys.fill(SDL_SCANCODE_UNKNOWN);
    hk_pad.fill(kPadNone);
    auto set = [&](Hotkey h, SDL_Scancode k, int p) {
        hk_keys[static_cast<int>(h)] = k;
        hk_pad[static_cast<int>(h)] = p;
    };
    set(Hotkey::Menu, SDL_SCANCODE_ESCAPE, SDL_GAMEPAD_BUTTON_GUIDE);
    set(Hotkey::SaveState, SDL_SCANCODE_F5, kPadNone);
    set(Hotkey::LoadState, SDL_SCANCODE_F9, kPadNone);
    set(Hotkey::PrevSlot, SDL_SCANCODE_F6, kPadNone);
    set(Hotkey::NextSlot, SDL_SCANCODE_F7, kPadNone);
    // Sin botón de mando de fábrica: el gatillo derecho lo usan juegos de varias consolas (R2 en CD, por ejemplo).
    set(Hotkey::FastForward, SDL_SCANCODE_TAB, kPadNone);
    set(Hotkey::Rewind, SDL_SCANCODE_BACKSPACE, kPadLT);
    set(Hotkey::Pause, SDL_SCANCODE_P, kPadNone);
    set(Hotkey::Reset, SDL_SCANCODE_F4, kPadNone);
    set(Hotkey::Meter, SDL_SCANCODE_F3, kPadNone);
    set(Hotkey::Screenshot, SDL_SCANCODE_F12, kPadNone);
    set(Hotkey::Fullscreen, SDL_SCANCODE_F11, kPadNone);
    set(Hotkey::Screens, SDL_SCANCODE_F2, SDL_GAMEPAD_BUTTON_RIGHT_STICK);
}

Mapping Input::DefaultMapping(const SystemDef& sys, bool keyboard_keys) {
    Mapping m;
    auto has = [&](int id) {
        return std::any_of(sys.buttons.begin(), sys.buttons.end(), [&](const auto& b) { return b.first == id; });
    };
    if (keyboard_keys) {
        m.keys[RETRO_DEVICE_ID_JOYPAD_UP] = SDL_SCANCODE_UP;
        m.keys[RETRO_DEVICE_ID_JOYPAD_DOWN] = SDL_SCANCODE_DOWN;
        m.keys[RETRO_DEVICE_ID_JOYPAD_LEFT] = SDL_SCANCODE_LEFT;
        m.keys[RETRO_DEVICE_ID_JOYPAD_RIGHT] = SDL_SCANCODE_RIGHT;
        m.keys[RETRO_DEVICE_ID_JOYPAD_A] = SDL_SCANCODE_X;
        m.keys[RETRO_DEVICE_ID_JOYPAD_B] = SDL_SCANCODE_Z;
        m.keys[RETRO_DEVICE_ID_JOYPAD_X] = SDL_SCANCODE_S;
        m.keys[RETRO_DEVICE_ID_JOYPAD_Y] = SDL_SCANCODE_A;
        m.keys[RETRO_DEVICE_ID_JOYPAD_START] = SDL_SCANCODE_RETURN;
        m.keys[RETRO_DEVICE_ID_JOYPAD_SELECT] = SDL_SCANCODE_RSHIFT;
        if (has(RETRO_DEVICE_ID_JOYPAD_L)) m.keys[RETRO_DEVICE_ID_JOYPAD_L] = SDL_SCANCODE_Q;
        if (has(RETRO_DEVICE_ID_JOYPAD_R)) m.keys[RETRO_DEVICE_ID_JOYPAD_R] = SDL_SCANCODE_W;
    }
    m.pad[RETRO_DEVICE_ID_JOYPAD_UP] = SDL_GAMEPAD_BUTTON_DPAD_UP;
    m.pad[RETRO_DEVICE_ID_JOYPAD_DOWN] = SDL_GAMEPAD_BUTTON_DPAD_DOWN;
    m.pad[RETRO_DEVICE_ID_JOYPAD_LEFT] = SDL_GAMEPAD_BUTTON_DPAD_LEFT;
    m.pad[RETRO_DEVICE_ID_JOYPAD_RIGHT] = SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
    // Como en las consolas de Nintendo: B a la izquierda/abajo, A a la derecha.
    m.pad[RETRO_DEVICE_ID_JOYPAD_A] = SDL_GAMEPAD_BUTTON_EAST;
    m.pad[RETRO_DEVICE_ID_JOYPAD_B] = SDL_GAMEPAD_BUTTON_SOUTH;
    m.pad[RETRO_DEVICE_ID_JOYPAD_X] = SDL_GAMEPAD_BUTTON_NORTH;
    m.pad[RETRO_DEVICE_ID_JOYPAD_Y] = SDL_GAMEPAD_BUTTON_WEST;
    m.pad[RETRO_DEVICE_ID_JOYPAD_START] = SDL_GAMEPAD_BUTTON_START;
    m.pad[RETRO_DEVICE_ID_JOYPAD_SELECT] = SDL_GAMEPAD_BUTTON_BACK;
    if (has(RETRO_DEVICE_ID_JOYPAD_L)) m.pad[RETRO_DEVICE_ID_JOYPAD_L] = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
    if (has(RETRO_DEVICE_ID_JOYPAD_R)) m.pad[RETRO_DEVICE_ID_JOYPAD_R] = SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
    // Segundos gatillos y clic de los sticks (mando de PlayStation).
    if (has(RETRO_DEVICE_ID_JOYPAD_L2)) {
        if (keyboard_keys) m.keys[RETRO_DEVICE_ID_JOYPAD_L2] = SDL_SCANCODE_E;
        m.pad[RETRO_DEVICE_ID_JOYPAD_L2] = kPadLT;
    }
    if (has(RETRO_DEVICE_ID_JOYPAD_R2)) {
        if (keyboard_keys) m.keys[RETRO_DEVICE_ID_JOYPAD_R2] = SDL_SCANCODE_R;
        m.pad[RETRO_DEVICE_ID_JOYPAD_R2] = kPadRT;
    }
    if (has(RETRO_DEVICE_ID_JOYPAD_L3)) {
        if (keyboard_keys) m.keys[RETRO_DEVICE_ID_JOYPAD_L3] = SDL_SCANCODE_F;
        m.pad[RETRO_DEVICE_ID_JOYPAD_L3] = SDL_GAMEPAD_BUTTON_LEFT_STICK;
    }
    if (has(RETRO_DEVICE_ID_JOYPAD_R3)) {
        if (keyboard_keys) m.keys[RETRO_DEVICE_ID_JOYPAD_R3] = SDL_SCANCODE_G;
        m.pad[RETRO_DEVICE_ID_JOYPAD_R3] = SDL_GAMEPAD_BUTTON_RIGHT_STICK;
    }
    if (sys.layout == Layout::N64) {
        // 64 (botones independientes): A = B del RetroPad, B = Y, Z = L2, L = Select, R = R2,
        // C = X/A/L/R. Las flechas quedan para el stick; la cruceta va en el teclado numérico.
        if (keyboard_keys) {
            m.keys[RETRO_DEVICE_ID_JOYPAD_UP] = SDL_SCANCODE_KP_8;
            m.keys[RETRO_DEVICE_ID_JOYPAD_DOWN] = SDL_SCANCODE_KP_2;
            m.keys[RETRO_DEVICE_ID_JOYPAD_LEFT] = SDL_SCANCODE_KP_4;
            m.keys[RETRO_DEVICE_ID_JOYPAD_RIGHT] = SDL_SCANCODE_KP_6;
            m.keys[RETRO_DEVICE_ID_JOYPAD_B] = SDL_SCANCODE_X;       // A
            m.keys[RETRO_DEVICE_ID_JOYPAD_Y] = SDL_SCANCODE_Z;       // B
            m.keys[RETRO_DEVICE_ID_JOYPAD_L2] = SDL_SCANCODE_A;      // Z
            m.keys[RETRO_DEVICE_ID_JOYPAD_SELECT] = SDL_SCANCODE_Q;  // L
            m.keys[RETRO_DEVICE_ID_JOYPAD_R2] = SDL_SCANCODE_W;      // R
            m.keys[RETRO_DEVICE_ID_JOYPAD_X] = SDL_SCANCODE_I;       // C arriba
            m.keys[RETRO_DEVICE_ID_JOYPAD_A] = SDL_SCANCODE_K;       // C abajo
            m.keys[RETRO_DEVICE_ID_JOYPAD_L] = SDL_SCANCODE_J;       // C izquierda
            m.keys[RETRO_DEVICE_ID_JOYPAD_R] = SDL_SCANCODE_L;       // C derecha
            m.keys[RETRO_DEVICE_ID_JOYPAD_START] = SDL_SCANCODE_RETURN;
        }
        // Mando: A abajo, B a la izquierda, Z en el gatillo izquierdo, L y R en los hombros;
        // los C van en el stick derecho (y también en Y para C arriba y B para C abajo).
        m.pad[RETRO_DEVICE_ID_JOYPAD_B] = SDL_GAMEPAD_BUTTON_SOUTH;
        m.pad[RETRO_DEVICE_ID_JOYPAD_Y] = SDL_GAMEPAD_BUTTON_WEST;
        m.pad[RETRO_DEVICE_ID_JOYPAD_L2] = kPadLT;
        m.pad[RETRO_DEVICE_ID_JOYPAD_SELECT] = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
        m.pad[RETRO_DEVICE_ID_JOYPAD_R2] = SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
        // Los cuatro C en la palanca derecha, como en cualquier mando moderno.
        m.pad[RETRO_DEVICE_ID_JOYPAD_X] = kPadStick + 4;     // C arriba
        m.pad[RETRO_DEVICE_ID_JOYPAD_A] = kPadStick + 5;     // C abajo
        m.pad[RETRO_DEVICE_ID_JOYPAD_L] = kPadStick + 6;     // C izquierda
        m.pad[RETRO_DEVICE_ID_JOYPAD_R] = kPadStick + 7;     // C derecha
        m.pad[RETRO_DEVICE_ID_JOYPAD_START] = SDL_GAMEPAD_BUTTON_START;
    }
    if (sys.layout == Layout::Genesis || sys.layout == Layout::Saturn) {
        // Mando de 6 botones: A B C abajo y X Y Z arriba, como quedan en un mando moderno.
        // (BlastEm: A = B del RetroPad, B = A, C = R, X = Y, Y = X, Z = L.)
        if (keyboard_keys) {
            m.keys[RETRO_DEVICE_ID_JOYPAD_B] = SDL_SCANCODE_Z;  // A
            m.keys[RETRO_DEVICE_ID_JOYPAD_A] = SDL_SCANCODE_X;  // B
            m.keys[RETRO_DEVICE_ID_JOYPAD_R] = SDL_SCANCODE_C;  // C
            m.keys[RETRO_DEVICE_ID_JOYPAD_Y] = SDL_SCANCODE_A;  // X
            m.keys[RETRO_DEVICE_ID_JOYPAD_X] = SDL_SCANCODE_S;  // Y
            m.keys[RETRO_DEVICE_ID_JOYPAD_L] = SDL_SCANCODE_D;  // Z
        }
        m.pad[RETRO_DEVICE_ID_JOYPAD_B] = SDL_GAMEPAD_BUTTON_WEST;            // A
        m.pad[RETRO_DEVICE_ID_JOYPAD_A] = SDL_GAMEPAD_BUTTON_SOUTH;           // B
        m.pad[RETRO_DEVICE_ID_JOYPAD_R] = SDL_GAMEPAD_BUTTON_EAST;            // C
        m.pad[RETRO_DEVICE_ID_JOYPAD_Y] = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;   // X
        m.pad[RETRO_DEVICE_ID_JOYPAD_X] = SDL_GAMEPAD_BUTTON_NORTH;           // Y
        m.pad[RETRO_DEVICE_ID_JOYPAD_L] = SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;  // Z
    }
    return m;
}

std::map<std::string, Mapping>& Input::ProfilesOf(const std::string& sys) {
    auto& p = profiles_[sys];
    if (p.empty()) {
        if (const SystemDef* def = SystemById(sys)) p[kDefaultProfile] = DefaultMapping(*def, true);
        else p[kDefaultProfile] = Mapping{};
    }
    return p;
}

void Input::Load(const Ini& ini) {
    profiles_.clear();
    for (const auto& sys : Systems()) {
        auto& profs = profiles_[sys.id];
        std::vector<std::string> names = Split(ini.Get("perfiles." + sys.id), '|');
        for (const auto& name : names) {
            Mapping m = DefaultMapping(sys, true);
            const std::string base = "perfil." + sys.id + "." + name + ".";
            for (const auto& [id, label] : sys.buttons) {
                (void)label;
                const std::string k = base + "t" + std::to_string(id);
                const std::string p = base + "m" + std::to_string(id);
                if (ini.Values().count(k)) {
                    const std::string kv = ini.Get(k);
                    m.keys[id] = kv.empty() ? SDL_SCANCODE_UNKNOWN : SDL_GetScancodeFromName(kv.c_str());
                }
                if (ini.Values().count(p)) m.pad[id] = PadCodeFromId(ini.Get(p));
            }
            // Build 15: en la 64, los C sin botón del mando pasan a la palanca derecha.
            if (sys.id == "n64") {
                const int cs[] = {RETRO_DEVICE_ID_JOYPAD_X, RETRO_DEVICE_ID_JOYPAD_A, RETRO_DEVICE_ID_JOYPAD_L, RETRO_DEVICE_ID_JOYPAD_R};
                if (m.pad[RETRO_DEVICE_ID_JOYPAD_L] == kPadNone && m.pad[RETRO_DEVICE_ID_JOYPAD_R] == kPadNone)
                    for (int i = 0; i < 4; ++i) m.pad[cs[i]] = kPadStick + 4 + i;
            }
            profs[name] = m;
        }
        // Migración de las builds 1 y 2 (un solo juego de controles para la NES).
        if (sys.id == "octa" && names.empty() && ini.Values().count("controles.teclado.0")) {
            Mapping m = DefaultMapping(sys, true);
            for (const auto& [id, label] : sys.buttons) {
                (void)label;
                const std::string kv = ini.Get("controles.teclado." + std::to_string(id));
                if (!kv.empty()) m.keys[id] = SDL_GetScancodeFromName(kv.c_str());
                const std::string pv = ini.Get("controles.mando." + std::to_string(id));
                if (!pv.empty()) m.pad[id] = PadCodeFromId(pv);
            }
            profs[kDefaultProfile] = m;
        }
        if (profs.empty()) profs[kDefaultProfile] = DefaultMapping(sys, true);
        for (int pl = 0; pl < kPlayers; ++pl) {
            std::string name = ini.Get("jugador." + std::to_string(pl + 1) + ".perfil." + sys.id, kDefaultProfile);
            if (!profs.count(name)) name = profs.begin()->first;
            player_profile_[pl][sys.id] = name;
        }
    }
    for (int pl = 0; pl < kPlayers; ++pl) {
        const std::string b = "jugador." + std::to_string(pl + 1) + ".";
        players[pl].keyboard = ini.GetBool(b + "teclado", pl == 0);
        players[pl].want_pos = std::clamp(ini.GetInt(b + "mando", pl), -1, kPlayers - 1);
        players[pl].guid = ini.Get(b + "mando_id");
        players[pl].bound = 0;
    }
    ResolvePads();
    for (int i = 0; i < static_cast<int>(Hotkey::Count); ++i) {
        const std::string b = "atajo." + std::to_string(i) + ".";
        if (ini.Values().count(b + "t")) {
            const std::string kv = ini.Get(b + "t");
            hk_keys[i] = kv.empty() ? SDL_SCANCODE_UNKNOWN : SDL_GetScancodeFromName(kv.c_str());
        }
        if (ini.Values().count(b + "m")) hk_pad[i] = PadCodeFromId(ini.Get(b + "m"));
    }
    deadzone = std::clamp(ini.GetFloat("controles.zona_muerta", 0.25f), 0.05f, 0.9f);
    tilt_source = std::clamp(ini.GetInt("controles.inclinacion.fuente", 0), 0, 1);
    tilt_invert_x = ini.GetBool("controles.inclinacion.invertir_x", false);
    tilt_invert_y = ini.GetBool("controles.inclinacion.invertir_y", false);
    stick_dpad = ini.GetBool("controles.stick_cruceta", true);
    log_buttons = ini.GetBool("controles.registro", false);
}

void Input::Save(Ini& ini) const {
    // Se borran los perfiles viejos antes de escribir los actuales.
    std::vector<std::string> stale;
    for (const auto& [k, v] : ini.Values())
        if (k.rfind("perfil.", 0) == 0 || k.rfind("controles.teclado.", 0) == 0 || k.rfind("controles.mando.", 0) == 0) stale.push_back(k);
    for (const auto& k : stale) ini.Erase(k);
    for (const auto& [sys_id, profs] : profiles_) {
        const SystemDef* sys = SystemById(sys_id);
        if (!sys) continue;
        std::string names;
        for (const auto& [name, m] : profs) {
            names += (names.empty() ? "" : "|") + name;
            const std::string base = "perfil." + sys_id + "." + name + ".";
            for (const auto& [id, label] : sys->buttons) {
                (void)label;
                const char* kn = m.keys[id] == SDL_SCANCODE_UNKNOWN ? "" : SDL_GetScancodeName(m.keys[id]);
                ini.Set(base + "t" + std::to_string(id), kn ? kn : "");
                ini.Set(base + "m" + std::to_string(id), PadCodeId(m.pad[id]));
            }
        }
        ini.Set("perfiles." + sys_id, names);
        for (int pl = 0; pl < kPlayers; ++pl) {
            auto it = player_profile_[pl].find(sys_id);
            if (it != player_profile_[pl].end()) ini.Set("jugador." + std::to_string(pl + 1) + ".perfil." + sys_id, it->second);
        }
    }
    for (int pl = 0; pl < kPlayers; ++pl) {
        const std::string b = "jugador." + std::to_string(pl + 1) + ".";
        ini.SetBool(b + "teclado", players[pl].keyboard);
        ini.SetInt(b + "mando", players[pl].pad >= 0 ? players[pl].pad : players[pl].want_pos);
        ini.Set(b + "mando_id", players[pl].guid);
    }
    for (int i = 0; i < static_cast<int>(Hotkey::Count); ++i) {
        const std::string b = "atajo." + std::to_string(i) + ".";
        const char* kn = hk_keys[i] == SDL_SCANCODE_UNKNOWN ? "" : SDL_GetScancodeName(hk_keys[i]);
        ini.Set(b + "t", kn ? kn : "");
        ini.Set(b + "m", PadCodeId(hk_pad[i]));
    }
    ini.SetFloat("controles.zona_muerta", deadzone);
    ini.SetInt("controles.inclinacion.fuente", tilt_source);
    ini.SetBool("controles.inclinacion.invertir_x", tilt_invert_x);
    ini.SetBool("controles.inclinacion.invertir_y", tilt_invert_y);
    ini.SetBool("controles.stick_cruceta", stick_dpad);
    ini.SetBool("controles.registro", log_buttons);
}

// ---------------------------------------------------------------- perfiles

std::vector<std::string> Input::Profiles(const std::string& sys) const {
    std::vector<std::string> out;
    auto it = profiles_.find(sys);
    if (it != profiles_.end())
        for (const auto& [name, m] : it->second) out.push_back(name);
    if (out.empty()) out.push_back(kDefaultProfile);
    // El predeterminado siempre primero.
    std::stable_partition(out.begin(), out.end(), [](const std::string& n) { return n == kDefaultProfile; });
    return out;
}

const std::string& Input::PlayerProfile(int player, const SystemDef& sys) {
    auto& profs = ProfilesOf(sys.id);
    std::string& name = player_profile_[player][sys.id];
    if (name.empty() || !profs.count(name)) name = profs.count(kDefaultProfile) ? kDefaultProfile : profs.begin()->first;
    return name;
}

void Input::SetPlayerProfile(int player, const SystemDef& sys, const std::string& name) {
    if (ProfilesOf(sys.id).count(name)) player_profile_[player][sys.id] = name;
}

Mapping& Input::Edit(int player, const SystemDef& sys) { return ProfilesOf(sys.id)[PlayerProfile(player, sys)]; }

bool Input::NewProfile(const SystemDef& sys, const std::string& name, int copy_from_player) {
    auto& profs = ProfilesOf(sys.id);
    if (!ValidProfileName(name) || profs.count(name)) return false;
    profs[name] = Edit(copy_from_player, sys);
    player_profile_[copy_from_player][sys.id] = name;
    return true;
}

bool Input::RenameProfile(const SystemDef& sys, const std::string& from, const std::string& to) {
    auto& profs = ProfilesOf(sys.id);
    if (from == kDefaultProfile || !profs.count(from) || !ValidProfileName(to) || profs.count(to)) return false;
    profs[to] = profs[from];
    profs.erase(from);
    for (auto& pp : player_profile_)
        if (pp[sys.id] == from) pp[sys.id] = to;
    return true;
}

void Input::DeleteProfile(const SystemDef& sys, const std::string& name) {
    auto& profs = ProfilesOf(sys.id);
    if (name == kDefaultProfile) return;
    profs.erase(name);
    for (auto& pp : player_profile_)
        if (pp[sys.id] == name) pp[sys.id] = kDefaultProfile;
    ProfilesOf(sys.id);
}

void Input::AutoMapPad(int player, const SystemDef& sys) {
    const Mapping def = DefaultMapping(sys, false);
    Edit(player, sys).pad = def.pad;
}

void Input::ResetProfile(int player, const SystemDef& sys) { Edit(player, sys) = DefaultMapping(sys, player == 0); }

void Input::ClearProfile(int player, const SystemDef& sys) { Edit(player, sys) = Mapping{}; }

// ---------------------------------------------------------------- asignación

void Input::StartCapture(int player, const SystemDef& sys, int button, bool pad, bool any) {
    capture = {};
    capture.any = any;
    capture.player = player;
    capture.sys = sys.id;
    capture.button = button;
    capture.pad = pad;
}

void Input::StartWizard(int player, const SystemDef& sys, bool pad) {
    capture = {};
    capture.any = true;
    capture.player = player;
    capture.sys = sys.id;
    capture.pad = pad;
    for (const auto& [id, label] : sys.buttons) {
        (void)label;
        if (sys.turbo && (id == RETRO_DEVICE_ID_JOYPAD_X || id == RETRO_DEVICE_ID_JOYPAD_Y)) continue; // los turbos se dejan
        capture.queue.push_back(id);
    }
    capture.button = capture.queue.front();
    capture.queue.erase(capture.queue.begin());
}

void Input::StartHotkeyCapture(Hotkey h, bool pad) {
    capture = {};
    capture.any = true;
    capture.hotkey = true;
    capture.button = static_cast<int>(h);
    capture.pad = pad;
}

void Input::CancelCapture() { capture = {}; }

void Input::AssignCaptured(SDL_Scancode key, int pad_code) {
    if (capture.hotkey) {
        const int i = capture.button;
        if (capture.pad) hk_pad[i] = pad_code;
        else hk_keys[i] = key;
        // ¿Esa tecla ya la usa algún perfil de alguna consola?
        if (!capture.pad && key != SDL_SCANCODE_UNKNOWN) {
            for (const auto& [sys_id, profs] : profiles_) {
                const SystemDef* sys = SystemById(sys_id);
                if (!sys) continue;
                for (const auto& [name, m] : profs)
                    for (const auto& [id, label] : sys->buttons)
                        if (m.keys[id] == key && conflict_.empty())
                            conflict_ = "Ojo: " + KeyLabel(key) + " también es «" + label + "» en " + sys->full + " (perfil " + name +
                                        "). Mientras juegas hará las dos cosas.";
            }
        }
        capture = {};
        return;
    }
    const SystemDef* sys = SystemById(capture.sys);
    if (!sys) {
        capture = {};
        return;
    }
    Mapping& m = Edit(capture.player, *sys);
    const int id = capture.button;
    if (capture.pad) {
        // El mismo botón no puede quedar en dos lugares del mismo perfil.
        for (auto& p : m.pad)
            if (p == pad_code) p = kPadNone;
        m.pad[id] = pad_code;
    } else {
        for (auto& k : m.keys)
            if (k == key) k = SDL_SCANCODE_UNKNOWN;
        m.keys[id] = key;
        for (int i = 0; i < static_cast<int>(Hotkey::Count); ++i)
            if (hk_keys[i] == key && key != SDL_SCANCODE_UNKNOWN)
                conflict_ = "Ojo: " + KeyLabel(key) + " también es el atajo «" + HotkeyInfo(static_cast<Hotkey>(i)).name +
                            "». Mientras juegas hará las dos cosas; puedes cambiar el atajo en Controles » Atajos.";
    }
    if (!capture.queue.empty()) {
        capture.button = capture.queue.front();
        capture.queue.erase(capture.queue.begin());
    } else {
        capture = {};
    }
}

void Input::Note(const std::string& what) {
    last_input = what;
    last_input_time = SDL_GetTicks() / 1000.0;
}

// ---------------------------------------------------------------- eventos

void Input::HandleEvent(const SDL_Event& e) {
    auto slot_of = [&](SDL_JoystickID id) {
        for (size_t i = 0; i < pads_.size(); ++i)
            if (pads_[i].id == id) return static_cast<int>(i);
        return -1;
    };
    switch (e.type) {
    case SDL_EVENT_GAMEPAD_ADDED:
        if (slot_of(e.gdevice.which) < 0) {
            if (SDL_Gamepad* pad = SDL_OpenGamepad(e.gdevice.which)) {
                pads_.push_back({pad, e.gdevice.which, {false, false}, {}});
                Log::Info("Mando conectado: %s (%s)", SDL_GetGamepadName(pad), FamilyName(FamilyOf(SDL_GetGamepadType(pad))));
                ResolvePads();
            }
        }
        break;
    case SDL_EVENT_JOYSTICK_ADDED:
        // Para el registro: un mando que SDL ve pero no sabe leer como mando (sin mapa) no aparece en Controles.
        if (!SDL_IsGamepad(e.jdevice.which)) {
            const char* n = SDL_GetJoystickNameForID(e.jdevice.which);
            char guid[64] = {};
            SDL_GUIDToString(SDL_GetJoystickGUIDForID(e.jdevice.which), guid, sizeof(guid));
            Log::Warn("Mando sin mapa: %s · %s", n ? n : "?", guid);
            // En Android lo toma el puente de mandos (Input::BridgeTick), como hace Satronho NX.
        }
        break;
    case SDL_EVENT_GAMEPAD_REMOVED: {
        const int s = slot_of(e.gdevice.which);
        if (s >= 0) {
            Log::Info("Mando desconectado: %s", SDL_GetGamepadName(pads_[s].pad));
            SDL_CloseGamepad(pads_[s].pad);
            pads_.erase(pads_.begin() + s);
            ResolvePads();
        }
        break;
    }
    case SDL_EVENT_KEY_DOWN: {
        if (e.key.repeat) break;
        Note(std::string(Tr("Teclado")) + " · " + KeyLabel(e.key.scancode));
        if (Capturing()) {
            if (e.key.scancode == SDL_SCANCODE_ESCAPE) CancelCapture();
            else if (!capture.pad || capture.any) {
                capture.pad = false;
                AssignCaptured(e.key.scancode, kPadNone);
            }
            break;
        }
        // Con Alt no cuentan los atajos: Alt+F4 cierra (sin reiniciar el juego por F4) y Alt+Enter
        // es la pantalla completa.
        if (e.key.mod & SDL_KMOD_ALT) break;
        for (int i = 0; i < static_cast<int>(Hotkey::Count); ++i)
            if (hk_keys[i] == e.key.scancode && !HotkeyInfo(static_cast<Hotkey>(i)).hold) {
                presses_.push_back(static_cast<Hotkey>(i));
                if (static_cast<Hotkey>(i) == Hotkey::Menu) menu_from_keyboard = true;
            }
        break;
    }
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN: {
        const int s = slot_of(e.gbutton.which);
        const PadFamily fam = FamilyOfSlot(s);
        Note(std::string(Tr("Mando")) + " " + std::to_string(s + 1) + " · " + PadCodeLabel(e.gbutton.button, fam));
        if (Capturing()) {
            if (capture.pad || capture.any) {
                capture.pad = true;
                AssignCaptured(SDL_SCANCODE_UNKNOWN, e.gbutton.button);
            }
            break;
        }
        if (!claim_.empty() && s >= 0) {
            const int pl = claim_.front();
            claim_.erase(claim_.begin());
            SetPlayerPad(pl, s);
            Greet(s);
            claim_note_ = std::string(Tr("Jugador")) + " " + std::to_string(pl + 1) + ": " + SDL_GetGamepadName(pads_[s].pad);
            Log::Info("Jugador %d tomó el mando %d (%s)", pl + 1, s + 1, SDL_GetGamepadName(pads_[s].pad));
            break;
        }
        if (GameUses(e.gbutton.button)) break;
        for (int i = 0; i < static_cast<int>(Hotkey::Count); ++i)
            if (hk_pad[i] == e.gbutton.button && !HotkeyInfo(static_cast<Hotkey>(i)).hold) {
                presses_.push_back(static_cast<Hotkey>(i));
                if (static_cast<Hotkey>(i) == Hotkey::Menu) menu_from_keyboard = false;
            }
        break;
    }
    case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
        if (e.gaxis.axis != SDL_GAMEPAD_AXIS_LEFT_TRIGGER && e.gaxis.axis != SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
            // Palancas: al pasar del 60 % en una dirección cuentan como «botón» para asignar.
            const int s = slot_of(e.gaxis.which);
            if (s < 0) break;
            const bool right = e.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHTX || e.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHTY;
            const bool vertical = e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY || e.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHTY;
            const float v = e.gaxis.value / 32767.0f;
            for (int neg = 0; neg < 2; ++neg) {
                // Orden: arriba, abajo, izquierda, derecha.
                const int dir = vertical ? (neg ? 0 : 1) : (neg ? 2 : 3);
                const int idx = (right ? 4 : 0) + dir;
                const bool down = neg ? v < -0.6f : v > 0.6f;
                if (down && !pads_[s].stick[idx]) {
                    const int code = kPadStick + idx;
                    Note(std::string(Tr("Mando")) + " " + std::to_string(s + 1) + " · " + PadCodeLabel(code, FamilyOfSlot(s)));
                    if (Capturing() && (capture.pad || capture.any) && !capture.hotkey) {
                        capture.pad = true;
                        AssignCaptured(SDL_SCANCODE_UNKNOWN, code);
                    }
                }
                pads_[s].stick[idx] = down;
            }
            break;
        }
        const int s = slot_of(e.gaxis.which);
        if (s < 0) break;
        const int t = e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER ? 0 : 1;
        const bool down = e.gaxis.value / 32767.0f > kTrigger;
        if (down && !pads_[s].trig[t]) {
            const int code = t ? kPadRT : kPadLT;
            Note(std::string(Tr("Mando")) + " " + std::to_string(s + 1) + " · " + PadCodeLabel(code, FamilyOfSlot(s)));
            if (Capturing()) {
                if (capture.pad || capture.any) {
                    capture.pad = true;
                    AssignCaptured(SDL_SCANCODE_UNKNOWN, code);
                }
            } else if (!GameUses(code)) {
                for (int i = 0; i < static_cast<int>(Hotkey::Count); ++i)
                    if (hk_pad[i] == code && !HotkeyInfo(static_cast<Hotkey>(i)).hold) {
                        presses_.push_back(static_cast<Hotkey>(i));
                        if (static_cast<Hotkey>(i) == Hotkey::Menu) menu_from_keyboard = false;
                    }
            }
        }
        pads_[s].trig[t] = down;
        break;
    }
    default:
        break;
    }
}

std::vector<Hotkey> Input::TakeHotkeyPresses() {
    std::vector<Hotkey> out;
    out.swap(presses_);
    return out;
}

SDL_Gamepad* Input::PadAt(int slot) const {
    return slot >= 0 && slot < static_cast<int>(pads_.size()) ? pads_[slot].pad : nullptr;
}

bool Input::PadCodeDown(SDL_Gamepad* pad, int code) const {
    if (!pad || code == kPadNone) return false;
    if (code == kPadLT) return SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) / 32767.0f > kTrigger;
    if (code == kPadRT) return SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) / 32767.0f > kTrigger;
    if (IsStickCode(code)) {
        const int idx = code - kPadStick;
        const bool right = idx >= 4;
        const int dir = idx % 4;
        const SDL_GamepadAxis ax = dir < 2 ? (right ? SDL_GAMEPAD_AXIS_RIGHTY : SDL_GAMEPAD_AXIS_LEFTY)
                                           : (right ? SDL_GAMEPAD_AXIS_RIGHTX : SDL_GAMEPAD_AXIS_LEFTX);
        const float v = SDL_GetGamepadAxis(pad, ax) / 32767.0f;
        return (dir == 0 || dir == 2) ? v < -0.5f : v > 0.5f;
    }
    return SDL_GetGamepadButton(pad, static_cast<SDL_GamepadButton>(code));
}

bool Input::HotkeyHeld(Hotkey h) const {
    const int i = static_cast<int>(h);
    const bool* ks = SDL_GetKeyboardState(nullptr);
    if (hk_keys[i] != SDL_SCANCODE_UNKNOWN && ks[hk_keys[i]]) return true;
    if (GameUses(hk_pad[i])) return false; // ese botón es del juego (L2/R2 en CD)
    for (const auto& p : pads_)
        if (PadCodeDown(p.pad, hk_pad[i])) return true;
    return false;
}

bool Input::GameUses(int code) const {
    return code != kPadNone && std::find(game_codes_.begin(), game_codes_.end(), code) != game_codes_.end();
}

bool Input::MenuComboPressed() {
    bool combo = false;
    for (const auto& p : pads_)
        if (SDL_GetGamepadButton(p.pad, SDL_GAMEPAD_BUTTON_BACK) && SDL_GetGamepadButton(p.pad, SDL_GAMEPAD_BUTTON_START)) combo = true;
    if (combo && !combo_latched_) {
        combo_latched_ = true;
        return true;
    }
    if (!combo) combo_latched_ = false;
    return false;
}

// ---------------------------------------------------------------- estado para el núcleo

void Input::LiveSticks(int player, float out[4]) const {
    out[0] = out[1] = out[2] = out[3] = 0;
    if (SDL_Gamepad* pad = PadAt(players[player].pad)) {
        const SDL_GamepadAxis ax[] = {SDL_GAMEPAD_AXIS_LEFTX, SDL_GAMEPAD_AXIS_LEFTY, SDL_GAMEPAD_AXIS_RIGHTX, SDL_GAMEPAD_AXIS_RIGHTY};
        for (int i = 0; i < 4; ++i) out[i] = SDL_GetGamepadAxis(pad, ax[i]) / 32767.0f;
    }
}

uint16_t Input::LiveButtons(int player, const SystemDef& sys) {
    const PlayerConfig& pc = players[player];
    const Mapping& m = Edit(player, sys);
    uint16_t bits = 0;
    if (pc.keyboard) {
        const bool* ks = SDL_GetKeyboardState(nullptr);
        for (const auto& [id, label] : sys.buttons) {
            (void)label;
            if (m.keys[id] != SDL_SCANCODE_UNKNOWN && ks[m.keys[id]]) bits |= static_cast<uint16_t>(1u << id);
        }
    }
    if (SDL_Gamepad* pad = PadAt(pc.pad)) {
        for (const auto& [id, label] : sys.buttons) {
            (void)label;
            if (PadCodeDown(pad, m.pad[id])) bits |= static_cast<uint16_t>(1u << id);
        }
        if (stick_dpad) {
            const float lx = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f;
            const float ly = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;
            if (lx < -deadzone) bits |= 1u << RETRO_DEVICE_ID_JOYPAD_LEFT;
            if (lx > deadzone) bits |= 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT;
            if (ly < -deadzone) bits |= 1u << RETRO_DEVICE_ID_JOYPAD_UP;
            if (ly > deadzone) bits |= 1u << RETRO_DEVICE_ID_JOYPAD_DOWN;
        }
    }
    return bits;
}

void Input::Update(CoreHost& core, const SystemDef& sys, bool keyboard_enabled) {
    for (auto& p : core.pads) p = {};
    // Los botones del mando que este juego usa no disparan atajos (por ejemplo, L2 y R2 en CD
    // no adelantan ni rebobinan).
    game_codes_.clear();
    for (int pl = 0; pl < kPlayers; ++pl) {
        if (players[pl].pad < 0) continue;
        const Mapping& m = Edit(pl, sys);
        for (const auto& [id, label] : sys.buttons) {
            (void)label;
            if (m.pad[id] != kPadNone) game_codes_.push_back(m.pad[id]);
        }
    }
    if (Capturing()) return;
    // Turbo a 15 pulsaciones por segundo (2 cuadros presionado, 2 suelto).
    const bool phase = (turbo_tick_++ / 2) % 2 == 0;
    for (int pl = 0; pl < kPlayers; ++pl) {
        const bool kb = players[pl].keyboard;
        if (!keyboard_enabled) players[pl].keyboard = false;
        uint16_t bits = LiveButtons(pl, sys);
        players[pl].keyboard = kb;

        if (log_buttons && bits != logged_[pl]) {
            const uint16_t down = bits & ~logged_[pl];
            for (const auto& [id, label] : sys.buttons)
                if (down & (1u << id)) Log::Info("Jugador %d presionó %s (cuadro %llu)", pl + 1, label, static_cast<unsigned long long>(core.FrameSerial()));
            logged_[pl] = bits;
        }

        // Turbo: X/Y del RetroPad se convierten en A/B que se repiten 15 veces por segundo.
        if (sys.turbo && (bits & (1u << RETRO_DEVICE_ID_JOYPAD_X))) {
            bits &= ~(1u << RETRO_DEVICE_ID_JOYPAD_X);
            if (phase) bits |= 1u << RETRO_DEVICE_ID_JOYPAD_A;
        }
        if (sys.turbo && (bits & (1u << RETRO_DEVICE_ID_JOYPAD_Y))) {
            bits &= ~(1u << RETRO_DEVICE_ID_JOYPAD_Y);
            if (phase) bits |= 1u << RETRO_DEVICE_ID_JOYPAD_B;
        }
        core.pads[pl].buttons = bits;
        if (SDL_Gamepad* pad = PadAt(players[pl].pad)) {
            core.pads[pl].analog[0][0] = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX);
            core.pads[pl].analog[0][1] = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY);
            // El stick derecho solo va al juego en consolas con dos sticks (en las portátiles
            // sirve para inclinar).
            if (sys.analog) {
                core.pads[pl].analog[1][0] = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTX);
                core.pads[pl].analog[1][1] = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTY);
            }
        }
        // Consolas con stick (64 y CD): con el teclado, las flechas mueven el stick izquierdo
        // si no están asignadas a otro botón (en la 64 casi todo se juega con el stick).
        if (sys.analog && players[pl].keyboard && keyboard_enabled && core.pads[pl].analog[0][0] == 0 &&
            core.pads[pl].analog[0][1] == 0) {
            const Mapping& m = Edit(pl, sys);
            auto bound = [&](SDL_Scancode sc) {
                for (const auto& [id, label] : sys.buttons) {
                    (void)label;
                    if (m.keys[id] == sc) return true;
                }
                return false;
            };
            const bool* ks = SDL_GetKeyboardState(nullptr);
            auto key = [&](SDL_Scancode sc) { return !bound(sc) && ks[sc]; };
            const int x = (key(SDL_SCANCODE_RIGHT) ? 1 : 0) - (key(SDL_SCANCODE_LEFT) ? 1 : 0);
            const int y = (key(SDL_SCANCODE_DOWN) ? 1 : 0) - (key(SDL_SCANCODE_UP) ? 1 : 0);
            core.pads[pl].analog[0][0] = static_cast<int16_t>(x * 0x7fff);
            core.pads[pl].analog[0][1] = static_cast<int16_t>(y * 0x7fff);
        }
    }
}

// ---------------------------------------------------------------- mandos de cada jugador

std::string Input::GuidOfSlot(int slot) const {
    if (slot < 0 || slot >= static_cast<int>(pads_.size())) return {};
    char buf[64] = {};
    SDL_GUIDToString(SDL_GetGamepadGUIDForID(pads_[slot].id), buf, sizeof buf);
    return buf;
}

void Input::SetPlayerPad(int player, int slot) {
    PlayerConfig& pc = players[player];
    if (slot < 0) {
        pc.guid.clear();
        pc.want_pos = -1;
        pc.bound = 0;
    } else {
        pc.want_pos = slot;
        pc.guid = GuidOfSlot(slot);
        pc.bound = slot < static_cast<int>(pads_.size()) ? pads_[slot].id : 0;
        // Si otro jugador tenía ese mando, lo suelta.
        for (int o = 0; o < kPlayers; ++o)
            if (o != player && pc.bound && players[o].bound == pc.bound) {
                players[o].bound = 0;
                players[o].pad = -1;
            }
    }
    ResolvePads();
}

void Input::ResolvePads() {
    const int n = static_cast<int>(pads_.size());
    std::vector<bool> taken(n, false);
    int slot_of[kPlayers];
    for (int pl = 0; pl < kPlayers; ++pl) slot_of[pl] = -1;
    // 1) Quien ya tiene un mando conectado lo conserva.
    for (int pl = 0; pl < kPlayers; ++pl) {
        PlayerConfig& pc = players[pl];
        if (!pc.bound) continue;
        for (int i = 0; i < n; ++i)
            if (pads_[i].id == pc.bound && !taken[i]) {
                slot_of[pl] = i;
                taken[i] = true;
            }
        if (slot_of[pl] < 0) pc.bound = 0;
    }
    // 2) Quien recuerda la identidad de su mando, lo recupera al reconectarlo.
    for (int pl = 0; pl < kPlayers; ++pl) {
        PlayerConfig& pc = players[pl];
        if (slot_of[pl] >= 0 || pc.guid.empty()) continue;
        for (int i = 0; i < n; ++i)
            if (!taken[i] && GuidOfSlot(i) == pc.guid) {
                slot_of[pl] = i;
                taken[i] = true;
                pc.bound = pads_[i].id;
                break;
            }
    }
    // 3) Por posición (configuraciones de antes, o si su mando no está pero hay otro libre en
    //    esa posición): así nadie se queda sin jugar por haber cambiado de mando.
    for (int pl = 0; pl < kPlayers; ++pl) {
        PlayerConfig& pc = players[pl];
        if (slot_of[pl] >= 0 || pc.want_pos < 0 || pc.want_pos >= n || taken[pc.want_pos]) continue;
        slot_of[pl] = pc.want_pos;
        taken[pc.want_pos] = true;
        if (pc.guid.empty()) {
            pc.guid = GuidOfSlot(pc.want_pos);
            pc.bound = pads_[pc.want_pos].id;
        }
    }
    for (int pl = 0; pl < kPlayers; ++pl) players[pl].pad = slot_of[pl];
}

int Input::ActivePlayers() const {
    int n = 0;
    for (int pl = 0; pl < kPlayers; ++pl)
        if (players[pl].pad >= 0 || (pl == 0 && players[pl].keyboard)) n = pl + 1;
    return std::max(n, 1);
}

void Input::StartClaim(int player) {
    if (player < 0 || player >= kPlayers) return;
    CancelCapture();
    claim_ = {player};
}

void Input::StartClaimAll() {
    CancelCapture();
    claim_.clear();
    const int n = std::max<int>(static_cast<int>(pads_.size()), 1);
    for (int pl = 0; pl < std::min(n, kPlayers); ++pl) claim_.push_back(pl);
}

void Input::Greet(int slot) {
    if (SDL_Gamepad* pad = PadAt(slot)) SDL_RumbleGamepad(pad, 0x6000, 0xA000, 260);
}

void Input::ApplyRumble(const CoreHost& core, bool enabled) {
    const Uint64 now = SDL_GetTicks();
    for (int pl = 0; pl < kPlayers; ++pl) {
        SDL_Gamepad* pad = PadAt(players[pl].pad);
        if (!pad) continue;
        const uint16_t strong = enabled ? core.rumble[pl][RETRO_RUMBLE_STRONG] : 0;
        const uint16_t weak = enabled ? core.rumble[pl][RETRO_RUMBLE_WEAK] : 0;
        const bool changed = strong != rumble_sent_[pl][0] || weak != rumble_sent_[pl][1];
        // Se repite cada tanto mientras siga vibrando (cada orden dura 250 ms por seguridad:
        // si el juego se cierra de golpe, el mando no se queda vibrando).
        if (!changed && (!(strong || weak) || now - rumble_time_[pl] < 150)) continue;
        SDL_RumbleGamepad(pad, strong, weak, (strong || weak) ? 250 : 0);
        rumble_sent_[pl][0] = strong;
        rumble_sent_[pl][1] = weak;
        rumble_time_[pl] = now;
    }
}

// ---------------------------------------------------------------- inclinación

bool Input::PadHasMotion(int slot) const {
    SDL_Gamepad* pad = PadAt(slot);
    return pad && SDL_GamepadHasSensor(pad, SDL_SENSOR_ACCEL);
}

void Input::TiltNow(float* x, float* y, float* gyro) {
    float tx = 0, ty = 0, gz = 0;
    const PlayerConfig& pc = players[0];
    if (pc.keyboard) {
        const bool* ks = SDL_GetKeyboardState(nullptr);
        if (ks[tilt_keys[0]]) ty -= 1;
        if (ks[tilt_keys[1]]) ty += 1;
        if (ks[tilt_keys[2]]) tx -= 1;
        if (ks[tilt_keys[3]]) tx += 1;
    }
    if (SDL_Gamepad* pad = PadAt(pc.pad)) {
        if (tilt_source == 1 && SDL_GamepadHasSensor(pad, SDL_SENSOR_ACCEL)) {
            // Mando de PS4/PS5/Switch: se inclina el mando de verdad.
            if (!SDL_GamepadSensorEnabled(pad, SDL_SENSOR_ACCEL)) SDL_SetGamepadSensorEnabled(pad, SDL_SENSOR_ACCEL, true);
            if (SDL_GamepadHasSensor(pad, SDL_SENSOR_GYRO) && !SDL_GamepadSensorEnabled(pad, SDL_SENSOR_GYRO))
                SDL_SetGamepadSensorEnabled(pad, SDL_SENSOR_GYRO, true);
            float a[3] = {};
            if (SDL_GetGamepadSensorData(pad, SDL_SENSOR_ACCEL, a, 3)) {
                tx += a[0] / SDL_STANDARD_GRAVITY;
                ty += a[2] / SDL_STANDARD_GRAVITY;
            }
            float g[3] = {};
            if (SDL_GetGamepadSensorData(pad, SDL_SENSOR_GYRO, g, 3)) gz = g[2];
        } else {
            auto axis = [&](SDL_GamepadAxis ax) {
                const float v = SDL_GetGamepadAxis(pad, ax) / 32767.0f;
                return std::fabs(v) < deadzone * 0.6f ? 0.0f : v;
            };
            tx += axis(SDL_GAMEPAD_AXIS_RIGHTX);
            ty += axis(SDL_GAMEPAD_AXIS_RIGHTY);
            gz = axis(SDL_GAMEPAD_AXIS_RIGHTX) * 2.0f;
        }
    }
    if (tilt_invert_x) tx = -tx;
    if (tilt_invert_y) ty = -ty;
    *x = std::clamp(tx, -1.0f, 1.0f);
    *y = std::clamp(ty, -1.0f, 1.0f);
    if (gyro) *gyro = gz;
}

float Input::Sensor(unsigned id) {
    float x, y, gz;
    TiltNow(&x, &y, &gz);
    switch (id) {
    case RETRO_SENSOR_ACCELEROMETER_X: return x;
    case RETRO_SENSOR_ACCELEROMETER_Y: return y;
    case RETRO_SENSOR_ACCELEROMETER_Z: return 1.0f;
    case RETRO_SENSOR_GYROSCOPE_Z: return gz;
    default: return 0.0f;
    }
}

// ---------------------------------------------------------------- nombres

std::vector<Input::PadInfo> Input::Gamepads() const {
    std::vector<PadInfo> out;
    for (size_t i = 0; i < pads_.size(); ++i) {
        const char* n = SDL_GetGamepadName(pads_[i].pad);
        const PadFamily f = FamilyOf(SDL_GetGamepadType(pads_[i].pad));
        out.push_back({pads_[i].id, n ? n : "Mando", f, FamilyName(f), static_cast<int>(i)});
    }
    return out;
}

PadFamily Input::FamilyOfSlot(int slot) const {
    SDL_Gamepad* p = PadAt(slot);
    return p ? FamilyOf(SDL_GetGamepadType(p)) : PadFamily::Xbox;
}

std::string Input::PadCodeLabel(int code, PadFamily f) {
    if (code == kPadNone) return "—";
    const bool ps = f == PadFamily::PlayStation, sw = f == PadFamily::Switch;
    if (code == kPadLT) return ps ? "L2" : sw ? "ZL" : "LT";
    if (code == kPadRT) return ps ? "R2" : sw ? "ZR" : "RT";
    if (IsStickCode(code)) {
        static const char* dirs[] = {"arriba", "abajo", "izq.", "der."};
        const int idx = code - kPadStick;
        return std::string(idx >= 4 ? "Pal. der. " : "Pal. izq. ") + dirs[idx % 4];
    }
    switch (static_cast<SDL_GamepadButton>(code)) {
    case SDL_GAMEPAD_BUTTON_SOUTH: return ps ? "Cruz" : sw ? "B" : "A";
    case SDL_GAMEPAD_BUTTON_EAST: return ps ? "Círculo" : sw ? "A" : "B";
    case SDL_GAMEPAD_BUTTON_WEST: return ps ? "Cuadrado" : sw ? "Y" : "X";
    case SDL_GAMEPAD_BUTTON_NORTH: return ps ? "Triángulo" : sw ? "X" : "Y";
    case SDL_GAMEPAD_BUTTON_BACK: return ps ? "Share" : sw ? "−" : "View";
    case SDL_GAMEPAD_BUTTON_START: return ps ? "Options" : sw ? "+" : "Menu";
    case SDL_GAMEPAD_BUTTON_GUIDE: return ps ? "PS" : sw ? "Home" : "Xbox";
    case SDL_GAMEPAD_BUTTON_LEFT_STICK: return ps ? "L3" : "LS";
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK: return ps ? "R3" : "RS";
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return ps ? "L1" : sw ? "L" : "LB";
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return ps ? "R1" : sw ? "R" : "RB";
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return "Cruceta arriba";
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return "Cruceta abajo";
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return "Cruceta izq.";
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return "Cruceta der.";
    case SDL_GAMEPAD_BUTTON_MISC1: return ps ? "Micrófono" : sw ? "Captura" : "Compartir";
    case SDL_GAMEPAD_BUTTON_TOUCHPAD: return "Panel táctil";
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1: return "Paleta R1";
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE1: return "Paleta L1";
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2: return "Paleta R2";
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE2: return "Paleta L2";
    default: return "Botón " + std::to_string(code);
    }
}

std::string Input::KeyLabel(SDL_Scancode sc) {
    switch (sc) {
    case SDL_SCANCODE_UNKNOWN: return "—";
    case SDL_SCANCODE_UP: return "Flecha arriba";
    case SDL_SCANCODE_DOWN: return "Flecha abajo";
    case SDL_SCANCODE_LEFT: return "Flecha izquierda";
    case SDL_SCANCODE_RIGHT: return "Flecha derecha";
    case SDL_SCANCODE_RETURN: return "Enter";
    case SDL_SCANCODE_KP_ENTER: return "Enter numérico";
    case SDL_SCANCODE_RSHIFT: return "Shift derecho";
    case SDL_SCANCODE_LSHIFT: return "Shift izquierdo";
    case SDL_SCANCODE_RCTRL: return "Ctrl derecho";
    case SDL_SCANCODE_LCTRL: return "Ctrl izquierdo";
    case SDL_SCANCODE_LALT: return "Alt";
    case SDL_SCANCODE_RALT: return "Alt Gr";
    case SDL_SCANCODE_SPACE: return "Espacio";
    case SDL_SCANCODE_BACKSPACE: return "Retroceso";
    case SDL_SCANCODE_ESCAPE: return "Esc";
    case SDL_SCANCODE_DELETE: return "Supr";
    case SDL_SCANCODE_HOME: return "Inicio";
    case SDL_SCANCODE_END: return "Fin";
    case SDL_SCANCODE_PAGEUP: return "Re Pág";
    case SDL_SCANCODE_PAGEDOWN: return "Av Pág";
    default: {
        const char* n = SDL_GetScancodeName(sc);
        return n && *n ? n : "?";
    }
    }
}



// ---------------------------------------------------------------- puente de mandos (Android)

// ¿SDL ya lee este mando como mando de verdad? Por nombre, o por fabricante y modelo (por USB SDL
// a veces le pone otro nombre).
bool Input::RealGamepadFor(const Bridge& b) const {
    int n = 0;
    bool found = false;
    if (SDL_JoystickID* js = SDL_GetJoysticks(&n)) {
        for (int i = 0; i < n && !found; ++i) {
            // Solo cuenta si SDL lo lee como mando de verdad; si lo ve sin mapa, va por el puente.
            if (SDL_IsJoystickVirtual(js[i]) || !SDL_IsGamepad(js[i])) continue;
            const char* jn = SDL_GetJoystickNameForID(js[i]);
            if (jn && b.name == jn) found = true;
            if (b.vendor && b.product && SDL_GetJoystickVendorForID(js[i]) == b.vendor && SDL_GetJoystickProductForID(js[i]) == b.product)
                found = true;
        }
        SDL_free(js);
    }
    return found;
}

void Input::BridgeOffer(int android_id, const std::string& name, int vendor, int product) {
    for (const auto& b : bridges_)
        if (b.android_id == android_id) return;
    Bridge b;
    b.android_id = android_id;
    b.name = name.empty() ? "Mando" : name;
    b.vendor = vendor;
    b.product = product;
    b.since = SDL_GetTicks();
    bridges_.push_back(b);
    Log::Info("Android ve un mando: %s (id %d)", b.name.c_str(), android_id);
}

void Input::BridgeDetach(Bridge& b) {
    if (b.joy) SDL_CloseJoystick(b.joy);
    if (b.jid) SDL_DetachVirtualJoystick(b.jid);
    b.joy = nullptr;
    b.jid = 0;
}

void Input::BridgeRemove(int android_id) {
    for (size_t i = 0; i < bridges_.size(); ++i) {
        if (bridges_[i].android_id != android_id) continue;
        if (bridges_[i].jid) Log::Info("Puente de mando quitado: %s", bridges_[i].name.c_str());
        BridgeDetach(bridges_[i]);
        bridges_.erase(bridges_.begin() + i);
        return;
    }
}

void Input::BridgeTick() {
    const Uint64 now = SDL_GetTicks();
    for (auto& b : bridges_) {
        if (b.sdl_has_it) continue;
        if (b.jid) {
            // SDL lo tomó por su cuenta: se usa el suyo (con vibración y todo) y se quita el puente.
            if (RealGamepadFor(b)) {
                Log::Info("SDL ya toma %s: quito el puente", b.name.c_str());
                BridgeDetach(b);
                b.sdl_has_it = true;
            }
            continue;
        }
        // SDL revisa los mandos cada ~3 s: se le da tiempo antes de decidir.
        if (now - b.since < 4000) continue;
        if (RealGamepadFor(b)) {
            b.sdl_has_it = true;
            continue;
        }
        SDL_VirtualJoystickDesc desc;
        SDL_INIT_INTERFACE(&desc);
        desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
        desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
        desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
        desc.name = b.name.c_str();
        b.jid = SDL_AttachVirtualJoystick(&desc);
        if (b.jid) b.joy = SDL_OpenJoystick(b.jid);
        if (b.joy) {
            // Gatillos sueltos en 0 (el centro de un eje de gatillo en SDL es -32768).
            SDL_SetJoystickVirtualAxis(b.joy, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, -32768);
            SDL_SetJoystickVirtualAxis(b.joy, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, -32768);
            Log::Info("Puente de mando (como Satronho NX): %s", b.name.c_str());
        } else {
            Log::Warn("No pude hacer el puente de %s: %s", b.name.c_str(), SDL_GetError());
            BridgeDetach(b);
            b.sdl_has_it = true; // no insistir cada cuadro
        }
    }
}

void Input::BridgeButton(int android_id, int button, bool down) {
    if (button < 0 || button >= SDL_GAMEPAD_BUTTON_COUNT) return;
    for (auto& b : bridges_)
        if (b.android_id == android_id && b.joy) SDL_SetJoystickVirtualButton(b.joy, button, down);
}

void Input::BridgeAxis(int android_id, int axis, int value) {
    if (axis < 0 || axis >= SDL_GAMEPAD_AXIS_COUNT) return;
    value = std::clamp(value, -32768, 32767);
    for (auto& b : bridges_)
        if (b.android_id == android_id && b.joy) SDL_SetJoystickVirtualAxis(b.joy, axis, static_cast<Sint16>(value));
}

std::vector<int> Input::BridgedIds() const {
    std::vector<int> out;
    for (const auto& b : bridges_)
        if (b.joy) out.push_back(b.android_id);
    return out;
}

} // namespace Satronho
