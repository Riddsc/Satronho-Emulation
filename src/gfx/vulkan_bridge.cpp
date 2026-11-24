// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#define VK_NO_PROTOTYPES
#include "gfx/vulkan_bridge.h"

#include <algorithm>
#include <cstring>

#include <SDL3/SDL.h>

#include "gfx/gl.h"
#include "gfx/hw_render.h"
#include "libretro_vulkan.h"
#include "util/log.h"

namespace Satronho {

namespace {
#define SATRONHO_VK_INSTANCE(X)                     \
    X(vkDestroyInstance)                            \
    X(vkEnumeratePhysicalDevices)                   \
    X(vkGetPhysicalDeviceProperties)                \
    X(vkGetPhysicalDeviceQueueFamilyProperties)     \
    X(vkGetPhysicalDeviceMemoryProperties)          \
    X(vkGetPhysicalDeviceFeatures)                  \
    X(vkCreateDevice)                               \
    X(vkGetDeviceProcAddr)

#define SATRONHO_VK_DEVICE(X)            \
    X(vkDestroyDevice)                   \
    X(vkGetDeviceQueue)                  \
    X(vkDeviceWaitIdle)                  \
    X(vkCreateBuffer)                    \
    X(vkDestroyBuffer)                   \
    X(vkGetBufferMemoryRequirements)     \
    X(vkAllocateMemory)                  \
    X(vkFreeMemory)                      \
    X(vkBindBufferMemory)                \
    X(vkMapMemory)                       \
    X(vkUnmapMemory)                     \
    X(vkInvalidateMappedMemoryRanges)    \
    X(vkCreateCommandPool)               \
    X(vkDestroyCommandPool)              \
    X(vkAllocateCommandBuffers)          \
    X(vkBeginCommandBuffer)              \
    X(vkEndCommandBuffer)                \
    X(vkResetCommandBuffer)              \
    X(vkCmdPipelineBarrier)              \
    X(vkCmdCopyImageToBuffer)            \
    X(vkQueueSubmit)                     \
    X(vkCreateFence)                     \
    X(vkDestroyFence)                    \
    X(vkWaitForFences)                   \
    X(vkResetFences)

struct Loader {
    void* lib = nullptr;
    PFN_vkGetInstanceProcAddr gipa = nullptr;
    PFN_vkCreateInstance create_instance = nullptr;
    bool Load() {
        if (gipa) return true;
#ifdef _WIN32
        const char* names[] = {"vulkan-1.dll"};
#elif defined(__APPLE__)
        const char* names[] = {"libvulkan.1.dylib", "libMoltenVK.dylib"};
#else
        const char* names[] = {"libvulkan.so.1", "libvulkan.so"};
#endif
        for (const char* n : names) {
            lib = SDL_LoadObject(n);
            if (lib) break;
        }
        if (!lib) return false;
        gipa = reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_LoadFunction(static_cast<SDL_SharedObject*>(lib), "vkGetInstanceProcAddr"));
        if (!gipa) return false;
        create_instance = reinterpret_cast<PFN_vkCreateInstance>(gipa(VK_NULL_HANDLE, "vkCreateInstance"));
        return create_instance != nullptr;
    }
};
Loader& Vk() {
    static Loader l;
    return l;
}

VkApplicationInfo AppInfo() {
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "Satronho Emulation";
    app.applicationVersion = 1;
    app.pEngineName = "Satronho";
    app.engineVersion = 1;
    app.apiVersion = VK_API_VERSION_1_1;
    return app;
}
} // namespace

struct VulkanState {
    HwRender* owner = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice gpu = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    uint32_t queue_family = 0;
    bool core_made_device = false;
#define SATRONHO_VK_DECL(name) PFN_##name name = nullptr;
    SATRONHO_VK_INSTANCE(SATRONHO_VK_DECL)
    SATRONHO_VK_DEVICE(SATRONHO_VK_DECL)
#undef SATRONHO_VK_DECL

    retro_hw_render_interface_vulkan iface{};
    std::mutex queue_mutex;

    // Lo que entregó el núcleo para este cuadro.
    retro_vulkan_image image{};
    bool has_image = false;
    std::vector<VkSemaphore> wait_semaphores;
    VkSemaphore signal_semaphore = VK_NULL_HANDLE;
    uint32_t src_family = VK_QUEUE_FAMILY_IGNORED;

    // Copia a la memoria.
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize buffer_size = 0;
    void* mapped = nullptr;
    bool coherent = true;
    std::vector<uint8_t> last; // último cuadro en RGBA (para capturas)
    unsigned last_w = 0, last_h = 0;
    bool last_bgra = false;

