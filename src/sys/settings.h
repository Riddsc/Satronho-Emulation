// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <vector>

#include "sys/hardware.h"

namespace Satronho {

class Ini;

enum class ScaleMode { Fit = 0, Integer = 1, Stretch = 2 };
enum class Filter { Pixel = 0, SharpSmooth = 1, Smooth = 2 };
enum class Effect { None = 0, Scanlines = 1, Crt = 2 };

struct Settings {
    // Video
    bool fullscreen = false;
    int scale_mode = static_cast<int>(ScaleMode::Fit);
    int filter = static_cast<int>(Filter::SharpSmooth);
    int effect = static_cast<int>(Effect::None);
    float effect_strength = 0.45f;
    bool ambient = true;           // fondo con los colores del juego
    bool vsync = true;
    std::string renderer = "auto"; // motor gráfico (se aplica al reiniciar)

    // Audio
    float volume = 0.8f;
    int latency_ms = 64;
    bool mute_fast_forward = true;

    // Rendimiento
    int runahead = 0;              // cuadros de adelanto para reducir el retraso
    bool rewind = true;
    int rewind_seconds = 10;
    int fast_forward = 3;          // ×

    // Medidor
    int meter = 1;                 // 0 apagado, 1 compacto, 2 detallado
    int meter_corner = 1;          // 0 arriba izq., 1 arriba der., 2 abajo izq., 3 abajo der.
    int meter_color = 0;

    // Guías e interfaz
    bool guides = true;
    bool bubble = true;
    float ui_scale = 1.0f;

    // Biblioteca
    std::vector<std::string> folders;
    int sort = 0;                  // 0 A-Z, 1 recientes, 2 más jugados
    bool favorites_only = false;

    // Estado
    bool first_run_done = false;
    std::string preset;            // última configuración sugerida aplicada
    int last_seen_build = 0;

    void Load(const Ini& ini);
    void Save(Ini& ini) const;
};

// Configuración sugerida según el equipo (la revisa Ñoño).
struct Suggestion {
    std::string tier;                  // "Alto", "Equilibrado" o "Básico"
    std::string summary;               // frase de Ñoño
    std::vector<std::string> reasons;  // por qué
    std::vector<std::string> changes;  // qué cambia
    Settings result;
};

Suggestion Suggest(const HardwareInfo& hw, const Settings& current);

const char* MeterColorName(int index);
unsigned MeterColor(int index); // 0xRRGGBB

} // namespace Satronho
