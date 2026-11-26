// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ajustes y ventanas de la interfaz nativa de Android (build 25). Cada pantalla se describe con NPage
// (native_page.h) y Kotlin la dibuja con el estilo de Satronho NX. Los textos son los mismos de PC
// (así ya están traducidos). Gráficos y Núcleo están en app_graphics.cpp y Herramientas en
// app_tools.cpp, porque usan cosas de esos archivos.
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <thread>
#include <fstream>

#include "app.h"
#include "native_page.h"
#include "sys/android.h"
#include "sys/native_ui.h"
#include "util/i18n.h"
#include "util/log.h"
#include "libretro.h"

namespace Satronho {

using NativeUi::Quote;

namespace {
std::string Fmt(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return buf;
}
std::string HexColor(unsigned rgb) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "#%06X", rgb & 0xFFFFFF);
    return buf;
}
} // namespace

std::vector<std::pair<std::string, std::pair<std::string, std::string>>> NativeSystemList(bool only_gpu) {
    std::vector<std::pair<std::string, std::pair<std::string, std::string>>> out;
    for (const auto& s : Systems())
        if (!only_gpu || s.gpu) out.push_back({s.id, {s.name, HexColor(s.color)}});
    return out;
}

// ---------------------------------------------------------------- páginas

std::string App::NativeSettingsJson() {
    NPage p;
    const std::string& pg = native_page_;
    const std::string& arg = native_page_arg_;
    std::string title = "Ajustes", subtitle = "Todo se guarda al momento", icon = "ajustes";
    auto save = [this] { SaveConfig(); };

    if (pg == "inicio") {
        title = "Ajustes";
        subtitle = TrFmt("Satronho Emulation · build %d", build_);
        p.Section("Satronho", "Novedades, la configuración para tu celular y los guías, en un solo lugar.");
        p.Link("novedades", "Novedades", "Lo que trae cada build de Satronho Emulation.", "novedades", "", HexColor(Guide(GuideId::Juana).color));
        p.Link("sugerida", "Configuración sugerida", "Ñoño revisa tu celular y deja todo listo con un toque.", "sugerida",
               settings_.preset.empty() ? "" : settings_.preset, HexColor(Guide(GuideId::Nono).color));
        p.Link("chat", "Pregúntale a los guías", "Samy, Troya, Ñoño, Juana y MechaRidds responden sobre funciones y ajustes.", "chat", "",
               HexColor(Guide(GuideId::Samy).color));
        p.Link("medidor", "Medidor", "Cuadros por segundo, velocidad y tirones, arriba del juego.", "medidor",
               settings_.meter == 0 ? "Apagado" : settings_.meter == 1 ? "Compacto" : "Detallado");
        p.Section("Juego");
        p.Link("graficos", "Gráficos", "Tamaño, filtro, efectos, motor y resolución de cada consola.", "graficos");
        p.Link("sonido", "Sonido", "Volumen y latencia.", "sonido", TrFmt("%d %%", static_cast<int>(settings_.volume * 100 + 0.5f)));
        p.Link("controles", "Controles", "Asigna los botones de cada consola a tu mando.", "mando");
        p.Link("tactil", "Control en pantalla", "Cuándo se muestra, tamaño, transparencia, vibración y cruceta.", "tactil",
               settings_.touch_show == 0 ? "Automático" : settings_.touch_show == 1 ? "Siempre" : "Nunca");
        p.Link("rendimiento", "Rendimiento", "Rebobinar, avance rápido, guardar al salir y el registro.", "rendimiento");
        p.Link("nucleo", "Núcleo", "Las opciones propias del emulador de cada consola.", "nucleo");
        p.Section("Satronho Emulation");
        p.Link("biblioteca", "Biblioteca", "Carpetas de juegos, carátulas, juegos ocultos y «Satronho Games».", "carpeta",
               TrFmt("%d juegos", static_cast<int>(library_.Count())));
        p.Link("guias", "Guías", "Mostrar a los guías y sus consejos.", "patita", settings_.guides ? "Encendido" : "Apagado");
        p.Link("apariencia", "Apariencia", "Tema de colores, idioma y el logo de fondo.", "paleta");
        p.Link("herramientas", "Herramientas", "Prueba de mandos, Memory Cards de PS1 y carátulas.", "herramientas");
        p.Link("acerca", "Acerca de", "Versión, actualizaciones y créditos.", "info", TrFmt("Build %d", build_));
    } else if (pg == "sonido") {
        title = "Sonido";
        subtitle = "Volumen y latencia";
        icon = "sonido";
        p.Section("");
        p.Slider("vol", "Volumen", "", settings_.volume * 100, 0, 100, 1, "%.0f %%", [this](float v) {
            settings_.volume = std::clamp(v / 100.0f, 0.0f, 1.0f);
            ApplyAudioSettings();
            SaveConfig();
        }, "sonido");
        p.Slider("lat", "Latencia", "Menos es más rápido, pero si el sonido chisporrotea súbela un poco.", static_cast<float>(settings_.latency_ms),
                 24, 160, 8, "%.0f ms", [this](float v) {
                     settings_.latency_ms = static_cast<int>(v);
                     ApplyAudioSettings();
                     SaveConfig();
                 });
        p.Toggle("ffm", "Silenciar al avanzar rápido", "Evita el sonido acelerado durante el avance rápido.", settings_.mute_fast_forward,
                 [this](bool v) { settings_.mute_fast_forward = v; SaveConfig(); });
        if (!audio_.IsOpen()) p.Guide("nono", "No encontré una salida de sonido. Revisa que el celular no esté en silencio o conectado a otro aparato.");
    } else if (pg == "rendimiento") {
        title = "Rendimiento";
        subtitle = "Rebobinar, avance rápido y más";
        icon = "rendimiento";
        p.Section("Rebobinar");
        p.Toggle("rw", "Rebobinar", "Vuelve atrás en el tiempo (con el atajo del mando).", settings_.rewind, [this](bool v) { settings_.rewind = v; SaveConfig(); });
        if (settings_.rewind) {
            p.Toggle("rwh", "También en las consolas pesadas", "PS1, DS, Saturn y 64. Usa más memoria y procesador.", settings_.rewind_heavy,
                     [this](bool v) { settings_.rewind_heavy = v; SaveConfig(); });
            p.Slider("rws", "Cuánto se puede rebobinar", "Segundos hacia atrás como máximo.", static_cast<float>(settings_.rewind_seconds), 5, 60, 1,
                     "%.0f s", [this](float v) { settings_.rewind_seconds = static_cast<int>(v); SaveConfig(); });
            p.Slider("rwm", "Memoria para el historial", "", static_cast<float>(settings_.rewind_mb), 64, 1024, 32, "%.0f MB",
                     [this](float v) { settings_.rewind_mb = static_cast<int>(v); SaveConfig(); });
        }
        p.Section("Velocidad");
        p.Choice("ra", "Adelanto de cuadros", "Reduce el retraso entre presionar un botón y verlo en pantalla. Solo en Satronho Octa.",
                 settings_.runahead, {"Apagado", "1 cuadro", "2 cuadros"}, [this](int v) { settings_.runahead = std::clamp(v, 0, 2); SaveConfig(); });
        p.Toggle("ffon", "Avance rápido", "Apagado de fábrica: el gatillo choca con botones de varias consolas.", settings_.fast_forward_enabled,
                 [this](bool v) { settings_.fast_forward_enabled = v; SaveConfig(); });
        if (settings_.fast_forward_enabled) {
            static const int speeds[] = {2, 3, 4, 6};
            int fi = 1;
            for (int i = 0; i < 4; ++i)
                if (settings_.fast_forward == speeds[i]) fi = i;
            p.Choice("ff", "Velocidad del avance rápido", "", fi, {"2×", "3×", "4×", "6×"},
                     [this](int v) { settings_.fast_forward = speeds[std::clamp(v, 0, 3)]; SaveConfig(); });
        }
        p.Section("Al jugar");
        p.Toggle("autosave", "Guardar dónde quedé al salir", "Al volver a la biblioteca se guarda una ranura automática; «Continuar» sigue justo ahí.",
                 settings_.autosave_exit, [this](bool v) { settings_.autosave_exit = v; SaveConfig(); });
        p.Toggle("rumble", "Vibración del mando", "Los juegos que vibran hacen vibrar tu mando.", settings_.rumble,
                 [this](bool v) { settings_.rumble = v; SaveConfig(); });
#ifdef __ANDROID__
        {
            const char* names[] = {"básica", "media", "alta", "muy alta"};
            p.Section("Prueba del teléfono", "Ñoño mide la tarjeta gráfica y con eso elige la resolución automática de las consolas 3D.");
            p.Action("prueba", TrFmt("Potencia %s (%s ms)", Tr(names[PhonePower()]), ini_.Get("telefono.prueba_ms").c_str()),
                     "Vuelve a probar si cambiaste algo (o si un juego 3D va lento).", "chip", [this] { RunPhoneTest(true); }, "", "", "Probar otra vez");
        }
#endif
        p.Section("Si algo falla");
        p.Action("registro", "Enviar el registro", "MechaRidds junta lo que pasó (esta vez y la anterior) para mandarlo por WhatsApp o correo.",
                 "compartir", [this] { SendLog(); }, "primario");
    } else if (pg == "medidor") {
        title = "Medidor";
        subtitle = "Lo que pasa mientras juegas";
        icon = "medidor";
        p.Section("");
        p.Choice("mm", "Modo", "Compacto: una línea fina arriba. Detallado: también los tirones y el uso del procesador.", settings_.meter,
                 {"Apagado", "Compacto", "Detallado"}, [this](int v) { settings_.meter = std::clamp(v, 0, 2); SaveConfig(); }, "medidor");
        p.Choice("me", "Esquina", "", settings_.meter_corner, {"Arriba izq.", "Arriba der.", "Abajo izq.", "Abajo der."},
                 [this](int v) { settings_.meter_corner = std::clamp(v, 0, 3); SaveConfig(); });
        std::vector<std::string> colors;
        for (int i = 0; i < 5; ++i) colors.push_back(MeterColorName(i));
        p.Choice("mc", "Color", "", settings_.meter_color, colors, [this](int v) { settings_.meter_color = std::clamp(v, 0, 4); SaveConfig(); });
    } else if (pg == "tactil") {
        title = "Control en pantalla";
        subtitle = "Como en Satronho NX: encima del juego";
        icon = "tactil";
        p.Section("");
        p.Choice("ver", "Mostrar el control en pantalla", "Automático: se esconde cuando conectas un mando.", settings_.touch_show,
                 {"Automático", "Siempre", "Nunca"}, [this](int v) { settings_.touch_show = std::clamp(v, 0, 2); SaveConfig(); }, "tactil");
        p.Slider("tam", "Tamaño de los botones", "", settings_.touch_scale * 100, 60, 160, 5, "%.0f %%", [this](float v) {
            settings_.touch_scale = std::clamp(v / 100.0f, 0.6f, 1.6f);
            native_touch_cache_.clear();
            SaveConfig();
        });
        p.Slider("op", "Opacidad de los botones", "", settings_.touch_opacity * 100, 15, 100, 5, "%.0f %%", [this](float v) {
            settings_.touch_opacity = std::clamp(v / 100.0f, 0.15f, 1.0f);
            SaveConfig();
        });
        p.Toggle("vib", "Vibrar al tocar", "Una vibración cortita cada vez que aprietas un botón en pantalla.", settings_.touch_haptics,
                 [this](bool v) { settings_.touch_haptics = v; SaveConfig(); });
        p.Toggle("solo4", "Solo 4 direcciones", "La cruceta no marca diagonales: útil en juegos de laberintos, menús y peleas.", settings_.touch_dpad4,
                 [this](bool v) { settings_.touch_dpad4 = v; SaveConfig(); });
        p.Info("Mover los botones", "Dentro de un juego: menú del juego » «Mover los botones». Arrastra cada grupo a donde te quede cómodo.", "mando");
        p.Action("restablecer", "Volver a poner los botones en su lugar", "Para todas las consolas.", "refrescar", [this] {
            settings_.touch_offsets.clear();
            native_touch_cache_.clear();
            SaveConfig();
            Toast("Listo: los botones volvieron a su lugar.", GuideId::Samy, 3);
        }, "", "¿Volver a poner todos los botones en su lugar?");
    } else if (pg == "guias") {
        title = "Guías";
        subtitle = "Samy, Troya, Ñoño, Juana y MechaRidds";
        icon = "patita";
        p.Section("");
        p.Toggle("g", "Mostrar a los guías", "Samy, Troya, Ñoño, Juana y MechaRidds te explican las cosas por el camino.", settings_.guides,
                 [this](bool v) { settings_.guides = v; SaveConfig(); }, "patita");
        p.Toggle("b", "Consejos en la biblioteca", "La carita del guía abajo a la derecha: tócala para un consejo.", settings_.bubble,
                 [this](bool v) { settings_.bubble = v; SaveConfig(); });
        p.Link("chat", "Pregúntale a los guías", "Escribe tu duda y te responde el que más sabe.", "chat");
    } else if (pg == "apariencia") {
        title = "Apariencia";
        subtitle = "Colores, idioma y logo";
        icon = "paleta";
        p.Section("Tema");
        std::vector<std::string> themes;
        for (const auto& t : Ui::Themes()) themes.push_back(t.name);
        p.Choice("tema", "Tema de colores", Ui::Themes()[std::clamp(settings_.theme, 0, static_cast<int>(themes.size()) - 1)].blurb, settings_.theme,
                 themes, [this](int v) {
                     settings_.theme = std::clamp(v, 0, static_cast<int>(Ui::Themes().size()) - 1);
                     ApplyTheme();
                     SaveConfig();
                 }, "paleta");
        p.Slider("marca", "Logo de Satronho de fondo", "Su opacidad. Va de 10 % a 100 %; no se puede apagar.", settings_.watermark * 100, 10, 100, 5,
                 "%.0f %%", [this](float v) { settings_.watermark = std::clamp(v / 100.0f, 0.10f, 1.0f); SaveConfig(); });
        p.Section("Satronho");
        p.Toggle("franja", "Captura Satronho", "Las capturas llevan abajo una franja con el logo, el juego, su consola y la fecha.", settings_.screenshot_strip,
                 [this](bool v) { settings_.screenshot_strip = v; SaveConfig(); }, "imagen");
        p.Toggle("bateria", "Aviso de calor", "Ñoño te avisa si el celular pasa de 42 °C mientras juegas.", settings_.battery_warn,
                 [this](bool v) { settings_.battery_warn = v; SaveConfig(); }, "info");
        p.Section("Idioma");
        {
            const auto& langs = I18n::Languages();
            std::vector<std::string> names{"Como el celular"};
            int cur = 0;
            for (size_t i = 0; i < langs.size(); ++i) {
                names.push_back(langs[i].second);
                if (settings_.language == langs[i].first) cur = static_cast<int>(i) + 1;
            }
            p.Choice("idioma", "Idioma de Satronho", "Español, English, Français, Português o Italiano. Los nombres de los juegos no cambian.", cur,
                     names, [this](int v) {
                         const auto& ls = I18n::Languages();
                         settings_.language = v <= 0 || v > static_cast<int>(ls.size()) ? "" : ls[v - 1].first;
                         I18n::Load(Paths::Assets() / "idiomas", settings_.language);
                         LoadAssets();
                         native_lib_t_ = -10;
                         SaveConfig();
                     }, "idioma");
        }
        p.Section("Preguntas");
        p.Toggle("conf_app", "Preguntar antes de cerrar la app", "Así un toque de más en «atrás» no te saca de golpe.", settings_.confirm_exit_app,
                 [this](bool v) { settings_.confirm_exit_app = v; SaveConfig(); });
        p.Toggle("conf_juego", "Preguntar antes de salir de un juego", "Desde el menú del juego, antes de volver a la biblioteca.",
                 settings_.confirm_exit_game, [this](bool v) { settings_.confirm_exit_game = v; SaveConfig(); });
    } else if (pg == "biblioteca") {
        title = "Biblioteca";
        subtitle = TrFmt("%d juegos", static_cast<int>(library_.Count()));
        icon = "carpeta";
        p.Section("Carpetas de juegos", "Satronho busca juegos en estas carpetas y los acomoda en su consola.");
        for (size_t i = 0; i < settings_.folders.size(); ++i) {
            const std::string f = settings_.folders[i];
            p.Action("quitar" + std::to_string(i), f, "", "carpeta", [this, f] {
                settings_.folders.erase(std::remove(settings_.folders.begin(), settings_.folders.end(), f), settings_.folders.end());
                SaveConfig();
                Rescan();
            }, "", "¿Quitar esta carpeta? Los juegos no se borran del celular.", "Quitar");
        }
        p.Action("agregar", "Agregar carpeta", "Elige dónde guardas tus juegos.", "agregar", [this] { OpenBrowser(true); }, "primario");
        p.Link("escaneo", "Buscar juegos en todo el teléfono", "Reviso descargas, WhatsApp, la tarjeta SD… y tú eliges qué agregar.", "buscar");
        if (NxAvailable()) {
            p.Section("Satronho NX · para juegos de Switch", "Los juegos de Switch se abren con Satronho NX, que el hub instala y prepara por ti.");
            p.Action("nx_asistente", "Preparar Satronho NX", "Instalarla, darle tus claves y tu firmware, y ver si tu teléfono sirve.", "mando",
                     [] { NativeUi::Emit("{\"t\":\"nx\",\"clave\":\"\"}"); }, "primario");
        }
        if (library_.Scanning()) p.Action("cancelar", "Buscando juegos…", "Toca para cancelar la búsqueda.", "refrescar", [this] { library_.CancelScan(); });
        else p.Action("buscar", "Volver a buscar", "Revisa las carpetas otra vez.", "refrescar", [this] { Rescan(); });
        p.Section("Carátulas");
        p.Toggle("caratulas", "Carátulas reales", "Se descargan solas de libretro-thumbnails (las de RetroArch).", settings_.covers, [this](bool v) {
            settings_.covers = v;
            SaveConfig();
            if (v) covers_.QueueDownloads(library_.Snapshot());
        }, "imagen");
        if (settings_.covers) {
            if (covers_.Downloading())
                p.Info(TrFmt("Buscando carátulas… faltan %d", covers_.Remaining()), "", "imagen");
            else
                p.Action("buscar_caratulas", "Buscar carátulas ahora", "", "refrescar", [this] {
                    covers_.QueueDownloads(library_.Snapshot());
                    Toast("Buscando carátulas…", GuideId::Troya, 3);
                });
        }
        p.Section("Juegos ocultos");
        {
            int hidden = 0;
            for (const auto& g : library_.Snapshot())
                if (g.hidden) {
                    ++hidden;
                    const std::string key = g.key;
                    p.Action("mostrar_" + key, g.title, Tr(SystemById(g.system) ? SystemById(g.system)->full : ""), "ojo", [this, key] {
                        library_.SetHidden(key, false);
                        games_dirty_ = true;
                        native_lib_t_ = -10;
                    }, "", "", "Mostrar");
                }
            if (hidden == 0) p.Info("No hay juegos ocultos", "Mantén apretado un juego en la biblioteca para ocultarlo.", "ojo");
        }
#ifdef __ANDROID__
        p.Section("«Satronho Games»", "Tus juegos, partidas y estados quedan en la memoria del celular: no se borran si desinstalas la app.");
        if (Paths::PublicRoot().empty()) {
            p.Action("permiso", "Dar permiso de archivos", "Así Satronho puede leer tus juegos y guardar en «Satronho Games».", "candado",
                     [] { Android::RequestFileAccess(); }, "primario");
        } else {
            p.Action("raiz", Paths::ToUtf8(Paths::PublicRoot()), "Toca para moverla a otra carpeta o a la tarjeta SD.", "carpeta",
                     [] { NativeUi::Emit("{\"t\":\"explorador\",\"raiz\":true}"); }, "", "", "Cambiar");
            int gb = static_cast<int>(settings_.free_margin_gb + 0.5f);
            p.Slider("margen", "Espacio que siempre dejo libre", "Al copiar o descomprimir juegos, para que al celular no le falte espacio.",
                     static_cast<float>(gb), 1, 32, 1, "%.0f GB", [this](float v) { settings_.free_margin_gb = v; SaveConfig(); });
        }
#endif
        p.Section("Juegos comprimidos (.zip y .7z)", "Se juegan sin descomprimir. Los de disco se sacan una vez a una caché de hasta 4 GB que se limpia sola.");
        p.Action("cache", "Vaciar la caché", "", "borrar", [this] {
            std::error_code ec;
            fs::remove_all(Paths::User() / "cache" / "descomprimidos", ec);
            Toast("Caché de juegos descomprimidos vacía.", GuideId::Samy, 3);
        }, "", "¿Vaciar la caché de discos descomprimidos?");
    } else if (pg == "acerca") {
        title = "Acerca de";
        subtitle = TrFmt("Satronho Emulation · build %d", build_);
        icon = "info";
        p.Section("");
        p.Info("Satronho Emulation", "La casa de los emuladores de la familia Satronho. Por Ridds.", "logo");
        p.Info("Tu equipo", hw_.cpu + "  ·  " + TrFmt("%.1f GB", hw_.ram_mb / 1024.0) + "  ·  " + hw_.gpu, "chip");
        p.Section("Actualizaciones");
        const Updater::State st = updater_.GetState();
        std::string ust = st == Updater::State::Available ? TrFmt("¡Ya salió la build %d!", updater_.LatestBuild())
                        : st == Updater::State::Checking ? Tr("Buscando actualizaciones…")
                        : st == Updater::State::Downloading ? Tr("Descargando la build nueva…")
                        : st == Updater::State::Ready ? Tr("Instalando…")
                        : st == Updater::State::UpToDate ? Tr("Tienes la última build")
                        : st == Updater::State::Failed ? Tr("No se pudo revisar si hay builds nuevas") : "";
        if (st == Updater::State::Available)
            p.Action("actualizar", ust, "Se descarga, se instala encima y Satronho Emulation se vuelve a abrir solo. Tus juegos, partidas y ajustes no se tocan.",
                     "cargar", [this] { updater_.Install(); }, "primario", "", "Actualizar ahora");
        else
            p.Action("revisar", ust.empty() ? Tr("Actualizaciones") : ust, "", "refrescar", [this] { if (build_ > 0) updater_.Check(build_); }, "", "",
                     "Buscar ahora");
        p.Toggle("auto", "Buscar al abrir", "", settings_.auto_update, [this](bool v) { settings_.auto_update = v; SaveConfig(); });
        p.Action("github", "Ver en GitHub", "", "info", [this] { SDL_OpenURL(updater_.ReleaseUrl().c_str()); });
        p.Section("Créditos");
        p.Info("Emuladores", "Nestopia UE, bsnes, Genesis Plus GX, mGBA, Beetle PSX HW, Mupen64Plus-Next, melonDS DS, Beetle Saturn, Flycast y PPSSPP, "
                             "con sus licencias (GPL). Gracias a sus autores.", "chip");
        p.Info("Guías", "Samy, Troya, Ñoño y Juana son los perros de Ridds. MechaRidds es Ridds.", "patita");
    } else if (pg == "novedades") {
        title = "Novedades";
        subtitle = TrFmt("Estás en la build %d", build_);
        icon = "novedades";
        if (updater_.Supported() && updater_.GetState() == Updater::State::Available && updater_.LatestBuild() > build_) {
            p.Section("Hay una build nueva", "Se instala encima: tus juegos, partidas y ajustes no se tocan.");
            p.Action("actualizar", TrFmt("¡Ya salió la build %d!", updater_.LatestBuild()), "Toca y Juana la baja e instala por ti.", "cargar",
                     [this] { updater_.Install(); }, "primario", "", "Actualizar ahora");
        }
        const Json& builds = news_["builds"];
        int n = 0;
        for (const auto& b : builds.arr) {
            if (++n > 8) break;
            p.Section(b["version"].String() + "  ·  " + b["fecha"].String(), b["titulo"].String());
            for (const auto& c : b["cambios"].arr) {
                const std::string tipo = c["tipo"].String();
                p.Info(tipo == "arreglo" ? "Arreglo" : tipo == "nuevo" ? "Nuevo" : "Mejora", c["texto"].String(),
                       tipo == "arreglo" ? "herramientas" : tipo == "nuevo" ? "estrella" : "flecha",
                       tipo == "arreglo" ? HexColor(Guide(GuideId::Nono).color) : tipo == "nuevo" ? HexColor(Guide(GuideId::Juana).color)
                                                                                    : HexColor(Guide(GuideId::Troya).color));
            }
        }
    } else if (pg == "sugerida") {
        title = "Configuración sugerida";
        subtitle = "Ñoño revisa tu celular";
        icon = "sugerida";
        const Suggestion s = Suggest(hw_, settings_);
        p.Section("");
        p.Guide("nono", s.summary + TrFmt(" (Detecté %s, %.0f GB de RAM y %s.)", hw_.cpu.c_str(), hw_.ram_mb / 1024.0, hw_.gpu.c_str()));
        p.Action("aplicar", TrFmt("Aplicar la configuración «%s»", Tr(s.tier).c_str()), "", "estrella", [this] { ApplySuggestion(); }, "primario");
        p.Section("Qué cambia");
        for (const auto& c : s.changes) p.Info(c, "", "check");
        p.Section("Cómo va cada consola en tu celular");
        static const char* levels[] = {"Va perfecto", "Va bien", "Puede ir justo", "Puede ir lento"};
        static const unsigned colors[] = {0x74D14C, 0x4FD1C5, 0xFFB23E, 0xFF5D73};
        for (const auto& v : s.systems) {
            const SystemDef* sd = SystemById(v.system);
            const int l = std::clamp(v.level, 0, 3);
            p.Info((sd ? Tr(sd->full) : v.system) + "  ·  " + Tr(levels[l]), v.text, "mando", HexColor(colors[l]));
        }
    } else if (pg == "juego") {
        // Ajustes de un solo juego.
        const Game* g = FindGame(arg);
        if (!g) {
            native_page_.clear();
            return "null";
        }
        const Game game = *g;
        const SystemDef* sd = SystemById(game.system);
        title = game.title;
        subtitle = sd ? Tr(sd->full) : "";
        icon = "ajustes_juego";
        NativeGameRecord(p, game);
        if (!sd || !sd->external) p.Section("Solo para este juego", "Lo demás sigue como la consola.");
        if (sd && sd->id == "cd") {
            const bool normal = library_.GameOption(game.key, "mando") == "normal";
            p.Toggle("mando", "Mando normal (sin palancas)", "Para juegos viejos de PS1 que no aceptan el DualShock.", normal, [this, game](bool v) {
                library_.SetGameOption(game.key, "mando", v ? "normal" : "");
            }, "mando");
        }
        if (sd && sd->gpu) {
            const int api = std::atoi(library_.GameOption(game.key, "motor").c_str());
            std::vector<std::string> apis{"Como la consola", "OpenGL", "Vulkan"};
            if (sd->gpu_cpu) apis.push_back("Procesador");
            p.Choice("motor", "Motor del juego", "", std::clamp(api, 0, static_cast<int>(apis.size()) - 1), apis, [this, game](int v) {
                library_.SetGameOption(game.key, "motor", v == 0 ? "" : std::to_string(v));
            }, "chip");
            const int a = api > 0 ? api : GpuApiFor(*sd);
            const std::vector<int> steps = GpuResSteps(*sd, a);
            const int res = std::atoi(library_.GameOption(game.key, "res").c_str());
            std::vector<std::string> labels{"Como la consola"};
            int sel = 0;
            for (size_t i = 0; i < steps.size(); ++i) {
                labels.push_back(ResLabel(steps[i], sd));
                if (res > 0 && ResHeight(steps[i], sd) == ResHeight(res, sd)) sel = static_cast<int>(i) + 1;
            }
            p.Choice("res", "Resolución", "Si Ñoño la bajó porque iba lento, aquí la ves y la puedes cambiar.", sel, labels, [this, game, steps](int v) {
                library_.SetGameOption(game.key, "res", v <= 0 || v > static_cast<int>(steps.size()) ? "" : std::to_string(steps[v - 1]));
            }, "resolucion");
        }
        p.Section("");
        if (sd && !sd->external)
            p.Action("respaldar", "Respaldar sus partidas", "Un .zip con su partida y sus estados. Se restaura en el PC o en otro celular.", "guardar",
                     [this, game] { BackupAndShare({game}, game.title); });
        p.Action("fav", game.favorite ? "Quitar de favoritos" : "Agregar a favoritos", "", "estrella", [this, game] {
            library_.ToggleFavorite(game.key);
            games_dirty_ = true;
            native_lib_t_ = -10;
        });
        p.Action("tiempo", "Borrar el tiempo de juego", game.playtime_s > 0 ? FormatPlaytime(game.playtime_s) : "", "borrar", [this, game] {
            library_.ResetPlaytime(game.key);
            games_dirty_ = true;
            native_lib_t_ = -10;
        }, "peligro", "¿Borrar el tiempo de juego de este juego?");
    } else if (pg == "controles") {
        NativePageControls(p);
        title = "Controles";
        subtitle = "Asigna los botones de cada consola y míralos en vivo";
        icon = "mando";
    } else if (pg == "graficos") {
        NativePageGraphics(p);
        title = "Gráficos";
        subtitle = "Imagen, motor y resolución por consola";
        icon = "graficos";
    } else if (pg == "nucleo") {
        NativePageCore(p);
        title = "Núcleo";
        subtitle = "Las opciones propias de cada emulador";
        icon = "nucleo";
    } else if (pg == "herramientas") {
        NativePageTools(p);
        title = "Herramientas";
        subtitle = "Cosas útiles para tus juegos, tus mandos y tus partidas";
        icon = "herramientas";
    } else if (pg == "escaneo") {
        title = "Buscar en el teléfono";
        subtitle = "Juegos, claves y firmware";
        icon = "buscar";
        NativePagePhoneScan(p);
    } else if (pg == "chat") {
        title = "Guías";
        subtitle = "Pregúntales lo que quieras";
        icon = "chat";
        p.Raw(NativeChatJson());
    } else {
        native_page_.clear();
        return "null";
    }
    native_setters_ = std::move(p.setters);
    (void)save;
    return p.Json(pg, title, subtitle, icon);
}

