// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "sys/perf_hint.h"

#ifdef __ANDROID__
#include <dlfcn.h>
#include <unistd.h>

#include "util/log.h"
#endif

namespace Satronho::PerfHint {

#ifdef __ANDROID__
namespace {
// Las funciones se buscan en libandroid.so en tiempo de ejecución: así la app sigue abriendo en
// Android 8 a 12, donde no existen.
struct Manager;
struct Session;
using GetManagerFn = Manager* (*)();
using CreateSessionFn = Session* (*)(Manager*, const int32_t*, size_t, int64_t);
using ReportFn = int (*)(Session*, int64_t);
using UpdateTargetFn = int (*)(Session*, int64_t);
using CloseFn = void (*)(Session*);

GetManagerFn get_manager = nullptr;
CreateSessionFn create_session = nullptr;
ReportFn report = nullptr;
UpdateTargetFn update_target = nullptr;
CloseFn close_session = nullptr;
bool loaded = false;
Session* session = nullptr;

bool Load() {
    if (loaded) return get_manager != nullptr;
    loaded = true;
    void* lib = dlopen("libandroid.so", RTLD_NOW);
    if (!lib) return false;
    get_manager = reinterpret_cast<GetManagerFn>(dlsym(lib, "APerformanceHint_getManager"));
    create_session = reinterpret_cast<CreateSessionFn>(dlsym(lib, "APerformanceHint_createSession"));
    report = reinterpret_cast<ReportFn>(dlsym(lib, "APerformanceHint_reportActualWorkDuration"));
    update_target = reinterpret_cast<UpdateTargetFn>(dlsym(lib, "APerformanceHint_updateTargetWorkDuration"));
    close_session = reinterpret_cast<CloseFn>(dlsym(lib, "APerformanceHint_closeSession"));
    if (!get_manager || !create_session || !report || !close_session) get_manager = nullptr;
    return get_manager != nullptr;
}
} // namespace

void Begin(int64_t target_ns) {
    if (!Load()) return;
    if (session) {
        if (update_target) update_target(session, target_ns);
        return;
    }
    Manager* m = get_manager();
    if (!m) return;
    const int32_t tid = static_cast<int32_t>(gettid());
    session = create_session(m, &tid, 1, target_ns);
    Log::Info("Modo juego de Android: %s", session ? "activado" : "no disponible");
}

void Report(int64_t actual_ns) {
    if (session && actual_ns > 0) report(session, actual_ns);
}

void End() {
    if (session) close_session(session);
    session = nullptr;
}
#else
void Begin(int64_t) {}
void Report(int64_t) {}
void End() {}
#endif

} // namespace Satronho::PerfHint
