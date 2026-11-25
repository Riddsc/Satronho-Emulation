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
  if(ANDROID)
    # En Android la actividad de Java carga libSDL3.so y luego libmain.so (la app).
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
  else()
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
  endif()
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SDL_TESTS OFF CACHE BOOL "" FORCE)
  set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(SDL3 GIT_REPOSITORY https://github.com/libsdl-org/SDL.git GIT_TAG ${SATRONHO_SDL_TAG} GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(SDL3)
endif()

# ---------------------------------------------------------------- descompresores (.zip y .7z)
file(GLOB SATRONHO_LZMA external/lzma/*.c)
add_library(satronho_archivos STATIC
  external/miniz/miniz.c external/miniz/miniz_tdef.c external/miniz/miniz_tinfl.c external/miniz/miniz_zip.c
  ${SATRONHO_LZMA})
target_include_directories(satronho_archivos PUBLIC external/miniz external/lzma)
target_compile_definitions(satronho_archivos PRIVATE MINIZ_NO_ZLIB_COMPATIBLE_NAMES)
set_target_properties(satronho_archivos PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------- programa
set(IMGUI external/imgui)
set(SATRONHO_SOURCES
  src/main.cpp
  src/app.cpp
  src/app_ui.cpp
  src/app_extra.cpp
  src/app_graphics.cpp
  src/app_import.cpp
  src/app_touch.cpp
  src/app_bar.cpp
  src/app_tools.cpp
  src/app_native.cpp
  src/app_native_pages.cpp
  src/app_phone.cpp
  src/app_backup.cpp
  src/sys/native_ui.cpp
  src/sys/phone_scan.cpp
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
  src/util/i18n.cpp
  src/util/ini.cpp
  src/util/json.cpp
  src/util/log.cpp
  src/util/net.cpp
  src/util/archive.cpp
  src/util/chd.cpp
  src/util/shortcut.cpp
  src/gfx/gl.cpp
  src/gfx/hw_render.cpp
  src/gfx/video_gl.cpp
  src/gfx/vulkan_bridge.cpp
  src/util/paths.cpp
  ${IMGUI}/imgui.cpp
  ${IMGUI}/imgui_draw.cpp
  ${IMGUI}/imgui_tables.cpp
  ${IMGUI}/imgui_widgets.cpp
  ${IMGUI}/backends/imgui_impl_sdl3.cpp
  ${IMGUI}/backends/imgui_impl_sdlrenderer3.cpp
  ${IMGUI}/backends/imgui_impl_opengl3.cpp
)
if(ANDROID)
  set(SATRONHO_TARGET main)
  add_library(main SHARED ${SATRONHO_SOURCES} src/sys/android.cpp)
  target_link_libraries(main PRIVATE log android GLESv3 EGL)
else()
  set(SATRONHO_TARGET satronho-emulation)
  add_executable(${SATRONHO_TARGET} WIN32 ${SATRONHO_SOURCES})
endif()
target_include_directories(${SATRONHO_TARGET} PRIVATE src ${IMGUI} ${IMGUI}/backends external/stb external/libretro external/vulkan)
target_compile_definitions(${SATRONHO_TARGET} PRIVATE SATRONHO_BUILD=${SATRONHO_BUILD} IMGUI_DISABLE_OBSOLETE_FUNCTIONS)
target_link_libraries(${SATRONHO_TARGET} PRIVATE satronho_archivos)
if(TARGET SDL3::SDL3-static)
  target_link_libraries(${SATRONHO_TARGET} PRIVATE SDL3::SDL3-static)
else()
  target_link_libraries(${SATRONHO_TARGET} PRIVATE SDL3::SDL3)
endif()

if(WIN32)
  # El .rc lleva el número de build (Propiedades del .exe) y la ruta completa del ícono.
  set(SATRONHO_ICON "${CMAKE_SOURCE_DIR}/dist/windows/satronho.ico")
  configure_file(dist/windows/satronho.rc.in ${CMAKE_BINARY_DIR}/satronho.rc @ONLY)
  target_sources(${SATRONHO_TARGET} PRIVATE ${CMAKE_BINARY_DIR}/satronho.rc)
  target_link_libraries(${SATRONHO_TARGET} PRIVATE dxgi psapi ole32 uuid)
  target_compile_definitions(${SATRONHO_TARGET} PRIVATE NOMINMAX)
  if(MINGW)
    target_link_options(${SATRONHO_TARGET} PRIVATE -static)
  endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(${SATRONHO_TARGET} PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
endif()

# Junto al ejecutable: assets/ (fuentes, guías, logo) y cores/. En Android los empaqueta Gradle.
if(NOT ANDROID)
  add_custom_command(TARGET ${SATRONHO_TARGET} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:${SATRONHO_TARGET}>/assets)
endif()
