// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "sys/updater.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include <SDL3/SDL.h>

#include "util/hash.h"
#include "util/json.h"
#include "util/log.h"
#include "util/paths.h"
#include "util/net.h"
#include "sys/android.h"
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <urlmon.h>
#endif

namespace Satronho {

namespace {
constexpr const char* kApi = "https://api.github.com/repos/Riddsc/Satronho-Emulation/releases/latest";
constexpr const char* kPage = "https://github.com/Riddsc/Satronho-Emulation/releases/latest";

#ifdef _WIN32
std::wstring Wide(const std::string& s) {
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n > 0 ? n - 1 : 0, L'\0');
    if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    return w;
}

// Descarga con urlmon (viene con Windows). Sin caché, para no recibir una respuesta vieja.
bool Download(const std::string& url, const fs::path& file) {
    using Fn = HRESULT(WINAPI*)(LPUNKNOWN, LPCWSTR, LPCWSTR, DWORD, LPBINDSTATUSCALLBACK);
    static HMODULE urlmon = LoadLibraryW(L"urlmon.dll");
    if (!urlmon) return false;
    auto fn = reinterpret_cast<Fn>(GetProcAddress(urlmon, "URLDownloadToFileW"));
    if (!fn) return false;
    using DelFn = BOOL(WINAPI*)(LPCWSTR);
    static HMODULE wininet = LoadLibraryW(L"wininet.dll");
    if (wininet) {
        if (auto del = reinterpret_cast<DelFn>(GetProcAddress(wininet, "DeleteUrlCacheEntryW"))) del(Wide(url).c_str());
    }
    return SUCCEEDED(fn(nullptr, Wide(url).c_str(), file.c_str(), 0, nullptr));
}

// En PowerShell las comillas simples se escapan duplicándolas.
std::string Quote(const fs::path& p) {
    std::string s = Paths::ToUtf8(p), out = "'";
    for (char c : s) out += c == '\'' ? std::string("''") : std::string(1, c);
    return out + "'";
}
#endif
} // namespace

Updater::~Updater() { Join(); }

void Updater::Join() {
    if (worker_.joinable()) worker_.join();
}

bool Updater::Supported() const {
#if defined(_WIN32) || defined(__ANDROID__)
    return true;
#else
    return false;
#endif
}

std::string Updater::ReleaseUrl() const {
    std::lock_guard lock(mutex_);
    return page_url_.empty() ? kPage : page_url_;
}

std::string Updater::Error() const {
    std::lock_guard lock(mutex_);
    return error_;
}

void Updater::Check(int current_build) {
    if (!Supported() || state_ == State::Checking || state_ == State::Downloading) return;
    Join();
    state_ = State::Checking;
    worker_ = std::thread([this, current_build] {
#ifdef _WIN32
        const fs::path tmp = fs::temp_directory_path() / "satronho-emulation-release.json";
        if (!Download(kApi, tmp)) {
            std::lock_guard lock(mutex_);
            error_ = "No se pudo consultar GitHub (¿sin internet?).";
            state_ = State::Failed;
            return;
        }
        std::ifstream in(tmp, std::ios::binary);
        std::stringstream ss;
        ss << in.rdbuf();
        in.close();
        std::error_code ec;
        fs::remove(tmp, ec);
        const Json j = Json::Parse(ss.str());
        const std::string tag = j["tag_name"].String();
        const int latest = tag.rfind("build-", 0) == 0 ? std::atoi(tag.c_str() + 6) : 0;
        std::string zip, sha;
        for (const auto& a : j["assets"].arr) {
            const std::string name = a["name"].String();
            if (name.size() > 15 && name.compare(name.size() - 15, 15, "windows-x64.zip") == 0) zip = a["browser_download_url"].String();
            if (name.size() > 22 && name.compare(name.size() - 22, 22, "windows-x64.zip.sha256") == 0)
                sha = a["browser_download_url"].String();
        }
        {
            std::lock_guard lock(mutex_);
            zip_url_ = zip;
            sha_url_ = sha;
            page_url_ = j["html_url"].String();
        }
        latest_ = latest;
        Log::Info("Actualizaciones: la última es la build %d (esta es la %d)", latest, current_build);
        state_ = (latest > current_build && current_build > 0 && !zip.empty()) ? State::Available : State::UpToDate;
#elif defined(__ANDROID__)
        // Android: la misma consulta, pero se busca el APK.
        const fs::path tmp = Paths::User() / "cache" / "release.json";
        if (!Net::Download(kApi, tmp)) {
            std::lock_guard lock(mutex_);
            error_ = "No se pudo consultar GitHub (¿sin internet?).";
            state_ = State::Failed;
            return;
        }
        std::ifstream in(tmp, std::ios::binary);
        std::stringstream ss;
        ss << in.rdbuf();
        in.close();
        const Json j = Json::Parse(ss.str());
        const std::string tag = j["tag_name"].String();
        const int latest = tag.rfind("build-", 0) == 0 ? std::atoi(tag.c_str() + 6) : 0;
        std::string apk, sha;
        for (const auto& a : j["assets"].arr) {
            const std::string name = a["name"].String();
            auto ends = [&](const char* suf) { const size_t n = std::strlen(suf); return name.size() > n && name.compare(name.size() - n, n, suf) == 0; };
            if (ends("android-arm64.apk")) apk = a["browser_download_url"].String();
            if (ends("android-arm64.apk.sha256")) sha = a["browser_download_url"].String();
        }
        {
            std::lock_guard lock(mutex_);
            zip_url_ = apk;
            sha_url_ = sha;
            page_url_ = j["html_url"].String();
        }
        latest_ = latest;
        Log::Info("Actualizaciones: la última es la build %d (esta es la %d)", latest, current_build);
        state_ = (latest > current_build && current_build > 0 && !apk.empty()) ? State::Available : State::UpToDate;
#else
        (void)current_build;
        state_ = State::UpToDate;
#endif
    });
}

