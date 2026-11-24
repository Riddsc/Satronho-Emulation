// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <SDL3/SDL.h>

#include "core/core_host.h"
#include "gfx/gl.h"

namespace Satronho {

class VulkanBridge;

// Atiende a los núcleos que dibujan con la tarjeta gráfica.
//
// OpenGL: el núcleo tiene su propio contexto, que comparte texturas con el de la ventana. Dibuja
// en una textura nuestra (a través de un framebuffer) y la ventana la muestra directamente.
// Vulkan: el núcleo dibuja en su imagen de Vulkan; se copia a la memoria y se sube a una textura de
// OpenGL para mostrarla (funciona en cualquier tarjeta con Vulkan).
class HwRender final : public HwRenderHost {
public:
    HwRender();
    ~HwRender() override;

    void Init(SDL_Window* window, SDL_GLContext main_ctx);
    void Shutdown();
    // Qué API conviene ofrecer al núcleo de la consola actual (RETRO_HW_CONTEXT_*).
    // RETRO_HW_CONTEXT_NONE = que dibuje con el procesador.
    void SetWanted(unsigned api) { wanted_ = api; }

    // ¿Hay Vulkan en este equipo? (se revisa una vez, en segundo plano, al abrir la app)
    void ProbeVulkan();
    bool VulkanAvailable() const { return vk_ok_; }
    bool VulkanProbed() const { return vk_probed_; }
    std::string VulkanGpu() const;

    // HwRenderHost
    bool Accept(retro_hw_render_callback* cb) override;
    unsigned Preferred() const override;
    bool Interface(const retro_hw_render_interface** out) override;
    bool Negotiation(const retro_hw_render_context_negotiation_interface* iface) override;
    bool Start(const retro_system_av_info& av) override;
    void Stop() override;
    void Enter() override;
    void Leave() override;
    void Frame(unsigned w, unsigned h) override;
    void Geometry(const retro_system_av_info& av) override;
    uintptr_t Framebuffer() override { return fbo_; }
    retro_proc_address_t ProcAddress(const char* sym) override;

    // Para dibujar en la ventana (en el contexto principal).
    bool Active() const { return active_; }
    GLuint Texture() const { return tex_; }
    int TexWidth() const { return tex_w_; }
    int TexHeight() const { return tex_h_; }
    unsigned FrameWidth() const { return frame_w_; }
    unsigned FrameHeight() const { return frame_h_; }
    bool BottomLeft() const { return bottom_left_; }
    bool Swizzle() const { return swizzle_bgra_; } // Vulkan con B8G8R8A8: ya se sube como BGRA
    std::string ApiName() const;                     // «OpenGL 4.6» o «Vulkan»
    unsigned Api() const { return api_; }
    // Copia el último cuadro en RGBA (capturas, miniaturas de estados, fondo ambiente).
    bool ReadRGBA(std::vector<uint8_t>& out, unsigned* w, unsigned* h);

private:
    friend class VulkanBridge;
    bool StartGl(const retro_system_av_info& av);
    void StopGl();
    void EnsureTexture(unsigned w, unsigned h, bool core_ctx_current);

    SDL_Window* window_ = nullptr;
    SDL_GLContext main_ = nullptr;
    SDL_GLContext core_ctx_ = nullptr;
    unsigned wanted_ = RETRO_HW_CONTEXT_OPENGL_CORE;
    unsigned api_ = RETRO_HW_CONTEXT_NONE; // lo que pidió el núcleo
    retro_hw_render_callback cb_{};
    bool active_ = false;
    bool in_core_ = false;

    GLuint tex_ = 0, fbo_ = 0, depth_ = 0, read_fbo_ = 0;
    int tex_w_ = 0, tex_h_ = 0;
    unsigned frame_w_ = 0, frame_h_ = 0;
    bool bottom_left_ = true;
    bool swizzle_bgra_ = false;

    VulkanBridge* vk_ = nullptr;
    const retro_hw_render_context_negotiation_interface* negotiation_ = nullptr;
    std::atomic<bool> vk_ok_{false}, vk_probed_{false};
    std::thread probe_;
    mutable std::mutex probe_mutex_;
    std::string vk_gpu_;
};

} // namespace Satronho