    bool LoadInstance() {
#define SATRONHO_VK_LOAD(name) name = reinterpret_cast<PFN_##name>(Vk().gipa(instance, #name));
        SATRONHO_VK_INSTANCE(SATRONHO_VK_LOAD)
#undef SATRONHO_VK_LOAD
        return vkEnumeratePhysicalDevices && vkCreateDevice && vkGetDeviceProcAddr;
    }
    bool LoadDevice() {
#define SATRONHO_VK_LOAD(name) name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(device, #name));
        SATRONHO_VK_DEVICE(SATRONHO_VK_LOAD)
#undef SATRONHO_VK_LOAD
        return vkQueueSubmit && vkCmdCopyImageToBuffer;
    }
    uint32_t MemoryType(uint32_t bits, VkMemoryPropertyFlags want, bool* coherent_out) {
        VkPhysicalDeviceMemoryProperties props;
        vkGetPhysicalDeviceMemoryProperties(gpu, &props);
        // Primero memoria visible y coherente con caché (leer rápido desde el procesador).
        const VkMemoryPropertyFlags tries[] = {want | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
                                               want | VK_MEMORY_PROPERTY_HOST_CACHED_BIT, want | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, want};
        for (VkMemoryPropertyFlags f : tries)
            for (uint32_t i = 0; i < props.memoryTypeCount; ++i)
                if ((bits & (1u << i)) && (props.memoryTypes[i].propertyFlags & f) == f) {
                    *coherent_out = props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
                    return i;
                }
        return UINT32_MAX;
    }
};

namespace {
// Funciones que el núcleo llama a través de la interfaz.
VulkanState* S(void* h) { return static_cast<VulkanState*>(h); }
void SetImage(void* h, const retro_vulkan_image* image, uint32_t n, const VkSemaphore* sems, uint32_t src_family) {
    VulkanState* s = S(h);
    if (image) {
        s->image = *image;
        s->has_image = true;
    } else {
        s->has_image = false;
    }
    s->wait_semaphores.assign(sems, sems + n);
    s->src_family = src_family;
}
uint32_t GetSyncIndex(void*) { return 0; }
uint32_t GetSyncIndexMask(void*) { return 1; } // se espera cada cuadro: con un índice basta
void SetCommandBuffers(void* h, uint32_t num, const VkCommandBuffer* cmds) {
    // Pocos núcleos lo usan: se envían en el momento.
    VulkanState* s = S(h);
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = num;
    si.pCommandBuffers = cmds;
    std::lock_guard lock(s->queue_mutex);
    s->vkQueueSubmit(s->queue, 1, &si, VK_NULL_HANDLE);
}
void WaitSyncIndex(void* h) {
    VulkanState* s = S(h);
    if (s->device) s->vkDeviceWaitIdle(s->device);
}
void LockQueue(void* h) { S(h)->queue_mutex.lock(); }
void UnlockQueue(void* h) { S(h)->queue_mutex.unlock(); }
void SetSignalSemaphore(void* h, VkSemaphore sem) { S(h)->signal_semaphore = sem; }
} // namespace

VulkanBridge::VulkanBridge(HwRender* owner) : owner_(owner) {}

VulkanBridge::~VulkanBridge() { delete st_; }

bool VulkanBridge::Probe(std::string* gpu_name) {
    if (!Vk().Load()) return false;
    VkApplicationInfo app = AppInfo();
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ci.pApplicationInfo = &app;
    VkInstance inst = VK_NULL_HANDLE;
    if (Vk().create_instance(&ci, nullptr, &inst) != VK_SUCCESS) return false;
    auto destroy = reinterpret_cast<PFN_vkDestroyInstance>(Vk().gipa(inst, "vkDestroyInstance"));
    auto enumerate = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(Vk().gipa(inst, "vkEnumeratePhysicalDevices"));
    auto props = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(Vk().gipa(inst, "vkGetPhysicalDeviceProperties"));
    uint32_t n = 0;
    bool ok = false;
    if (enumerate && props && enumerate(inst, &n, nullptr) == VK_SUCCESS && n > 0) {
        std::vector<VkPhysicalDevice> gpus(n);
        enumerate(inst, &n, gpus.data());
        // La mejor: dedicada, luego integrada; la de software (lavapipe) solo si no hay otra.
        int best = -1;
        VkPhysicalDeviceType best_type = VK_PHYSICAL_DEVICE_TYPE_OTHER;
        for (uint32_t i = 0; i < n; ++i) {
            VkPhysicalDeviceProperties p;
            props(gpus[i], &p);
            auto rank = [](VkPhysicalDeviceType t) {
                return t == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 4 : t == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 3 : t == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU ? 2 : 1;
            };
            if (best < 0 || rank(p.deviceType) > rank(best_type)) {
                best = static_cast<int>(i);
                best_type = p.deviceType;
                if (gpu_name) *gpu_name = p.deviceName;
            }
        }
        ok = best >= 0;
    }
    if (destroy) destroy(inst, nullptr);
    return ok;
}

