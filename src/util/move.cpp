// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "util/move.h"

#include <cstring>
#include <fstream>
#include <vector>

#include "util/log.h"

namespace Satronho::Move {

bool SameContent(const fs::path& a, const fs::path& b) {
    std::error_code ec;
    const auto sa = fs::file_size(a, ec);
    if (ec) return false;
    const auto sb = fs::file_size(b, ec);
    if (ec || sa != sb) return false;
    std::ifstream fa(a, std::ios::binary), fb(b, std::ios::binary);
    if (!fa || !fb) return false;
    std::vector<char> ba(1 << 20), bb(1 << 20);
    while (fa && fb) {
        fa.read(ba.data(), static_cast<std::streamsize>(ba.size()));
        fb.read(bb.data(), static_cast<std::streamsize>(bb.size()));
        const auto na = fa.gcount(), nb = fb.gcount();
        if (na != nb || std::memcmp(ba.data(), bb.data(), static_cast<size_t>(na)) != 0) return false;
        if (na == 0) break;
    }
    return true;
}

fs::path AsideName(const fs::path& p) {
    std::error_code ec;
    for (int n = 1;; ++n) {
        fs::path c = p;
        c += n == 1 ? std::string(".anterior") : ".anterior" + std::to_string(n);
        if (!fs::exists(c, ec)) return c;
    }
}

namespace {
bool RawMove(const fs::path& from, const fs::path& to) {
    std::error_code e;
    fs::create_directories(to.parent_path(), e);
    e.clear();
    fs::rename(from, to, e);
    if (!e) return true;
    // Otra unidad (en Android la carpeta de la app y la memoria pública lo son): se copia y, solo si
    // quedó completa, se quita el original.
    e.clear();
    const auto size = fs::file_size(from, e);
    if (e) return false;
    fs::copy_file(from, to, fs::copy_options::overwrite_existing, e);
    if (e || fs::file_size(to, e) != size || e) {
        std::error_code r;
        fs::remove(to, r);
        return false;
    }
    fs::remove(from, e);
    return true;
}
} // namespace

bool File(const fs::path& from, const fs::path& to, Conflict conflict, int* aside) {
    std::error_code e;
    if (fs::exists(to, e)) {
        if (SameContent(from, to)) {
            // Ya estaba (una mudanza que se cortó a la mitad, o una copia idéntica).
            fs::remove(from, e);
            return true;
        }
        if (conflict == Conflict::Keep) {
            Log::Warn("No muevo %s: en el destino hay otro archivo con ese nombre", Paths::ToUtf8(from).c_str());
            return false;
        }
        const fs::path old = AsideName(to);
        fs::rename(to, old, e);
        if (e) return false;
        Log::Info("Aparté %s como %s (era distinto)", Paths::ToUtf8(to.filename()).c_str(), Paths::ToUtf8(old.filename()).c_str());
        if (aside) ++*aside;
    }
    return RawMove(from, to);
}

} // namespace Satronho::Move
