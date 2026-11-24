// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "util/i18n.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <vector>

#include <SDL3/SDL.h>

#include "util/json.h"
#include "util/log.h"

namespace Satronho::I18n {

namespace {
std::string g_code = "es";
std::unordered_map<std::string, std::string> g_table;
// Trozos de frases que en el código se unen con números o nombres («Estado guardado en la ranura » + 3):
// si la frase completa no está, se traducen sus trozos. De más largo a más corto.
std::vector<std::pair<std::string, std::string>> g_pieces;
std::unordered_map<std::string, std::string> g_built; // frases ya armadas (con tope)
} // namespace

const std::vector<std::pair<std::string, std::string>>& Languages() {
    static const std::vector<std::pair<std::string, std::string>> v{
        {"es", "Español"}, {"en", "English"}, {"fr", "Français"}, {"pt", "Português"}, {"it", "Italiano"}};
    return v;
}

std::string SystemLanguage() {
    int n = 0;
    SDL_Locale** list = SDL_GetPreferredLocales(&n);
    std::string code = "en";
    if (list && n > 0 && list[0] && list[0]->language) {
        const std::string l = list[0]->language;
        for (const auto& [c, name] : Languages())
            if (l == c) code = c;
    }
    SDL_free(list);
    return code;
}

void Load(const fs::path& dir, const std::string& code_in) {
    const std::string code = code_in.empty() ? SystemLanguage() : code_in;
    g_code = code;
    g_table.clear();
    if (code == "es") {
        Log::Info("Idioma: es");
        return;
    }
    std::ifstream f(dir / (code + ".json"), std::ios::binary);
    if (!f) {
        Log::Warn("Idioma %s: no encontré su archivo", code.c_str());
        return;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    const Json j = Json::Parse(ss.str());
    g_pieces.clear();
    g_built.clear();
    for (const auto& [k, v] : j.obj) {
        g_table[k] = v.String();
        // Trozo: empieza o termina con espacio o signo (así se pegan en el código) y tiene letras.
        const bool edge = !k.empty() && (k.front() == ' ' || k.back() == ' ' || k.back() == '\xab' || k.front() == '.' ||
                                         k.front() == ',' || k.front() == ':' || k.front() == '!' || k.rfind("\xc2\xbb", 0) == 0 ||
                                         (k.size() >= 2 && k.compare(k.size() - 2, 2, "\xc2\xab") == 0));
        size_t letters = 0;
        for (unsigned char ch : k) letters += std::isalpha(ch) || ch >= 0x80;
        if (edge && letters >= 4 && k.find('%') == std::string::npos) g_pieces.emplace_back(k, v.String());
    }
    std::sort(g_pieces.begin(), g_pieces.end(), [](const auto& a, const auto& b) { return a.first.size() > b.first.size(); });
    Log::Info("Idioma: %s (%zu textos)", code.c_str(), g_table.size());
}

const std::string& Current() { return g_code; }

namespace {
// Busca la traducción. Las etiquetas de ImGui pueden traer «##id» al final: se traduce lo de antes.
const std::string* Pieces(const std::string& es) {
    if (es.size() < 6 || g_pieces.empty()) return nullptr;
    if (auto it = g_built.find(es); it != g_built.end()) return it->second.empty() ? nullptr : &it->second;
    if (g_built.size() > 4000) g_built.clear(); // textos que cambian solos (números) no llenan la memoria
    std::string out = es;
    bool changed = false;
    for (const auto& [k, v] : g_pieces) {
        size_t pos = 0;
        while ((pos = out.find(k, pos)) != std::string::npos) {
            out.replace(pos, k.size(), v);
            pos += v.size();
            changed = true;
        }
    }
    auto& slot = g_built[es];
    slot = changed ? out : std::string();
    return changed ? &slot : nullptr;
}

const std::string* Find(const std::string& es) {
    auto it = g_table.find(es);
    if (it != g_table.end() && !it->second.empty()) return &it->second;
    const size_t hh = es.find("##");
    if (hh == std::string::npos || hh == 0) return Pieces(es);
    static std::unordered_map<std::string, std::string> joined; // «traducción##id», estable en memoria
    auto jt = joined.find(es);
    if (jt != joined.end()) return &jt->second;
    auto base = g_table.find(es.substr(0, hh));
    if (base == g_table.end() || base->second.empty()) return nullptr;
    return &joined.emplace(es, base->second + es.substr(hh)).first->second;
}
} // namespace

const char* Tr(const char* es) {
    if (!es || !*es || g_table.empty()) return es;
    const std::string* t = Find(es);
    return t ? t->c_str() : es;
}

std::string Tr(const std::string& es) {
    if (es.empty() || g_table.empty()) return es;
    const std::string* t = Find(es);
    return t ? *t : es;
}

} // namespace Satronho::I18n
