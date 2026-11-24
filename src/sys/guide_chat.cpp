// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "sys/guide_chat.h"

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "sys/systems.h"
#include "util/i18n.h"

namespace Satronho {

namespace {
enum G { Mecha = 0, Samy = 1, Troya = 2, Nono = 3, Juana = 4 };
const char* kNames[] = {"MechaRidds", "Samy", "Troya", "Ñoño", "Juana"};

std::string Fmt(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, I18n::Tr(fmt), ap); // los guías hablan en el idioma de la app
    va_end(ap);
    return buf;
}

// Minúsculas, sin tildes ni signos: «¿Cómo va el N64?» → «como va el n64».
std::string Normalize(const std::string& in) {
    static const std::pair<const char*, char> accents[] = {
        {"á", 'a'}, {"é", 'e'}, {"í", 'i'}, {"ó", 'o'}, {"ú", 'u'}, {"ü", 'u'}, {"ñ", 'n'},
        {"Á", 'a'}, {"É", 'e'}, {"Í", 'i'}, {"Ó", 'o'}, {"Ú", 'u'}, {"Ü", 'u'}, {"Ñ", 'n'},
    };
    std::string out = " ";
    for (size_t i = 0; i < in.size();) {
        bool done = false;
        for (const auto& [from, to] : accents) {
            const size_t n = std::strlen(from);
            if (in.compare(i, n, from) == 0) {
                out += to;
                i += n;
                done = true;
                break;
            }
        }
        if (done) continue;
        const unsigned char c = in[i++];
        if (std::isalnum(c)) out += static_cast<char>(std::tolower(c));
        else if (c < 0x80) out += ' ';
    }
    out += ' ';
    // Espacios seguidos → uno.
    std::string clean;
    for (char c : out)
        if (!(c == ' ' && !clean.empty() && clean.back() == ' ')) clean += c;
    if (clean.empty() || clean.front() != ' ') clean = " " + clean;
    return clean;
}

// Preguntas en inglés, francés, portugués o italiano: las palabras clave se pasan al español
// antes de buscar el tema (los guías entienden lo esencial en los cinco idiomas).
std::string ToSpanishKeywords(std::string q) {
    static const std::pair<const char*, const char*> words[] = {
        // inglés
        {"games", "juegos"}, {"game", "juego"}, {"controller", "mando"}, {"controllers", "mandos"}, {"gamepad", "mando"},
        {"save", "guardar"}, {"saves", "partidas"}, {"hotkeys", "atajos"}, {"shortcuts", "atajos"}, {"filter", "filtro"},
        {"theme", "tema"}, {"screen", "pantalla"}, {"sound", "sonido"}, {"audio", "sonido"}, {"computer", "equipo"},
        {"pc", "pc"}, {"run", "va"}, {"runs", "va"}, {"news", "novedades"}, {"new", "nuevo"}, {"update", "actualizar"},
        {"version", "build"}, {"consoles", "consolas"}, {"console", "consola"}, {"who", "quienes"}, {"you", "ustedes"},
        {"played", "jugue"}, {"most", "mas"}, {"favorites", "favoritos"}, {"covers", "caratulas"}, {"add", "agregar"},
        {"folder", "carpeta"}, {"missing", "faltan"}, {"help", "ayuda"}, {"hello", "hola"}, {"hi", "hola"},
        {"thanks", "gracias"}, {"thank", "gracias"}, {"settings", "ajustes"}, {"graphics", "graficos"},
        {"resolution", "resolucion"}, {"rewind", "rebobinar"}, {"fast", "rapido"}, {"forward", "avance"},
        {"what", "que"}, {"how", "como"}, {"many", "cuantos"}, {"setup", "configuracion"}, {"suggested", "sugerida"},
        {"configure", "configurame"}, {"everything", "todo"}, {"can", "puedes"}, {"do", "hacer"}, {"connected", "conectados"},
        // francés
        {"jeux", "juegos"}, {"jeu", "juego"}, {"manette", "mando"}, {"manettes", "mandos"}, {"sauvegarder", "guardar"},
        {"raccourcis", "atajos"}, {"filtre", "filtro"}, {"theme", "tema"}, {"ecran", "pantalla"}, {"son", "sonido"},
        {"ordinateur", "equipo"}, {"nouveautes", "novedades"}, {"nouveau", "nuevo"}, {"qui", "quienes"}, {"vous", "ustedes"},
        {"dossier", "carpeta"}, {"ajouter", "agregar"}, {"merci", "gracias"}, {"bonjour", "hola"}, {"salut", "hola"},
        {"comment", "como"}, {"combien", "cuantos"}, {"quoi", "que"},
        // portugués
        {"jogos", "juegos"}, {"jogo", "juego"}, {"controle", "mando"}, {"controles", "mandos"}, {"salvar", "guardar"},
        {"atalhos", "atajos"}, {"tela", "pantalla"}, {"som", "sonido"}, {"computador", "equipo"}, {"novidades", "novedades"},
        {"pasta", "carpeta"}, {"adicionar", "agregar"}, {"obrigado", "gracias"}, {"obrigada", "gracias"}, {"ola", "hola"},
        {"quantos", "cuantos"}, {"voces", "ustedes"}, {"quem", "quienes"}, {"joguei", "jugue"},
        // italiano
        {"giochi", "juegos"}, {"gioco", "juego"}, {"controller", "mando"}, {"salvare", "guardare"}, {"scorciatoie", "atajos"},
        {"schermo", "pantalla"}, {"suono", "sonido"}, {"novita", "novedades"}, {"cartella", "carpeta"}, {"aggiungere", "agregar"},
        {"grazie", "gracias"}, {"ciao", "hola"}, {"quanti", "cuantos"}, {"chi", "quienes"}, {"siete", "son"}, {"voi", "ustedes"},
    };
    for (const auto& [from, to] : words) {
        const std::string needle = std::string(" ") + from + " ";
        size_t pos = 0;
        while ((pos = q.find(needle, pos)) != std::string::npos) {
            q.replace(pos, needle.size(), std::string(" ") + to + " ");
            pos += std::strlen(to) + 1;
        }
    }
    return q;
}

// ¿Aparece alguna de las palabras? Cada una se busca como palabra entera (o frase); con «*»
// al final vale como comienzo de palabra («actualiz*» → actualizar, actualización).
int Hits(const std::string& q, std::initializer_list<const char*> words) {
    int n = 0;
    for (const char* w : words) {
        std::string s(w);
        const bool prefix = !s.empty() && s.back() == '*';
        if (prefix) s.pop_back();
        const std::string needle = " " + s + (prefix ? "" : " ");
        if (q.find(needle) != std::string::npos) ++n;
    }
    return n;
}

// Consola de la que se habla, si se nombra alguna.
const SystemDef* MentionedSystem(const std::string& q) {
    struct Alias {
        const char* id;
        std::initializer_list<const char*> words;
    };
    static const Alias aliases[] = {
        {"bolsillo_plus", {"bolsillo plus", "gba", "game boy advance", "gameboy advance", "advance"}},
        {"bolsillo", {"bolsillo", "game boy", "gameboy", "gb", "gbc", "game boy color"}},
        {"hexa", {"hexa", "snes", "super nintendo", "super famicom", "famicom"}},
        {"octa", {"octa", "nes", "nintendinho", "family"}},
        {"n64", {"n64", "64", "nintendo 64", "satronho 64"}},
        {"dual", {"dual", "ds", "nds", "nintendo ds", "3ds"}},
        {"sueno", {"sueno", "dreamcast", "dc", "satronho sueno"}},
        {"portatil", {"portatil", "psp", "satronho portatil"}},
        {"mega", {"mega", "genesis", "mega drive", "megadrive", "sega genesis"}},
        {"anillo", {"anillo", "saturn", "sega saturn"}},
        {"cd", {"cd", "ps1", "psx", "playstation", "play station", "play 1", "play"}},
    };
    for (const auto& a : aliases)
        for (const char* w : a.words)
            if (q.find(" " + std::string(w) + " ") != std::string::npos) return SystemById(a.id);
    return nullptr;
}

std::string Join(const std::vector<std::string>& items, const char* last = " y ") {
    std::string s;
    for (size_t i = 0; i < items.size(); ++i) {
        if (i) s += i + 1 == items.size() ? last : ", ";
        s += items[i];
    }
    return s;
}

const char* LevelWords(int level) {
    switch (level) {
    case 0: return "va perfecto";
    case 1: return "va bien";
    case 2: return "puede ir justo";
    default: return "puede ir lento";
    }
}

// Si se le preguntó a un guía que no es el experto, lo dice con su estilo y responde igual.
std::string Handoff(int asked, int expert) {
    if (asked < 0 || asked == expert) return "";
    static const char* intro[] = {
        "Eso es más de %s, pero te lo cuento yo: ",
        "¡Guau! Eso lo sabe mejor %s, pero aquí va: ",
        "Eso es de %s, aunque yo también me lo sé: ",
        "Eso es más de %s. Te ayudo igual: ",
        "Ay, eso lo explica mejor %s, mijo, pero te digo: ",
    };
    return Fmt(intro[asked], kNames[expert]);
}

struct Topic {
    int score = 0;
    int id = 0;
};

enum Intent {
    kNone, kGreet, kThanks, kWho, kHelp, kHardware, kPerf, kSuggest, kLibrary, kAddGames, kPads, kHotkeys, kStates,
    kBios, kVideo, kTheme, kEngine, kNews, kCovers, kAudio, kExit, kSystem, kFastForward, kRewind, kZip,
};
} // namespace

