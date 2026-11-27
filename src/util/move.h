// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "util/paths.h"

// Mover archivos sin perder nada (build 28). Se usa al mudar la carpeta de datos y al ordenar
// los juegos en «Satronho Games».
namespace Satronho::Move {

// Qué hacer si en el destino ya hay un archivo con ese nombre y es distinto.
enum class Conflict {
    Keep,  // no se mueve (sigue donde estaba): para juegos, otro juego con el mismo nombre
    Aside, // el del destino se aparta como «…anterior» y se mueve el nuestro: partidas y estados
};

// ¿Los dos archivos son iguales byte por byte?
bool SameContent(const fs::path& a, const fs::path& b);

// Un nombre libre para apartar un archivo: «Zelda.srm.anterior», «Zelda.srm.anterior2»…
fs::path AsideName(const fs::path& p);

// Mueve un archivo. Si el destino ya existe y es idéntico, solo se quita el original. Si es distinto,
// decide `conflict`. Entre unidades distintas copia, comprueba el tamaño y solo entonces borra el
// original. `aside` (opcional) cuenta los archivos apartados. Devuelve true si quedó en `to`.
bool File(const fs::path& from, const fs::path& to, Conflict conflict, int* aside = nullptr);

} // namespace Satronho::Move
