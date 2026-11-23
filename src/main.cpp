// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <SDL3/SDL_main.h>

#include "app.h"

int main(int argc, char** argv) {
    Satronho::App app;
    return app.Run(argc, argv);
}