int PickGuide(unsigned r) {
    // De 1000: MechaRidds 400; el resto (600) → Samy y Troya 195 cada uno, Ñoño 150, Juana 60.
    const unsigned v = r % 1000;
    if (v < 400) return Mecha;
    if (v < 595) return Samy;
    if (v < 790) return Troya;
    if (v < 940) return Nono;
    return Juana;
}

std::vector<std::string> SuggestedQuestions(int guide) {
    switch (guide) {
    case Samy: return {"¿Qué mandos tengo conectados?", "¿Cómo guardo la partida?", "¿Cuántos juegos tengo?", "¿Cómo agrego juegos?"};
    case Troya: return {"¿Qué filtro me recomiendas?", "¿Cómo cambio el tema?", "¿Qué es Vulkan?", "¿Cómo pongo la vista en carrusel?"};
    case Nono: return {"¿Cómo es mi equipo?", "¿Cómo va el 64 en mi PC?", "¿Qué BIOS me faltan?", "Configúrame todo"};
    case Juana: return {"¿Qué hay de nuevo?", "¿Hay una build nueva?", "¿En qué build estoy?", "¿Qué consolas vienen?"};
    default: return {"¿Quiénes son ustedes?", "¿Qué juego jugué más?", "¿Cómo va la PS1 en mi PC?", "¿Qué puedes hacer?"};
    }
}

