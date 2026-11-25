// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Build 26: buscar juegos en todo el teléfono, la carpeta «Satronho Games» con una carpeta por
// consola, ordenar lo encontrado ahí, y Satronho NX (juegos de Switch) abierto desde el hub.
#include "app.h"

#include <algorithm>
#include <fstream>

#include "native_page.h"
#include "sys/android.h"
#include "sys/native_ui.h"
#include "util/i18n.h"
#include "util/log.h"

namespace Satronho {

namespace {

std::string Join(const std::vector<std::string>& v) {
    std::string s;
    for (const auto& x : v) s += (s.empty() ? "" : "|") + x;
    return s;
}

std::vector<std::string> SplitBar(const std::string& s) {
    std::vector<std::string> out;
    size_t a = 0;
    while (a <= s.size() && !s.empty()) {
        const size_t b = s.find('|', a);
        const std::string part = s.substr(a, b == std::string::npos ? std::string::npos : b - a);
        if (!part.empty()) out.push_back(part);
        if (b == std::string::npos) break;
        a = b + 1;
    }
    return out;
}

void WriteIfMissing(const fs::path& file, const std::string& text) {
    std::error_code ec;
    if (fs::exists(file, ec)) return;
    std::ofstream out(file, std::ios::binary);
    out << text;
}

// Los archivos que van con un juego de disco (.cue → .bin, .gdi → pistas, .m3u → discos).
std::vector<fs::path> PartsOf(const Game& g) {
    std::vector<fs::path> parts;
    if (g.Archived() || (g.ext != "cue" && g.ext != "gdi" && g.ext != "m3u")) return parts;
    std::ifstream in(g.path);
    std::string line;
    int lines = 0;
    while (std::getline(in, line) && ++lines < 400) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::string name;
        if (g.ext == "cue") {
            const size_t a = line.find('"'), b = line.rfind('"');
            std::string low = line;
            for (auto& c : low) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (low.find("file") == std::string::npos || a == std::string::npos || b <= a) continue;
            name = line.substr(a + 1, b - a - 1);
        } else if (g.ext == "gdi") {
            // «1 0 4 2352 track01.bin 0» (el nombre puede ir entre comillas)
            const size_t q1 = line.find('"');
            if (q1 != std::string::npos) {
                const size_t q2 = line.find('"', q1 + 1);
                if (q2 != std::string::npos) name = line.substr(q1 + 1, q2 - q1 - 1);
            } else {
                std::vector<std::string> w;
                size_t a = 0;
                while (a < line.size()) {
                    while (a < line.size() && line[a] == ' ') ++a;
                    size_t b = line.find(' ', a);
                    if (a < line.size()) w.push_back(line.substr(a, b == std::string::npos ? std::string::npos : b - a));
                    if (b == std::string::npos) break;
                    a = b;
                }
                if (w.size() >= 5) name = w[4];
            }
        } else {
            if (line.empty() || line[0] == '#') continue;
            name = line;
        }
        if (!name.empty()) parts.push_back(g.path.parent_path() / Paths::FromUtf8(name));
    }
    return parts;
}

} // namespace

// ---------------------------------------------------------------- carpeta «Satronho Games»

// Una carpeta para cada consola (con un «Léeme» que dice qué va ahí), más Claves y firmware (para
// Satronho NX) y BIOS. Si ya existe no se toca nada de lo que hay adentro: solo se agrega lo que falta.
bool App::EnsureGamesTree(const fs::path& root) {
    if (root.empty()) return false;
    std::error_code ec;
    const bool nueva = !fs::exists(root / "Juegos" / "Octa", ec);
    std::vector<const SystemDef*> lista;
    for (const auto& s : Systems()) lista.push_back(&s);
    if (NxAvailable()) lista.push_back(&NxSystem());
    for (const SystemDef* s : lista) {
        const fs::path dir = root / "Juegos" / Paths::FromUtf8(s->name);
        fs::create_directories(dir, ec);
        std::string exts;
        for (const auto& e : s->extensions) exts += (exts.empty() ? "." : ", .") + e;
        WriteIfMissing(dir / "Léeme.txt", TrFmt("%s · para %s.\n\nPon aquí tus juegos (%s; también sirven .zip y .7z). Satronho Emulation los encuentra solo.\n",
                                                Tr(s->full).c_str(), Tr(s->games).c_str(), exts.c_str()));
    }
    fs::create_directories(root / "Claves y firmware", ec);
    WriteIfMissing(root / "Claves y firmware" / "Léeme.txt",
                   Tr("Para Satronho NX (juegos de Switch): pon aquí tus prod.keys y title.keys y el firmware (.zip) sacados de tu propia consola. "
                      "Satronho Emulation se los pasa a NX.\n"));
    fs::create_directories(root / "BIOS", ec);
    WriteIfMissing(root / "BIOS" / "Léeme.txt",
                   Tr("Pon aquí las BIOS de tus consolas (por ejemplo scph5501.bin para CD). Satronho Emulation las reconoce y las usa; también deja aquí una "
                      "copia de las que ya tenía, para que las respaldes.\n"));
    if (nueva) Log::Info("Satronho Games: carpetas por consola creadas en %s", Paths::ToUtf8(root).c_str());
    return nueva;
}

