// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Build 26: respaldar y restaurar partidas, y pasarlas entre PC y Android.
//
// Un respaldo es un .zip con una carpeta por juego («hexa/Super Mario World/»): adentro van su
// partida (lo que guarda el juego) y sus estados. «satronho-respaldo.json» dice de qué juego es cada
// carpeta con su huella: el principio y el final del archivo del juego, y su tamaño. Así, al
// restaurar en otro equipo, la partida cae en el juego correcto aunque el archivo se llame distinto.
// En el celular, el mismo .zip lleva también las partidas de Satronho NX (nx/partidas-nx.zip).
#include "app.h"

#include <algorithm>
#include <ctime>
#include <fstream>
#include <map>

#include "native_page.h"
#include "sys/android.h"
#include "sys/native_ui.h"
#include "util/archive.h"
#include "util/hash.h"
#include "util/i18n.h"
#include "util/json.h"
#include "util/log.h"

namespace Satronho {

namespace {

std::string LowerB(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool ReadAll(const fs::path& p, std::vector<uint8_t>& out) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return false;
    out.assign(std::istreambuf_iterator<char>(in), {});
    return true;
}

bool WriteAll(const fs::path& p, const std::vector<uint8_t>& data) {
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    std::ofstream out(p, std::ios::binary);
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(out);
}

// El archivo que «es» el juego: el de la primera pista en los .cue/.gdi, el primer disco en los .m3u.
fs::path MainFile(const Game& g) {
    if (g.Archived() || (g.ext != "cue" && g.ext != "m3u")) return g.path;
    std::ifstream in(g.path);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (g.ext == "cue") {
            const size_t a = line.find('"'), b = line.rfind('"');
            if (LowerB(line).find("file") != std::string::npos && a != std::string::npos && b > a)
                return g.path.parent_path() / Paths::FromUtf8(line.substr(a + 1, b - a - 1));
        } else if (!line.empty() && line[0] != '#') {
            return g.path.parent_path() / Paths::FromUtf8(line);
        }
    }
    return g.path;
}

// Huella del juego: tamaño + primer MB + último MB (rápida aun con discos de 700 MB).
std::string Fingerprint(const Game& g) {
    if (g.Archived()) {
        std::vector<Archive::Entry> es;
        if (Archive::List(g.path, es))
            for (const auto& e : es)
                if (e.name == g.entry) return "z:" + std::to_string(e.size) + ":" + LowerB(fs::path(Paths::FromUtf8(e.name)).filename().string());
        return {};
    }
    const fs::path f = MainFile(g);
    std::ifstream in(f, std::ios::binary);
    if (!in) return {};
    in.seekg(0, std::ios::end);
    const uint64_t size = static_cast<uint64_t>(in.tellg());
    const uint64_t chunk = 1024 * 1024;
    std::vector<uint8_t> buf;
    const std::string sz = std::to_string(size);
    buf.insert(buf.end(), sz.begin(), sz.end());
    auto take = [&](uint64_t at, uint64_t n) {
        std::vector<uint8_t> part(static_cast<size_t>(n));
        in.seekg(static_cast<std::streamoff>(at));
        in.read(reinterpret_cast<char*>(part.data()), static_cast<std::streamsize>(n));
        part.resize(static_cast<size_t>(in.gcount()));
        buf.insert(buf.end(), part.begin(), part.end());
        in.clear();
    };
    take(0, std::min(size, chunk));
    if (size > chunk) take(size - std::min(size - chunk, chunk), std::min(size - chunk, chunk));
    return Hash::Sha256(buf.data(), buf.size());
}

// Nombres base que pueden tener la partida de este juego («Super Mario World (USA)»). Un cartucho
// dentro de un .zip usa el nombre del .zip; un disco dentro de un .zip se saca a la caché y usa el
// nombre del archivo de adentro (build 28: antes solo se buscaba el del .zip y esas partidas no
// entraban al respaldo si se llamaban distinto). El primero es el que se usa al restaurar si no hay nada.
std::vector<std::string> SaveStems(const Game& g) {
    const std::string outer = Paths::SafeName(Paths::ToUtf8(g.path.stem()));
    if (!g.Archived()) return {outer};
    const std::string inner = Paths::SafeName(Paths::ToUtf8(fs::path(Paths::FromUtf8(g.entry)).stem()));
    const SystemDef* sys = SystemById(g.system);
    std::vector<std::string> out = sys && sys->disc ? std::vector<std::string>{inner, outer} : std::vector<std::string>{outer, inner};
    if (out[0] == out[1]) out.pop_back();
    return out;
}

// El núcleo que usa esta consola en este equipo: los estados de un núcleo no sirven en otro (Mega
// usa BlastEm en el PC y Genesis Plus GX en el celular).
std::string CoreId(const SystemDef& s) {
    std::string f = CoreFile(s);
    if (f.rfind("lib", 0) == 0) f = f.substr(3);
    const size_t at = f.find("_libretro");
    return at == std::string::npos ? f : f.substr(0, at);
}

// Lo que es de toda la consola y no de un juego: las tarjetas de PSP y las VMU de Sueño.
bool IsConsoleWide(const std::string& sys, const std::string& rel_lower) {
    if (sys == "portatil") return rel_lower.rfind("psp/", 0) == 0;
    if (sys == "sueno") return rel_lower.rfind("vmu_save", 0) == 0;
    return false;
}

} // namespace

