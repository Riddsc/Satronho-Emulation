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

// Elige qué guía aparece (consejos, saludos): MechaRidds cerca del 40 %; el resto se reparte
// Samy y Troya 65 % (mitad y mitad), Ñoño 25 % y Juana 10 %.
int PickGuide(unsigned random);

} // namespace Satronho
