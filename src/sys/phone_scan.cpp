// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "sys/phone_scan.h"

#include <algorithm>
#include <map>

#include "sys/systems.h"
#include "util/archive.h"
#include "util/log.h"

namespace Satronho {

namespace {

std::string Lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Carpetas de la raíz que nunca tienen juegos (y pueden tener miles de fotos).
bool SkipTopFolder(const std::string& name) {
    static const char* skip[] = {"dcim", "pictures", "movies", "music", "ringtones", "alarms", "notifications", "podcasts",
                                 "recordings", "audiobooks", "lost.dir"};
    const std::string n = Lower(name);
    for (const char* s : skip)
        if (n == s) return true;
    return false;
}

} // namespace

PhoneScan::~PhoneScan() {
    cancel_ = true;
    if (worker_.joinable()) worker_.join();
}

void PhoneScan::Start(const std::vector<fs::path>& roots, std::set<std::string> known, const fs::path& skip_root) {
    if (running_) return;
    if (worker_.joinable()) worker_.join();
    cancel_ = false;
    done_ = false;
    files_ = 0;
    found_ = 0;
    running_ = true;
    worker_ = std::thread([this, roots, known = std::move(known), skip_root]() mutable { Run(roots, std::move(known), skip_root); });
}

std::string PhoneScan::Current() const {
    std::lock_guard lock(mutex_);
    return current_;
}

bool PhoneScan::TakeDone(Result& out) {
    if (!done_.exchange(false)) return false;
    if (worker_.joinable()) worker_.join();
    std::lock_guard lock(mutex_);
    out = std::move(result_);
    result_ = {};
    return true;
}

void PhoneScan::Run(std::vector<fs::path> roots, std::set<std::string> known, fs::path skip_root) {
    std::vector<std::string> extensions = AllExtensions();
    extensions.push_back("zip");
    extensions.push_back("7z");
    std::vector<Game> games;
    Result res;
    std::map<fs::path, int> ncas; // carpetas con archivos .nca (firmware de la Switch)
    std::set<std::string> seen_roots;
    const std::string skip_partidas = skip_root.empty() ? "" : Lower(Paths::ToUtf8(skip_root / "Partidas"));
    const std::string skip_estados = skip_root.empty() ? "" : Lower(Paths::ToUtf8(skip_root / "Estados"));

    for (const auto& root : roots) {
        std::error_code ec;
        const std::string rkey = Paths::ToUtf8(fs::weakly_canonical(root, ec));
        if (!seen_roots.insert(rkey).second) continue;
        fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
        for (; !ec && it != end && !cancel_; it.increment(ec)) {
            const fs::path& p = it->path();
            const std::string name = Paths::ToUtf8(p.filename());
            if (it->is_directory(ec)) {
                bool skip = it.depth() > 10 || name.empty() || name[0] == '.';
                if (!skip && it.depth() == 0) skip = SkipTopFolder(name);
                // De Android/ solo interesa media/ (WhatsApp, Telegram); data/ y obb/ son de las apps.
                if (!skip && it.depth() == 1 && Lower(Paths::ToUtf8(p.parent_path().filename())) == "android" && Lower(name) != "media") skip = true;
                if (!skip) {
                    const std::string lp = Lower(Paths::ToUtf8(p));
                    skip = (!skip_partidas.empty() && lp == skip_partidas) || (!skip_estados.empty() && lp == skip_estados);
                }
                if (skip) it.disable_recursion_pending();
                else {
                    std::lock_guard lock(mutex_);
                    current_ = name;
                }
                continue;
            }
            if (!it->is_regular_file(ec)) continue;
            ++files_;
            const std::string lname = Lower(name);
            std::string ext = Lower(Paths::ToUtf8(p.extension()));
            if (!ext.empty() && ext[0] == '.') ext.erase(0, 1);
            if (ext == "keys") {
                if (lname == "prod.keys" || lname == "title.keys") res.keys.push_back(p);
                continue;
            }
            if (ext == "nca") {
                ++ncas[p.parent_path()];
                continue;
            }
            if (std::find(extensions.begin(), extensions.end(), ext) == extensions.end()) continue;
            const uint64_t size = it->file_size(ec);
            if (size < 1024) continue;
            if (Archive::IsArchive(ext) && lname.find("firmware") != std::string::npos) {
                res.firmware.push_back(p);
                continue;
            }
            // Un .bin de más de 16 MB es una pista de disco, no un cartucho de Mega.
            if (ext == "bin" && size > 16u * 1024 * 1024) continue;
            Game g = Library::MakeGame(p);
            if (g.system.empty()) continue;
            games.push_back(std::move(g));
            ++found_;
            if (games.size() > 20000) break;
        }
    }
    for (const auto& [dir, n] : ncas)
        if (n >= 30) res.firmware.push_back(dir);

    Library::DropDiscParts(games);
    std::map<fs::path, Folder> by_folder;
    for (auto& g : games) {
        Folder& f = by_folder[g.path.parent_path()];
        f.path = g.path.parent_path();
        if (known.count(g.key)) ++f.known;
        else f.games.push_back(std::move(g));
    }
    for (auto& [path, f] : by_folder)
        if (!f.games.empty()) res.folders.push_back(std::move(f));
    // Las carpetas con más juegos primero.
    std::sort(res.folders.begin(), res.folders.end(), [](const Folder& a, const Folder& b) { return a.games.size() > b.games.size(); });
    res.files = files_;
    Log::Info("Búsqueda en el teléfono: %d archivos, %d juegos nuevos en %zu carpetas, %zu claves, %zu firmware%s", res.files, res.NewGames(),
              res.folders.size(), res.keys.size(), res.firmware.size(), cancel_ ? " (cancelada)" : "");
    {
        std::lock_guard lock(mutex_);
        result_ = std::move(res);
        current_.clear();
    }
    running_ = false;
    done_ = true;
}

} // namespace Satronho