// ---------------------------------------------------------------- respaldar

bool App::BackupSaves(const std::vector<Game>& games, const fs::path& out, int* saved_games) {
    std::vector<std::pair<std::string, std::vector<uint8_t>>> files;
    std::string manifest = "{\"version\":1,\"app\":\"Satronho Emulation\",\"build\":" + std::to_string(build_) + ",\"juegos\":[";
    bool first = true;
    int count = 0;
    std::set<std::string> console_done;
    std::set<std::string> used_dirs;
    for (const auto& g : games) {
        const SystemDef* sys = SystemById(g.system);
        if (!sys || sys->external) continue;
        const fs::path saves = Paths::Saves() / sys->id;
        std::vector<std::string> stems;
        for (const auto& st : SaveStems(g)) stems.push_back(LowerB(st));
        std::string dir = sys->id + "/" + Paths::SafeName(g.title);
        for (int n = 2; used_dirs.count(dir); ++n) dir = sys->id + "/" + Paths::SafeName(g.title) + " " + std::to_string(n);
        int added = 0;
        std::error_code ec;
        // La partida: los archivos de la carpeta de la consola que empiezan con el nombre del juego.
        if (fs::exists(saves, ec))
            for (auto it = fs::directory_iterator(saves, ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
                if (!it->is_regular_file(ec)) continue;
                const std::string name = Paths::ToUtf8(it->path().filename());
                const std::string low = LowerB(name);
                size_t stem_len = 0;
                for (const auto& stem : stems)
                    if (low.rfind(stem, 0) == 0 && low.size() > stem.size() && low[stem.size()] == '.') stem_len = stem.size();
                if (stem_len == 0) continue;
                if (low.size() > 4 && low.substr(low.size() - 4) == ".bak") continue;
                if (low.find(".anterior") != std::string::npos) continue;
                std::vector<uint8_t> data;
                if (!ReadAll(it->path(), data)) continue;
                files.push_back({dir + "/partida/{juego}" + name.substr(stem_len), std::move(data)});
                ++added;
            }
        // Los estados (las ranuras y el automático, con su miniatura).
        const std::string prefix = g.key + "-";
        if (fs::exists(Paths::States(), ec))
            for (auto it = fs::directory_iterator(Paths::States(), ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
                const std::string name = Paths::ToUtf8(it->path().filename());
                if (name.rfind(prefix, 0) != 0) continue;
                const size_t dot = name.find(".estado");
                if (dot == std::string::npos) continue;
                std::vector<uint8_t> data;
                if (!ReadAll(it->path(), data)) continue;
                files.push_back({dir + "/estados/" + name.substr(dot + 1), std::move(data)});
                ++added;
            }
        // Lo de toda la consola, una sola vez por consola.
        if (!console_done.count(sys->id) && fs::exists(saves, ec)) {
            for (auto it = fs::recursive_directory_iterator(saves, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
                if (!it->is_regular_file(ec)) continue;
                const std::string rel = Paths::ToUtf8(it->path().lexically_relative(saves));
                std::string rl = LowerB(rel);
                for (auto& c : rl)
                    if (c == '\\') c = '/';
                if (!IsConsoleWide(sys->id, rl)) continue;
                std::vector<uint8_t> data;
                if (!ReadAll(it->path(), data)) continue;
                std::string zr = rel;
                for (auto& c : zr)
                    if (c == '\\') c = '/';
                files.push_back({sys->id + "/_consola/" + zr, std::move(data)});
                console_done.insert(sys->id);
            }
        }
        if (added == 0) continue;
        used_dirs.insert(dir);
        ++count;
        manifest += (first ? "" : ",") + std::string("{\"carpeta\":") + NativeUi::Quote(dir) + ",\"sistema\":" + NativeUi::Quote(sys->id) +
                    ",\"titulo\":" + NativeUi::Quote(g.title) + ",\"huella\":" + NativeUi::Quote(Fingerprint(g)) + ",\"archivo\":" +
                    NativeUi::Quote(Paths::ToUtf8(g.path.filename())) + ",\"nucleo\":" + NativeUi::Quote(CoreId(*sys)) + "}";
        first = false;
    }
    manifest += "]}";
    if (saved_games) *saved_games = count;
    if (count == 0 && console_done.empty()) return false;
    files.insert(files.begin(), {"satronho-respaldo.json", std::vector<uint8_t>(manifest.begin(), manifest.end())});
    const bool ok = Archive::WriteZip(out, files);
    Log::Info("Respaldo de partidas: %d juegos en %s%s", count, Paths::ToUtf8(out).c_str(), ok ? "" : " (falló)");
    return ok;
}

fs::path App::BackupFolder() const {
    if (!Paths::PublicRoot().empty()) return Paths::PublicRoot() / "Respaldos";
    return Paths::User() / "Respaldos";
}

void App::BackupAndShare(const std::vector<Game>& games, const std::string& what) {
    // Con un juego abierto, su partida se guarda antes: si no, el respaldo llevaba la de antes.
    if (in_game_) core_.SaveSram();
    char stamp[32];
    const std::time_t now = std::time(nullptr);
    std::strftime(stamp, sizeof stamp, "%Y-%m-%d %H-%M", std::localtime(&now));
    const fs::path out = BackupFolder() / Paths::FromUtf8("Satronho - partidas " + Paths::SafeName(what) + " " + stamp + ".zip");
    int n = 0;
    const bool ok = BackupSaves(games, out, &n);
    if (NativeMode()) {
        // Kotlin le suma las partidas de Satronho NX (si las hay) y abre «Compartir».
        NativeUi::Emit("{\"t\":\"respaldo\",\"ruta\":" + NativeUi::Quote(Paths::ToUtf8(out)) + ",\"juegos\":" + std::to_string(n) +
                       ",\"todos\":" + (games.size() > 1 ? "true" : "false") + ",\"hecho\":" + (ok ? "true" : "false") + "}");
        return;
    }
    if (!ok) {
        Toast(games.size() == 1 ? "Este juego todavía no tiene partidas ni estados guardados." : "Todavía no hay partidas ni estados para respaldar.",
              GuideId::Juana, 5);
        return;
    }
    Toast(TrFmt("Listo: respaldé las partidas de %d juegos en «Respaldos». Ese .zip se restaura aquí o en el celular.", n), GuideId::Juana, 6);
    std::error_code ec;
    fs::create_directories(BackupFolder(), ec);
    SDL_OpenURL(("file:///" + Paths::ToUtf8(BackupFolder())).c_str());
}

// ---------------------------------------------------------------- restaurar

std::string App::RestoreSaves(const fs::path& zip, int* restored, std::vector<std::string>* missing) {
    std::vector<uint8_t> manifest_raw;
    if (!Archive::ExtractToMemory(zip, "satronho-respaldo.json", manifest_raw)) return Tr("Ese .zip no es un respaldo de Satronho Emulation.");
    const Json m = Json::Parse(std::string(manifest_raw.begin(), manifest_raw.end()));
    std::vector<Archive::Entry> entries;
    Archive::List(zip, entries);
    const std::vector<Game> lib = library_.Snapshot();
    std::map<std::string, std::string> fp_cache; // clave → huella (se calcula solo si hace falta)
    auto fp_of = [&](const Game& g) -> const std::string& {
        auto it = fp_cache.find(g.key);
        if (it == fp_cache.end()) it = fp_cache.emplace(g.key, Fingerprint(g)).first;
        return it->second;
    };
    auto norm = [](std::string s) {
        std::string o;
        for (char c : LowerB(s))
            if (std::isalnum(static_cast<unsigned char>(c))) o += c;
        return o;
    };
    int ok = 0;
    int states_skipped = 0;
    for (const auto& j : m["juegos"].arr) {
        const std::string dir = j["carpeta"].String(), sys = j["sistema"].String(), title = j["titulo"].String(), fp = j["huella"].String();
        const std::string core = j["nucleo"].String();
        const Game* match = nullptr;
        for (const auto& g : lib)
            if (g.system == sys && !fp.empty() && fp_of(g) == fp) {
                match = &g;
                break;
            }
        if (!match)
            for (const auto& g : lib)
                if (g.system == sys && norm(g.title) == norm(title)) {
                    match = &g;
                    break;
                }
        if (!match) {
            if (missing) missing->push_back(title);
            continue;
        }
        const fs::path saves = Paths::Saves() / sys;
        // El nombre con el que este juego guarda: el que ya tenga partida, o el que usará al abrirse.
        const std::vector<std::string> stems = SaveStems(*match);
        std::string stem = stems.front();
        {
            std::error_code ec;
            for (const auto& st : stems)
                for (auto it = fs::directory_iterator(saves, ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
                    const std::string low = LowerB(Paths::ToUtf8(it->path().filename())), ls = LowerB(st);
                    if (low.rfind(ls, 0) == 0 && low.size() > ls.size() && low[ls.size()] == '.') { stem = st; break; }
                }
        }
        // Estados de otro núcleo (por ejemplo Mega del PC en el celular) no cargan: no se copian.
        const SystemDef* sdef = SystemById(sys);
        const bool same_core = core.empty() || !sdef || core == CoreId(*sdef);
        for (const auto& e : entries) {
            if (e.name.rfind(dir + "/", 0) != 0) continue;
            const std::string rest = e.name.substr(dir.size() + 1);
            fs::path dst;
            if (rest.rfind("partida/{juego}", 0) == 0) dst = saves / Paths::FromUtf8(stem + rest.substr(15));
            else if (rest.rfind("estados/", 0) == 0) {
                if (!same_core) {
                    ++states_skipped;
                    continue;
                }
                dst = Paths::States() / Paths::FromUtf8(match->key + "-" + Paths::SafeName(match->title) + "." + rest.substr(8));
            } else continue;
            std::vector<uint8_t> data;
            if (!Archive::ExtractToMemory(zip, e.name, data)) continue;
            std::error_code ec;
            if (fs::exists(dst, ec)) {
                fs::path bak = dst;
                bak += ".bak";
                fs::copy_file(dst, bak, fs::copy_options::overwrite_existing, ec);
            }
            WriteAll(dst, data);
        }
        ++ok;
    }
    // Lo de toda la consola (tarjetas de PSP, VMU de Sueño).
    for (const auto& e : entries) {
        const size_t c = e.name.find("/_consola/");
        if (c == std::string::npos) continue;
        const std::string sys = e.name.substr(0, c);
        if (!SystemById(sys)) continue;
        const std::string rel = e.name.substr(c + 10);
        if (rel.empty() || rel.find("..") != std::string::npos) continue;
        std::vector<uint8_t> data;
        if (Archive::ExtractToMemory(zip, e.name, data)) WriteAll(Paths::Saves() / sys / Paths::FromUtf8(rel), data);
    }
    if (restored) *restored = ok;
    if (states_skipped > 0) {
        Log::Info("Restaurar: %d estados de otro núcleo no se copiaron", states_skipped);
        restore_states_skipped_ = states_skipped;
    }
    Log::Info("Restaurar partidas de %s: %d juegos", Paths::ToUtf8(zip).c_str(), ok);
    return {};
}

void App::RestoreAndTell(const fs::path& zip) {
    // Con un juego abierto, al salir su partida en memoria pisaría la restaurada.
    if (in_game_) {
        Toast("Primero sal del juego y luego restauras el respaldo; si no, el juego pisa la partida restaurada.", GuideId::Juana, 7);
        return;
    }
    restore_states_skipped_ = 0;
    int n = 0;
    std::vector<std::string> missing;
    const std::string err = RestoreSaves(zip, &n, &missing);
    if (!err.empty()) {
        Toast(err, GuideId::Nono, 6);
        return;
    }
    if (missing.empty())
        Toast(TrFmt("¡Listo! Restauré las partidas de %d juegos. Si tenías algo guardado, quedó una copia .bak.", n), GuideId::Juana, 6);
    else
        Toast(TrFmt("Restauré %d juegos. %d no están en tu biblioteca (por ejemplo «%s»): agrégalos y vuelve a restaurar.", n,
                    static_cast<int>(missing.size()), missing.front().c_str()),
              GuideId::Juana, 9);
    if (restore_states_skipped_ > 0)
        Toast(TrFmt("%d estados no los pasé: son de otro núcleo (otra versión de la consola) y no cargarían aquí. Las partidas sí quedaron.",
                    restore_states_skipped_),
              GuideId::Juana, 8);
    games_dirty_ = true;
    native_lib_t_ = -10;
}

} // namespace Satronho
