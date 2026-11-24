// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Juegos que llegan de otras apps: en Android, «Abrir con» o «Compartir» desde WhatsApp, Telegram,
// Drive o Archivos (SatronhoActivity copia el archivo a «recibidos»). Aquí se acomodan:
// - Un .zip/.7z con un solo juego de cartucho queda comprimido (así ya se juega).
// - Uno con varios juegos (o de disco) se descomprime y cada juego va a la carpeta de su consola.
// - Las BIOS (scph5501.bin…) van a la carpeta del sistema.
// - Lo que ya está en la biblioteca no se repite.
// Antes de descomprimir, Ñoño revisa que haya espacio.

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>

#include "app.h"
#include "imgui.h"
#include "sys/android.h"
#include "ui/kit.h"
#include "util/archive.h"
#include "util/i18n.h"
#include "util/log.h"

namespace Satronho {

using namespace Ui;

namespace {
std::string Lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string ExtOf(const fs::path& p) {
    std::string e = Lower(Paths::ToUtf8(p.extension()));
    if (!e.empty() && e[0] == '.') e.erase(0, 1);
    return e;
}

// ¿Es una BIOS conocida? Por el nombre exacto, o por el tamaño y una pista clara en el nombre.
const BiosFile* BiosFor(const std::string& name_lower, uint64_t size, const SystemDef** sys_out) {
    for (const auto& sd : Systems())
        for (const auto& b : sd.bios)
            if (name_lower == b.name) {
                *sys_out = &sd;
                return &b;
            }
    const bool hint = name_lower.find("scph") != std::string::npos || name_lower.find("bios") != std::string::npos ||
                      name_lower.find("firmware") != std::string::npos || name_lower.find("mpr-") != std::string::npos;
    if (!hint) return nullptr;
    for (const auto& sd : Systems())
        for (const auto& b : sd.bios) {
            if (b.size != size) continue;
            std::string kw = b.keywords;
            size_t pos = 0;
            while (pos <= kw.size()) {
                const size_t bar = kw.find('|', pos);
                const std::string word = kw.substr(pos, bar == std::string::npos ? std::string::npos : bar - pos);
                if (!word.empty() && name_lower.find(word) != std::string::npos) {
                    *sys_out = &sd;
                    return &b;
                }
                if (bar == std::string::npos) break;
                pos = bar + 1;
            }
        }
    return nullptr;
}

// Archivos que nombra un .cue (sus pistas).
std::vector<std::string> CueFiles(const fs::path& cue) {
    std::vector<std::string> out;
    std::ifstream in(cue, std::ios::binary);
    std::string line;
    while (std::getline(in, line)) {
        const size_t f = Lower(line).find("file");
        if (f == std::string::npos) continue;
        const size_t a = line.find('"', f), b = a == std::string::npos ? a : line.find('"', a + 1);
        if (a != std::string::npos && b != std::string::npos) out.push_back(line.substr(a + 1, b - a - 1));
    }
    return out;
}

void Muted(const char* text, float size = 13.5f) {
    text = Tr(text);
    FontScope f(GetFonts().regular, size);
    ImGui::PushStyleColor(ImGuiCol_Text, V4(Color::Muted));
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

bool IsGameExt(const std::string& ext) {
    if (ext == "bin") return true; // Genesis o pista de un disco
    return SystemForExtension(ext) != nullptr;
}
} // namespace

fs::path App::ImportFolder() const { return Paths::User() / "Juegos"; }

void App::QueueImport(const fs::path& file) {
    std::lock_guard lock(import_mutex_);
    import_queue_.push_back(file);
    import_.active = true;
    import_.done = false;
    Log::Info("Llegó para importar: %s", Paths::ToUtf8(file).c_str());
}

void App::PollImports() {
    // De Android: lo que terminó de copiarse y lo que se está copiando.
    for (int guard = 0; guard < 16; ++guard) {
        const std::string r = Android::Received();
        if (r.empty()) break;
        std::vector<std::string> p;
        size_t pos = 0;
        while (true) {
            const size_t bar = r.find('|', pos);
            p.push_back(r.substr(pos, bar == std::string::npos ? std::string::npos : bar - pos));
            if (bar == std::string::npos) break;
            pos = bar + 1;
        }
        if (p[0] == "ok" && p.size() >= 2) {
            QueueImport(Paths::FromUtf8(r.substr(3)));
        } else {
            std::lock_guard lock(import_mutex_);
            import_.active = true;
            if (p[0] == "sinespacio" && p.size() >= 4)
                import_.errors.push_back(TrFmt("No cabe «%s»: pesa %s y tu celular tiene %s libres.", p[1].c_str(),
                                               HumanSize(std::stoull(p[2])).c_str(), HumanSize(std::stoull(p[3])).c_str()));
            else
                import_.errors.push_back(TrFmt("No pude recibir «%s» desde la otra app.", p.size() > 1 ? p[1].c_str() : "?"));
            Log::Warn("Recepción fallida: %s", r.c_str());
        }
    }
    {
        const std::string st = Android::ReceiveStatus();
        std::lock_guard lock(import_mutex_);
        import_.receiving = st;
        if (!st.empty()) import_.active = true;
    }
    // Arranca el trabajo en segundo plano si hay algo en la fila.
    bool start = false;
    {
        std::lock_guard lock(import_mutex_);
        start = !import_queue_.empty() && !import_busy_;
        if (start) import_busy_ = true;
    }
    if (start) {
        if (import_thread_.joinable()) import_thread_.join();
        import_thread_ = std::thread([this] { ImportWorker(); });
    }
    // Terminó: se agrega la carpeta y se busca.
    bool finished = false;
    {
        std::lock_guard lock(import_mutex_);
        if (import_.rescan) {
            import_.rescan = false;
            finished = true;
        }
    }
    if (finished) {
        const std::string stored = Paths::StoreFolder(ImportFolder());
        if (std::find(settings_.folders.begin(), settings_.folders.end(), stored) == settings_.folders.end()) settings_.folders.push_back(stored);
        SaveConfig();
        Rescan();
        if (in_game_) {
            std::lock_guard lock(import_mutex_);
            int n = 0;
            for (const auto& [k, c] : import_.added) n += c;
            Toast(TrFmt("Agregué %d juegos a tu biblioteca.", n), GuideId::Samy, 5);
        }
    }
}

void App::ImportWorker() {
    // Lo que ya está en la biblioteca (nombre y tamaño), para no repetir.
    std::set<std::pair<std::string, uint64_t>> have;
    for (const auto& g : library_.Snapshot()) {
        std::error_code ec;
        const uint64_t sz = fs::file_size(g.path, ec);
        if (!ec) have.insert({Lower(Paths::ToUtf8(g.path.filename())), sz});
        // Dentro de un .zip: el juego de adentro también cuenta.
        if (g.Archived()) {
            std::vector<Archive::Entry> es;
            if (Archive::List(g.path, es))
                for (const auto& en : es) {
                    const size_t cut = en.name.find_last_of("/\\");
                    have.insert({Lower(cut == std::string::npos ? en.name : en.name.substr(cut + 1)), en.size});
                }
        }
    }
    const fs::path root = ImportFolder();
    std::error_code ec;
    fs::create_directories(root, ec);

    auto set_phase = [&](int phase, const std::string& name, float progress) {
        std::lock_guard lock(import_mutex_);
        import_.phase = phase;
        import_.name = name;
        import_.progress = progress;
    };
    auto add = [&](const std::string& sys_name) {
        std::lock_guard lock(import_mutex_);
        ++import_.added[sys_name];
    };
    auto skip = [&] {
        std::lock_guard lock(import_mutex_);
        ++import_.skipped;
    };
    auto error = [&](const std::string& msg) {
        std::lock_guard lock(import_mutex_);
        import_.errors.push_back(msg);
    };
    auto unique_target = [](fs::path p) {
        std::error_code e;
        if (!fs::exists(p, e)) return p;
        const fs::path stem = p.stem(), ext = p.extension(), dir = p.parent_path();
        for (int i = 2; i < 100; ++i) {
            fs::path q = dir / (Paths::ToUtf8(stem) + " (" + std::to_string(i) + ")" + Paths::ToUtf8(ext));
            if (!fs::exists(q, e)) return q;
        }
        return p;
    };
    // Mueve (o copia y borra si está en otra unidad).
    auto move = [](const fs::path& from, const fs::path& to) {
        std::error_code e;
        fs::create_directories(to.parent_path(), e);
        fs::rename(from, to, e);
        if (e) {
            e.clear();
            fs::copy_file(from, to, fs::copy_options::overwrite_existing, e);
            if (!e) fs::remove(from, e);
        }
        return !e;
    };
    auto try_bios = [&](const fs::path& f) {
        std::error_code e;
        const uint64_t sz = fs::file_size(f, e);
        const SystemDef* sys = nullptr;
        const BiosFile* b = BiosFor(Lower(Paths::ToUtf8(f.filename())), sz, &sys);
        if (!b) return false;
        fs::create_directories(Paths::System(), e);
        fs::copy_file(f, Paths::System() / b->name, fs::copy_options::overwrite_existing, e);
        if (!e) {
            std::lock_guard lock(import_mutex_);
            import_.bios.push_back(std::string(b->name) + " (" + sys->full + ")");
            Log::Info("BIOS recibida: %s para %s", b->name, sys->full.c_str());
        }
        return true;
    };
    // Acomoda un juego suelto (o un .cue con sus pistas) en la carpeta de su consola.
    auto place_game = [&](const fs::path& f, const std::vector<fs::path>& extra) {
        std::error_code e;
        const uint64_t sz = fs::file_size(f, e);
        if (have.count({Lower(Paths::ToUtf8(f.filename())), sz})) {
            skip();
            if (f.parent_path().filename() == "recibidos") fs::remove(f, e);
            return;
        }
        const Game g = Library::MakeGame(f);
        const SystemDef* sys = SystemById(g.system);
        const std::string folder = sys ? sys->name : std::string("Otros");
        fs::path target;
        if (extra.empty()) {
            target = unique_target(root / Paths::FromUtf8(folder) / f.filename());
            move(f, target);
        } else {
            // Disco con varias pistas: todo junto en su carpeta.
            const fs::path dir = unique_target(root / Paths::FromUtf8(folder) / f.stem());
            move(f, dir / f.filename());
            for (const auto& x : extra) move(x, dir / x.filename());
            target = dir / f.filename();
        }
        have.insert({Lower(Paths::ToUtf8(f.filename())), sz});
        add(sys ? sys->full : std::string("Otros"));
        Log::Info("Importado: %s", Paths::ToUtf8(target).c_str());
    };

    while (true) {
        fs::path file;
        int index = 0, total = 0;
        {
            std::lock_guard lock(import_mutex_);
            if (import_queue_.empty()) {
                import_busy_ = false;
                import_.phase = 0;
                import_.done = true;
                import_.rescan = true;
                return;
            }
            file = import_queue_.front();
            import_queue_.erase(import_queue_.begin());
            index = ++import_.files_done;
            total = import_.files_done + static_cast<int>(import_queue_.size());
            import_.files_total = total;
        }
        const std::string name = Paths::ToUtf8(file.filename());
        const std::string ext = ExtOf(file);
        const bool received = file.parent_path().filename() == "recibidos";
        set_phase(2, name, 0);

        if (!Archive::IsArchive(ext)) {
            if (try_bios(file)) {
                if (received) fs::remove(file, ec);
            } else if (IsGameExt(ext)) {
                place_game(file, {});
            } else {
                error(TrFmt("«%s» no es un juego que Satronho conozca.", name.c_str()));
                if (received) fs::remove(file, ec);
            }
            continue;
        }

        std::vector<Archive::Entry> entries;
        if (!Archive::List(file, entries)) {
            error(TrFmt("No pude abrir «%s»: puede estar dañado o incompleto.", name.c_str()));
            continue;
        }
        uint64_t unpacked = 0;
        int games = 0, discs = 0;
        std::string only;
        uint64_t only_size = 0;
        for (const auto& en : entries) {
            unpacked += en.size;
            const std::string e2 = Lower(en.name.substr(en.name.find_last_of('.') == std::string::npos ? en.name.size() : en.name.find_last_of('.') + 1));
            if (e2 == "cue" || e2 == "chd" || e2 == "m3u" || e2 == "pbp" || e2 == "ccd" || e2 == "iso") ++discs;
            else if (e2 != "bin" && SystemForExtension(e2)) {
                ++games;
                only = en.name;
                only_size = en.size;
            }
        }
        // Un solo juego de cartucho: queda comprimido, así ya se juega.
        if (games == 1 && discs == 0 && entries.size() <= 3) {
            const std::string base = Lower(only.substr(only.find_last_of("/\\") == std::string::npos ? 0 : only.find_last_of("/\\") + 1));
            if (have.count({base, only_size})) {
                skip();
                if (received) fs::remove(file, ec);
                continue;
            }
            const Game g = Library::MakeGame(file);
            const SystemDef* sys = SystemById(g.system);
            const fs::path target = unique_target(root / Paths::FromUtf8(sys ? sys->name : std::string("Otros")) / file.filename());
            if (received) move(file, target);
            else fs::copy_file(file, target, ec);
            have.insert({base, only_size});
            add(sys ? sys->full : std::string("Otros"));
            continue;
        }
        // Varios juegos o de disco: se descomprime. Primero, ¿hay espacio?
        const auto space = fs::space(root, ec);
        const uint64_t need = unpacked + (64ull << 20);
        if (!ec && space.available < need) {
            error(TrFmt("No hay espacio para descomprimir «%s»: necesita %s y quedan %s libres.", name.c_str(), HumanSize(need).c_str(),
                        HumanSize(space.available).c_str()));
            continue;
        }
        const fs::path tmp = root / ".descomprimiendo" / Paths::SafeName(Paths::ToUtf8(file.stem()));
        fs::remove_all(tmp, ec);
        fs::create_directories(tmp, ec);
        set_phase(3, name, 0);
        const bool ok = Archive::ExtractAll(file, tmp, [&](float p) {
            std::lock_guard lock(import_mutex_);
            import_.progress = p;
            return true;
        });
        if (!ok) {
            error(TrFmt("No pude descomprimir «%s»: puede estar dañado o incompleto.", name.c_str()));
            fs::remove_all(tmp, ec);
            continue;
        }
        set_phase(4, name, 1);
        // Primero los .cue (se llevan sus pistas), después lo demás.
        std::vector<fs::path> all;
        for (auto it = fs::recursive_directory_iterator(tmp, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
            if (it->is_regular_file(ec)) all.push_back(it->path());
        std::set<fs::path> used;
        for (const auto& f : all) {
            const std::string e2 = ExtOf(f);
            if (e2 != "cue" && e2 != "ccd" && e2 != "m3u") continue;
            std::vector<fs::path> extra;
            for (const auto& t : CueFiles(f)) {
                const fs::path tp = f.parent_path() / Paths::FromUtf8(t);
                if (fs::exists(tp, ec)) {
                    extra.push_back(tp);
                    used.insert(tp);
                }
            }
            used.insert(f);
            // El tamaño que cuenta para saber si ya lo tienes es el del .cue (y su nombre).
            place_game(f, extra);
        }
        for (const auto& f : all) {
            if (used.count(f)) continue;
            if (try_bios(f)) continue;
            if (IsGameExt(ExtOf(f)) && !Archive::IsArchive(ExtOf(f))) place_game(f, {});
        }
        fs::remove_all(tmp, ec);
        if (received) fs::remove(file, ec);
    }
}

void App::DrawImport() {
    ImportState st;
    {
        std::lock_guard lock(import_mutex_);
        if (!import_.active) return;
        st = import_;
    }
    if (in_game_) return; // jugando, solo avisa con un mensaje al terminar
    const bool working = !st.done || !st.receiving.empty() || import_busy_;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(4, 3, 12, 190));
    ImGui::Begin("##velo_importar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::End();
    ImGui::PopStyleColor();

    const float w = std::min(vp->Size.x - S(24), S(620));
    ImGui::SetNextWindowPos({vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f}, 0, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({w, 0});
    ImGui::SetNextWindowFocus();
    ImGui::PushStyleColor(ImGuiCol_WindowBg, V4(Color::Surface));
    ImGui::PushStyleColor(ImGuiCol_Border, V4(Color::Accent, 0.35f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, S(18));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, S(1.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {S(22), S(20)});
    ImGui::Begin("##importar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                            ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);

    int added = 0;
    for (const auto& [k, c] : st.added) added += c;
    std::string text;
    float progress = -1;
    if (!st.receiving.empty()) {
        // «nombre|copiados|total|pendientes»
        std::vector<std::string> p;
        size_t pos = 0;
        while (true) {
            const size_t bar = st.receiving.find('|', pos);
            p.push_back(st.receiving.substr(pos, bar == std::string::npos ? std::string::npos : bar - pos));
            if (bar == std::string::npos) break;
            pos = bar + 1;
        }
        if (p.size() >= 4) {
            const double got = std::atof(p[1].c_str()), all = std::atof(p[2].c_str());
            progress = all > 0 ? static_cast<float>(got / all) : -1;
            text = TrFmt("Recibiendo «%s» desde la otra app… %s", p[0].c_str(), HumanSize(static_cast<uint64_t>(got)).c_str());
            if (std::atoi(p[3].c_str()) > 1) text += "  ·  " + TrFmt("faltan %d", std::atoi(p[3].c_str()));
        } else {
            text = Tr("Recibiendo desde la otra app…");
        }
    } else if (working) {
        progress = st.phase == 3 ? st.progress : -1;
        const char* what = st.phase == 3 ? "Descomprimiendo «%s»…" : st.phase == 4 ? "Acomodando los juegos de «%s» en sus consolas…" : "Revisando «%s»…";
        text = TrFmt(what, st.name.c_str());
        if (st.files_total > 1) text += "  ·  " + TrFmt("%d de %d", st.files_done, st.files_total);
    } else if (added > 0) {
        text = TrFmt("¡Listo! Agregué %d juegos a tu biblioteca:", added);
        bool first = true;
        for (const auto& [sys, c] : st.added) {
            text += (first ? " " : ", ") + std::to_string(c) + " " + Tr("de") + " " + sys;
            first = false;
        }
        text += ".";
    } else if (st.errors.empty() && st.bios.empty()) {
        text = Tr("Ya tenías todos esos juegos en tu biblioteca: no agregué nada repetido.");
    } else if (st.errors.empty()) {
        text = Tr("¡Listo!");
    } else {
        text = Tr("No pude agregar los juegos. Abajo te cuento qué pasó.");
    }
    GuideSays(working ? GuideId::Samy : (st.errors.empty() ? GuideId::Samy : GuideId::Nono), text.c_str(), w - S(44));
    if (working) {
        ImGui::Dummy({0, S(10)});
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float bw = ImGui::GetContentRegionAvail().x, bh = S(8);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p, {p.x + bw, p.y + bh}, U32(Color::Surface2), bh * 0.5f);
        if (progress >= 0) {
            dl->AddRectFilled(p, {p.x + bw * std::clamp(progress, 0.02f, 1.0f), p.y + bh}, U32(Color::Accent), bh * 0.5f);
        } else {
            // Sin porcentaje: una barrita que va y viene.
            const float t = static_cast<float>(std::fmod(ImGui::GetTime() * 0.7, 1.0));
            const float x0 = p.x + (bw * 1.3f) * t - bw * 0.3f;
            dl->AddRectFilled({std::max(p.x, x0), p.y}, {std::min(p.x + bw, x0 + bw * 0.3f), p.y + bh}, U32(Color::Accent), bh * 0.5f);
        }
        ImGui::Dummy({bw, bh + S(8)});
        Muted("Puedes dejar la app abierta mientras tanto. Los juegos grandes de PS1 tardan un poco.", 12.5f);
    } else {
        ImGui::Dummy({0, S(6)});
        if (st.skipped > 0) Muted(TrFmt("No repetí %d que ya tenías.", st.skipped).c_str(), 13);
        for (const auto& b : st.bios) Muted((std::string(Tr("BIOS agregada: ")) + b).c_str(), 13);
        for (const auto& e : st.errors) {
            ImGui::PushStyleColor(ImGuiCol_Text, V4(Color::Pink));
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + w - S(44));
            ImGui::TextWrapped("%s", e.c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
        }
        if (added > 0) Muted("Los guardé en la carpeta de juegos de Satronho, separados por consola. Ya puedes borrar el archivo del chat si quieres.", 12.5f);
        ImGui::Dummy({0, S(10)});
        const float bw = (ImGui::GetContentRegionAvail().x - S(10)) * 0.5f;
        bool close = false;
        if (added > 0) {
            if (Button("Ver mi biblioteca", ButtonKind::Primary, {bw, S(46)}, Icon::Library, true)) {
                page_ = Page::Library;
                lib_system_.clear();
                close = true;
            }
            ImGui::SameLine(0, S(10));
        }
        if (Button("Cerrar", ButtonKind::Secondary, {added > 0 ? bw : ImGui::GetContentRegionAvail().x, S(46)})) close = true;
        if (close) {
            std::lock_guard lock(import_mutex_);
            import_ = ImportState{};
        }
    }
    ImGui::End();
}

} // namespace Satronho
