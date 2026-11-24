// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <map>
#include <string>
#include <vector>

#include "sys/hardware.h"

namespace Satronho {

class Ini;

enum class ScaleMode { Fit = 0, Integer = 1, Stretch = 2 };
enum class Filter { Pixel = 0, SharpSmooth = 1, Smooth = 2 };
enum class Effect { None = 0, Scanlines = 1, Crt = 2, Lcd = 3, Auto = 4 };

// Imagen propia de una consola (si está activada, manda sobre la general).
struct VideoOverride {
    bool on = false;
    int scale_mode = 0;
    int filter = 1;
    int effect = 0;
    float effect_strength = 0.45f;
};

struct Settings {
    // Video
    bool fullscreen = false;
    int scale_mode = static_cast<int>(ScaleMode::Fit);
    int filter = static_cast<int>(Filter::SharpSmooth);
    int effect = static_cast<int>(Effect::None);
    float effect_strength = 0.45f;
    bool ambient = true;           // fondo con los colores del juego
    bool vsync = true;
    bool fullscreen_60hz = true;  // en pantalla completa, cambiar la pantalla a 60 Hz
    std::string renderer = "auto"; // motor gráfico (se aplica al reiniciar)

    // Audio
    float volume = 0.8f;
    int latency_ms = 64;
    bool mute_fast_forward = true;

    std::map<std::string, VideoOverride> video_for; // por consola (id)

    // Tarjeta gráfica en CD y 64: motor (0 automático, 1 OpenGL, 2 Vulkan, 3 procesador) y
    // resolución interna (0 automática, 1 a 8 veces la original).
    std::map<std::string, int> gpu_api, gpu_res;
    int GpuApi(const std::string& sys) const { auto it = gpu_api.find(sys); return it == gpu_api.end() ? 0 : it->second; }
    std::map<std::string, std::map<std::string, std::string>> gfx; // opciones de gráficos del núcleo por consola
    int GpuRes(const std::string& sys) const { auto it = gpu_res.find(sys); return it == gpu_res.end() ? 0 : it->second; }

    // Rendimiento
    int runahead = 0;              // cuadros de adelanto para reducir el retraso (solo NES por ahora)
    bool rewind = true;
    bool rewind_heavy = false;     // también en PS1, DS, Saturn y 64 (estados de varios MB)
    int rewind_seconds = 10;
    int rewind_mb = 256;           // tope de memoria del historial (comprimido)
    bool fast_forward_enabled = false; // apagado de fábrica: el gatillo choca con botones del juego
    int fast_forward = 3;          // ×
    bool pause_on_focus_loss = false;

    // Juego
    bool autosave_exit = true;     // al salir, guarda dónde quedaste (ranura automática)
    bool rumble = true;            // vibración del mando

    // Medidor
    int meter = 1;                 // 0 apagado, 1 compacto, 2 detallado
    int meter_corner = 1;          // 0 arriba izq., 1 arriba der., 2 abajo izq., 3 abajo der.
    int meter_color = 0;

    // Guías e interfaz
    bool guides = true;
    bool bubble = true;
    float ui_scale = 0.0f;           // 0 = automático según la pantalla
    int theme = 0;                   // tema de colores (Ui::Themes)
    float watermark = 0.18f;         // opacidad del logo de fondo: de 0.10 a 1.0 (nunca se apaga)
    bool confirm_exit_app = true;    // preguntar antes de cerrar la app
    bool confirm_exit_game = true;   // preguntar antes de salir de un juego
    bool game_bar = true;
    bool status_bar = true;          // barra de estado abajo del juego            // barra de menús arriba del juego (en ventana)

    // Control en pantalla (pantallas táctiles).
    int touch_show = 0;              // 0 automático (sin mando conectado), 1 siempre, 2 nunca
    float touch_scale = 1.0f;
    float touch_opacity = 0.6f;
    bool touch_haptics = true;       // vibración corta al tocar un botón
    std::map<std::string, std::pair<float, float>> touch_offsets; // «<consola>.<grupo>» → corrimiento

    // Biblioteca
    std::vector<std::string> folders;
    int sort = 0;                  // 0 A-Z, 1 recientes, 2 más jugados
    bool favorites_only = false;
    std::map<std::string, int> lib_view; // vista por consola ("" = todas): 0 cuadrícula, 1 carrusel, 2 lista
    int cover_size = 1;              // carátulas: 0 pequeñas, 1 medianas, 2 grandes
    bool covers = true;            // descargar carátulas reales de los juegos
    int LibView(const std::string& sys) const {
        auto it = lib_view.find(sys.empty() ? "todas" : sys);
        return it == lib_view.end() ? 0 : it->second;
    }

    // Actualizaciones
    bool auto_update = true;

    // Estado
    bool first_run_done = false;
    std::string preset;            // última configuración sugerida aplicada
    int last_seen_build = 0;

    void Load(const Ini& ini);
    void Save(Ini& ini) const;

    // Ajustes de imagen que valen para una consola (los suyos si los tiene, si no los generales).
    Settings ForSystem(const std::string& sys) const;
};

// Qué tan bien va cada consola en este equipo (lo dice Ñoño).
struct SystemVerdict {
    std::string system;  // id
    int level;           // 0 va perfecto, 1 va bien, 2 puede ir justo, 3 puede ir lento
    std::string text;
};

// Configuración sugerida según el equipo (la revisa Ñoño).
struct Suggestion {
    std::string tier;                  // "Alto", "Equilibrado" o "Básico"
    std::string summary;               // frase de Ñoño
    std::vector<std::string> reasons;  // por qué
    std::vector<std::string> changes;  // qué cambia
    Settings result;
    std::vector<SystemVerdict> systems;
};

Suggestion Suggest(const HardwareInfo& hw, const Settings& current);

const char* MeterColorName(int index);
unsigned MeterColor(int index); // 0xRRGGBB

} // namespace Satronho