// ---------------------------------------------------------------- buscar en todo el teléfono

void App::StartPhoneScan() {
    if (phone_scan_.Running()) return;
    std::vector<fs::path> roots;
    for (const auto& r : Android::Roots()) roots.push_back(Paths::FromUtf8(r));
    if (roots.empty()) {
        Toast("Para buscar en todo el teléfono necesito el permiso de archivos.", GuideId::Samy, 5);
        Android::RequestFileAccess();
        return;
    }
    std::set<std::string> known;
    for (const auto& g : library_.Snapshot()) known.insert(g.key);
    phone_result_ = {};
    phone_result_ready_ = false;
    phone_scan_.Start(roots, std::move(known), Paths::PublicRoot());
    ini_.Set("escaneo.hecho", "1");
    native_page_t_ = -10;
}

void App::PollPhoneScan() {
    if (!phone_scan_.TakeDone(phone_result_)) return;
    phone_result_ready_ = true;
    // Lo de Switch se recuerda para el asistente de NX (aunque la persona no agregue nada).
    std::vector<std::string> keys, fw;
    for (const auto& k : phone_result_.keys) keys.push_back(Paths::ToUtf8(k));
    for (const auto& f : phone_result_.firmware) fw.push_back(Paths::ToUtf8(f));
    if (!keys.empty()) ini_.Set("nx.claves", Join(keys));
    if (!fw.empty()) ini_.Set("nx.firmware", Join(fw));
    SaveConfig();
    native_lib_t_ = -10;
    native_page_t_ = -10;
    const int n = phone_result_.NewGames();
    if (native_page_ != "escaneo") {
        if (n > 0)
            NativeUi::Emit("{\"t\":\"tarjeta\",\"id\":\"escaneo\",\"guia\":\"samy\",\"titulo\":" + NativeUi::Quote(Tr("¡Encontré juegos!")) + ",\"texto\":" +
                           NativeUi::Quote(TrFmt("Hay %d juegos en tu teléfono que todavía no están en la biblioteca. ¿Te los muestro?", n)) +
                           ",\"botones\":[[\"no\"," + NativeUi::Quote(Tr("Ahora no")) + "],[\"ver\"," + NativeUi::Quote(Tr("Ver")) + "]]}");
        else
            Toast("Revisé todo el teléfono: no hay juegos nuevos.", GuideId::Samy, 4);
    }
}