// ---------------------------------------------------------------- Controles

void App::NativePageControls(NPage& p) {
    const SystemDef* sys = SystemById(ctrl_system_);
    if (!sys) sys = &Systems().front();
    p.Section("");
    p.Systems("consola", sys->id, NativeSystemList(false), [this](const std::string& v) {
        if (SystemById(v)) ctrl_system_ = v;
        input_.CancelCapture();
    });
    const int tab = ctrl_tab_ <= 0 ? 0 : ctrl_tab_ >= kCtrlHotkeys ? 2 : 1;
    p.Tabs("pestana", tab, {"General", "Jugadores", "Atajos"}, [this](int v) {
        ctrl_tab_ = v <= 0 ? 0 : v >= 2 ? kCtrlHotkeys : std::max(ctrl_last_player_, 1);
        input_.CancelCapture();
        input_.CancelClaim();
    });
    if (tab == 1) {
        std::vector<std::string> js;
        for (int pl = 0; pl < CtrlPlayersShown(); ++pl) js.push_back(std::to_string(pl + 1));
        p.Tabs("jugador", ctrl_tab_ - 1, js, [this](int v) {
            ctrl_tab_ = std::clamp(v + 1, 1, static_cast<int>(Input::kPlayers));
            ctrl_last_player_ = ctrl_tab_;
            input_.CancelCapture();
        });
    }
    if (tab == 0) {
        p.Section("Mandos");
        p.Slider("zona", "Zona muerta de las palancas", "Cuánto hay que mover la palanca para que cuente.", input_.deadzone * 100, 5, 60, 1, "%.0f %%",
                 [this](float v) { input_.deadzone = v / 100.0f; SaveConfig(); });
        p.Toggle("palanca_cruceta", "La palanca también mueve la cruceta", "En consolas sin palancas, la izquierda sirve de cruceta.", input_.stick_dpad,
                 [this](bool v) { input_.stick_dpad = v; SaveConfig(); });
        const auto pads = input_.Gamepads();
        if (pads.empty()) p.Info("No hay mandos conectados", "Conecta uno por Bluetooth o USB y aparece aquí al momento.", "mando");
        for (const auto& pd : pads) p.Info(TrFmt("Mando %d · %s", pd.slot + 1, pd.name.c_str()), pd.family_name, "mando");
        p.Section("Jugadores", "Hasta 8. NES, Super Nintendo, Genesis, PlayStation y Saturn conectan solas su adaptador de varios jugadores; 64 y Dreamcast traen 4 puertos.");
        const int claiming = input_.Claiming();
        if (claiming >= 0) {
            p.Guide("samy", TrFmt("Jugador %d: aprieta cualquier botón en tu mando.", claiming + 1));
            p.Action("puesto_cancelar", "Cancelar", "", "cerrar", [this] { input_.CancelClaim(); });
        } else {
            p.Action("puestos", "Toma tu puesto", "Cada uno aprieta un botón en su mando, en orden: Jugador 1, 2, 3… Su mando vibra al quedar.", "mando",
                     [this] { input_.StartClaimAll(); }, "primario");
        }
        for (int pl = 0; pl < CtrlPlayersShown(); ++pl) {
            const PlayerConfig& pc = input_.players[pl];
            std::string who = Tr("Sin mando");
            for (const auto& pd : pads)
                if (pd.slot == pc.pad) who = pd.name;
            if (pc.pad < 0 && pc.Disconnected()) who = Tr("Su mando está desconectado");
            p.Action("puesto" + std::to_string(pl), TrFmt("Jugador %d", pl + 1), who, "mando", [this, pl] { input_.StartClaim(pl); }, claiming == pl ? "primario" : "",
                     "", claiming == pl ? Tr("Esperando…") : Tr("Elegir con un botón"));
        }
        return;
    }
    if (tab == 2) {
        p.Section("Atajos", "Funciones de Satronho con un botón del mando. Toca uno y aprieta el botón que quieras.");
        const PadFamily fam = input_.Gamepads().empty() ? PadFamily::Generic : input_.FamilyOfSlot(input_.Gamepads().front().slot);
        for (int h = 0; h < static_cast<int>(Hotkey::Count); ++h) {
            const bool cap = input_.capture.hotkey && input_.capture.button == h;
            const int code = input_.hk_pad[h];
            const std::string val = cap ? Tr("Aprieta un botón…") : code == kPadNone ? Tr("Sin asignar") : Tr(Input::PadCodeLabel(code, fam));
            p.Action("atajo" + std::to_string(h), HotkeyInfo(static_cast<Hotkey>(h)).name, "", "mando",
                     [this, h] { input_.StartHotkeyCapture(static_cast<Hotkey>(h), true); }, cap ? "primario" : "", "", val);
        }
        p.Action("atajos_restablecer", "Restablecer los atajos", "", "refrescar", [this] { input_.ResetHotkeys(); SaveConfig(); }, "", "¿Restablecer los atajos?");
        return;
    }
    const int player = std::clamp(ctrl_tab_ - 1, 0, static_cast<int>(Input::kPlayers) - 1);
    // Mando del jugador.
    {
        const auto pads = input_.Gamepads();
        std::vector<std::string> names{"Sin mando"};
        int cur = 0;
        for (size_t i = 0; i < pads.size(); ++i) {
            names.push_back(TrFmt("Mando %d · %s", pads[i].slot + 1, pads[i].name.c_str()));
            if (input_.players[player].pad == pads[i].slot) cur = static_cast<int>(i) + 1;
        }
        p.Section("");
        p.Choice("mando_jugador", "Mando", input_.players[player].Disconnected() ? "Su mando está desconectado." : "", cur, names, [this, player](int v) {
            const auto ps = input_.Gamepads();
            input_.SetPlayerPad(player, v <= 0 || v > static_cast<int>(ps.size()) ? -1 : ps[v - 1].slot);
            SaveConfig();
        }, "mando");
    }
    // El control grande al centro y las asignaciones alrededor.
    const Mapping& m = input_.Edit(player, *sys);
    const int slot = input_.players[player].pad;
    const PadFamily fam = slot >= 0 ? input_.FamilyOfSlot(slot) : PadFamily::Generic;
    const uint16_t live = input_.LiveButtons(player, *sys);
    std::string bs = "[";
    bool first = true;
    auto add = [&](int b, const char* name, const char* zone) {
        const bool cap = !input_.capture.hotkey && input_.capture.button == b && input_.capture.player == player && input_.capture.sys == sys->id;
        const int code = m.pad[b];
        const std::string val = cap ? Tr("Aprieta…") : code == kPadNone ? "—" : Tr(Input::PadCodeLabel(code, fam));
        bs += (first ? "" : ",") + std::string("{\"id\":") + std::to_string(b) + ",\"nombre\":" + Quote(Tr(name)) + ",\"asignado\":" + Quote(val) +
              ",\"zona\":" + Quote(zone) + ",\"apretado\":" + ((live >> b) & 1 ? "true" : "false") + ",\"capturando\":" + (cap ? "true" : "false") + "}";
        first = false;
        p.setters["boton" + std::to_string(b)] = [this, player, sys, b](const std::string&) { input_.StartCapture(player, *sys, b, true, true); };
    };
    static const int dirs[] = {RETRO_DEVICE_ID_JOYPAD_UP, RETRO_DEVICE_ID_JOYPAD_DOWN, RETRO_DEVICE_ID_JOYPAD_LEFT, RETRO_DEVICE_ID_JOYPAD_RIGHT};
    static const char* dir_names[] = {"Arriba", "Abajo", "Izquierda", "Derecha"};
    for (int i = 0; i < 4; ++i) add(dirs[i], dir_names[i], "cruceta");
    for (int b : sys->shoulders) {
        const char* n = "";
        for (const auto& [id, name] : sys->buttons)
            if (id == b) n = name;
        add(b, n, n[0] == 'L' || n[0] == 'Z' ? "gatillo_izq" : "gatillo_der");
    }
    for (const auto& row : sys->face)
        for (int b : row) {
            const char* n = "";
            for (const auto& [id, name] : sys->buttons)
                if (id == b) n = name;
            add(b, n, "accion");
        }
    for (int b : sys->center) {
        const char* n = "";
        for (const auto& [id, name] : sys->buttons)
            if (id == b) n = name;
        add(b, n, "centro");
    }
    bs += "]";
    static const char* formas[] = {"nes", "gameboy", "gba", "genesis", "snes", "playstation", "n64", "ds", "saturn", "dreamcast", "psp"};
    const int capturing = !input_.capture.hotkey && input_.capture.player == player && input_.capture.sys == sys->id ? input_.capture.button : -1;
    p.Raw("{\"tipo\":\"control\",\"forma\":" + Quote(formas[static_cast<int>(sys->layout)]) + ",\"apretados\":" + std::to_string(live) +
          ",\"capturando\":" + std::to_string(capturing) + ",\"consola\":" + Quote(Tr(sys->full)) + ",\"color\":" + Quote(HexColor(sys->color)) + ",\"botones\":" + bs +
          ",\"analogico\":" + (sys->analog ? "true" : "false") + ",\"ultimo\":" + Quote(input_.last_input) + "}");
    p.Section("");
    p.Action("auto", "Asignar automáticamente", "Pone los botones de tu mando en su lugar de siempre.", "estrella",
             [this, player, sys] { input_.AutoMapPad(player, *sys); SaveConfig(); }, "primario");
    p.Action("uno", "Uno por uno", "Te pido cada botón y tú lo aprietas.", "mando", [this, player, sys] { input_.StartWizard(player, *sys, true); });
    p.Action("reset", "Restablecer", "", "refrescar", [this, player, sys] { input_.ResetProfile(player, *sys); SaveConfig(); }, "",
             "¿Volver a los botones de fábrica de esta consola?");
    p.Action("borrar", "Borrar todo", "", "borrar", [this, player, sys] { input_.ClearProfile(player, *sys); SaveConfig(); }, "peligro",
             "¿Borrar todas las asignaciones de esta consola?");
}

