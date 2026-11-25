// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "sys/native_ui.h"
#include "util/i18n.h"

namespace Satronho {

// Una pantalla de Ajustes para la interfaz nativa (Android). C++ describe qué hay (interruptores,
// opciones, deslizadores, botones…) y Kotlin la dibuja con el estilo de Satronho NX: tarjetas con
// ícono, título, explicación y estado. Cada elemento con id guarda qué hacer cuando la persona lo
// cambia («ajuste|id|valor»). Los textos van en español y aquí se traducen (Tr).
class NPage {
public:
    using Setter = std::function<void(const std::string&)>;

    void Section(const std::string& title, const std::string& desc = "") {
        CloseSection();
        sections_ += sections_.empty() ? "" : ",";
        sections_ += "{\"titulo\":" + Q(T(title)) + ",\"descripcion\":" + Q(T(desc)) + ",\"items\":[";
        open_ = true;
        first_item_ = true;
    }

    void Toggle(const std::string& id, const std::string& title, const std::string& desc, bool value, std::function<void(bool)> set,
                const std::string& icon = "") {
        Item("{\"tipo\":\"interruptor\",\"id\":" + Q(id) + Common(title, desc, icon) + ",\"valor\":" + (value ? "true" : "false") + "}");
        setters[id] = [set](const std::string& v) { set(v == "1" || v == "true"); };
    }

    // Opciones: con pocas y cortas Kotlin pone botones segmentados; si no, una lista.
    void Choice(const std::string& id, const std::string& title, const std::string& desc, int value, const std::vector<std::string>& labels,
                std::function<void(int)> set, const std::string& icon = "", const std::string& note = "") {
        std::string ops = "[";
        for (size_t i = 0; i < labels.size(); ++i) ops += (i ? "," : "") + Q(T(labels[i]));
        ops += "]";
        Item("{\"tipo\":\"opciones\",\"id\":" + Q(id) + Common(title, desc, icon) + ",\"valor\":" + std::to_string(value) + ",\"opciones\":" + ops +
             ",\"nota\":" + Q(T(note)) + "}");
        setters[id] = [set](const std::string& v) { set(std::atoi(v.c_str())); };
    }

    void Slider(const std::string& id, const std::string& title, const std::string& desc, float value, float min, float max, float step,
                const std::string& fmt, std::function<void(float)> set, const std::string& icon = "") {
        Item("{\"tipo\":\"deslizador\",\"id\":" + Q(id) + Common(title, desc, icon) + ",\"valor\":" + Num(value) + ",\"min\":" + Num(min) +
             ",\"max\":" + Num(max) + ",\"paso\":" + Num(step) + ",\"formato\":" + Q(fmt) + "}");
        setters[id] = [set](const std::string& v) { set(static_cast<float>(std::atof(v.c_str()))); };
    }

    // Botón. style: "" normal, "primario", "peligro". confirm: si no está vacío, se pregunta antes.
    void Action(const std::string& id, const std::string& title, const std::string& desc, const std::string& icon, std::function<void()> run,
                const std::string& style = "", const std::string& confirm = "", const std::string& value = "") {
        Item("{\"tipo\":\"accion\",\"id\":" + Q(id) + Common(title, desc, icon) + ",\"estilo\":" + Q(style) + ",\"confirmar\":" + Q(T(confirm)) +
             ",\"valor\":" + Q(T(value)) + "}");
        setters[id] = [run](const std::string&) { run(); };
    }

    // Va a otra pantalla de Ajustes.
    void Link(const std::string& dest, const std::string& title, const std::string& desc, const std::string& icon,
              const std::string& value = "", const std::string& color = "") {
        Item("{\"tipo\":\"pagina\",\"destino\":" + Q(dest) + Common(title, desc, icon) + ",\"valor\":" + Q(T(value)) + ",\"color\":" + Q(color) + "}");
    }

    void Info(const std::string& title, const std::string& desc, const std::string& icon = "", const std::string& color = "") {
        Item("{\"tipo\":\"texto\"" + Common(title, desc, icon) + ",\"color\":" + Q(color) + "}");
    }

    // Un guía habla (banner con su carita).
    void Guide(const std::string& guide, const std::string& text) {
        Item("{\"tipo\":\"guia\",\"guia\":" + Q(guide) + ",\"texto\":" + Q(T(text)) + "}");
    }

    // Fila de consolas para elegir una (Gráficos, Núcleo, Controles).
    void Systems(const std::string& id, const std::string& current, const std::vector<std::pair<std::string, std::pair<std::string, std::string>>>& list,
                 std::function<void(const std::string&)> set) {
        std::string ops = "[";
        for (size_t i = 0; i < list.size(); ++i)
            ops += (i ? "," : "") + std::string("{\"id\":") + Q(list[i].first) + ",\"nombre\":" + Q(T(list[i].second.first)) + ",\"color\":" +
                   Q(list[i].second.second) + "}";
        ops += "]";
        Item("{\"tipo\":\"consolas\",\"id\":" + Q(id) + ",\"valor\":" + Q(current) + ",\"opciones\":" + ops + "}");
        setters[id] = std::move(set);
    }

    void Tabs(const std::string& id, int value, const std::vector<std::string>& labels, std::function<void(int)> set) {
        std::string ops = "[";
        for (size_t i = 0; i < labels.size(); ++i) ops += (i ? "," : "") + Q(T(labels[i]));
        ops += "]";
        Item("{\"tipo\":\"pestanas\",\"id\":" + Q(id) + ",\"valor\":" + std::to_string(value) + ",\"opciones\":" + ops + "}");
        setters[id] = [set](const std::string& v) { set(std::atoi(v.c_str())); };
    }

    // Elemento ya armado (control de Controles, chat…). Sus acciones se registran aparte.
    void Raw(const std::string& json) { Item(json); }

    std::string Json(const std::string& page, const std::string& title, const std::string& subtitle, const std::string& icon = "") {
        CloseSection();
        return "{\"pagina\":" + Q(page) + ",\"titulo\":" + Q(T(title)) + ",\"subtitulo\":" + Q(T(subtitle)) + ",\"icono\":" + Q(icon) +
               ",\"secciones\":[" + sections_ + "]}";
    }

    std::map<std::string, Setter> setters;

    static std::string Q(const std::string& s) { return NativeUi::Quote(s); }
    static std::string T(const std::string& s) { return s.empty() ? s : Tr(s); }
    static std::string Num(float v) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%.4g", v);
        return buf;
    }

private:
    std::string Common(const std::string& title, const std::string& desc, const std::string& icon) {
        return ",\"titulo\":" + Q(T(title)) + ",\"descripcion\":" + Q(T(desc)) + ",\"icono\":" + Q(icon);
    }
    void Item(const std::string& json) {
        if (!open_) Section("");
        sections_ += first_item_ ? "" : ",";
        sections_ += json;
        first_item_ = false;
    }
    void CloseSection() {
        if (open_) sections_ += "]}";
        open_ = false;
    }
    std::string sections_;
    bool open_ = false, first_item_ = true;
};

} // namespace Satronho