std::string App::NxFilesJson() const {
    std::string s = "{\"claves\":[";
    bool first = true;
    for (const auto& k : SplitBar(ini_.Get("nx.claves"))) {
        s += (first ? "" : ",") + NativeUi::Quote(k);
        first = false;
    }
    // Lo que la persona puso en «Claves y firmware» también cuenta.
    const fs::path mine = Paths::PublicRoot().empty() ? fs::path{} : Paths::PublicRoot() / "Claves y firmware";
    std::vector<std::string> extra_fw;
    std::error_code ec;
    if (!mine.empty() && fs::exists(mine, ec))
        for (auto it = fs::directory_iterator(mine, ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
            std::string n = Paths::ToUtf8(it->path().filename());
            for (auto& c : n) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (n == "prod.keys" || n == "title.keys") {
                s += (first ? "" : ",") + NativeUi::Quote(Paths::ToUtf8(it->path()));
                first = false;
            } else if (n.size() > 4 && n.substr(n.size() - 4) == ".zip") {
                extra_fw.push_back(Paths::ToUtf8(it->path()));
            }
        }
    s += "],\"firmware\":[";
    first = true;
    for (const auto& f : extra_fw) {
        s += (first ? "" : ",") + NativeUi::Quote(f);
        first = false;
    }
    for (const auto& f : SplitBar(ini_.Get("nx.firmware"))) {
        s += (first ? "" : ",") + NativeUi::Quote(f);
        first = false;
    }
    s += "]}";
    return s;
}

// Lo encontrado: o se agregan sus carpetas a la biblioteca tal como están, o los juegos se mudan a
// «Satronho Games», cada uno a la carpeta de su consola.
void App::AddFoundFolders() {
    int added = 0;
    for (const auto& f : phone_result_.folders) {
        if (!f.chosen) continue;
        const std::string stored = Paths::StoreFolder(f.path);
        if (std::find(settings_.folders.begin(), settings_.folders.end(), stored) == settings_.folders.end()) {
            settings_.folders.push_back(stored);
            ++added;
        }
    }
    SaveConfig();
    Rescan();
    phone_result_ready_ = false;
    Toast(TrFmt("Listo: agregué %d carpetas a la biblioteca. Los juegos se quedan donde estaban.", added), GuideId::Samy, 5);
}

void App::BeginOrganize() {
    if (organize_state_ != 0 || move_state_ != 0) return;
    const fs::path root = Paths::PublicRoot();
    if (root.empty()) {
        Toast("Primero dame el permiso de archivos para armar tu carpeta Satronho Games.", GuideId::Samy, 5);
        Android::RequestFileAccess();
        return;
    }
    EnsureGamesTree(root);
    struct Item {
        Game g;
        std::vector<fs::path> parts;
    };
    std::vector<Item> items;
    for (const auto& f : phone_result_.folders)
        if (f.chosen)
            for (const auto& g : f.games) items.push_back({g, PartsOf(g)});
    phone_result_ready_ = false;
    if (items.empty()) return;
    organize_done_ = 0;
    organize_total_ = static_cast<int>(items.size());
    organize_failed_ = 0;
    organize_state_ = 1;
    if (move_thread_.joinable()) move_thread_.join();
    move_thread_ = std::thread([this, items, root] {
        std::vector<std::pair<std::string, std::string>> rekeys;
        int failed = 0;
        auto move_one = [](const fs::path& from, const fs::path& to) {
            std::error_code e;
            if (fs::exists(to, e)) return fs::file_size(to, e) == fs::file_size(from, e); // ya estaba
            fs::create_directories(to.parent_path(), e);
            fs::rename(from, to, e);
            if (!e) return true;
            e.clear();
            const uint64_t size = fs::file_size(from, e);
            fs::copy_file(from, to, fs::copy_options::overwrite_existing, e);
            if (e || fs::file_size(to, e) != size) return false;
            fs::remove(from, e);
            return true;
        };
        for (const auto& it : items) {
            const SystemDef* sys = SystemById(it.g.system);
            const fs::path dir = root / "Juegos" / Paths::FromUtf8(sys ? sys->name : std::string("Otros"));
            const fs::path dst = dir / it.g.path.filename();
            bool ok = true;
            // Las pistas primero; el .cue/.gdi/.m3u al final (así nunca queda uno sin sus pistas).
            for (const auto& part : it.parts) {
                std::error_code e;
                if (!fs::exists(part, e)) continue;
                ok &= move_one(part, dir / part.filename());
            }
            ok = ok && move_one(it.g.path, dst);
            if (ok) rekeys.push_back({it.g.key, Library::KeyFor(dst)});
            else {
                ++failed;
                Log::Warn("No se pudo ordenar %s", it.g.path_utf8.c_str());
            }
            ++organize_done_;
        }
        {
            std::lock_guard lock(move_mutex_);
            move_rekeys_ = std::move(rekeys);
            organize_failed_ = failed;
        }
        organize_state_ = 2;
    });
}

void App::PollOrganize() {
    if (organize_state_ != 2) return;
    if (move_thread_.joinable()) move_thread_.join();
    std::vector<std::pair<std::string, std::string>> rekeys;
    {
        std::lock_guard lock(move_mutex_);
        rekeys.swap(move_rekeys_);
    }
    library_.Rekey(rekeys);
    const std::string games = Paths::StoreFolder(Paths::PublicRoot() / "Juegos");
    if (std::find(settings_.folders.begin(), settings_.folders.end(), games) == settings_.folders.end()) settings_.folders.push_back(games);
    SaveConfig();
    Rescan();
    organize_state_ = 0;
    const int failed = organize_failed_;
    Log::Info("Ordenar en Satronho Games: %d juegos, %d sin mover", organize_total_.load(), failed);
    if (failed > 0)
        Toast(TrFmt("Ordené tus juegos en Satronho Games, pero %d no se pudieron mover y siguen donde estaban.", failed), GuideId::Nono, 7);
    else
        Toast(TrFmt("¡Listo! %d juegos ordenados en Satronho Games, cada uno en la carpeta de su consola.", organize_total_.load()), GuideId::Samy, 6);
}

void App::NativePagePhoneScan(NPage& p) {
    if (phone_scan_.Running()) {
        p.Section("");
        p.Guide("samy", TrFmt("Estoy revisando tu teléfono… %d archivos mirados, %d juegos hasta ahora.", phone_scan_.Files(), phone_scan_.Found()));
        if (const std::string c = phone_scan_.Current(); !c.empty()) p.Info(TrFmt("Ahora en «%s»", c.c_str()), "", "carpeta");
        p.Info("Puedes cerrar esta pantalla: sigo buscando y te aviso al terminar.", "", "info");
        p.Action("escaneo_cancelar", "Detener la búsqueda", "", "cerrar", [this] { phone_scan_.Cancel(); });
        return;
    }
    if (!phone_result_ready_) {
        p.Section("");
        p.Guide("samy", "Busco juegos en todo tu teléfono: descargas, WhatsApp, Telegram, la tarjeta SD… Me salto las fotos y lo de otras apps para ir rápido. "
                        "Al final tú eliges qué agregar; no muevo nada sin preguntarte.");
        p.Action("escaneo_empezar", "Buscar juegos en todo el teléfono", "", "buscar", [this] { StartPhoneScan(); }, "primario");
        return;
    }
    const PhoneScan::Result& r = phone_result_;
    const int n = r.NewGames();
    p.Section("");
    if (n == 0) p.Guide("samy", TrFmt("Revisé %d archivos y no hay juegos nuevos: todo lo que tienes ya está en la biblioteca.", r.files));
    else
        p.Guide("samy", TrFmt("Encontré %d juegos nuevos en %d carpetas. Elige cuáles quieres y cómo: ordenados en Satronho Games (cada uno en su consola) o "
                              "dejándolos donde están.",
                              n, static_cast<int>(r.folders.size())));
    if (n > 0) {
        p.Section("Carpetas");
        for (size_t i = 0; i < r.folders.size(); ++i) {
            const auto& f = r.folders[i];
            std::map<std::string, int> por;
            for (const auto& g : f.games) ++por[g.system];
            std::string desc;
            for (const auto& [sid, c] : por) {
                const SystemDef* sd = SystemById(sid);
                desc += (desc.empty() ? "" : " · ") + TrFmt("%d de %s", c, sd ? Tr(sd->name).c_str() : sid.c_str());
            }
            std::string where = Paths::ToUtf8(f.path);
            for (const auto& root : Android::Roots())
                if (where.rfind(root, 0) == 0) where = where.substr(root.size());
            if (where.empty()) where = "/";
            p.Toggle("escaneo_carpeta" + std::to_string(i), where, desc, f.chosen, [this, i](bool v) {
                if (i < phone_result_.folders.size()) phone_result_.folders[i].chosen = v;
            }, "carpeta");
        }
        p.Section("");
        p.Action("escaneo_ordenar", "Ordenar en Satronho Games", "Los muevo a la carpeta de su consola. Sus partidas y su tiempo de juego van con ellos.",
                 "estrella", [this] { BeginOrganize(); }, "primario");
        p.Action("escaneo_agregar", "Agregarlos sin moverlos", "Sumo esas carpetas a la biblioteca y los juegos se quedan donde están.", "carpeta",
                 [this] { AddFoundFolders(); });
    }
    if (NxAvailable() && (!r.keys.empty() || !r.firmware.empty())) {
        p.Section("Para Satronho NX");
        if (!r.keys.empty()) p.Info(TrFmt("Encontré tus claves (%d archivos)", static_cast<int>(r.keys.size())), "Se las paso a NX cuando juegues algo de Switch.", "llave");
        if (!r.firmware.empty()) p.Info("Encontré un firmware", "Lo instalo en NX desde el asistente, cuando juegues algo de Switch.", "chip");
    }
    p.Section("");
    p.Action("escaneo_otra", "Buscar otra vez", "", "refrescar", [this] { StartPhoneScan(); });
}

std::string App::NativeOrganizeJson() const {
    if (organize_state_ != 1) return {};
    const int done = organize_done_, total = organize_total_;
    return ",\"progreso\":{\"id\":\"ordenar\",\"guia\":\"samy\",\"texto\":" +
           NativeUi::Quote(TrFmt("Estoy ordenando tus juegos en Satronho Games, cada uno en la carpeta de su consola. %d de %d.", done, total)) +
           ",\"avance\":" + NPage::Num(total > 0 ? static_cast<float>(done) / total : -1.0f) + ",\"nota\":" +
           NativeUi::Quote(Tr("No cierres la app mientras tanto.")) + ",\"botones\":[]}";
}

// ---------------------------------------------------------------- Satronho NX

void App::LaunchExternal(const Game& g) {
    if (!NxAvailable()) {
        Toast("Los juegos de Switch se juegan con Satronho NX, en el celular.", GuideId::MechaRidds, 5);
        return;
    }
    const std::string nombre = library_.GameOption(g.key, "nombre");
    NativeUi::Emit("{\"t\":\"nx\",\"clave\":" + NativeUi::Quote(g.key) + ",\"ruta\":" + NativeUi::Quote(g.path_utf8) + ",\"titulo\":" +
                   NativeUi::Quote(nombre.empty() ? g.title : nombre) + "}");
    Log::Info("Satronho NX: abrir %s", g.path_utf8.c_str());
}

bool App::NativeCommandPhone(const std::vector<std::string>& a) {
    const std::string& cmd = a[0];
    auto arg = [&](size_t i) -> std::string { return i < a.size() ? a[i] : std::string(); };
    if (cmd == "escaneo") {
        if (arg(1) == "empezar") StartPhoneScan();
        native_page_ = "escaneo";
        native_page_arg_.clear();
        native_page_t_ = -10;
        return true;
    }
    if (cmd == "restaurar") {
        RestoreAndTell(Paths::FromUtf8(arg(1)));
        return true;
    }
    if (cmd == "aviso") {
        // Un aviso que arma Kotlin (respaldos de NX…), con la voz de un guía.
        Toast(arg(2), arg(1) == "nono" ? GuideId::Nono : arg(1) == "troya" ? GuideId::Troya : GuideId::Juana, 6);
        return true;
    }
    if (cmd == "nx_info") {
        // nx_info|clave|titulo|programa|png del ícono
        if (const Game* g = FindGame(arg(1))) {
            if (!arg(2).empty()) library_.SetGameOption(g->key, "nombre", arg(2));
            if (!arg(3).empty()) library_.SetGameOption(g->key, "programa", arg(3));
            if (!arg(4).empty() && !covers_.HasCustom(*g)) covers_.SetCustom(*g, Paths::FromUtf8(arg(4)));
            games_dirty_ = true;
            native_lib_t_ = -10;
        }
        return true;
    }
    if (cmd == "nx_volvio") {
        // El juego de NX terminó y volvimos al hub: su tiempo de juego queda aquí.
        const long long s = std::atoll(arg(2).c_str());
        if (const Game* g = FindGame(arg(1))) {
            library_.MarkPlayed(g->key);
            if (s > 0) library_.AddPlaytime(g->key, s);
            games_dirty_ = true;
            native_lib_t_ = -10;
            Log::Info("Satronho NX: %s, %lld s", g->title.c_str(), s);
        }
        return true;
    }
    return false;
}

} // namespace Satronho
