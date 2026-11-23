// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "sys/settings.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "util/ini.h"

namespace Satronho {

void Settings::Load(const Ini& ini) {
    fullscreen = ini.GetBool("video.pantalla_completa", fullscreen);
    scale_mode = std::clamp(ini.GetInt("video.escala", scale_mode), 0, 2);
    filter = std::clamp(ini.GetInt("video.filtro", filter), 0, 2);
    effect = std::clamp(ini.GetInt("video.efecto", effect), 0, 2);
    effect_strength = std::clamp(ini.GetFloat("video.intensidad", effect_strength), 0.0f, 1.0f);
    ambient = ini.GetBool("video.fondo_ambiente", ambient);
    vsync = ini.GetBool("video.vsync", vsync);
    renderer = ini.Get("video.motor", renderer);

    volume = std::clamp(ini.GetFloat("audio.volumen", volume), 0.0f, 1.0f);
    latency_ms = std::clamp(ini.GetInt("audio.latencia", latency_ms), 24, 200);
    mute_fast_forward = ini.GetBool("audio.silencio_avance", mute_fast_forward);

    runahead = std::clamp(ini.GetInt("rendimiento.adelanto", runahead), 0, 3);
    rewind = ini.GetBool("rendimiento.rebobinar", rewind);
    rewind_seconds = std::clamp(ini.GetInt("rendimiento.rebobinar_segundos", rewind_seconds), 5, 60);
    fast_forward = std::clamp(ini.GetInt("rendimiento.avance", fast_forward), 2, 8);

    meter = std::clamp(ini.GetInt("medidor.modo", meter), 0, 2);
    meter_corner = std::clamp(ini.GetInt("medidor.esquina", meter_corner), 0, 3);
    meter_color = std::clamp(ini.GetInt("medidor.color", meter_color), 0, 4);

    guides = ini.GetBool("guias.activas", guides);
    bubble = ini.GetBool("guias.burbuja", bubble);
    ui_scale = std::clamp(ini.GetFloat("interfaz.escala", ui_scale), 0.75f, 1.75f);

    folders.clear();
    const int n = ini.GetInt("biblioteca.carpetas", 0);
    for (int i = 0; i < n; ++i) {
        std::string f = ini.Get("biblioteca.carpeta." + std::to_string(i));
        if (!f.empty()) folders.push_back(f);
    }
    sort = std::clamp(ini.GetInt("biblioteca.orden", sort), 0, 2);
    favorites_only = ini.GetBool("biblioteca.solo_favoritos", favorites_only);

    auto_update = ini.GetBool("actualizaciones.buscar", auto_update);
    first_run_done = ini.GetBool("estado.bienvenida", first_run_done);
    preset = ini.Get("estado.sugerida", preset);
    last_seen_build = ini.GetInt("estado.ultima_build", last_seen_build);
}

void Settings::Save(Ini& ini) const {
    ini.SetBool("video.pantalla_completa", fullscreen);
    ini.SetInt("video.escala", scale_mode);
    ini.SetInt("video.filtro", filter);
    ini.SetInt("video.efecto", effect);
    ini.SetFloat("video.intensidad", effect_strength);
    ini.SetBool("video.fondo_ambiente", ambient);
    ini.SetBool("video.vsync", vsync);
    ini.Set("video.motor", renderer);

    ini.SetFloat("audio.volumen", volume);
    ini.SetInt("audio.latencia", latency_ms);
    ini.SetBool("audio.silencio_avance", mute_fast_forward);

    ini.SetInt("rendimiento.adelanto", runahead);
    ini.SetBool("rendimiento.rebobinar", rewind);
    ini.SetInt("rendimiento.rebobinar_segundos", rewind_seconds);
    ini.SetInt("rendimiento.avance", fast_forward);

    ini.SetInt("medidor.modo", meter);
    ini.SetInt("medidor.esquina", meter_corner);
    ini.SetInt("medidor.color", meter_color);

    ini.SetBool("guias.activas", guides);
    ini.SetBool("guias.burbuja", bubble);
    ini.SetFloat("interfaz.escala", ui_scale);

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

Suggestion Suggest(const HardwareInfo& hw, const Settings& current) {
    Suggestion s;
    s.result = current;
    Settings& r = s.result;
    const double ram_gb = hw.ram_mb / 1024.0;

    char buf[256];
    std::snprintf(buf, sizeof buf, "Procesador con %d hilos: %s.", hw.threads, hw.threads >= 4 ? "de sobra para NES" : "justo, mejor ahorrar");
    s.reasons.push_back(buf);
    std::snprintf(buf, sizeof buf, "%.1f GB de RAM: %s.", ram_gb, ram_gb >= 6 ? "alcanza para rebobinar sin problema" : "conviene no guardar mucho historial");
    s.reasons.push_back(buf);
    if (hw.software)
        s.reasons.push_back("Se está dibujando sin tarjeta gráfica (modo software), así que los efectos se quedan apagados.");
    else
        s.reasons.push_back("Tarjeta gráfica: " + hw.gpu + " con " + (hw.renderer.empty() ? std::string("el motor automático") : hw.renderer) + ".");
    std::snprintf(buf, sizeof buf, "Pantalla a %.0f Hz.", hw.refresh);
    s.reasons.push_back(buf);

    if (hw.software || hw.threads <= 2 || ram_gb < 3.5) {
        s.tier = "Básico";
        s.summary = "Tu equipo va bien para NES, pero vamos a cuidar cada recurso.";
        r.effect = static_cast<int>(Effect::None);
        r.filter = static_cast<int>(Filter::Pixel);
        r.runahead = 0;
        r.rewind = false;
        r.latency_ms = 96;
        r.ambient = false;
    } else if (hw.threads >= 6 && ram_gb >= 7.5) {
        s.tier = "Alto";
        s.summary = "¡Tu equipo le sobra a la NES! Activé lo mejor: efecto CRT, adelanto de un cuadro y rebobinado largo.";
        r.effect = static_cast<int>(Effect::Crt);
        r.effect_strength = 0.5f;
        r.filter = static_cast<int>(Filter::SharpSmooth);
        r.runahead = 1;
        r.rewind = true;
        r.rewind_seconds = 30;
        r.latency_ms = 48;
        r.ambient = true;
    } else {
        s.tier = "Equilibrado";
        s.summary = "Tu equipo va muy bien. Dejé un equilibrio entre buena imagen y respuesta rápida.";
        r.effect = static_cast<int>(Effect::Scanlines);
        r.effect_strength = 0.4f;
        r.filter = static_cast<int>(Filter::SharpSmooth);
        r.runahead = 1;
        r.rewind = true;
        r.rewind_seconds = 15;
        r.latency_ms = 64;
        r.ambient = true;
    }
    r.vsync = true;

    static const char* effects[] = {"Ninguno", "Líneas de barrido", "Pantalla CRT"};
    static const char* filters[] = {"Píxel exacto", "Nítido suave", "Suave"};
    s.changes.push_back(std::string("Efecto de pantalla: ") + effects[r.effect]);
    s.changes.push_back(std::string("Filtro: ") + filters[r.filter]);
    s.changes.push_back(r.runahead ? "Adelanto de 1 cuadro (los controles responden más rápido)" : "Sin adelanto de cuadros");
    s.changes.push_back(r.rewind ? "Rebobinar: " + std::to_string(r.rewind_seconds) + " segundos" : std::string("Rebobinar: apagado"));
    s.changes.push_back("Latencia del sonido: " + std::to_string(r.latency_ms) + " ms");
    s.changes.push_back(r.ambient ? "Fondo ambiente con los colores del juego" : "Fondo negro");
    s.changes.push_back("Sincronización vertical: activada");
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