// ---------------------------------------------------------------- conversación con los guías

std::string App::NativeChatJson() {
    std::string s = "{\"tipo\":\"chat\",\"mensajes\":[";
    for (size_t i = 0; i < chat_.size(); ++i) {
        const auto& m = chat_[i];
        s += (i ? "," : "") + std::string("{\"guia\":") + Quote(m.guide < 0 ? "" : Guide(static_cast<GuideId>(m.guide)).key) + ",\"texto\":" + Quote(Tr(m.text)) +
             ",\"botones\":[";
        for (size_t j = 0; j < m.buttons.size(); ++j) s += (j ? "," : "") + Quote(Tr(m.buttons[j].label));
        s += "]}";
    }
    s += "],\"sugerencias\":[";
    const auto sugg = SuggestedQuestions(chat_guide_ >= 0 ? chat_guide_ : 0);
    for (size_t i = 0; i < sugg.size(); ++i) s += (i ? "," : "") + Quote(Tr(sugg[i]));
    s += "]}";
    return s;
}

// ---------------------------------------------------------------- ventanas sueltas

std::string App::NativeExtrasJson() {
    std::string s;
    // ¿Cerrar la app? / ¿Salir del juego?
    if (confirm_ == 1 || confirm_ == 2) {
        const bool app = confirm_ == 1;
        std::string t;
        if (app && in_game_) t = settings_.autosave_exit ? "Estás jugando. Antes de cerrar guardo dónde quedaste." : "Estás jugando; lo que no hayas guardado se pierde.";
        else if (app) t = "¡Hasta pronto! Tus juegos y ajustes quedan guardados.";
        else t = settings_.autosave_exit ? "Guardo dónde quedaste y con «Continuar» sigues justo ahí." : "Lo que no hayas guardado se pierde.";
        s += ",\"confirmar\":{\"tipo\":" + std::to_string(confirm_) + ",\"guia\":" + Quote(app ? "samy" : "troya") + ",\"titulo\":" +
             Quote(app ? Tr("¿Cerrar Satronho Emulation?") : TrFmt("¿Salir de «%s»?", current_.title.c_str())) + ",\"texto\":" + Quote(Tr(t)) +
             ",\"si\":" + Quote(Tr(app ? "Cerrar" : "Salir")) + ",\"no\":" + Quote(Tr(app ? "Quedarme" : "Seguir jugando")) +
             ",\"no_preguntar\":" + Quote(Tr("No volver a preguntar")) + "}";
    }
    // Bienvenida (primera vez).
    if (show_welcome_) {
        std::string guide, text;
        if (welcome_step_ == 0) {
            guide = "mecharidds";
            text = Tr("¡Hola! Soy MechaRidds. Esta es la casa de los emuladores de la familia Satronho. Ya están " + SystemsSentence() +
                      ", y pronto llegan más consolas. Te presento a mi equipo en dos pasos.");
        } else if (welcome_step_ == 1) {
            guide = "nono";
            const Suggestion sg = Suggest(hw_, settings_);
            text = Tr(sg.summary) + TrFmt(" (Detecté %s, %.0f GB de RAM y %s.)", hw_.cpu.c_str(), hw_.ram_mb / 1024.0, hw_.gpu.c_str());
        } else {
            guide = "samy";
            text = Tr("Dime en qué carpeta tienes tus juegos y los acomodo en la biblioteca, cada uno en su consola. En el juego, «atrás» abre el "
                      "menú. Al salir guardo dónde quedaste.");
        }
        s += ",\"bienvenida\":{\"paso\":" + std::to_string(welcome_step_) + ",\"guia\":" + Quote(guide) + ",\"texto\":" + Quote(text) +
             ",\"titulo\":" + Quote(Tr(welcome_step_ == 0 ? "¡Bienvenido a Satronho Emulation!" : welcome_step_ == 1 ? "Tu equipo" : "Tus juegos")) + "}";
    }
    if (show_whats_new_ && !show_welcome_) {
        s += ",\"novedad\":" + Quote(TrFmt("¡Ya llegó la build %d! Mira lo nuevo.", build_));
        // El título de esta build (la primera entrada de novedades.json), para la tarjeta de Juana.
        const Json& builds = news_["builds"];
        if (!builds.arr.empty()) s += ",\"novedad_titulo\":" + Quote(Tr(builds.arr.front()["titulo"].String()));
    }
    // Una build nueva del hub: tarjeta de Juana con «Actualizar» y su avance.
    {
        const Updater::State ust = updater_.GetState();
        const bool mostrar = ust == Updater::State::Downloading || ust == Updater::State::Ready ||
                             (ust == Updater::State::Available && !update_card_later_);
        if (mostrar && !show_welcome_ && updater_.LatestBuild() > build_) {
            s += ",\"actualizacion\":{\"build\":" + std::to_string(updater_.LatestBuild()) + ",\"actual\":" + std::to_string(build_) +
                 ",\"estado\":" + Quote(ust == Updater::State::Available ? "disponible" : ust == Updater::State::Downloading ? "bajando" : "instalando") +
                 ",\"avance\":" + NPage::Num(ust == Updater::State::Downloading ? updater_.Progress() : -1.0f) + "}";
        }
    }
    // Un .zip con varios juegos: ¿cuál?
    if (archive_pick_.Archived()) {
        s += ",\"zip\":{\"titulo\":" + Quote(TrFmt("«%s» trae varios juegos adentro. ¿Cuál abrimos?", archive_pick_.title.c_str())) + ",\"opciones\":[";
        for (size_t i = 0; i < archive_pick_.choices.size(); ++i) {
            const Game g = Library::MakeGameEntry(archive_pick_.path, archive_pick_.choices[i]);
            s += (i ? "," : "") + std::string("[") + Quote(archive_pick_.choices[i]) + "," + Quote(g.title) + "]";
        }
        s += "]}";
    }
    // Recibiendo juegos, mudanza de carpeta.
    s += NativeImportJson();
    s += NativeOrganizeJson();
    if (move_state_ == 1)
        s += ",\"progreso\":{\"id\":\"mudanza\",\"guia\":\"samy\",\"texto\":" +
             Quote(TrFmt("Estoy pasando tus juegos, partidas y estados a «%s», en la memoria del celular: así no se borran si desinstalas la app. %d de %d archivos.",
                         Paths::ToUtf8(move_to_.filename()).c_str(), move_done_.load(), move_total_.load())) +
             ",\"avance\":" + NPage::Num(move_total_ > 0 ? static_cast<float>(move_done_) / move_total_ : -1.0f) +
             ",\"nota\":" + Quote(Tr("No cierres la app mientras tanto.")) + ",\"botones\":[]}";
    s += ",\"capturando\":" + std::string(input_.Capturing() || input_.Claiming() >= 0 ? "true" : "false");
    return s;
}

