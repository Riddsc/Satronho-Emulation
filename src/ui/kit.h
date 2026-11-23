// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <vector>

#include "imgui.h"
#include "sys/systems.h"

struct SDL_Texture;
struct SDL_Renderer;
struct SDL_Surface;

namespace Satronho::Ui {

// ---------------------------------------------------------------- colores
namespace Color {
constexpr unsigned Bg = 0x0F0D17;
constexpr unsigned Side = 0x13111D;
constexpr unsigned Surface = 0x1B1829;
constexpr unsigned Surface2 = 0x241F38;
constexpr unsigned Surface3 = 0x2E2848;
constexpr unsigned Border = 0x2E2848;
constexpr unsigned Text = 0xEBE8F5;
constexpr unsigned Muted = 0xA9A4C2;
constexpr unsigned Dim = 0x6E6A86;
constexpr unsigned Accent = 0x9B8CFF;   // Troya, lila
constexpr unsigned Accent2 = 0xA78BFA;
constexpr unsigned Gold = 0xFFD166;     // anillo del planeta
constexpr unsigned Amber = 0xFFB23E;    // Samy
constexpr unsigned Teal = 0x4FD1C5;     // Ñoño
constexpr unsigned Pink = 0xFF8FB5;     // Juana
constexpr unsigned Green = 0x74D14C;    // MechaRidds
constexpr unsigned Red = 0xFF5D73;
constexpr unsigned Orange = 0xFF9F43;
} // namespace Color

ImU32 U32(unsigned rgb, float alpha = 1.0f);
ImVec4 V4(unsigned rgb, float alpha = 1.0f);
ImU32 Hsv(float hue_deg, float s, float v, float a = 1.0f);

// ---------------------------------------------------------------- tipografía
struct Fonts {
    ImFont* regular = nullptr;
    ImFont* medium = nullptr;
    ImFont* semibold = nullptr;
    ImFont* bold = nullptr;
    ImFont* mono = nullptr;
};
Fonts& GetFonts();
float S(float px);              // aplica la escala de la interfaz
void SetScale(float scale);
float Scale();
void LoadFonts(const std::string& dir);
void ApplyStyle();

struct FontScope {
    FontScope(ImFont* f, float px);
    ~FontScope();
};

// ---------------------------------------------------------------- imágenes
struct Image {
    SDL_Texture* tex = nullptr;
    int w = 0, h = 0;
    ImTextureRef Ref() const;
    explicit operator bool() const { return tex != nullptr; }
};
Image LoadImage(SDL_Renderer* r, const std::string& file);
Image ImageFromRGBA(SDL_Renderer* r, const unsigned char* rgba, int w, int h);
void FreeImage(Image& img);
SDL_Surface* LoadSurface(const std::string& file); // RGBA, para el ícono de la ventana
bool SavePng(const std::string& file, const unsigned char* rgba, int w, int h);

// ---------------------------------------------------------------- íconos dibujados
enum class Icon {
    Library, Gamepad, Gear, Sparkle, Bell, Info, Search, Play, Plus, Folder, Star, StarFill,
    Clock, ChevronRight, ChevronLeft, Close, Pause, Save, Load, Restart, Camera, Exit, Meter,
    Volume, Rewind, Forward, Check, Cpu, Memory, Monitor, Chip, Keyboard, Heart, Refresh, File,
    Grid, Palette,
};
void DrawIcon(ImDrawList* dl, Icon icon, ImVec2 center, float size, ImU32 color);
void IconText(Icon icon, const char* text, ImU32 color);

// ---------------------------------------------------------------- controles
enum class ButtonKind { Primary, Secondary, Ghost, Danger };
bool Button(const char* label, ButtonKind kind = ButtonKind::Secondary, ImVec2 size = {0, 0}, Icon icon = Icon::Check, bool with_icon = false);
bool IconButton(const char* id, Icon icon, float size, ImU32 color, const char* tooltip = nullptr, bool active = false);
bool NavItem(const char* label, Icon icon, bool selected, const char* badge = nullptr, bool disabled = false, unsigned accent = Color::Accent,
             const char* hint = nullptr);
bool Toggle(const char* id, bool* value);
bool Segmented(const char* id, int* value, const std::vector<const char*>& items, float width = 0);
bool SliderPercent(const char* id, float* value, float width);
bool SliderIntStyled(const char* id, int* value, int min, int max, const char* fmt, float width);
bool ComboStyled(const char* id, int* current, const std::vector<std::string>& items, float width);

// Fila de ajuste: título y explicación a la izquierda, control a la derecha.
void RowBegin(const char* title, const char* help);
void RowEnd();

void Chip(const char* text, unsigned color, bool filled = false);
void SectionLabel(const char* text);
void Title(const char* title, const char* subtitle = nullptr);
void Spinner(float radius, unsigned color);
void HelpTooltip(const char* text);

// Panel con fondo y borde redondeado (usa BeginChild).
bool PanelBegin(const char* id, ImVec2 size, unsigned bg = Color::Surface, float rounding = 16, ImVec2 padding = {18, 16});
void PanelEnd();

// Tarjeta de un guía: avatar, nombre, rol y mensaje.
void GuideBanner(const Image& avatar, const char* name, const char* role, unsigned color, const char* text, float width = 0);

// Portada dibujada para un juego de NES (cartucho con etiqueta de color).
void DrawCartridge(ImDrawList* dl, ImVec2 p0, ImVec2 p1, unsigned hue, const std::string& title, float rounding, bool hovered,
                   Layout layout = Layout::Nes, const char* label = "OCTA");

std::string Initials(const std::string& title);
void Ellipsize(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, ImU32 color, const std::string& text, float max_w, int max_lines = 1);

} // namespace Satronho::Ui
