// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "libretro.h"

namespace Satronho {

class HwRender;
struct VulkanState;

// La parte de Vulkan: crea la instancia y el dispositivo (o deja que el núcleo los cree), le da
// al núcleo la interfaz de libretro y copia cada cuadro a una textura de OpenGL.
// Vulkan se carga al vuelo (vulkan-1.dll): si el equipo no lo tiene, todo sigue con OpenGL.
class VulkanBridge {
public:
    explicit VulkanBridge(HwRender* owner);
    ~VulkanBridge();

    static bool Probe(std::string* gpu_name);
    bool Start(const retro_hw_render_context_negotiation_interface* negotiation);
    void Stop(const retro_hw_render_context_negotiation_interface* negotiation);
    const retro_hw_render_interface* Interface();
    void CopyFrame(unsigned w, unsigned h);
    bool LastFrame(std::vector<uint8_t>& out);

private:
    HwRender* owner_;
    VulkanState* st_ = nullptr;
};

} // namespace Satronho