void App::NativeMeterJson(std::string& s) {
    if (!in_game_ || settings_.meter == 0) return;
    const double target = core_.GameLoaded() ? core_.Fps() : 60.0;
    const double speed = target > 0 ? perf_.fps / target * 100.0 : 0;
    float core_avg = 0;
    for (int i = 0; i < 180; ++i) core_avg += perf_.core_ms[i];
    core_avg /= 180.0f;
    std::string text = TrFmt("%.0f FPS", perf_.fps) + "  " + Fmt("%.0f %%", speed) + "  " + Fmt("%.2f ms", core_avg);
    if (settings_.meter == 2) {
        text += "  ·  " + TrFmt("%d tirones/min", perf_.stutters_minute);
        if (proc_.cpu_percent > 0) text += "  ·  " + TrFmt("CPU %.0f %%", proc_.cpu_percent);
    }
    s += ",\"medidor\":{\"texto\":" + Quote(text) + ",\"esquina\":" + std::to_string(settings_.meter_corner) + ",\"color\":" +
         Quote(HexColor(MeterColor(settings_.meter_color))) + ",\"lento\":" + (speed < 95 && !paused_ ? "true" : "false") + "}";
    std::string chip;
    if (rewinding_) chip = rewind_.Empty() ? "Sin más historial" : "Rebobinando";
    else if (fast_forward_) chip = Fmt("Avance rápido ×%d", settings_.fast_forward);
    else if (disk_swap_target_ >= 0) chip = "Cambiando de disco";
    else if (paused_) chip = "En pausa";
    if (!chip.empty()) s += ",\"chip\":" + Quote(Tr(chip));
}

