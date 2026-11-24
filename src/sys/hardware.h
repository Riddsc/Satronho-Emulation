// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <vector>

struct SDL_Renderer;
struct SDL_Window;

namespace Satronho {

struct HardwareInfo {
    std::string cpu = "Desconocido";
    int threads = 1;
    int ram_mb = 0;
    std::string gpu = "Desconocida";
    int vram_mb = 0;
    std::string renderer;         // motor gráfico que eligió SDL
    bool software = false;        // dibujo por software (sin tarjeta gráfica)
    bool gpu_render = false;      // los núcleos pueden dibujar con la tarjeta gráfica (OpenGL/Vulkan)
    int display_w = 0, display_h = 0;
    float refresh = 60.0f;
    std::string os;
};

HardwareInfo DetectHardware(SDL_Window* window, SDL_Renderer* renderer);

// Uso del proceso para el medidor.
struct ProcessUsage {
    double cpu_percent = 0.0;  // del total del equipo, como en el Administrador de tareas
    double ram_mb = 0.0;
};

class UsageSampler {
public:
    // Devuelve el uso medido desde la última llamada (llamar ~2 veces por segundo).
    ProcessUsage Sample(int threads);

private:
    double last_cpu_s_ = -1.0;
    double last_wall_s_ = 0.0;
    ProcessUsage last_{};
};

} // namespace Satronho
