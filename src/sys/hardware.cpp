// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "sys/hardware.h"

#include <cstdio>
#include <cstring>
#include <fstream>

#include <SDL3/SDL.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dxgi.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace Satronho {
namespace {

std::string Trim(std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '\n' || s.back() == '\r' || s.back() == '\0')) s.pop_back();
    size_t i = 0;
    while (i < s.size() && s[i] == ' ') ++i;
    return s.substr(i);
}

#ifdef _WIN32
std::string Narrow(const wchar_t* w) {
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    std::string out(n > 0 ? n - 1 : 0, '\0');
    if (n > 1) WideCharToMultiByte(CP_UTF8, 0, w, -1, out.data(), n, nullptr, nullptr);
    return out;
}

std::string CpuName() {
    HKEY key;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &key) != ERROR_SUCCESS)
        return "Desconocido";
    wchar_t buf[256] = {};
    DWORD size = sizeof(buf) - sizeof(wchar_t);
    DWORD type = 0;
    std::string name = "Desconocido";
    if (RegQueryValueExW(key, L"ProcessorNameString", nullptr, &type, reinterpret_cast<LPBYTE>(buf), &size) == ERROR_SUCCESS)
        name = Trim(Narrow(buf));
    RegCloseKey(key);
    return name;
}

void GpuInfo(HardwareInfo& hw) {
    using CreateFactoryFn = HRESULT(WINAPI*)(REFIID, void**);
    HMODULE dxgi = LoadLibraryW(L"dxgi.dll");
    if (!dxgi) return;
    auto create = reinterpret_cast<CreateFactoryFn>(GetProcAddress(dxgi, "CreateDXGIFactory1"));
    IDXGIFactory1* factory = nullptr;
    if (create && SUCCEEDED(create(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory))) && factory) {
        // Se elige la tarjeta con más memoria dedicada (evita reportar la integrada en portátiles).
        IDXGIAdapter1* adapter = nullptr;
        SIZE_T best = 0;
        for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 desc{};
            if (SUCCEEDED(adapter->GetDesc1(&desc)) && !(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
                if (desc.DedicatedVideoMemory >= best || hw.gpu == "Desconocida") {
                    best = desc.DedicatedVideoMemory;
                    hw.gpu = Trim(Narrow(desc.Description));
                    hw.vram_mb = static_cast<int>(desc.DedicatedVideoMemory / (1024 * 1024));
                }
            }
            adapter->Release();
        }
        factory->Release();
    }
    FreeLibrary(dxgi);
}
#else
std::string CpuName() {
    std::ifstream in("/proc/cpuinfo");
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("model name", 0) == 0) {
            const size_t c = line.find(':');
            if (c != std::string::npos) return Trim(line.substr(c + 1));
        }
    }
    return "Desconocido";
}
void GpuInfo(HardwareInfo&) {}
#endif

} // namespace

HardwareInfo DetectHardware(SDL_Window* window, SDL_Renderer* renderer) {
    HardwareInfo hw;
    hw.cpu = CpuName();
    hw.threads = SDL_GetNumLogicalCPUCores();
    hw.ram_mb = SDL_GetSystemRAM();
    GpuInfo(hw);
    if (renderer) {
        const char* name = SDL_GetRendererName(renderer);
        hw.renderer = name ? name : "";
        hw.software = hw.renderer == "software";
    }
    if (window) {
        const SDL_DisplayID d = SDL_GetDisplayForWindow(window);
        if (const SDL_DisplayMode* m = SDL_GetCurrentDisplayMode(d)) {
            hw.display_w = m->w;
            hw.display_h = m->h;
            if (m->refresh_rate > 1.0f) hw.refresh = m->refresh_rate;
        }
    }
    hw.os = SDL_GetPlatform();
    return hw;
}

ProcessUsage UsageSampler::Sample(int threads) {
    double cpu_s = 0.0;
    double ram_mb = 0.0;
#ifdef _WIN32
    FILETIME c, e, k, u;
    if (GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u)) {
        auto to_s = [](FILETIME f) {
            ULARGE_INTEGER v;
            v.LowPart = f.dwLowDateTime;
            v.HighPart = f.dwHighDateTime;
            return static_cast<double>(v.QuadPart) / 1e7;
        };
        cpu_s = to_s(k) + to_s(u);
    }
    PROCESS_MEMORY_COUNTERS pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof pmc))
        ram_mb = static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
#else
    rusage ru{};
    if (getrusage(RUSAGE_SELF, &ru) == 0)
        cpu_s = ru.ru_utime.tv_sec + ru.ru_utime.tv_usec / 1e6 + ru.ru_stime.tv_sec + ru.ru_stime.tv_usec / 1e6;
    std::ifstream statm("/proc/self/statm");
    long pages = 0, rss = 0;
    if (statm >> pages >> rss) ram_mb = static_cast<double>(rss) * sysconf(_SC_PAGESIZE) / (1024.0 * 1024.0);
#endif
    const double wall = static_cast<double>(SDL_GetTicksNS()) / 1e9;
    if (last_cpu_s_ >= 0.0 && wall - last_wall_s_ > 0.05) {
        const double pct = (cpu_s - last_cpu_s_) / (wall - last_wall_s_) / (threads > 0 ? threads : 1) * 100.0;
        last_.cpu_percent = pct < 0 ? 0 : pct;
    }
    last_.ram_mb = ram_mb;
    last_cpu_s_ = cpu_s;
    last_wall_s_ = wall;
    return last_;
}

} // namespace Satronho
