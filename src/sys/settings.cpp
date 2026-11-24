// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "sys/settings.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "sys/systems.h"
#include "util/ini.h"

namespace Satronho {

void Settings::Load(const Ini& ini) {
    fullscreen = ini.GetBool("video.pantalla_completa", fullscreen);
    scale_mode = std::clamp(ini.GetInt("video.escala", scale_mode), 0, 2);
    filter = std::clamp(ini.GetInt("video.filtro", filter), 0, 2);
    effect = std::clamp(ini.GetInt("video.efecto", effect), 0, 4);
    effect_strength = std::clamp(ini.GetFloat("video.intensidad", effect_strength), 0.0f, 1.0f);
    ambient = ini.GetBool("video.fondo_ambiente", ambient);
    vsync = ini.GetBool("video.vsync", vsync);
    fullscreen_60hz = ini.GetBool("video.completa_60hz", fullscreen_60hz);
    renderer = ini.Get("video.motor", renderer);

    volume = std::clamp(ini.GetFloat("audio.volumen", volume), 0.0f, 1.0f);
    latency_ms = std::clamp(ini.GetInt("audio.latencia", latency_ms), 24, 200);
    mute_fast_forward = ini.GetBool("audio.silencio_avance", mute_fast_forward);

    runahead = std::clamp(ini.GetInt("rendimiento.adelanto", runahead), 0, 3);
    rewind = ini.GetBool("rendimiento.rebobinar", rewind);
    rewind_heavy = ini.GetBool("rendimiento.rebobinar_pesadas", rewind_heavy);
    rewind_seconds = std::clamp(ini.GetInt("rendimiento.rebobinar_segundos", rewind_seconds), 5, 60);
    rewind_mb = std::clamp(ini.GetInt("rendimiento.rebobinar_mb", rewind_mb), 64, 1024);
    fast_forward_enabled = ini.GetBool("rendimiento.avance_activo", fast_forward_enabled);
    fast_forward = std::clamp(ini.GetInt("rendimiento.avance", fast_forward), 2, 8);
    pause_on_focus_loss = ini.GetBool("rendimiento.pausa_sin_foco", pause_on_focus_loss);
    autosave_exit = ini.GetBool("juego.guardar_al_salir", autosave_exit);
    rumble = ini.GetBool("juego.vibracion", rumble);

    video_for.clear();
    for (const auto& sys : Systems()) {
        const std::string b = "video." + sys.id + ".";
        if (!ini.Values().count(b + "propio")) continue;
        VideoOverride v;
        v.on = ini.GetBool(b + "propio", false);
        v.scale_mode = std::clamp(ini.GetInt(b + "escala", scale_mode), 0, 2);
        v.filter = std::clamp(ini.GetInt(b + "filtro", filter), 0, 2);
        v.effect = std::clamp(ini.GetInt(b + "efecto", effect), 0, 4);
        v.effect_strength = std::clamp(ini.GetFloat(b + "intensidad", effect_strength), 0.0f, 1.0f);
        video_for[sys.id] = v;
    }

    meter = std::clamp(ini.GetInt("medidor.modo", meter), 0, 2);
    meter_corner = std::clamp(ini.GetInt("medidor.esquina", meter_corner), 0, 3);
    meter_color = std::clamp(ini.GetInt("medidor.color", meter_color), 0, 4);

    guides = ini.GetBool("guias.activas", guides);
    bubble = ini.GetBool("guias.burbuja", bubble);
    ui_scale = ini.GetFloat("interfaz.escala2", ui_scale);
    if (ui_scale != 0.0f) ui_scale = std::clamp(ui_scale, 0.75f, 1.75f);
    theme = std::clamp(ini.GetInt("interfaz.tema", theme), 0, 4);
    watermark = std::clamp(ini.GetFloat("interfaz.marca_agua", watermark), 0.10f, 1.0f);
    confirm_exit_app = ini.GetBool("avisos.salir_app", confirm_exit_app);
    confirm_exit_game = ini.GetBool("avisos.salir_juego", confirm_exit_game);

    folders.clear();
    const int n = ini.GetInt("biblioteca.carpetas", 0);
    for (int i = 0; i < n; ++i) {
        std::string f = ini.Get("biblioteca.carpeta." + std::to_string(i));
        if (!f.empty()) folders.push_back(f);
    }
    sort = std::clamp(ini.GetInt("biblioteca.orden", sort), 0, 2);
    favorites_only = ini.GetBool("biblioteca.solo_favoritos", favorites_only);
    covers = ini.GetBool("biblioteca.caratulas", covers);
    lib_view.clear();
    for (const auto& [k, v] : ini.Values())
        if (k.rfind("biblioteca.vista.", 0) == 0) lib_view[k.substr(17)] = std::clamp(std::atoi(v.c_str()), 0, 1);

    auto_update = ini.GetBool("actualizaciones.buscar", auto_update);
    first_run_done = ini.GetBool("estado.bienvenida", first_run_done);
    preset = ini.Get("estado.sugerida", preset);
    last_seen_build = ini.GetInt("estado.ultima_build", last_seen_build);
    // Hasta la build 6 la configuración «Alto» dejaba 48 ms, muy justo en pantallas de 144 Hz.
    if (last_seen_build > 0 && last_seen_build < 7 && latency_ms == 48) latency_ms = 64;
}

void Settings::Save(Ini& ini) const {
    ini.SetBool("video.pantalla_completa", fullscreen);
    ini.SetInt("video.escala", scale_mode);
    ini.SetInt("video.filtro", filter);
    ini.SetInt("video.efecto", effect);
    ini.SetFloat("video.intensidad", effect_strength);
    ini.SetBool("video.fondo_ambiente", ambient);
    ini.SetBool("video.vsync", vsync);
    ini.SetBool("video.completa_60hz", fullscreen_60hz);
    ini.Set("video.motor", renderer);

    ini.SetFloat("audio.volumen", volume);
    ini.SetInt("audio.latencia", latency_ms);
    ini.SetBool("audio.silencio_avance", mute_fast_forward);

    ini.SetInt("rendimiento.adelanto", runahead);
    ini.SetBool("rendimiento.rebobinar", rewind);
    ini.SetBool("rendimiento.rebobinar_pesadas", rewind_heavy);
    ini.SetInt("rendimiento.rebobinar_segundos", rewind_seconds);
    ini.SetInt("rendimiento.rebobinar_mb", rewind_mb);
    ini.SetBool("rendimiento.avance_activo", fast_forward_enabled);
    ini.SetInt("rendimiento.avance", fast_forward);
    ini.SetBool("rendimiento.pausa_sin_foco", pause_on_focus_loss);
    ini.SetBool("juego.guardar_al_salir", autosave_exit);
    ini.SetBool("juego.vibracion", rumble);
    for (const auto& [id, v] : video_for) {
        const std::string b = "video." + id + ".";
        ini.SetBool(b + "propio", v.on);
        ini.SetInt(b + "escala", v.scale_mode);
        ini.SetInt(b + "filtro", v.filter);
        ini.SetInt(b + "efecto", v.effect);
        ini.SetFloat(b + "intensidad", v.effect_strength);
    }

    ini.SetInt("medidor.modo", meter);
    ini.SetInt("medidor.esquina", meter_corner);
    ini.SetInt("medidor.color", meter_color);

    ini.SetBool("guias.activas", guides);
    ini.SetBool("guias.burbuja", bubble);
    ini.SetFloat("interfaz.escala2", ui_scale);
    ini.SetInt("interfaz.tema", theme);
    ini.SetFloat("interfaz.marca_agua", watermark);
    ini.SetBool("avisos.salir_app", confirm_exit_app);
    ini.SetBool("avisos.salir_juego", confirm_exit_game);
    ini.SetBool("biblioteca.caratulas", covers);
    for (const auto& [k, v] : lib_view) ini.SetInt("biblioteca.vista." + k, v);
    ini.Erase("interfaz.escala");

    // Se borran las carpetas viejas antes de escribir la lista nueva.
    for (int i = 0; i < 64; ++i) ini.Erase("biblioteca.carpeta." + std::to_string(i));
    ini.SetInt("biblioteca.carpetas", static_cast<int>(folders.size()));
    for (size_t i = 0; i < folders.size(); ++i) ini.Set("biblioteca.carpeta." + std::to_string(i), folders[i]);
    ini.SetInt("biblioteca.orden", sort);
    ini.SetBool("biblioteca.solo_favoritos", favorites_only);

    ini.SetBool("actualizaciones.buscar", auto_update);
    ini.SetBool("estado.bienvenida", first_run_done);
    ini.Set("estado.sugerida", preset);
    ini.SetInt("estado.ultima_build", last_seen_build);
}

Settings Settings::ForSystem(const std::string& sys) const {
    Settings s = *this;
    auto it = video_for.find(sys);
    if (it != video_for.end() && it->second.on) {
        s.scale_mode = it->second.scale_mode;
        s.filter = it->second.filter;
        s.effect = it->second.effect;
        s.effect_strength = it->second.effect_strength;
    }
    return s;
}

namespace {
// Lo que Ñoño dice de cada consola según los hilos del procesador (no se mide la velocidad de
// cada núcleo, así que es una guía: un procesador viejo de 8 hilos puede ir más lento).
std::vector<SystemVerdict> Verdicts(const HardwareInfo& hw) {
    std::vector<SystemVerdict> out;
    const int t = hw.threads;
    for (const auto& sys : Systems()) {
        SystemVerdict v{sys.id, 0, "Va perfecto."};
        switch (sys.cpu_need) {
        case 1:
            if (t >= 4) v = {sys.id, 1, "Va bien."};
            else v = {sys.id, 2, sys.id == "cd" ? "Puede ir justo: si va lento, pon la resolución interna en 1x." : "Puede ir justo en algunos juegos."};
            break;
        case 2:
            if (t >= 8) v = {sys.id, 1, "Va bien."};
            else if (t >= 4) v = {sys.id, 2, "Puede ir justo en los juegos más exigentes."};
            else v = {sys.id, 3, "Puede ir lento: la Saturn pide mucho procesador."};
            break;
        case 3:
            if (t >= 12) v = {sys.id, 1, "Va bien en la mayoría de juegos."};
            else if (t >= 6) v = {sys.id, 2, "Algunos juegos pueden ir lentos: se dibuja todo con el procesador."};
            else v = {sys.id, 3, "Va a ir lento: por ahora la 64 se dibuja solo con el procesador."};
            break;
        default: break;
        }
        out.push_back(v);
    }
    return out;
}
} // namespace

Suggestion Suggest(const HardwareInfo& hw, const Settings& current) {
    Suggestion s;
    s.result = current;
    Settings& r = s.result;
    const double ram_gb = hw.ram_mb / 1024.0;

    char buf[256];
    std::snprintf(buf, sizeof buf, "Procesador con %d hilos: %s.", hw.threads,
                  hw.threads >= 8 ? "de sobra para las clásicas y bien para las más pesadas"
                  : hw.threads >= 4 ? "de sobra para las clásicas; las más pesadas pueden ir justas"
                                    : "justo, mejor ahorrar");
    s.reasons.push_back(buf);
    std::snprintf(buf, sizeof buf, "%.1f GB de RAM: %s.", ram_gb, ram_gb >= 6 ? "alcanza para rebobinar sin problema" : "conviene no guardar mucho historial");
    s.reasons.push_back(buf);
    if (hw.software)
        s.reasons.push_back("Se está dibujando sin tarjeta gráfica (modo software), así que los efectos se quedan apagados.");
    else
        s.reasons.push_back("Tarjeta gráfica: " + hw.gpu + " con " + (hw.renderer.empty() ? std::string("el motor automático") : hw.renderer) + ".");
    std::snprintf(buf, sizeof buf, "Pantalla a %.0f Hz%s.", hw.refresh,
                  hw.refresh > 75 ? ": en pantalla completa la cambio a 60 Hz para que el movimiento sea perfectamente parejo" : "");
    s.reasons.push_back(buf);

    if (hw.software || hw.threads <= 2 || ram_gb < 3.5) {
        s.tier = "Básico";
        s.summary = "Tu equipo va bien con las consolas clásicas, pero vamos a cuidar cada recurso.";
        r.effect = static_cast<int>(Effect::None);
        r.filter = static_cast<int>(Filter::Pixel);
        r.runahead = 0;
        r.rewind = false;
        r.latency_ms = 96;
        r.ambient = false;
    } else if (hw.threads >= 6 && ram_gb >= 7.5) {
        s.tier = "Alto";
        s.summary = "¡A tu equipo le sobra fuerza! Activé lo mejor: efecto de pantalla según la consola, adelanto de un cuadro en NES y rebobinado largo.";
        r.effect = static_cast<int>(Effect::Auto);
        r.effect_strength = 0.5f;
        r.filter = static_cast<int>(Filter::SharpSmooth);
        r.runahead = 1;
        r.rewind = true;
        r.rewind_seconds = 30;
        r.latency_ms = 64;
        r.ambient = true;
    } else {
        s.tier = "Equilibrado";
        s.summary = "Tu equipo va muy bien. Dejé un equilibrio entre buena imagen y respuesta rápida.";
        r.effect = static_cast<int>(Effect::Auto);
        r.effect_strength = 0.35f;
        r.filter = static_cast<int>(Filter::SharpSmooth);
        r.runahead = 1;
        r.rewind = true;
        r.rewind_seconds = 15;
        r.latency_ms = 64;
        r.ambient = true;
    }
    r.vsync = true;
    r.fullscreen_60hz = true;

    static const char* effects[] = {"Ninguno", "Líneas de barrido", "Pantalla CRT", "Pantalla LCD", "Según la consola (CRT en TV, LCD en portátiles)"};
    static const char* filters[] = {"Píxel exacto", "Nítido suave", "Suave"};
    s.changes.push_back(std::string("Efecto de pantalla: ") + effects[r.effect]);
    s.changes.push_back(std::string("Filtro: ") + filters[r.filter]);
    s.changes.push_back(r.runahead ? "Adelanto de 1 cuadro en Satronho Octa (los controles responden más rápido)" : "Sin adelanto de cuadros");
    s.changes.push_back(r.rewind ? "Rebobinar: " + std::to_string(r.rewind_seconds) + " segundos" : std::string("Rebobinar: apagado"));
    s.changes.push_back("Latencia del sonido: " + std::to_string(r.latency_ms) + " ms");
    s.changes.push_back(r.ambient ? "Fondo ambiente con los colores del juego" : "Fondo negro");
    s.changes.push_back("Sincronización vertical: activada");
    s.systems = Verdicts(hw);
    return s;
}

const char* MeterColorName(int index) {
    static const char* names[] = {"Satronho (verde agua)", "Samy (ámbar)", "Troya (lila)", "MechaRidds (verde)", "Juana (rosa)"};
    return names[std::clamp(index, 0, 4)];
}

unsigned MeterColor(int index) {
    static const unsigned colors[] = {0x4FD1C5, 0xFFB23E, 0x9B8CFF, 0x74D14C, 0xFF8FB5};
    return colors[std::clamp(index, 0, 4)];
}

} // namespace Satronho
