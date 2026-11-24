// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "util/log.h"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <mutex>

#include <SDL3/SDL.h>

#include "util/paths.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace Satronho::Log {
namespace {
std::FILE* g_file = nullptr;
std::mutex g_mutex;
Uint64 g_last_flush = 0;

void Write(const char* level, const char* fmt, va_list args) {
    char msg[2048];
    std::vsnprintf(msg, sizeof msg, fmt, args);
    const double t = static_cast<double>(SDL_GetTicks()) / 1000.0;
    std::lock_guard lock(g_mutex);
    if (g_file) {
        std::fprintf(g_file, "[%9.3f] %s %s\n", t, level, msg);
        // Se vacía como mucho una vez por segundo (o siempre en errores) para no perder
        // las últimas líneas si algo se cuelga.
        const Uint64 now = SDL_GetTicks();
        if (level[0] == 'E' || now - g_last_flush > 1000) {
            std::fflush(g_file);
            g_last_flush = now;
        }
    }
    SDL_Log("%s %s", level, msg);
}
} // namespace

void Open() {
    std::lock_guard lock(g_mutex);
    // El registro de la sesión anterior se guarda como satronho.anterior.log.
    {
        std::error_code ec;
        fs::path prev = Paths::LogFile();
        prev.replace_filename("satronho.anterior.log");
        if (fs::exists(Paths::LogFile(), ec)) {
            fs::remove(prev, ec);
            fs::rename(Paths::LogFile(), prev, ec);
        }
    }
#ifdef _WIN32
    g_file = _wfopen(Paths::LogFile().c_str(), L"w");
#else
    g_file = std::fopen(Paths::LogFile().c_str(), "w");
#endif
}

void Close() {
    std::lock_guard lock(g_mutex);
    if (g_file) std::fclose(g_file);
    g_file = nullptr;
}

#define SAT_LOG_IMPL(name, tag)        \
    void name(const char* fmt, ...) {  \
        va_list args;                  \
        va_start(args, fmt);           \
        Write(tag, fmt, args);         \
        va_end(args);                  \
    }
SAT_LOG_IMPL(Info, "Info ")
SAT_LOG_IMPL(Warn, "Aviso")
SAT_LOG_IMPL(Error, "Error")

#ifdef _WIN32
namespace {
LONG WINAPI OnCrash(EXCEPTION_POINTERS* info) {
    const DWORD code = info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionCode : 0;
    void* addr = info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionAddress : nullptr;
    // Archivo (exe o dll) donde estaba la instrucción que falló y su posición dentro de él.
    char module[MAX_PATH] = "?";
    HMODULE mod = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           static_cast<LPCSTR>(addr), &mod) && mod) {
        GetModuleFileNameA(mod, module, sizeof module);
    }
    const char* base = module;
    for (const char* c = module; *c; ++c)
        if (*c == '\\' || *c == '/') base = c + 1;
    const unsigned long long offset =
        mod ? static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(addr) - reinterpret_cast<uintptr_t>(mod)) : 0ull;
    // Puede que el fallo haya pasado mientras se escribía el registro: no se espera al candado.
    const bool locked = g_mutex.try_lock();
    if (g_file) {
        std::fprintf(g_file, "[cierre] Error Cierre inesperado: código 0x%08lX en %s+0x%llX\n", static_cast<unsigned long>(code),
                     base, offset);
        std::fflush(g_file);
    }
    if (locked) g_mutex.unlock();
    return EXCEPTION_CONTINUE_SEARCH;
}
} // namespace

void InstallCrashHandler() { SetUnhandledExceptionFilter(&OnCrash); }
#else
void InstallCrashHandler() {}
#endif

} // namespace Satronho::Log
