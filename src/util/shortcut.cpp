// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "util/shortcut.h"

#ifdef _WIN32
#include <windows.h>
#include <objbase.h>
#include <shlobj.h>
#include <shobjidl.h>
#endif

namespace Satronho::Shortcut {

bool CreateOnDesktop(const std::string& name, const fs::path& rom, const fs::path& icon_hint, std::string* error) {
#ifdef _WIN32
    (void)icon_hint;
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    PWSTR desktop = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &desktop))) {
        if (error) *error = "No encontré el escritorio.";
        return false;
    }
    // Nombre de archivo sin caracteres que Windows no acepta.
    std::wstring file = Paths::FromUtf8(name).wstring();
    for (auto& c : file)
        if (wcschr(L"<>:\"/\\|?*", c)) c = L' ';
    const std::wstring lnk = std::wstring(desktop) + L"\\" + file + L".lnk";
    CoTaskMemFree(desktop);
    const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IShellLinkW* link = nullptr;
    bool ok = false;
    if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, reinterpret_cast<void**>(&link)))) {
        link->SetPath(exe);
        const std::wstring args = L"\"" + rom.wstring() + L"\"";
        link->SetArguments(args.c_str());
        std::wstring dir(exe);
        dir = dir.substr(0, dir.find_last_of(L"\\/"));
        link->SetWorkingDirectory(dir.c_str());
        link->SetIconLocation(exe, 0);
        link->SetDescription(L"Satronho Emulation");
        IPersistFile* pf = nullptr;
        if (SUCCEEDED(link->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&pf)))) {
            ok = SUCCEEDED(pf->Save(lnk.c_str(), TRUE));
            pf->Release();
        }
        link->Release();
    }
    if (SUCCEEDED(init)) CoUninitialize();
    if (!ok && error) *error = "Windows no dejó crear el acceso directo.";
    return ok;
#else
    (void)name;
    (void)rom;
    (void)icon_hint;
    if (error) *error = "Los accesos directos solo se crean en Windows.";
    return false;
#endif
}

} // namespace Satronho::Shortcut
