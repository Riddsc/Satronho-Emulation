// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <vector>

#include "sys/hardware.h"
#include "sys/library.h"
#include "sys/settings.h"

namespace Satronho {

// Conversación con los guías. Todo funciona sin internet: las respuestas se arman con lo que
// la app sabe de verdad (tu equipo, tus juegos, tus mandos, tus ajustes). Los guías son:
// 0 MechaRidds, 1 Samy, 2 Troya, 3 Ñoño y 4 Juana (el mismo orden que GuideId).

enum class ChatAction {
    None, Library, System, Controls, Hotkeys, Suggested, ApplySuggestion, News, CheckUpdates, AddFolder,
    SettingsVideo, SettingsAudio, SettingsPerformance, SettingsCore, SettingsLibrary, SettingsAppearance,
    SettingsGuides, DownloadCovers, PlayGame,
};

struct ChatButton {
    std::string label;
    ChatAction action = ChatAction::None;
    std::string arg; // consola o juego, según la acción
};

struct ChatReply {
    int guide = 0;
    std::string text;
    std::vector<ChatButton> buttons;
};

// Lo que la app le cuenta al motor de respuestas antes de cada pregunta.
struct ChatContext {
    const HardwareInfo* hw = nullptr;
    const Settings* settings = nullptr;
    const std::vector<Game>* games = nullptr;
    std::vector<SystemVerdict> verdicts;     // qué tan bien va cada consola (Ñoño)
    std::string tier;                         // «Alto», «Equilibrado» o «Básico»
    std::vector<std::string> pads;            // mandos conectados: «Mando de Xbox (Xbox Series)»
    std::vector<std::pair<std::string, std::string>> hotkeys; // nombre del atajo, tecla
    std::vector<std::string> bios_missing;    // consolas que necesitan BIOS y no la tienen
    std::vector<std::string> bios_optional;   // consolas que usan BIOS opcional y no la tienen
    std::vector<std::string> missing_cores;
    int build = 0;
    std::string update_text;                  // estado del actualizador, en palabras
    std::string gfx;                          // motor gráfico en uso: «OpenGL 4.6 · NVIDIA…»
    bool vulkan = false;                      // hay Vulkan en este equipo
    int covers = 0;                           // carátulas guardadas
};

// Responde una pregunta escrita. asked = guía a quien se le preguntó (-1 = a quien sepa más).
ChatReply AnswerQuestion(const std::string& question, const ChatContext& ctx, int asked);
// Saludo de un guía al abrir la conversación.
ChatReply ChatGreeting(const ChatContext& ctx, int guide);
// Preguntas sugeridas (botoncitos) para empezar.
std::vector<std::string> SuggestedQuestions(int guide);

// Reparto de las apariciones de los guías (regla de Ridds, de 1000): MechaRidds 300 (30 %); el
// resto (700) se reparte 65 % Samy y Troya, 25 % Ñoño y 10 % Juana. En el orden de GuideId:
// MechaRidds, Samy, Troya, Ñoño, Juana. El mismo reparto está en Kotlin (Guias.reparto) y en NX.
constexpr int kGuideShare[5] = {300, 230, 225, 175, 70};

// Elige qué guía aparece (consejos, saludos) al azar, según el reparto.
int PickGuide(unsigned random);

// ¿El texto es la presentación del propio guía («Soy Juana…»)? Esos los dice siempre él.
bool NamesItself(int guide, const std::string& text);

// Guía fijo para un texto que se dibuja muchas veces (explicaciones de las pantallas): sale del
// texto mismo, así no cambia en cada cuadro, y en el conjunto de textos se cumple el reparto.
int StableGuide(int wanted, const std::string& text);
// La app guarda a quién le tocó cada texto fijo y los reparte parejo (App::StableSpeaker); sin la
// app (pruebas), se usa el texto mismo.
using StableGuideFn = int (*)(void* user, int wanted, const std::string& key);
void SetStableGuideHook(StableGuideFn fn, void* user);
// Lo mismo con la clave del guía («samy», «nono»…), para lo que se arma en JSON.
std::string StableGuideKey(const std::string& wanted_key, const std::string& text);

} // namespace Satronho