void Updater::Install() {
    if (state_ != State::Available) return;
    Join();
    state_ = State::Downloading;
    progress_ = 0;
    worker_ = std::thread([this] {
#ifdef _WIN32
        std::string url, sha_url;
        {
            std::lock_guard lock(mutex_);
            url = zip_url_;
            sha_url = sha_url_;
        }
        std::error_code ec;
        fs::path tmp_root = fs::temp_directory_path(ec);
        if (ec) tmp_root = Paths::User();
        const fs::path dir = tmp_root / "satronho-emulation-update";
        fs::remove_all(dir, ec);
        fs::create_directories(dir, ec);
        const fs::path zip = dir / "update.zip";
        if (!Download(url, zip) || fs::file_size(zip, ec) < 1024 * 1024) {
            std::lock_guard lock(mutex_);
            error_ = "No se pudo descargar la build nueva.";
            state_ = State::Failed;
            return;
        }
        // Huella del zip (desde la build 12 cada Release la publica): si no coincide, el archivo
        // llegó incompleto o dañado y no se instala.
        if (!sha_url.empty()) {
            const fs::path sha_file = dir / "update.zip.sha256";
            std::string expected;
            if (Download(sha_url, sha_file)) {
                std::ifstream in(sha_file, std::ios::binary);
                in >> expected;
                for (auto& c : expected) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            const std::string got = Hash::Sha256File(zip);
            if (expected.size() != 64 || got != expected) {
                Log::Warn("Actualizaciones: la huella no coincide (esperada %s, obtenida %s)", expected.c_str(), got.c_str());
                std::lock_guard lock(mutex_);
                error_ = "La descarga llegó dañada (la huella no coincide). Inténtalo de nuevo.";
                state_ = State::Failed;
                return;
            }
            Log::Info("Actualizaciones: huella SHA-256 correcta");
        } else {
            Log::Warn("Actualizaciones: este Release no trae huella SHA-256; se instala sin comprobarla");
        }
        progress_ = 1.0f;
        // El instalador espera a que Satronho Emulation se cierre, descomprime, copia encima
        // (la carpeta user/ del modo portátil no viene en el zip, así que no se toca) y lo abre.
        const fs::path base = Paths::Base();
        const fs::path out = dir / "x";
        const fs::path script = dir / "instalar.ps1";
        std::ofstream ps(script, std::ios::binary);
        ps << "\xEF\xBB\xBF"
           << "$ErrorActionPreference = 'Stop'\r\n"
           << "Wait-Process -Id " << GetCurrentProcessId() << " -Timeout 30 -ErrorAction SilentlyContinue\r\n"
           << "Start-Sleep -Milliseconds 400\r\n"
           << "try {\r\n"
           << "  Expand-Archive -LiteralPath " << Quote(zip) << " -DestinationPath " << Quote(out) << " -Force\r\n"
           << "  $src = Join-Path " << Quote(out) << " 'SatronhoEmulation'\r\n"
           << "  $dst = " << Quote(base) << "\r\n"
           // Las carpetas del programa se reemplazan enteras (así no quedan archivos viejos, como un
           // núcleo renombrado). Primero se apartan con .viejo y, si algo falla, se devuelven.
           << "  $dirs = @('cores', 'assets', 'licencias')\r\n"
           << "  try {\r\n"
           << "    foreach ($d in $dirs) {\r\n"
           << "      $p = Join-Path $dst $d\r\n"
           << "      if (Test-Path -LiteralPath \"$p.viejo\") { Remove-Item -LiteralPath \"$p.viejo\" -Recurse -Force }\r\n"
           << "      if (Test-Path -LiteralPath $p) { Rename-Item -LiteralPath $p -NewName \"$d.viejo\" }\r\n"
           << "    }\r\n"
           << "    Get-ChildItem -LiteralPath $src | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $dst -Recurse -Force }\r\n"
           << "    foreach ($d in $dirs) { $p = Join-Path $dst \"$d.viejo\"; if (Test-Path -LiteralPath $p) { Remove-Item -LiteralPath $p -Recurse -Force } }\r\n"
           << "  } catch {\r\n"
           << "    foreach ($d in $dirs) {\r\n"
           << "      $p = Join-Path $dst $d\r\n"
           << "      if (Test-Path -LiteralPath \"$p.viejo\") {\r\n"
           << "        if (Test-Path -LiteralPath $p) { Remove-Item -LiteralPath $p -Recurse -Force }\r\n"
           << "        Rename-Item -LiteralPath \"$p.viejo\" -NewName $d\r\n"
           << "      }\r\n"
           << "    }\r\n"
           << "    throw\r\n"
           << "  }\r\n"
           << "} catch {\r\n"
           << "  Add-Type -AssemblyName PresentationFramework\r\n"
           << "  [System.Windows.MessageBox]::Show('No se pudo instalar la actualización: ' + $_.Exception.Message, 'Satronho Emulation') | Out-Null\r\n"
           << "}\r\n"
           << "Start-Process -FilePath (Join-Path " << Quote(base) << " 'satronho-emulation.exe') -WorkingDirectory " << Quote(base) << "\r\n";
        ps.close();
        {
            std::lock_guard lock(mutex_);
            script_ = Paths::ToUtf8(script);
        }
        state_ = State::Ready;
#elif defined(__ANDROID__)
        std::string url, sha_url;
        {
            std::lock_guard lock(mutex_);
            url = zip_url_;
            sha_url = sha_url_;
        }
        std::error_code ec;
        const fs::path apk = Paths::User() / "cache" / "actualizacion.apk";
        fs::remove(apk, ec);
        if (!Net::Download(url, apk) || fs::file_size(apk, ec) < 1024 * 1024) {
            std::lock_guard lock(mutex_);
            error_ = "No se pudo descargar la build nueva.";
            state_ = State::Failed;
            return;
        }
        if (!sha_url.empty()) {
            const fs::path sha_file = Paths::User() / "cache" / "actualizacion.apk.sha256";
            std::string expected;
            if (Net::Download(sha_url, sha_file)) {
                std::ifstream in(sha_file, std::ios::binary);
                in >> expected;
                for (auto& c : expected) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            if (expected.size() != 64 || Hash::Sha256File(apk) != expected) {
                std::lock_guard lock(mutex_);
                error_ = "La descarga llegó dañada (la huella no coincide). Inténtalo de nuevo.";
                state_ = State::Failed;
                return;
            }
        }
        progress_ = 1.0f;
        {
            std::lock_guard lock(mutex_);
            script_ = Paths::ToUtf8(apk);
        }
        state_ = State::Ready;
#endif
    });
}

bool Updater::LaunchInstaller() {
#ifdef _WIN32
    std::string script;
    {
        std::lock_guard lock(mutex_);
        script = script_;
    }
    if (script.empty()) return false;
    std::wstring cmd = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"" + Wide(script) + L"\"";
    STARTUPINFOW si{};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) return false;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
#elif defined(__ANDROID__)
    std::string apk;
    {
        std::lock_guard lock(mutex_);
        apk = script_;
    }
    // Android muestra su ventana para confirmar; si falta el permiso de instalar, abre los ajustes.
    return !apk.empty() && Android::InstallApk(apk);
#else
    return false;
#endif
}

} // namespace Satronho