// Órdenes de las ventanas nativas (las demás están en NativeCommand).
bool App::NativeCommandExtra(const std::vector<std::string>& a) {
    auto arg = [&](size_t i) -> std::string { return i < a.size() ? a[i] : std::string(); };
    const std::string& cmd = a[0];
    if (cmd == "ajustes") {
        native_page_ = arg(1).empty() ? "inicio" : arg(1);
        native_page_arg_ = arg(2);
        native_page_t_ = -10;
        input_.CancelCapture();
        if (native_page_ == "chat" && chat_.empty()) AskGuides("hola");
    } else if (cmd == "ajustes_cerrar") {
        native_page_.clear();
        input_.CancelCapture();
    } else if (cmd == "precargar") {
        // Arranque rápido: el carrusel se detuvo en un juego. Se lee ya su núcleo y el principio del
        // juego para que estén en memoria cuando toques Jugar (la primera lectura del celular es lenta).
        if (const Game* g = FindGame(arg(1))) {
            const SystemDef* sd = SystemById(g->system);
            if (sd && !sd->external && g->key != prefetched_key_) {
                prefetched_key_ = g->key;
                std::vector<fs::path> files{Paths::Cores() / CoreFile(*sd), g->path};
                std::thread([files] {
                    std::vector<char> buf(1 << 20);
                    for (const auto& f : files) {
                        std::ifstream in(f, std::ios::binary);
                        size_t total = 0;
                        // Hasta 96 MB por archivo (los discos grandes no hace falta leerlos enteros).
                        while (in && total < (96u << 20)) {
                            in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
                            total += static_cast<size_t>(in.gcount());
                        }
                    }
                }).detach();
            }
        }
    } else if (cmd == "mando_android") {
        // Puente de mandos (como Satronho NX): ver Input::BridgeOffer.
        const std::string sub = arg(1);
        const int id = std::atoi(arg(2).c_str());
        if (sub == "ofrecer") input_.BridgeOffer(id, arg(3), std::atoi(arg(4).c_str()), std::atoi(arg(5).c_str()));
        else if (sub == "quitar") input_.BridgeRemove(id);
        else if (sub == "b") input_.BridgeButton(id, std::atoi(arg(3).c_str()), arg(4) == "1");
        else if (sub == "e") input_.BridgeAxis(id, std::atoi(arg(3).c_str()), std::atoi(arg(4).c_str()));
    } else if (cmd == "actualizar") {
        // Tarjeta de build nueva: se baja, se instala encima y la app se vuelve a abrir.
        if (updater_.GetState() == Updater::State::Available) updater_.Install();
    } else if (cmd == "actualizacion_despues") {
        update_card_later_ = true;
    } else if (cmd == "enviar_registro") {
        SendLog();
    } else if (cmd == "ajuste") {
        auto it = native_setters_.find(arg(1));
        if (it != native_setters_.end()) {
            const Page before = page_;
            const SettingsTab tab_before = tab_;
            it->second(arg(2));
            (void)before;
            (void)tab_before;
        }
        native_page_t_ = -10;
    } else if (cmd == "sugerencia") {
        const auto sugg = SuggestedQuestions(chat_guide_ >= 0 ? chat_guide_ : 0);
        const size_t i = static_cast<size_t>(std::atoi(arg(1).c_str()));
        if (i < sugg.size()) AskGuides(sugg[i]);
        native_page_t_ = -10;
    } else if (cmd == "preguntar") {
        AskGuides(arg(1));
        native_page_t_ = -10;
    } else if (cmd == "chat_boton") {
        const size_t i = static_cast<size_t>(std::atoi(arg(1).c_str())), j = static_cast<size_t>(std::atoi(arg(2).c_str()));
        if (i < chat_.size() && j < chat_[i].buttons.size()) {
            const ChatButton b = chat_[i].buttons[j];
            const Page before = page_;
            DoChatAction(b);
            // Lo que el guía abre en PC (una página de Ajustes) aquí es la página nativa.
            if (page_ != before || b.action == ChatAction::Hotkeys) {
                std::string dest;
                switch (b.action) {
                case ChatAction::Controls: dest = "controles"; break;
                case ChatAction::Hotkeys: dest = "controles"; ctrl_tab_ = kCtrlHotkeys; break;
                case ChatAction::Suggested: dest = "sugerida"; break;
                case ChatAction::News: case ChatAction::CheckUpdates: dest = "novedades"; break;
                case ChatAction::SettingsVideo: dest = "graficos"; break;
                case ChatAction::SettingsAudio: dest = "sonido"; break;
                case ChatAction::SettingsPerformance: dest = "rendimiento"; break;
                case ChatAction::SettingsCore: dest = "nucleo"; break;
                case ChatAction::SettingsLibrary: dest = "biblioteca"; break;
                case ChatAction::SettingsAppearance: dest = "apariencia"; break;
                case ChatAction::SettingsGuides: dest = "guias"; break;
                case ChatAction::Library: case ChatAction::System: dest = ""; break;
                default: dest = native_page_; break;
                }
                page_ = Page::Library;
                if (dest.empty()) {
                    native_page_.clear();
                    if (b.action == ChatAction::System) NativeUi::Emit("{\"t\":\"consola\",\"id\":" + Quote(b.arg) + "}");
                } else {
                    native_page_ = dest;
                    native_page_arg_.clear();
                }
                NativeUi::Emit("{\"t\":\"ir\",\"pagina\":" + Quote(native_page_) + "}");
            }
        }
        native_page_t_ = -10;
    } else if (cmd == "confirmar") {
        const int what = confirm_;
        if (arg(2) == "1") (what == 1 ? settings_.confirm_exit_app : settings_.confirm_exit_game) = false;
        confirm_ = 0;
        if (arg(1) == "si") {
            SaveConfig();
            if (what == 1) running_ = false;
            else if (in_game_) StopGame();
        } else {
            SaveConfig();
        }
    } else if (cmd == "bienvenida") {
        const std::string a1 = arg(1);
        if (a1 == "empezar") welcome_step_ = 1;
        else if (a1 == "aplicar") { ApplySuggestion(); welcome_step_ = 2; }
        else if (a1 == "despues") welcome_step_ = 2;
        else if (a1 == "carpeta" || a1 == "fin") {
            show_welcome_ = false;
            settings_.first_run_done = true;
            SaveConfig();
            if (a1 == "carpeta") OpenBrowser(true);
        }
    } else if (cmd == "novedades_visto") {
        show_whats_new_ = false;
    } else if (cmd == "zip") {
        if (archive_pick_.Archived()) {
            start_entry_ = arg(1);
            RequestStart(archive_pick_.path);
            archive_pick_ = Game{};
        }
    } else if (cmd == "zip_cancelar") {
        archive_pick_ = Game{};
    } else if (cmd == "progreso") {
        if (arg(1) == "cerrar" || arg(1) == "ver") {
            std::lock_guard lock(import_mutex_);
            import_ = ImportState{};
        }
        if (arg(1) == "permiso") Android::RequestFileAccess();
    } else if (cmd == "cancelar_descompresion") {
        unpack_cancel_ = true;
    } else if (cmd == "carpeta_agregar") {
        browser_open_ = false;
        std::lock_guard lock(pending_mutex_);
        pending_folders_.push_back(arg(1));
    } else if (cmd == "raiz") {
        browser_open_ = false;
        fs::path root = Paths::FromUtf8(arg(1));
        if (root.filename() != "Satronho Games") root /= "Satronho Games";
        if (root != Paths::PublicRoot()) {
            settings_.games_root = Paths::ToUtf8(root);
            SaveConfig();
            BeginDataMove(root);
        }
    } else if (cmd == "explorador_cerrar") {
        browser_open_ = false;
    } else if (cmd == "permiso") {
        Android::RequestFileAccess();
    } else if (cmd == "tactil_mover") {
        // Kotlin manda cuánto se movió un grupo, en proporción de la pantalla.
        if (sys_) {
            auto& o = settings_.touch_offsets[sys_->id + "." + arg(1)];
            o.first += static_cast<float>(std::atof(arg(2).c_str()));
            o.second += static_cast<float>(std::atof(arg(3).c_str()));
            native_touch_cache_.clear();
        }
    } else if (cmd == "tactil_restablecer") {
        if (sys_)
            for (auto it = settings_.touch_offsets.begin(); it != settings_.touch_offsets.end();)
                it = it->first.rfind(sys_->id + ".", 0) == 0 ? settings_.touch_offsets.erase(it) : std::next(it);
        native_touch_cache_.clear();
    } else if (cmd == "tactil_listo") {
        touch_edit_ = false;
        native_touch_cache_.clear();
        SaveConfig();
    } else {
        return false;
    }
    return true;
}

} // namespace Satronho
