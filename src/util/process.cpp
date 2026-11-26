// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "util/process.h"

#include <chrono>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace Satronho::Process {

long long RunAndWait(const fs::path& exe, const std::vector<std::string>& args) {
#ifdef _WIN32
    std::wstring cmd = L"\"" + exe.wstring() + L"\"";
    for (const auto& a : args) cmd += L" \"" + Paths::FromUtf8(a).wstring() + L"\"";
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    const std::wstring dir = exe.parent_path().wstring();
    if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, dir.c_str(), &si, &pi)) return -1;
    const auto t0 = std::chrono::steady_clock::now();
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - t0).count();
#else
    (void)exe;
    (void)args;
    return -1;
#endif
}

} // namespace Satronho::Process
