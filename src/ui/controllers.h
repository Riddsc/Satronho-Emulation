// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>

#include "imgui.h"
#include "sys/systems.h"

namespace Satronho::Ui {

// Dibuja el control (o la consola portátil) con la distribución original de botones.
// `pressed`: bits del RetroPad presionados ahora (se iluminan). `capturing`: botón que espera
// asignación (parpadea). Devuelve el botón sobre el que está el mouse, o -1.
int DrawController(ImDrawList* dl, Layout layout, ImVec2 pos, ImVec2 size, uint16_t pressed, int capturing, ImVec2 mouse);

// Proporción ancho/alto del dibujo de cada control.
float ControllerAspect(Layout layout);

} // namespace Satronho::Ui