ChatReply ChatGreeting(const ChatContext& ctx, int guide) {
    ChatReply r;
    r.guide = guide;
    size_t n = 0;
    if (ctx.games)
        for (const auto& g : *ctx.games) n += !g.hidden;
    switch (guide) {
    case Samy: r.text = I18n::Tr("¡Hola! Soy Samy. Pregúntame por tus juegos, tus mandos, los atajos o cómo guardar la partida."); break;
    case Troya: r.text = I18n::Tr("Hola, soy Troya. De imagen, filtros, temas y el motor gráfico sé todo. ¿Qué quieres saber?"); break;
    case Nono: r.text = I18n::Tr("¡Hola! Soy Ñoño. Conozco tu equipo de pies a cabeza: pregúntame cómo te va cada consola o qué te conviene."); break;
    case Juana: r.text = Fmt("Hola, mijo. Soy Juana. Estás en la %s. Pregúntame qué hay de nuevo.", ctx.build > 0 ? Fmt("build %d", ctx.build).c_str() : I18n::Tr("versión de desarrollo")); break;
    default:
        r.text = n ? Fmt("¡Hola! Soy MechaRidds. Tienes %zu juegos en la biblioteca. Escribe lo que quieras saber y te responde quien más sepa del tema.", n)
                   : I18n::Tr("¡Hola! Soy MechaRidds. Escribe lo que quieras saber y te responde quien más sepa del tema.");
    }
    return r;
}

