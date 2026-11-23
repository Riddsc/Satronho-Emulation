// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "util/log.h"

#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <mutex>

#include <SDL3/SDL.h>

#include "util/paths.h"

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

} // namespace Satronho::Log
