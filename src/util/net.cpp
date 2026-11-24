// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "util/net.h"

#include <cstdlib>
#include <cstdio>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <urlmon.h>
#endif

namespace Satronho::Net {

namespace {
#ifdef _WIN32
std::wstring Wide(const std::string& s) {
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n > 0 ? n - 1 : 0, L'\0');
    if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    return w;
}
#endif
} // namespace

bool Download(const std::string& url, const fs::path& file) {
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    fs::path part = file;
    part += ".part";
    fs::remove(part, ec);
    bool ok = false;
#ifdef _WIN32
    using Fn = HRESULT(WINAPI*)(LPUNKNOWN, LPCWSTR, LPCWSTR, DWORD, LPBINDSTATUSCALLBACK);
    static HMODULE urlmon = LoadLibraryW(L"urlmon.dll");
    if (urlmon) {
        if (auto fn = reinterpret_cast<Fn>(GetProcAddress(urlmon, "URLDownloadToFileW")))
            ok = SUCCEEDED(fn(nullptr, Wide(url).c_str(), part.c_str(), 0, nullptr));
    }
#else
    auto quote = [](const std::string& v) {
        std::string q = "'";
        for (char c : v) q += c == '\'' ? std::string("'\\''") : std::string(1, c);
        return q + "'";
    };
    std::string cmd = "curl -sfL --max-time 30 -o " + quote(Paths::ToUtf8(part)) + " " + quote(url) + " >/dev/null 2>&1";
    ok = std::system(cmd.c_str()) == 0;
#endif
    if (ok && fs::file_size(part, ec) > 0 && !ec) {
        fs::rename(part, file, ec);
        if (ec) {
            fs::remove(file, ec);
            fs::rename(part, file, ec);
        }
        return !ec;
    }
    fs::remove(part, ec);
    return false;
}

std::string UrlEncode(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~' ||
            c == '(' || c == ')' || c == '\'' || c == '!' || c == ',') {
            out += static_cast<char>(c);
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 15];
        }
    }
    return out;
}

std::string UrlDecode(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            out += static_cast<char>(std::strtol(s.substr(i + 1, 2).c_str(), nullptr, 16));
            i += 2;
        } else if (s[i] == '+') {
            out += ' ';
        } else {
            out += s[i];
        }
    }
    return out;
}

} // namespace Satronho::Net
