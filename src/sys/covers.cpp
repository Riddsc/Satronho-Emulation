// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "sys/covers.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>

#include <SDL3/SDL.h>

#include "util/log.h"
#include "util/net.h"

namespace Satronho {

namespace {
constexpr int kMaxTextures = 220;   // carátulas en la tarjeta gráfica a la vez
constexpr int kMaxSide = 512;       // se reducen a este tamaño como máximo
constexpr long long kRetryDays = 30; // si no se encontró, se vuelve a intentar al mes

// Nombre de la consola en libretro-thumbnails.
std::string RepoFor(const std::string& system, const std::string& ext) {
    if (system == "octa") return "Nintendo - Nintendo Entertainment System";
    if (system == "hexa") return "Nintendo - Super Nintendo Entertainment System";
    if (system == "n64") return "Nintendo - Nintendo 64";
    if (system == "bolsillo") return ext == "gbc" ? "Nintendo - Game Boy Color" : "Nintendo - Game Boy";
    if (system == "bolsillo_plus") return "Nintendo - Game Boy Advance";
    if (system == "dual") return "Nintendo - Nintendo DS";
    if (system == "mega") return "Sega - Mega Drive - Genesis";
    if (system == "anillo") return "Sega - Saturn";
    if (system == "cd") return "Sony - PlayStation";
    if (system == "sueno") return "Sega - Dreamcast";
    if (system == "portatil") return "Sony - PlayStation Portable";
    return "";
}

std::string Underscored(std::string s) {
    for (char& c : s)
        if (c == ' ') c = '_';
    return s;
}

// libretro-thumbnails cambia estos caracteres por «_» en los nombres de archivo.
std::string ThumbName(std::string s) {
    for (char& c : s)
        if (std::strchr("&*/:`<>?\\|\"", c)) c = '_';
    return s;
}

// Título para comparar: sin etiquetas entre paréntesis o corchetes, sin artículos movidos
// («Legend of Zelda, The»), en minúsculas y solo letras y números.
std::string Norm(const std::string& name) {
    std::string base;
    int depth = 0;
    for (char c : name) {
        if (c == '(' || c == '[') ++depth;
        else if ((c == ')' || c == ']') && depth > 0) --depth;
        else if (!depth) base += c;
    }
    std::string low;
    for (unsigned char c : base) low += static_cast<char>(std::tolower(c));
    for (const char* art : {", the", ", a", ", an"}) {
        const size_t p = low.find(art);
        if (p != std::string::npos) low.erase(p, std::strlen(art));
    }
    if (low.rfind("the ", 0) == 0) low.erase(0, 4);
    std::string out;
    for (size_t i = 0; i < low.size(); ++i) {
        const unsigned char c = low[i];
        if (c == '&') out += "and";
        else if (std::isalnum(c) || c >= 0x80) out += static_cast<char>(c);
    }
    return out;
}

std::string Tags(const std::string& name) {
    std::string t;
    int depth = 0;
    for (char c : name) {
        if (c == '(' || c == '[') ++depth;
        if (depth) t += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if ((c == ')' || c == ']') && depth > 0) --depth;
    }
    return t;
}

int RegionScore(const std::string& tags, const std::string& wanted) {
    int s = 0;
    // Mismas etiquetas que el juego: lo mejor.
    if (!wanted.empty() && tags == wanted) s += 100;
    for (const char* r : {"usa", "world", "europe", "japan"}) {
        if (!wanted.empty() && wanted.find(r) != std::string::npos && tags.find(r) != std::string::npos) s += 30;
    }
    if (tags.find("usa") != std::string::npos) s += 12;
    else if (tags.find("world") != std::string::npos) s += 10;
    else if (tags.find("europe") != std::string::npos) s += 8;
    else if (tags.find("japan") != std::string::npos) s += 5;
    if (tags.find("beta") != std::string::npos || tags.find("proto") != std::string::npos || tags.find("demo") != std::string::npos) s -= 20;
    if (tags.find("rev") != std::string::npos) s -= 1;
    return s;
}

std::string ReadAll(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// Saca los nombres de las carátulas de una página (índice de la web o lista de GitHub).
std::vector<std::string> ParseNames(const std::string& text) {
    std::vector<std::string> out;
    size_t pos = 0;
    while ((pos = text.find(".png", pos)) != std::string::npos) {
        size_t start = pos;
        while (start > 0 && text[start - 1] != '"' && text[start - 1] != '/' && text[start - 1] != '>' && text[start - 1] != '\n') --start;
        std::string name = Net::UrlDecode(text.substr(start, pos - start));
        pos += 4;
        if (!name.empty() && name.size() < 300) out.push_back(name);
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}
} // namespace

Covers::~Covers() { Shutdown(); }

void Covers::Init() {
    dir_ = Paths::User() / "caratulas";
    std::error_code ec;
    fs::create_directories(dir_ / "propias", ec);
    quit_ = false;
    decoder_ = std::thread(&Covers::DecodeLoop, this);
    downloader_ = std::thread(&Covers::DownloadLoop, this);
}

void Covers::Shutdown() {
    quit_ = true;
    cancel_ = true;
    cv_.notify_all();
    dl_cv_.notify_all();
    if (decoder_.joinable()) decoder_.join();
    if (downloader_.joinable()) downloader_.join();
    DropTextures();
}

void Covers::DropTextures() {
    for (auto& [k, e] : cache_) {
        Ui::FreeImage(e.img);
        Ui::FreeImage(e.blur);
    }
    cache_.clear();
    std::lock_guard lock(mutex_);
    decoded_.clear();
    decode_queue_.clear();
}

size_t Covers::Count() const {
    size_t n = 0;
    std::error_code ec;
    for (fs::recursive_directory_iterator it(dir_, ec), end; it != end && !ec; it.increment(ec))
        if (it->is_regular_file(ec) && it->path().extension() != ".nada") ++n;
    return n;
}

fs::path Covers::DownloadPath(const std::string& system, const std::string& key) const { return dir_ / system / (key + ".png"); }
fs::path Covers::CustomPath(const std::string& key) const { return dir_ / "propias" / (key + ".png"); }

fs::path Covers::FileFor(const Game& g) const {
    std::error_code ec;
    for (const char* ext : {".png", ".jpg"}) {
        fs::path p = dir_ / "propias" / (g.key + ext);
        if (fs::exists(p, ec)) return p;
    }
    const fs::path d = DownloadPath(g.system, g.key);
    if (fs::exists(d, ec)) return d;
    return {};
}

bool Covers::HasCustom(const Game& g) const {
    std::error_code ec;
    return fs::exists(dir_ / "propias" / (g.key + ".png"), ec) || fs::exists(dir_ / "propias" / (g.key + ".jpg"), ec);
}

bool Covers::HasCover(const Game& g) const { return !FileFor(g).empty(); }

const Ui::Image* Covers::Get(const Game& g) {
    auto it = cache_.find(g.key);
    if (it != cache_.end()) {
        it->second.used = SDL_GetTicks() / 1000.0;
        if (it->second.img) return &it->second.img;
        return nullptr;
    }
    Entry& e = cache_[g.key];
    e.used = SDL_GetTicks() / 1000.0;
    const fs::path f = FileFor(g);
    if (f.empty()) {
        e.missing = true;
        return nullptr;
    }
    e.loading = true;
    {
        std::lock_guard lock(mutex_);
        decode_queue_.emplace_back(g.key, f);
    }
    cv_.notify_one();
    return nullptr;
}

const Ui::Image* Covers::GetBlur(const Game& g) {
    Get(g);
    auto it = cache_.find(g.key);
    return it != cache_.end() && it->second.blur ? &it->second.blur : nullptr;
}

void Covers::Pump() {
    std::vector<Decoded> ready;
    std::vector<std::string> reload;
    {
        std::lock_guard lock(mutex_);
        reload.swap(pending_reload_);
        // Unas pocas por cuadro para que la interfaz no dé tirones.
        const size_t n = std::min<size_t>(decoded_.size(), 4);
        ready.assign(std::make_move_iterator(decoded_.begin()), std::make_move_iterator(decoded_.begin() + n));
        decoded_.erase(decoded_.begin(), decoded_.begin() + n);
    }
    for (const auto& k : reload) {
        auto it = cache_.find(k);
        if (it != cache_.end() && !it->second.loading) {
            Ui::FreeImage(it->second.img);
            Ui::FreeImage(it->second.blur);
            cache_.erase(it);
        }
    }
    for (auto& d : ready) {
        auto it = cache_.find(d.key);
        if (it == cache_.end()) continue;
        it->second.loading = false;
        if (d.ok) {
            it->second.img = Ui::ImageFromRGBA(d.rgba.data(), d.w, d.h);
            it->second.blur = Ui::ImageFromRGBA(d.small.data(), d.sw, d.sh);
        }
        else it->second.missing = true;
    }
    // Si hay demasiadas en la tarjeta gráfica, se sueltan las que hace más tiempo no se ven.
    int loaded = 0;
    for (auto& [k, e] : cache_) loaded += e.img ? 1 : 0;
    if (loaded > kMaxTextures) {
        std::vector<std::pair<double, std::string>> order;
        for (auto& [k, e] : cache_)
            if (e.img) order.emplace_back(e.used, k);
        std::sort(order.begin(), order.end());
        for (int i = 0; i < loaded - kMaxTextures; ++i) {
            Ui::FreeImage(cache_[order[i].second].img);
            Ui::FreeImage(cache_[order[i].second].blur);
            cache_.erase(order[i].second);
        }
    }
}

void Covers::DecodeLoop() {
    while (!quit_) {
        std::pair<std::string, fs::path> job;
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [&] { return quit_ || !decode_queue_.empty(); });
            if (quit_) return;
            job = decode_queue_.front();
            decode_queue_.pop_front();
        }
        Decoded d;
        d.key = job.first;
        std::vector<unsigned char> px;
        int w = 0, h = 0;
        if (Ui::DecodeImage(Paths::ToUtf8(job.second), px, &w, &h) && w > 0 && h > 0) {
            // Se reduce (promediando) si es muy grande: ahorra memoria de la tarjeta gráfica.
            int k = 1;
            while (std::max(w, h) / k > kMaxSide) ++k;
            if (k > 1) {
                const int nw = w / k, nh = h / k;
                std::vector<unsigned char> small(static_cast<size_t>(nw) * nh * 4);
                for (int y = 0; y < nh; ++y)
                    for (int x = 0; x < nw; ++x)
                        for (int c = 0; c < 4; ++c) {
                            unsigned sum = 0;
                            for (int yy = 0; yy < k; ++yy)
                                for (int xx = 0; xx < k; ++xx) sum += px[((static_cast<size_t>(y) * k + yy) * w + x * k + xx) * 4 + c];
                            small[(static_cast<size_t>(y) * nw + x) * 4 + c] = static_cast<unsigned char>(sum / (k * k));
                        }
                px.swap(small);
                w = nw;
                h = nh;
            }
            // Versión diminuta (24 px de ancho) para el fondo borroso.
            d.sw = 24;
            d.sh = std::max(1, h * 24 / w);
            d.small.resize(static_cast<size_t>(d.sw) * d.sh * 4);
            for (int y = 0; y < d.sh; ++y)
                for (int x = 0; x < d.sw; ++x) {
                    const int x0 = x * w / d.sw, x1 = std::max(x0 + 1, (x + 1) * w / d.sw);
                    const int y0 = y * h / d.sh, y1 = std::max(y0 + 1, (y + 1) * h / d.sh);
                    unsigned sum[4] = {}, n = 0;
                    for (int yy = y0; yy < y1; yy += 2)
                        for (int xx = x0; xx < x1; xx += 2, ++n)
                            for (int c = 0; c < 4; ++c) sum[c] += px[(static_cast<size_t>(yy) * w + xx) * 4 + c];
                    for (int c = 0; c < 4; ++c) d.small[(static_cast<size_t>(y) * d.sw + x) * 4 + c] = static_cast<unsigned char>(sum[c] / std::max(1u, n));
                }
            d.rgba = std::move(px);
            d.w = w;
            d.h = h;
            d.ok = true;
        }
        std::lock_guard lock(mutex_);
        decoded_.push_back(std::move(d));
    }
}

void Covers::SetCustom(const Game& g, const fs::path& image) {
    std::vector<unsigned char> px;
    int w = 0, h = 0;
    if (!Ui::DecodeImage(Paths::ToUtf8(image), px, &w, &h)) return;
    RemoveCustom(g);
    // Se guarda siempre como PNG, así no importa de dónde venga.
    Ui::SavePng(Paths::ToUtf8(CustomPath(g.key)), px.data(), w, h);
    auto it = cache_.find(g.key);
    if (it != cache_.end()) {
        Ui::FreeImage(it->second.img);
        Ui::FreeImage(it->second.blur);
        cache_.erase(it);
    }
    changed_ = true;
}

void Covers::RemoveCustom(const Game& g) {
    std::error_code ec;
    fs::remove(dir_ / "propias" / (g.key + ".png"), ec);
    fs::remove(dir_ / "propias" / (g.key + ".jpg"), ec);
    auto it = cache_.find(g.key);
    if (it != cache_.end()) {
        Ui::FreeImage(it->second.img);
        Ui::FreeImage(it->second.blur);
        cache_.erase(it);
    }
    changed_ = true;
}

void Covers::QueueDownloads(const std::vector<Game>& games) {
    std::error_code ec;
    int added = 0;
    {
        std::lock_guard lock(mutex_);
        for (const auto& g : games) {
            if (queued_.count(g.key) || HasCover(g) || RepoFor(g.system, g.ext).empty()) continue;
            // Si la última vez no se encontró, se espera un mes antes de volver a buscar.
            const fs::path none = dir_ / g.system / (g.key + ".nada");
            if (fs::exists(none, ec)) {
                const auto age = fs::last_write_time(none, ec);
                const auto now = fs::file_time_type::clock::now();
                if (!ec && now - age < std::chrono::hours(24 * kRetryDays)) continue;
            }
            Job j{g.key, g.system, g.title, Paths::ToUtf8(g.path.stem()), g.ext, g.tags};
            jobs_.push_back(j);
            queued_.insert(g.key);
            ++added;
        }
        remaining_ = static_cast<int>(jobs_.size());
    }
    if (added) {
        cancel_ = false;
        dl_cv_.notify_one();
    }
}

void Covers::CancelDownloads() {
    std::lock_guard lock(mutex_);
    jobs_.clear();
    queued_.clear();
    remaining_ = 0;
    cancel_ = true;
}

const std::vector<std::string>& Covers::Index(const std::string& repo) {
    auto it = index_.find(repo);
    if (it != index_.end()) return it->second;
    std::vector<std::string>& names = index_[repo];
    // El índice se guarda una semana para no pedirlo cada vez.
    const fs::path file = dir_ / "indices" / (Underscored(repo) + ".txt");
    std::error_code ec;
    bool fresh = false;
    if (fs::exists(file, ec)) {
        const auto age = fs::file_time_type::clock::now() - fs::last_write_time(file, ec);
        fresh = !ec && age < std::chrono::hours(24 * 7);
    }
    if (!fresh) {
        const fs::path raw = dir_ / "indices" / (Underscored(repo) + ".html");
        const std::string url = "https://thumbnails.libretro.com/" + Net::UrlEncode(repo) + "/Named_Boxarts/";
        bool ok = Net::Download(url, raw);
        if (!ok) {
            // Plan B: la lista del repositorio en GitHub.
            ok = Net::Download("https://api.github.com/repos/libretro-thumbnails/" + Underscored(repo) + "/git/trees/master?recursive=1", raw);
        }
        if (ok) {
            std::vector<std::string> parsed = ParseNames(ReadAll(raw));
            fs::remove(raw, ec);
            if (!parsed.empty()) {
                std::ofstream out(file, std::ios::binary);
                for (const auto& n : parsed) out << n << '\n';
            }
        }
    }
    std::ifstream in(file, std::ios::binary);
    std::string line;
    while (std::getline(in, line))
        if (!line.empty()) names.push_back(line);
    Log::Info("Carátulas: índice de %s con %zu nombres", repo.c_str(), names.size());
    return names;
}

bool Covers::FetchOne(const Job& j) {
    const std::string repo = RepoFor(j.system, j.ext);
    // 1) El nombre exacto del archivo (los juegos con nombre No-Intro coinciden tal cual).
    std::vector<std::string> candidates;
    candidates.push_back(ThumbName(j.stem));
    // 2) Por el índice: el mismo título sin etiquetas, con la región más parecida.
    const std::vector<std::string>& idx = Index(repo);
    if (!idx.empty()) {
        const std::string want = Norm(j.stem);
        const std::string want_tags = Tags(j.stem);
        std::vector<std::pair<int, std::string>> scored;
        for (const auto& n : idx) {
            if (n == candidates.front()) continue;
            if (Norm(n) == want) scored.emplace_back(RegionScore(Tags(n), want_tags), n);
        }
        std::sort(scored.begin(), scored.end(), [](auto& a, auto& b) { return a.first > b.first; });
        for (size_t i = 0; i < scored.size() && i < 3; ++i) candidates.push_back(scored[i].second);
        // Si el exacto no está en el índice, no vale la pena pedirlo.
        if (!std::binary_search(idx.begin(), idx.end(), candidates.front())) candidates.erase(candidates.begin());
    }
    const fs::path out = DownloadPath(j.system, j.key);
    for (const auto& name : candidates) {
        if (cancel_ || quit_) return false;
        const std::string file = Net::UrlEncode(name + ".png");
        if (Net::Download("https://thumbnails.libretro.com/" + Net::UrlEncode(repo) + "/Named_Boxarts/" + file, out) ||
            Net::Download("https://raw.githubusercontent.com/libretro-thumbnails/" + Underscored(repo) + "/master/Named_Boxarts/" + file, out)) {
            // Que de verdad sea una imagen (no una página de error).
            std::vector<unsigned char> px;
            int w, h;
            if (Ui::DecodeImage(Paths::ToUtf8(out), px, &w, &h)) return true;
            std::error_code ec;
            fs::remove(out, ec);
        }
    }
    return false;
}

void Covers::DownloadLoop() {
    while (!quit_) {
        Job j;
        {
            std::unique_lock lock(mutex_);
            dl_cv_.wait(lock, [&] { return quit_ || !jobs_.empty(); });
            if (quit_) return;
            j = jobs_.front();
            jobs_.pop_front();
            remaining_ = static_cast<int>(jobs_.size()) + 1;
        }
        downloading_ = true;
        if (FetchOne(j)) {
            ++found_;
            changed_ = true;
            // Que la biblioteca la cargue la próxima vez que pida esa carátula.
            std::lock_guard lock(mutex_);
            pending_reload_.push_back(j.key);
        } else if (!cancel_ && !quit_) {
            std::error_code ec;
            fs::create_directories(dir_ / j.system, ec);
            std::ofstream(dir_ / j.system / (j.key + ".nada")) << "sin carátula\n";
        }
        {
            std::lock_guard lock(mutex_);
            queued_.erase(j.key);
            remaining_ = static_cast<int>(jobs_.size());
            if (jobs_.empty()) downloading_ = false;
        }
    }
}

} // namespace Satronho
