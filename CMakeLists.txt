# SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.21)
project(SatronhoEmulation VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(SATRONHO_BUILD "0" CACHE STRING "Número de build (lo pone el CI)")
option(SATRONHO_FETCH_SDL "Descargar SDL3 si no se encuentra" ON)
set(SATRONHO_SDL_TAG "release-3.4.20" CACHE STRING "Versión de SDL3")

# ---------------------------------------------------------------- SDL3 (estática)
find_package(SDL3 CONFIG QUIET)
if(NOT SDL3_FOUND)
  if(NOT SATRONHO_FETCH_SDL)
    message(FATAL_ERROR "No se encontró SDL3")
  endif()
  include(FetchContent)
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SDL_TESTS OFF CACHE BOOL "" FORCE)
  set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(SDL3 GIT_REPOSITORY https://github.com/libsdl-org/SDL.git GIT_TAG ${SATRONHO_SDL_TAG} GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(SDL3)
endif()

# ---------------------------------------------------------------- programa
set(IMGUI external/imgui)
add_executable(satronho-emulation WIN32
  src/main.cpp
  src/app.cpp
  src/app_ui.cpp
  src/app_extra.cpp
  src/core/audio.cpp
  src/core/core_host.cpp
  src/core/input.cpp
  src/core/rewind.cpp
  src/core/video.cpp
  src/sys/hardware.cpp
  src/sys/library.cpp
  src/sys/covers.cpp
  src/sys/guide_chat.cpp
  src/sys/settings.cpp
  src/sys/updater.cpp
  src/ui/kit.cpp
  src/ui/controllers.cpp
  src/sys/systems.cpp
  src/util/hash.cpp
  src/util/ini.cpp
  src/util/json.cpp
  src/util/log.cpp
  src/util/net.cpp
  src/util/paths.cpp
  ${IMGUI}/imgui.cpp
  ${IMGUI}/imgui_draw.cpp
  ${IMGUI}/imgui_tables.cpp
  ${IMGUI}/imgui_widgets.cpp
  ${IMGUI}/backends/imgui_impl_sdl3.cpp
  ${IMGUI}/backends/imgui_impl_sdlrenderer3.cpp
)
target_include_directories(satronho-emulation PRIVATE src ${IMGUI} ${IMGUI}/backends external/stb external/libretro)
target_compile_definitions(satronho-emulation PRIVATE SATRONHO_BUILD=${SATRONHO_BUILD} IMGUI_DISABLE_OBSOLETE_FUNCTIONS)
if(TARGET SDL3::SDL3-static)
  target_link_libraries(satronho-emulation PRIVATE SDL3::SDL3-static)
else()
  target_link_libraries(satronho-emulation PRIVATE SDL3::SDL3)
endif()

if(WIN32)
  # El .rc lleva el número de build (Propiedades del .exe) y la ruta completa del ícono.
  set(SATRONHO_ICON "${CMAKE_SOURCE_DIR}/dist/windows/satronho.ico")
  configure_file(dist/windows/satronho.rc.in ${CMAKE_BINARY_DIR}/satronho.rc @ONLY)
  target_sources(satronho-emulation PRIVATE ${CMAKE_BINARY_DIR}/satronho.rc)
  target_link_libraries(satronho-emulation PRIVATE dxgi psapi)
  target_compile_definitions(satronho-emulation PRIVATE NOMINMAX)
  if(MINGW)
    target_link_options(satronho-emulation PRIVATE -static)
  endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(satronho-emulation PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
endif()

# Junto al ejecutable: assets/ (fuentes, guías, logo) y cores/.
add_custom_command(TARGET satronho-emulation POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:satronho-emulation>/assets)