bool VulkanBridge::Start(const retro_hw_render_context_negotiation_interface* negotiation) {
    Stop(nullptr);
    if (!Vk().Load()) return false;
    st_ = new VulkanState();
    st_->owner = owner_;
    VulkanState& s = *st_;
    const auto* neg = negotiation && negotiation->interface_type == RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN
                          ? reinterpret_cast<const retro_hw_render_context_negotiation_interface_vulkan*>(negotiation)
                          : nullptr;

    // Instancia.
    VkApplicationInfo app = AppInfo();
    if (neg && neg->get_application_info) {
        if (const VkApplicationInfo* a = neg->get_application_info()) {
            app = *a;
            if (app.apiVersion < VK_API_VERSION_1_1) app.apiVersion = VK_API_VERSION_1_1;
        }
    }
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ci.pApplicationInfo = &app;
    if (Vk().create_instance(&ci, nullptr, &s.instance) != VK_SUCCESS || !s.LoadInstance()) {
        Log::Error("Vulkan: no se pudo crear la instancia");
        Stop(nullptr);
        return false;
    }

    // Tarjeta: dedicada si hay.
    uint32_t n = 0;
    s.vkEnumeratePhysicalDevices(s.instance, &n, nullptr);
    std::vector<VkPhysicalDevice> gpus(n);
    if (n) s.vkEnumeratePhysicalDevices(s.instance, &n, gpus.data());
    int best_rank = -1;
    for (VkPhysicalDevice g : gpus) {
        VkPhysicalDeviceProperties p;
        s.vkGetPhysicalDeviceProperties(g, &p);
        const int rank = p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 4 : p.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 3 : 1;
        if (rank > best_rank) {
            best_rank = rank;
            s.gpu = g;
        }
    }
    if (!s.gpu) {
        Stop(nullptr);
        return false;
    }

    // Dispositivo: si el núcleo sabe crearlo (pide extensiones propias), lo crea él.
    if (neg && neg->create_device) {
        retro_vulkan_context ctx{};
        // Sin extensiones ni capas obligatorias (no se presenta con Vulkan), pero con una lista de
        // funciones vacía de verdad: algunos núcleos no aceptan punteros nulos.
        static const char* no_names[] = {nullptr};
        VkPhysicalDeviceFeatures none{};
        if (neg->create_device(&ctx, s.instance, s.gpu, VK_NULL_HANDLE, Vk().gipa, no_names, 0, no_names, 0, &none)) {
            s.gpu = ctx.gpu;
            s.device = ctx.device;
            s.queue = ctx.queue;
            s.queue_family = ctx.queue_family_index;
            s.core_made_device = true;
        }
    }
    if (!s.device) {
        uint32_t qn = 0;
        s.vkGetPhysicalDeviceQueueFamilyProperties(s.gpu, &qn, nullptr);
        std::vector<VkQueueFamilyProperties> qf(qn);
        s.vkGetPhysicalDeviceQueueFamilyProperties(s.gpu, &qn, qf.data());
        int family = -1;
        for (uint32_t i = 0; i < qn; ++i)
            if ((qf[i].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) == (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) {
                family = static_cast<int>(i);
                break;
            }
        if (family < 0) {
            Stop(nullptr);
            return false;
        }
        const float prio = 1.0f;
        VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        qi.queueFamilyIndex = static_cast<uint32_t>(family);
        qi.queueCount = 1;
        qi.pQueuePriorities = &prio;
        VkPhysicalDeviceFeatures features;
        s.vkGetPhysicalDeviceFeatures(s.gpu, &features);
        VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        di.queueCreateInfoCount = 1;
        di.pQueueCreateInfos = &qi;
        di.pEnabledFeatures = &features;
        if (s.vkCreateDevice(s.gpu, &di, nullptr, &s.device) != VK_SUCCESS) {
            Log::Error("Vulkan: no se pudo crear el dispositivo");
            Stop(nullptr);
            return false;
        }
        s.queue_family = static_cast<uint32_t>(family);
    }
    if (!s.LoadDevice()) {
        Stop(negotiation);
        return false;
    }
    if (!s.queue) s.vkGetDeviceQueue(s.device, s.queue_family, 0, &s.queue);

    VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pi.queueFamilyIndex = s.queue_family;
    s.vkCreateCommandPool(s.device, &pi, nullptr, &s.pool);
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = s.pool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    s.vkAllocateCommandBuffers(s.device, &ai, &s.cmd);
    VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    s.vkCreateFence(s.device, &fi, nullptr, &s.fence);

    auto& it = s.iface;
    it.interface_type = RETRO_HW_RENDER_INTERFACE_VULKAN;
    it.interface_version = RETRO_HW_RENDER_INTERFACE_VULKAN_VERSION;
    it.handle = &s;
    it.instance = s.instance;
    it.gpu = s.gpu;
    it.device = s.device;
    it.get_device_proc_addr = s.vkGetDeviceProcAddr;
    it.get_instance_proc_addr = Vk().gipa;
    it.queue = s.queue;
    it.queue_index = s.queue_family;
    it.set_image = &SetImage;
    it.get_sync_index = &GetSyncIndex;
    it.get_sync_index_mask = &GetSyncIndexMask;
    it.set_command_buffers = &SetCommandBuffers;
    it.wait_sync_index = &WaitSyncIndex;
    it.lock_queue = &LockQueue;
    it.unlock_queue = &UnlockQueue;
    it.set_signal_semaphore = &SetSignalSemaphore;

    VkPhysicalDeviceProperties p;
    s.vkGetPhysicalDeviceProperties(s.gpu, &p);
    Log::Info("Vulkan listo: %s%s", p.deviceName, s.core_made_device ? " (dispositivo del núcleo)" : "");
    return true;
}

void VulkanBridge::Stop(const retro_hw_render_context_negotiation_interface* negotiation) {
    if (!st_) return;
    VulkanState& s = *st_;
    if (s.device) {
        if (s.vkDeviceWaitIdle) s.vkDeviceWaitIdle(s.device);
        if (s.mapped) s.vkUnmapMemory(s.device, s.memory);
        if (s.buffer) s.vkDestroyBuffer(s.device, s.buffer, nullptr);
        if (s.memory) s.vkFreeMemory(s.device, s.memory, nullptr);
        if (s.fence) s.vkDestroyFence(s.device, s.fence, nullptr);
        if (s.pool) s.vkDestroyCommandPool(s.device, s.pool, nullptr);
    }
    const auto* neg = negotiation && negotiation->interface_type == RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN
                          ? reinterpret_cast<const retro_hw_render_context_negotiation_interface_vulkan*>(negotiation)
                          : nullptr;
    if (s.core_made_device && neg && neg->destroy_device) neg->destroy_device();
    if (s.device && s.vkDestroyDevice) s.vkDestroyDevice(s.device, nullptr);
    if (s.instance && s.vkDestroyInstance) s.vkDestroyInstance(s.instance, nullptr);
    delete st_;
    st_ = nullptr;
}

const retro_hw_render_interface* VulkanBridge::Interface() {
    return st_ ? reinterpret_cast<const retro_hw_render_interface*>(&st_->iface) : nullptr;
}

void VulkanBridge::CopyFrame(unsigned w, unsigned h) {
    if (!st_ || !st_->has_image) return;
    VulkanState& s = *st_;
    const VkImage image = s.image.create_info.image;
    const VkFormat fmt = s.image.create_info.format;
    const bool bgra = fmt == VK_FORMAT_B8G8R8A8_UNORM || fmt == VK_FORMAT_B8G8R8A8_SRGB;
    if (!(bgra || fmt == VK_FORMAT_R8G8B8A8_UNORM || fmt == VK_FORMAT_R8G8B8A8_SRGB || fmt == VK_FORMAT_A8B8G8R8_UNORM_PACK32)) {
        static bool warned = false;
        if (!warned) Log::Warn("Vulkan: formato de imagen %d sin soporte", static_cast<int>(fmt));
        warned = true;
        return;
    }
    const VkDeviceSize need = static_cast<VkDeviceSize>(w) * h * 4;
    if (need > s.buffer_size) {
        if (s.mapped) s.vkUnmapMemory(s.device, s.memory);
        if (s.buffer) s.vkDestroyBuffer(s.device, s.buffer, nullptr);
        if (s.memory) s.vkFreeMemory(s.device, s.memory, nullptr);
        s.mapped = nullptr;
        VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bi.size = need;
        bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        s.vkCreateBuffer(s.device, &bi, nullptr, &s.buffer);
        VkMemoryRequirements req;
        s.vkGetBufferMemoryRequirements(s.device, s.buffer, &req);
        VkMemoryAllocateInfo mi{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        mi.allocationSize = req.size;
        mi.memoryTypeIndex = s.MemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &s.coherent);
        if (mi.memoryTypeIndex == UINT32_MAX || s.vkAllocateMemory(s.device, &mi, nullptr, &s.memory) != VK_SUCCESS) {
            Log::Error("Vulkan: sin memoria para copiar el cuadro");
            s.vkDestroyBuffer(s.device, s.buffer, nullptr);
            s.buffer = VK_NULL_HANDLE;
            s.buffer_size = 0;
            return;
        }
        s.vkBindBufferMemory(s.device, s.buffer, s.memory, 0);
        s.vkMapMemory(s.device, s.memory, 0, VK_WHOLE_SIZE, 0, &s.mapped);
        s.buffer_size = need;
    }

    s.vkResetCommandBuffer(s.cmd, 0);
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    s.vkBeginCommandBuffer(s.cmd, &begin);
    const bool transfer_family = s.src_family != VK_QUEUE_FAMILY_IGNORED && s.src_family != s.queue_family;
    VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    b.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    b.oldLayout = s.image.image_layout;
    b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    b.srcQueueFamilyIndex = transfer_family ? s.src_family : VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = transfer_family ? s.queue_family : VK_QUEUE_FAMILY_IGNORED;
    b.image = image;
    b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    s.vkCmdPipelineBarrier(s.cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b);
    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {w, h, 1};
    s.vkCmdCopyImageToBuffer(s.cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, s.buffer, 1, &region);
    // De vuelta como estaba, para que el núcleo la siga usando.
    VkImageMemoryBarrier back = b;
    back.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    back.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    back.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    back.newLayout = s.image.image_layout;
    back.srcQueueFamilyIndex = transfer_family ? s.queue_family : VK_QUEUE_FAMILY_IGNORED;
    back.dstQueueFamilyIndex = transfer_family ? s.src_family : VK_QUEUE_FAMILY_IGNORED;
    s.vkCmdPipelineBarrier(s.cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &back);
    s.vkEndCommandBuffer(s.cmd);

    std::vector<VkPipelineStageFlags> stages(s.wait_semaphores.size(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.waitSemaphoreCount = static_cast<uint32_t>(s.wait_semaphores.size());
    si.pWaitSemaphores = s.wait_semaphores.data();
    si.pWaitDstStageMask = stages.data();
    si.commandBufferCount = 1;
    si.pCommandBuffers = &s.cmd;
    if (s.signal_semaphore) {
        si.signalSemaphoreCount = 1;
        si.pSignalSemaphores = &s.signal_semaphore;
    }
    {
        std::lock_guard lock(s.queue_mutex);
        s.vkResetFences(s.device, 1, &s.fence);
        if (s.vkQueueSubmit(s.queue, 1, &si, s.fence) != VK_SUCCESS) return;
    }
    s.signal_semaphore = VK_NULL_HANDLE;
    s.wait_semaphores.clear();
    s.vkWaitForFences(s.device, 1, &s.fence, VK_TRUE, 2000000000ull);
    if (!s.coherent) {
        VkMappedMemoryRange r{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
        r.memory = s.memory;
        r.size = VK_WHOLE_SIZE;
        s.vkInvalidateMappedMemoryRanges(s.device, 1, &r);
    }
    // A la textura de OpenGL (contexto principal, que es el activo con Vulkan).
    Gl::BindTexture(GL_TEXTURE_2D, owner_->Texture());
    Gl::PixelStorei(GL_UNPACK_ALIGNMENT, 4);
    Gl::PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    Gl::TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, bgra ? GL_BGRA : GL_RGBA, GL_UNSIGNED_BYTE, s.mapped);
    Gl::BindTexture(GL_TEXTURE_2D, 0);
    s.last_w = w;
    s.last_h = h;
    s.last_bgra = bgra;
}

bool VulkanBridge::LastFrame(std::vector<uint8_t>& out) {
    if (!st_ || !st_->mapped || !st_->last_w) return false;
    VulkanState& s = *st_;
    out.assign(static_cast<const uint8_t*>(s.mapped), static_cast<const uint8_t*>(s.mapped) + static_cast<size_t>(s.last_w) * s.last_h * 4);
    for (size_t i = 0; i < out.size(); i += 4) {
        if (s.last_bgra) std::swap(out[i], out[i + 2]);
        out[i + 3] = 255;
    }
    return true;
}

} // namespace Satronho