ChatReply AnswerQuestion(const std::string& question, const ChatContext& ctx, int asked) {
    const std::string q = ToSpanishKeywords(Normalize(question));
    const SystemDef* sys = MentionedSystem(q);
    std::vector<Game> games;
    if (ctx.games)
        for (const auto& g : *ctx.games)
            if (!g.hidden) games.push_back(g);

    // Puntaje de cada tema según las palabras de la pregunta.
    auto score = [&](Intent i) -> int {
        switch (i) {
        case kGreet: return Hits(q, {"hola", "buenas", "buenos dias", "buenas tardes", "buenas noches", "hey", "saludos", "que mas", "quiubo"});
        case kThanks: return Hits(q, {"gracias", "genial", "excelente", "chevere", "bacano", "perfecto"});
        case kWho: return Hits(q, {"quien eres", "quienes son", "quienes", "guias", "tu nombre", "como te llamas", "perros", "perritos"}) * 2;
        case kHelp: return Hits(q, {"ayuda", "ayudame", "que puedes", "que sabes", "que haces", "como funciona", "para que sirve"});
        case kHardware: return Hits(q, {"equipo", "pc", "computador", "computadora", "procesador", "cpu", "ram", "memoria", "grafica", "gpu", "tarjeta", "specs", "maquina", "potencia", "laptop", "portatil"});
        case kPerf: return Hits(q, {"va bien", "como va", "como me va", "lento", "lenta", "rendimiento", "corre", "correr", "funciona", "lag", "tirones", "fps", "fluido", "trabado", "se traba", "aguanta", "puedo jugar"}) * (sys ? 2 : 1);
        case kSuggest: return Hits(q, {"configuracion sugerida", "configurame", "configura", "optimiza*", "recomienda*", "mejor configuracion", "ajusta todo", "configurar todo"}) * 2;
        case kLibrary: return Hits(q, {"cuantos juegos", "mis juegos", "biblioteca", "mas jugado", "mas juego", "jugue mas", "jugue", "ultimo juego", "favorito*", "tiempo jugado", "horas", "juegos tengo", "cuantos"});
        case kAddGames: return Hits(q, {"agregar", "agrego", "anadir", "anado", "carpeta", "roms", "rom", "no aparece*", "donde pongo", "importar", "meter juegos", "poner juegos"});
        case kPads: return Hits(q, {"mando", "mandos", "control", "controles", "joystick", "gamepad", "xbox", "dualshock", "dualsense", "ps4", "ps5", "switch pro", "conectado*", "teclado", "botones"});
        case kHotkeys: return Hits(q, {"atajo*", "teclas", "tecla", "f5", "f9", "esc", "menu", "pantalla completa", "captura*", "pausa*"});
        case kStates: return Hits(q, {"guardar", "guardo", "guarda", "cargar", "cargo", "estado*", "partida*", "ranura*", "continuar", "progreso"});
        case kBios: return Hits(q, {"bios"}) * 3;
        case kVideo: return Hits(q, {"filtro*", "imagen", "pixel*", "borros*", "crt", "efecto*", "scanlines", "lineas", "nitido", "escala", "lcd", "televisor", "tele", "se ve"});
        case kTheme: return Hits(q, {"tema*", "colores", "color", "fondo", "logo", "opacidad", "carrusel", "vista", "apariencia", "claro", "oscuro", "cuadricula"});
        case kEngine: return Hits(q, {"vulkan", "opengl", "motor grafico", "motor", "resolucion", "hd", "nvidia", "amd", "radeon", "intel", "directx", "4k", "1080"}) * 2;
        case kNews: return Hits(q, {"novedades", "nuevo", "nueva", "actualiz*", "build", "version", "vienen", "pronto", "proxima", "en camino"});
        case kCovers: return Hits(q, {"caratula*", "portada*", "cajas", "box art", "dibujos de los juegos"}) * 2;
        case kZip: return Hits(q, {"zip", "7z", "comprimido*", "descomprimir", "rar", "chd", "espacio", "ocupa*", "pesan", "convertir"}) * 2;
        case kAudio: return Hits(q, {"sonido", "audio", "volumen", "latencia", "crujido*", "chasquido*", "se corta el sonido", "musica"});
        case kExit: return Hits(q, {"salir", "cerrar", "cierro", "salgo", "confirmacion", "me pregunta"});
        case kFastForward: return Hits(q, {"avance rapido", "acelerar", "rapido", "turbo"});
        case kRewind: return Hits(q, {"rebobinar", "rebobina", "devolver", "atras"}) * 2;
        default: return 0;
        }
    };
    Topic best;
    for (int i = kGreet; i <= kZip; ++i) {
        if (i == kSystem) continue;
        const int s = score(static_cast<Intent>(i));
        if (s > best.score) best = {s, i};
    }
    if (best.score == 0 && sys) best = {1, kSystem};

    ChatReply r;
    auto say = [&](int expert, const std::string& text) {
        r.guide = asked >= 0 ? asked : expert;
        r.text = Handoff(asked, expert) + I18n::Tr(text);
    };
    auto button = [&](const std::string& label, ChatAction a, const std::string& arg = "") { r.buttons.push_back({label, a, arg}); };

    switch (best.id) {
    case kGreet: {
        static const char* lines[] = {"¡Hola, hola! ¿En qué te ayudo hoy?", "¡Guau, hola! ¿Qué quieres saber?", "Hola. Pregúntame lo que quieras de imagen y gráficos.",
                                      "¡Hola! ¿Reviso algo de tu equipo?", "Hola, mijo. ¿Qué te cuento?"};
        r.guide = asked >= 0 ? asked : Mecha;
        r.text = I18n::Tr(lines[r.guide]);
        break;
    }
    case kThanks: {
        static const char* lines[] = {"¡Con gusto! Aquí estoy para lo que necesites.", "¡Guau! Para eso estamos.", "De nada. Que lo disfrutes.",
                                      "¡Cuando quieras! Yo vigilo que todo vaya fluido.", "Ay, mijo, de nada."};
        r.guide = asked >= 0 ? asked : Mecha;
        r.text = I18n::Tr(lines[r.guide]);
        break;
    }
    case kWho:
        say(Mecha, I18n::Tr("Somos los guías de Satronho. Yo soy MechaRidds, el protagonista. Samy es la pitbull terrier café con blanco: sabe de "
                   "juegos, controles y atajos. Troya, la de las cejitas amarillas y gafitas, sabe de imagen y gráficos. Ñoño, el más grande, "
                   "revisa tu equipo. Y Juana, la viejita de moñito, te cuenta las novedades. Puedes preguntarle a cualquiera."));
        break;
    case kHelp:
        say(Mecha, I18n::Tr("Puedo decirte cómo va cada consola en tu equipo, cuántos juegos tienes y cuál jugaste más, qué mandos están conectados, "
                   "cuáles son tus atajos, qué BIOS faltan, qué filtro usar, cómo cambiar el tema o qué trae la build. Y te llevo a la "
                   "pantalla donde se cambia cada cosa."));
        button(I18n::Tr("Configuración sugerida"), ChatAction::Suggested);
        button("Controles", ChatAction::Controls);
        break;
    case kHardware: {
        if (!ctx.hw) break;
        const HardwareInfo& hw = *ctx.hw;
        std::string t = Fmt("Tienes un %s con %d hilos, %.0f GB de RAM y %s", hw.cpu.c_str(), hw.threads, hw.ram_mb / 1024.0, hw.gpu.c_str());
        if (hw.vram_mb > 0) t += Fmt(" (%d MB de video)", hw.vram_mb);
        t += Fmt(". Tu pantalla va a %.0f Hz. ", hw.refresh);
        if (!ctx.tier.empty()) t += I18n::Tr("Para mí es un equipo de nivel «") + ctx.tier + "». ";
        int good = 0;
        std::vector<std::string> slow;
        for (const auto& v : ctx.verdicts) {
            if (v.level <= 1) ++good;
            else if (const SystemDef* s = SystemById(v.system)) slow.push_back(s->name);
        }
        if (slow.empty()) t += I18n::Tr("Todas las consolas te van bien.");
        else t += (slow.size() > 1 ? Fmt("%d consolas te van bien; %s pueden ir justo.", good, Join(slow).c_str())
                                         : Fmt("%d consolas te van bien; %s puede ir justo.", good, Join(slow).c_str()));
        say(Nono, t);
        button(I18n::Tr("Ver la configuración sugerida"), ChatAction::Suggested);
        break;
    }
    case kPerf: {
        if (sys) {
            std::string t;
            for (const auto& v : ctx.verdicts)
                if (v.system == sys->id) t = sys->full + I18n::Tr(" en tu equipo: ") + v.text;
            if (t.empty()) t = sys->full + I18n::Tr(" debería ir bien.");
            if (sys->cpu_need >= 2) t += I18n::Tr(" Si notas tirones, apaga el rebobinado en esa consola y usa la configuración sugerida.");
            say(Nono, t);
            button(I18n::Tr("Configuración sugerida"), ChatAction::Suggested);
        } else {
            std::vector<std::string> lines;
            for (const auto& v : ctx.verdicts)
                if (const SystemDef* s = SystemById(v.system)) lines.push_back(s->name + " " + LevelWords(v.level));
            say(Nono, I18n::Tr("Así te va cada consola: ") + Join(lines) + I18n::Tr(". Pregúntame por una en particular si quieres el detalle."));
        }
        break;
    }
    case kSuggest:
        say(Nono, I18n::Tr("Te dejo todo listo para tu equipo: imagen, sonido, rebobinado y adelanto. Revisa lo que cambia y aplícalo con un clic."));
        button(I18n::Tr("Aplicar la configuración sugerida"), ChatAction::ApplySuggestion);
        button(I18n::Tr("Ver qué cambia"), ChatAction::Suggested);
        break;
    case kLibrary: {
        if (games.empty()) {
            say(Samy, I18n::Tr("Todavía no tienes juegos en la biblioteca. Agrega la carpeta donde los guardas y yo los acomodo."));
            button(I18n::Tr("Agregar carpeta"), ChatAction::AddFolder);
            break;
        }
        std::string t;
        if (Hits(q, {"mas jugado", "mas juego", "jugue mas", "tiempo jugado", "horas"})) {
            auto top = std::max_element(games.begin(), games.end(), [](const Game& a, const Game& b) { return a.playtime_s < b.playtime_s; });
            if (top->playtime_s <= 0) t = I18n::Tr("Todavía no has jugado ninguno. ¡Estrena uno!");
            else {
                long long total = 0;
                for (const auto& g : games) total += g.playtime_s;
                t = I18n::Tr("El que más jugaste es «") + top->title + I18n::Tr("», con ") + FormatPlaytime(top->playtime_s) + I18n::Tr(". En total llevas ") + FormatPlaytime(total) + I18n::Tr(" jugando.");
                button(I18n::Tr("Jugar ") + top->title, ChatAction::PlayGame, top->key);
            }
        } else if (Hits(q, {"ultimo", "ultima vez", "jugue"})) {
            auto last = std::max_element(games.begin(), games.end(), [](const Game& a, const Game& b) { return a.last_played < b.last_played; });
            if (last->last_played <= 0) t = I18n::Tr("Todavía no has jugado ninguno.");
            else {
                t = I18n::Tr("Lo último que jugaste fue «") + last->title + "», " + FormatLastPlayed(last->last_played) + ".";
                button(I18n::Tr("Seguir jugando"), ChatAction::PlayGame, last->key);
            }
        } else if (Hits(q, {"favorito*"})) {
            std::vector<std::string> favs;
            for (const auto& g : games)
                if (g.favorite) favs.push_back("«" + g.title + "»");
            if (favs.empty()) t = I18n::Tr("Aún no marcas favoritos. Toca la estrella de un juego en la biblioteca.");
            else {
                if (favs.size() > 6) favs.resize(6), favs.push_back(I18n::Tr("algunos más"));
                t = I18n::Tr("Tus favoritos: ") + Join(favs) + ".";
            }
        } else {
            std::vector<std::string> parts;
            for (const auto& s : Systems()) {
                if (sys && s.id != sys->id) continue;
                size_t n = 0;
                for (const auto& g : games) n += g.system == s.id;
                if (n) parts.push_back(Fmt("%zu de %s", n, s.name.c_str()));
            }
            if (sys && parts.empty()) t = I18n::Tr("No tienes juegos de ") + sys->full + I18n::Tr(" todavía.");
            else if (sys) t = I18n::Tr("Tienes ") + parts.front() + ".";
            else t = Fmt("Tienes %zu juegos: %s.", games.size(), Join(parts).c_str());
            if (sys) button(I18n::Tr("Ver ") + sys->full, ChatAction::System, sys->id);
        }
        say(Samy, t);
        break;
    }
    case kAddGames:
        say(Samy, Fmt("Toca «Agregar carpeta» y elige la carpeta donde guardas tus juegos; yo busco también en las subcarpetas y cada uno queda en su "
                      "consola. También puedes arrastrar un juego o una carpeta a la ventana. Si un juego no aparece, revisa que su extensión sea de "
                      "una de las %zu consolas (por ejemplo .nes, .sfc, .z64, .cue o .chd).",
                      Systems().size()));
        button(I18n::Tr("Agregar carpeta"), ChatAction::AddFolder);
        break;
    case kPads: {
        std::string t;
        if (ctx.pads.empty()) t = I18n::Tr("Ahora no veo ningún mando conectado; estás usando el teclado. Conecta uno (Xbox, PlayStation, Switch o genérico) y lo reconozco solo.");
        else t = (ctx.pads.size() > 1 ? Fmt("Tengo conectados: %s. ", Join(ctx.pads).c_str()) : Fmt("Tengo conectado: %s. ", Join(ctx.pads).c_str())) +
                 I18n::Tr("Cada consola tiene sus propios perfiles de botones, y el jugador 2 puede usar otro mando.");
        say(Samy, t);
        button(I18n::Tr("Abrir Controles"), ChatAction::Controls);
        break;
    }
    case kHotkeys: {
        std::vector<std::string> list;
        for (const auto& [name, key] : ctx.hotkeys)
            if (!key.empty()) list.push_back(name + ": " + key);
        if (list.size() > 8) list.resize(8);
        say(Samy, I18n::Tr("Tus atajos ahora mismo son: ") + Join(list, "; ") + I18n::Tr(". Con el mando, Select + Start abre el menú del juego. Jugando en ventana tienes arriba la barra de menús (Juego, Imagen, Sonido…); en pantalla completa, Esc la despliega y Esc la esconde."));
        button(I18n::Tr("Cambiar atajos"), ChatAction::Hotkeys);
        break;
    }
    case kStates: {
        std::string save = "F5", load = "F9";
        for (const auto& [name, key] : ctx.hotkeys) {
            if (name == I18n::Tr("Guardar estado") && !key.empty()) save = key;
            if (name == I18n::Tr("Cargar estado") && !key.empty()) load = key;
        }
        std::string t = I18n::Tr("Mientras juegas, ") + save + I18n::Tr(" guarda el estado y ") + load + I18n::Tr(" lo carga; tienes 8 ranuras. En el menú del juego (Esc) ves "
                        "las ranuras con su foto. ");
        t += ctx.settings && ctx.settings->autosave_exit ? I18n::Tr("Además, al salir guardo dónde quedaste y «Continuar» te lleva justo ahí.")
                                                         : I18n::Tr("Tienes apagado el guardado al salir; si lo enciendes, «Continuar» te lleva a donde quedaste.");
        say(Samy, t);
        break;
    }
    case kBios: {
        std::string t;
        if (!ctx.bios_missing.empty()) t = I18n::Tr("Necesitan BIOS y no la tienen: ") + Join(ctx.bios_missing) + I18n::Tr(". Sin ella esos juegos no arrancan. ");
        else t = I18n::Tr("Ninguna consola está bloqueada por falta de BIOS. ");
        if (!ctx.bios_optional.empty()) t += I18n::Tr("Funcionan sin BIOS, pero la original mejora la compatibilidad en: ") + Join(ctx.bios_optional) + ". ";
        t += I18n::Tr("La BIOS la pones tú (de tu consola); en Ajustes » Núcleo está «Agregar BIOS…» y yo la reconozco sola.");
        say(Nono, t);
        button(I18n::Tr("Ir a Ajustes » Núcleo"), ChatAction::SettingsCore, sys ? sys->id : "");
        break;
    }
    case kVideo: {
        std::string t = I18n::Tr("Para los juegos de píxeles te recomiendo «Nítido suave»: los píxeles quedan cuadrados y sin bordes temblorosos. «Píxel» es el "
                        "más crudo y «Suave» difumina. Si quieres el look del televisor, el efecto CRT con intensidad media queda precioso; en las "
                        "portátiles, la rejilla LCD. Y cada consola puede tener su propia imagen.");
        if (sys) t += I18n::Tr(" Para ") + sys->full + (sys->handheld ? I18n::Tr(" va lindo la rejilla LCD.") : I18n::Tr(" va lindo el CRT."));
        say(Troya, t);
        button(I18n::Tr("Ajustes de imagen"), ChatAction::SettingsVideo);
        break;
    }
    case kTheme:
        say(Troya, I18n::Tr("En Ajustes » Apariencia eliges entre cinco temas (Noche Satronho, Medianoche, Atardecer, Bosque y Claro) y la opacidad del "
                   "logo de Satronho del fondo, que va de 10 % a 100 %. En la biblioteca, el botón de vista cambia entre cuadrícula y carrusel, "
                   "y cada consola recuerda la suya."));
        button(I18n::Tr("Abrir Apariencia"), ChatAction::SettingsAppearance);
        break;
    case kEngine: {
        std::string t = I18n::Tr("El motor gráfico dibuja todo con tu tarjeta gráfica. ");
        if (!ctx.gfx.empty()) t += I18n::Tr("Ahora estoy usando ") + ctx.gfx + ". ";
        t += I18n::Tr("En CD (PS1) y 64 puedes elegir OpenGL o Vulkan y subir la resolución interna para que se vean en HD. Vulkan suele rendir más en "
             "tarjetas NVIDIA y AMD recientes; OpenGL es el más compatible, sobre todo con Intel.");
        if (!ctx.vulkan) t += I18n::Tr(" Tu equipo no tiene Vulkan, así que uso OpenGL.");
        say(Troya, t);
        button(I18n::Tr("Ajustes de imagen"), ChatAction::SettingsVideo);
        break;
    }
    case kNews: {
        if (Hits(q, {"vienen", "pronto", "proxima", "en camino", "consolas"})) {
            std::vector<std::string> soon;
            for (const auto& c : SoonSystems()) soon.push_back(std::string(c.name) + " (" + c.hint + ")");
            say(Juana, I18n::Tr("Vienen en camino, en este orden: ") + Join(soon) + ".");
            break;
        }
        std::string t = ctx.build > 0 ? Fmt("Estás en la build %d. ", ctx.build) : std::string(I18n::Tr("Esta es una versión de desarrollo. "));
        t += ctx.update_text;
        say(Juana, t);
        button(I18n::Tr("Ver novedades"), ChatAction::News);
        button(I18n::Tr("Buscar actualizaciones"), ChatAction::CheckUpdates);
        break;
    }
    case kCovers:
        say(Samy, Fmt("Las carátulas reales salen de libretro-thumbnails y se descargan solas cuando hay internet. Ya tengo %d guardadas. "
                      "Si alguna no aparece o no te gusta, clic derecho en el juego » «Elegir carátula…» y pones la tuya.", ctx.covers));
        button(I18n::Tr("Buscar carátulas ahora"), ChatAction::DownloadCovers);
        break;
    case kZip:
        say(Nono, I18n::Tr("Tus juegos en .zip o .7z se juegan tal cual, sin descomprimirlos: los de cartucho los abro en la memoria y los de disco "
                  "los saco una vez a una caché que se limpia sola. Y si quieres ahorrar espacio, en Ajustes » Biblioteca puedo convertir tus "
                  "discos .cue/.bin a .chd: es el mismo disco comprimido, ocupa como la mitad y abre igual. Es opcional; tus originales se "
                  "quedan salvo que me pidas borrarlos."));
        button(I18n::Tr("Ir a Ajustes » Biblioteca"), ChatAction::SettingsLibrary);
        break;
    case kAudio:
        say(Samy, Fmt("El volumen está en %d %% y la latencia en %d ms. Si el sonido cruje o se corta, sube la latencia un poco; si sientes "
                      "retraso, bájala.", ctx.settings ? static_cast<int>(ctx.settings->volume * 100) : 80, ctx.settings ? ctx.settings->latency_ms : 64));
        button(I18n::Tr("Ajustes de sonido"), ChatAction::SettingsAudio);
        break;
    case kExit:
        say(Samy, std::string(I18n::Tr("Antes de cerrar la app o salir de un juego te pregunto si estás seguro, para que no pierdas nada por un clic. ")) +
                      I18n::Tr("Si marcas «No volver a preguntar», lo vuelves a encender en Ajustes » Apariencia."));
        button(I18n::Tr("Abrir Apariencia"), ChatAction::SettingsAppearance);
        break;
    case kFastForward:
        say(Samy, ctx.settings && ctx.settings->fast_forward_enabled
                      ? Fmt("El avance rápido está encendido a %d×: mantén su tecla para acelerar.", ctx.settings->fast_forward)
                      : std::string(I18n::Tr("El avance rápido viene apagado para que no choque con los botones del juego. Lo enciendes en Ajustes » Rendimiento.")));
        button(I18n::Tr("Ajustes de rendimiento"), ChatAction::SettingsPerformance);
        break;
    case kRewind: {
        std::string key = "Retroceso";
        for (const auto& [name, k] : ctx.hotkeys)
            if (name.rfind("Rebobinar", 0) == 0 && !k.empty()) key = k;
        say(Samy, I18n::Tr("Mantén ") + key + I18n::Tr(" y el juego va hacia atrás, como en una cinta. ") +
                      std::string(ctx.settings && ctx.settings->rewind ? I18n::Tr("Está encendido") : I18n::Tr("Ahora está apagado")) +
                      I18n::Tr("; en las consolas pesadas (PS1, DS, Saturn y 64) se enciende aparte porque gasta más memoria."));
        button(I18n::Tr("Ajustes de rendimiento"), ChatAction::SettingsPerformance);
        break;
    }
    case kSystem: {
        size_t n = 0;
        for (const auto& g : games) n += g.system == sys->id;
        std::string t = sys->full + I18n::Tr(" es el emulador de Satronho para ") + sys->games + ". ";
        t += n ? (n == 1 ? std::string(I18n::Tr("Tienes 1 juego. ")) : Fmt("Tienes %zu juegos. ", n)) : std::string(I18n::Tr("Todavía no tienes juegos de esa consola. "));
        for (const auto& v : ctx.verdicts)
            if (v.system == sys->id) t += std::string(I18n::Tr("En tu equipo ")) + LevelWords(v.level) + ". ";
        if (sys->bios_required) t += I18n::Tr("Necesita la BIOS original de la consola.");
        say(Mecha, t);
        button(I18n::Tr("Ver ") + sys->full, ChatAction::System, sys->id);
        break;
    }
    default: {
        say(Mecha, I18n::Tr("Mmm, eso no lo entendí bien. Puedo contarte de tu equipo, tus juegos, tus mandos y atajos, las BIOS, la imagen y los temas, "
                   "o qué trae la build. Prueba con una de estas preguntas:"));
        r.guide = asked >= 0 ? asked : Mecha;
        break;
    }
    }
    if (r.text.empty()) r = ChatGreeting(ctx, asked >= 0 ? asked : Mecha);
    return r;
}

} // namespace Satronho
