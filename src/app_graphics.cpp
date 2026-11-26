// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ajustes » Gráficos, como la pestaña Graphics de Satronho NX pero por consola:
// - Arriba, lo de toda la app (pantalla completa, motor, tarjeta, sincronización…).
// - Una barra con las consolas: se elige cuál ajustar.
// - De cada consola: su imagen (tamaño, filtro, efecto), su motor y resolución (CD y 64) y las
//   opciones de imagen de su emulador, cada una explicada y con lo que hace el valor elegido.
// Las opciones del emulador se guardan igual que en Ajustes » Núcleo («nucleo.<consola>.<opción>»),
// así se aplican al abrir el juego y las dos pestañas muestran lo mismo.

#include <algorithm>
#include <cstdarg>
#include <cmath>
#include <cstdio>
#include <map>

#include "app.h"
#include "native_page.h"
#include "imgui.h"
#include "ui/controllers.h"
#include "ui/kit.h"
#include "util/i18n.h"
#include "util/log.h"

namespace Satronho {

using namespace Ui;

namespace {
std::string Fmt(const char* fmt, ...) {
    char buf[768];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof buf, Tr(fmt), args);
    va_end(args);
    return buf;
}

void Muted(const char* text, float size = 13.5f) {
    text = Tr(text);
    FontScope f(GetFonts().regular, size);
    ImGui::PushStyleColor(ImGuiCol_Text, V4(Color::Muted));
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

// Lo que hace el valor elegido, en una línea bajo la opción (color de acento suave).
void ChoiceNote(const char* text) {
    if (!text || !*text) return;
    FontScope f(GetFonts().medium, 12.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, V4(Color::Accent2, 0.95f));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
    ImGui::TextWrapped("%s", Tr(text));
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
}

// Título de sección que se pliega al tocarlo (flecha a la izquierda).
bool Section(const char* label, bool* open) {
    const char* text = Tr(label);
    FontScope f(GetFonts().semibold, 15);
    const float h = ImGui::GetFontSize() + S(14);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x;
    ImGui::PushID(label);
    if (ImGui::InvisibleButton("##sec", {w, h})) *open = !*open;
    const bool hov = ImGui::IsItemHovered();
    ImGui::PopID();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (hov) dl->AddRectFilled(p, {p.x + w, p.y + h}, U32(Color::Surface2, 0.6f), S(8));
    const float cy = p.y + h * 0.5f, a = S(5);
    const float cx = p.x + S(10);
    if (*open) dl->AddTriangleFilled({cx - a, cy - a * 0.5f}, {cx + a, cy - a * 0.5f}, {cx, cy + a * 0.7f}, U32(Color::Accent2));
    else dl->AddTriangleFilled({cx - a * 0.5f, cy - a}, {cx - a * 0.5f, cy + a}, {cx + a * 0.7f, cy}, U32(Color::Accent2));
    dl->AddText({p.x + S(24), cy - ImGui::GetFontSize() * 0.5f}, U32(Color::Text), text);
    return *open;
}

// ---------------------------------------------------------------- opciones conocidas de cada emulador
struct ValueInfo {
    const char* value;
    const char* label; // nullptr: el nombre que trae el núcleo
    const char* help;  // qué hace al elegirlo
};
struct OptionInfo {
    const char* key;
    const char* label;
    const char* help;
    std::vector<ValueInfo> values;
};

const std::vector<OptionInfo>& KnownOptions(const std::string& sys) {
    static const std::map<std::string, std::vector<OptionInfo>> m{
        {"octa",
         {{"nestopia_blargg_ntsc_filter", "Señal de televisor (NTSC)", "Imita el cable con el que se conectaba la consola a la tele.",
           {{"disabled", "Apagada", "Imagen limpia: píxeles nítidos, como en un monitor."},
            {"composite", "Compuesto (cable amarillo)", "Colores que se mezclan y un poco de borrosidad: como se veía en casa."},
            {"svideo", "S-Video", "Más nítido que el compuesto, con algo de mezcla de colores."},
            {"rgb", "RGB", "La señal más limpia que daba un televisor de antes."},
            {"monochrome", "Blanco y negro", "Como en un televisor sin color."}}},
          {"nestopia_palette", "Paleta de colores", "Cada televisor de la época mostraba los colores de la NES un poco distinto.",
           {{"cxa2025as", "Sony CXA2025AS (EE. UU.)", "Los colores de un televisor Sony americano: cálidos y fieles."},
            {"cxa2025as_jp", "Sony CXA2025AS (Japón)", "La misma, con el ajuste de los televisores japoneses."},
            {"consumer", "Televisor de casa", "Colores típicos de un televisor común."},
            {"canonical", "Canónica", "La paleta de referencia de Nestopia."},
            {"alternative", "Alternativa", "Otra versión de la canónica, un poco más suave."},
            {"rgb", "RGB (PlayChoice)", "Colores fuertes, como las máquinas de arcade PlayChoice."},
            {"pal", "Europea (PAL)", "Como en las consolas europeas."},
            {"digital-prime-fbx", "Digital Prime (FirebrandX)", "Colores vivos y muy fieles, medidos de una consola real."},
            {"magnum-fbx", "Magnum (FirebrandX)", "Más contraste y saturación."},
            {"smoothv2-fbx", "Smooth (FirebrandX)", "Colores suaves, agradables a la vista."},
            {"composite-direct-fbx", "Compuesto directo (FirebrandX)", "Como la señal compuesta, sin el ajuste de la tele."},
            {"pvm-style-d93-fbx", "Monitor profesional (FirebrandX)", "Como un monitor de estudio Sony PVM: frío y nítido."},
            {"royaltea", "Royaltea", "Colores equilibrados, para jugar de todo."}}},
          {"nestopia_aspect", "Proporción", "La forma de la imagen.",
           {{"auto", "Automática", "La que corresponde al juego."},
            {"ntsc", "Televisor NTSC", "Como se veía en un televisor de América y Japón (un poco más ancha)."},
            {"pal", "Televisor PAL", "Como en un televisor europeo."},
            {"4:3", "4:3", "La forma de las pantallas de antes."},
            {"uncorrected", "Píxeles cuadrados", "Sin corregir: cada píxel cuadrado (un poco más angosta)."}}},
          {"nestopia_overscan_v_top", "Recortar el borde de arriba", "Los televisores escondían unas líneas del borde; algunos juegos muestran basura ahí.", {}},
          {"nestopia_overscan_v_bottom", "Recortar el borde de abajo", "Igual que el de arriba, para el borde de abajo.", {}},
          {"nestopia_nospritelimit", "Quitar el límite de sprites", "La NES solo mostraba 8 objetos por línea y los demás parpadeaban.",
           {{"disabled", "Como la consola", "Con el parpadeo original cuando hay muchos objetos juntos."},
            {"enabled", "Sin parpadeo", "Se ve todo siempre; en pocos juegos rompe algún efecto."}}}}},
        {"hexa",
         {{"bsnes_aspect_ratio", "Proporción", "La forma de la imagen.",
           {{"Auto", "Automática", "La que corresponde al juego."},
            {"8:7", "8:7 (píxeles cuadrados)", "Cada píxel cuadrado: la imagen queda un poco angosta."},
            {"4:3", "4:3", "Como en un televisor de antes."},
            {"NTSC", "Televisor NTSC", "Como se veía en América y Japón."},
            {"PAL", "Televisor PAL", "Como en Europa."}}},
          {"bsnes_mode7_scale", "Modo 7 en HD", "Los suelos que giran y se inclinan (Mario Kart, F-Zero, Pilotwings) se dibujan a más resolución.",
           {{"1x", "Original (1×)", "Como la consola: los suelos se ven pixelados de lejos."},
            {"2x", "2×", "El doble de nítido."},
            {"3x", "3×", nullptr},
            {"4x", "4×", "Muy nítido; recomendado en pantallas Full HD o más."},
            {"5x", "5×", nullptr},
            {"6x", "6×", nullptr},
            {"7x", "7×", nullptr},
            {"8x", "8×", "Lo más nítido; pide más al procesador."}}},
          {"bsnes_mode7_perspective", "Modo 7: corregir la perspectiva", "Endereza las líneas del suelo en HD.",
           {{"ON", "Activada", "Suelo más parejo a lo lejos."}, {"OFF", "Desactivada", "Como lo dibuja la consola."}}},
          {"bsnes_mode7_supersample", "Modo 7: supermuestreo", "Suaviza los bordes del suelo en HD.",
           {{"OFF", "Desactivado", "Más liviano."}, {"ON", "Activado", "Más suave; pide más al procesador."}}},
          {"bsnes_mode7_mosaic", "Modo 7: efecto mosaico", "Algunos juegos pixelan la pantalla a propósito en las transiciones.",
           {{"ON", "Activado", "Se respeta el mosaico del juego."}, {"OFF", "Desactivado", "Se salta el mosaico."}}},
          {"bsnes_blur_emulation", "Desenfoque de televisor", "Algunos juegos usan el desenfoque de la tele para hacer transparencias.",
           {{"OFF", "Desactivado", "Imagen nítida."}, {"ON", "Activado", "Mezcla píxeles vecinos: las transparencias se ven como en la tele."}}},
          {"bsnes_ppu_show_overscan", "Mostrar los bordes ocultos", "Las líneas que el televisor escondía arriba y abajo.",
           {{"OFF", "Ocultos", "Como en la tele."}, {"ON", "Visibles", "Se ve la imagen completa (a veces con basura)."}}},
          {"bsnes_ppu_no_sprite_limit", "Quitar el límite de sprites", "La consola dejaba de dibujar objetos cuando había demasiados en una línea.",
           {{"OFF", "Como la consola", "Con los objetos que desaparecen o parpadean."}, {"ON", "Sin límite", "Se ve todo siempre."}}}}},
        {"bolsillo",
         {{"mgba_gb_colors", "Paleta del Game Boy", "Los colores de los juegos en blanco y negro del Game Boy original.", {}},
          {"mgba_color_correction", "Corrección de color", "La pantalla del Game Boy Color era más apagada que un monitor.",
           {{"OFF", "Desactivada", "Colores tal cual el juego: muy vivos."},
            {"GBC", "Como la pantalla del GBC", "Colores más suaves, como los veías en la consola."},
            {"GBA", "Como la pantalla del GBA", nullptr},
            {"Auto", "Automática", "Según el juego."}}},
          {"mgba_interframe_blending", "Mezcla de cuadros", "Algunos juegos hacen transparencias parpadeando cada cuadro.",
           {{"OFF", "Desactivada", "Imagen nítida; los efectos que parpadean se ven parpadeando."},
            {"mix", "Mezcla", "Quita el parpadeo de las transparencias."},
            {"mix_smart", "Mezcla inteligente", "Solo mezcla donde hay parpadeo: lo mejor de las dos."},
            {"lcd_ghosting", "Estela de pantalla LCD", "Imita el rastro que dejaba la pantalla vieja al moverse."},
            {"lcd_ghosting_fast", "Estela de LCD (rápida)", "Lo mismo, más liviano."}}},
          {"mgba_sgb_borders", "Marcos de Super Game Boy", "Algunos juegos traen un marco decorado para jugarlos en la tele.",
           {{"ON", "Activados", "Se muestra el marco alrededor del juego."}, {"OFF", "Desactivados", "Solo la pantalla del juego."}}}}},
        {"bolsillo_plus",
         {{"mgba_color_correction", "Corrección de color", "La pantalla del Game Boy Advance era mucho más apagada que un monitor.",
           {{"OFF", "Desactivada", "Colores tal cual el juego: muy vivos (a veces demasiado)."},
            {"GBA", "Como la pantalla del GBA", "Colores más suaves y naturales, como se diseñaron los juegos."},
            {"GBC", "Como la pantalla del GBC", nullptr},
            {"Auto", "Automática", "Según el juego."}}},
          {"mgba_interframe_blending", "Mezcla de cuadros", "Algunos juegos hacen transparencias parpadeando cada cuadro.",
           {{"OFF", "Desactivada", "Imagen nítida; los efectos que parpadean se ven parpadeando."},
            {"mix", "Mezcla", "Quita el parpadeo de las transparencias."},
            {"mix_smart", "Mezcla inteligente", "Solo mezcla donde hay parpadeo: lo mejor de las dos."},
            {"lcd_ghosting", "Estela de pantalla LCD", "Imita el rastro que dejaba la pantalla vieja al moverse."},
            {"lcd_ghosting_fast", "Estela de LCD (rápida)", "Lo mismo, más liviano."}}}}},
        {"cd",
         {{"beetle_psx_hw_msaa", "Antialiasing (MSAA)", "Suaviza los bordes dentados de los polígonos.",
           {{"1x", "Apagado", "Como la consola: bordes con escalones."},
            {"2x", "2×", "Bordes más suaves, casi sin costo."},
            {"4x", "4×", "Muy suaves; recomendado con una tarjeta dedicada."},
            {"8x", "8×", "Lo más suave; pide bastante a la tarjeta."},
            {"16x", "16×", "Exagerado: solo para tarjetas muy potentes."}}},
          {"beetle_psx_hw_super_sampling", "Supermuestreo", "Dibuja a la resolución alta y la achica a la original.",
           {{"disabled", "Desactivado", "Se ve a la resolución que elijas arriba."},
            {"enabled", "Activado", "Aspecto original (240p) pero sin dientes de sierra."}}},
          {"beetle_psx_hw_filter", "Filtrado de texturas", "Cómo se ven las texturas (las imágenes pegadas sobre los polígonos).",
           {{"nearest", "Píxel exacto", "Como la consola: texturas pixeladas y nítidas."},
            {"SABR", "SABR", "Redondea los píxeles de las texturas sin borronear."},
            {"xBR", "xBR", "Suaviza las texturas como si fueran dibujadas a mano."},
            {"bilinear", "Bilineal", "Texturas suaves y algo borrosas, como en la N64."},
            {"3-point", "3 puntos", "Suaves, con la técnica de la N64."},
            {"JINC2", "JINC2", "Suaves y bastante nítidas."}}},
          {"beetle_psx_hw_pgxp_mode", "Geometría precisa (PGXP)", "La PS1 redondeaba posiciones y los polígonos tiemblan al moverse.",
           {{"disabled", "Desactivada", "Como la consola: con el temblor típico."},
            {"memory only", "Activada", "Polígonos firmes; funciona en casi todos los juegos."},
            {"memory + CPU", "Activada + procesador", "Todavía más precisa, pero rompe algunos juegos."}}},
          {"beetle_psx_hw_pgxp_texture", "Texturas sin deformar (PGXP)", "Las texturas de la PS1 se doblan en los polígonos grandes. Necesita la geometría precisa.",
           {{"disabled", "Desactivado", "Como la consola: texturas que se deforman."},
            {"enabled", "Activado", "Texturas derechas, como en una consola moderna."}}},
          {"beetle_psx_hw_widescreen_hack", "Pantalla ancha (16:9)", "Abre la vista en los juegos 3D para llenar una pantalla ancha.",
           {{"disabled", "Desactivada", "4:3, como la consola."},
            {"enabled", "Activada", "Ves más a los lados; en algunos juegos aparecen cosas que no debían verse."}}},
          {"beetle_psx_hw_dither_mode", "Tramado", "La PS1 usaba una trama de puntitos para disimular que tenía pocos colores.",
           {{"1x(native)", "Original", "Los puntitos como en la consola."},
            {"internal resolution", "A la resolución alta", "Puntitos más finos y menos visibles."},
            {"disabled", "Desactivado", "Sin puntitos: imagen limpia (a veces con bandas de color)."}}},
          {"beetle_psx_hw_depth", "Profundidad de color", "Cuántos colores se usan para dibujar.",
           {{"native", "Original", "Como la consola."},
            {"16bpp(native)", "16 bits", "Como la consola: con bandas en los degradados."},
            {"32bpp", "32 bits", "Degradados suaves, sin bandas. Combínalo con el tramado desactivado."}}},
          {"beetle_psx_hw_aspect_ratio", "Proporción", "La forma de la imagen.",
           {{"corrected", "Corregida", "La forma correcta de cada juego (4:3)."},
            {"uncorrected", "Sin corregir", "Píxeles cuadrados."},
            {"4:3", "4:3", "Siempre 4:3."},
            {"ntsc", "Televisor NTSC", "Como en una tele de América y Japón."},
            {"16:9", "16:9", "Estirada a pantalla ancha (úsala con la pantalla ancha activada)."}}},
          {"beetle_psx_hw_crop_overscan", "Recortar los bordes", "Quita los bordes negros que dejaba la consola.",
           {{"disabled", "No", "Imagen completa, con bordes."},
            {"static", "A los lados", "Quita los bordes laterales."},
            {"smart", "A los lados y arriba/abajo", "Quita todos los bordes negros."}}},
          {"beetle_psx_hw_initial_scanline", "Recortar arriba", "Algunos juegos dejan una rayita de colores en el borde de arriba; en la tele no se veía.",
           {{"0", "Nada", "La imagen completa, con la rayita si el juego la tiene."},
            {"2", "2 líneas", "Esconde la rayita sin que se note el recorte."},
            {"4", "4 líneas", "Para juegos con más basura arriba."},
            {"8", "8 líneas", "Recorte grande."}}}}},
        {"n64",
         {{"mupen64plus-aspect", "Proporción", "La forma de la imagen.",
           {{"4:3", "Original (4:3)", "Como la consola."},
            {"16:9", "Ancha (estirada)", "Llena una pantalla ancha estirando la imagen."},
            {"16:9 adjusted", "Ancha (ajustada)", "Abre la vista a pantalla ancha sin estirar (OpenGL)."}}},
          {"mupen64plus-MultiSampling", "Antialiasing (MSAA)", "Suaviza los bordes dentados de los polígonos (OpenGL).",
           {{"0", "Apagado", "Bordes con escalones."},
            {"2", "2×", "Más suaves, casi sin costo."},
            {"4", "4×", "Muy suaves; recomendado con tarjeta dedicada."},
            {"8", "8×", "Lo más suave; pide bastante a la tarjeta."},
            {"16", "16×", "Exagerado: solo para tarjetas muy potentes."}}},
          {"mupen64plus-FXAA", "FXAA", "Otra forma de suavizar bordes, muy liviana (OpenGL).",
           {{"0", "Apagado", nullptr}, {"1", "Activado", "Bordes más suaves con muy poco costo; la imagen queda un poco más blanda."}}},
          {"mupen64plus-BilinearMode", "Filtro de texturas de la consola", "La N64 suavizaba sus texturas con un método propio.",
           {{"3point", "3 puntos (como la N64)", "El suavizado original de la consola: más fiel."},
            {"standard", "Bilineal estándar", "El suavizado de las tarjetas de PC: un poco más borroso."}}},
          {"mupen64plus-txFilterMode", "Suavizar o afilar texturas", "Un filtro extra sobre las texturas (OpenGL).",
           {{"None", "Ninguno", "Las texturas tal cual."},
            {"Smooth filtering 1", "Suave 1", "Un poco más suaves."},
            {"Smooth filtering 2", "Suave 2", nullptr},
            {"Smooth filtering 3", "Suave 3", nullptr},
            {"Smooth filtering 4", "Suave 4", "Lo más suave."},
            {"Sharp filtering 1", "Nítido 1", "Texturas más definidas."},
            {"Sharp filtering 2", "Nítido 2", "Lo más definido."}}},
          {"mupen64plus-txEnhancementMode", "Agrandar texturas", "Agranda las texturas con un filtro para que se vean menos pixeladas (OpenGL).",
           {{"None", "Ninguno", "Las texturas originales."},
            {"As Is", "Tal cual", "Las originales, sin filtro."},
            {"X2", "X2", nullptr},
            {"X2SAI", "2xSaI", "Bordes redondeados, estilo dibujo."},
            {"HQ2X", "HQ2x", "Suaviza sin perder los contornos."},
            {"HQ4X", "HQ4x", "Como HQ2x, más grande."},
            {"2xBRZ", "2xBRZ", "Muy limpio, estilo dibujo animado."},
            {"4xBRZ", "4xBRZ", nullptr},
            {"6xBRZ", "6xBRZ", "Lo más limpio; pide más a la tarjeta."}}},
          {"mupen64plus-parallel-rdp-downscaling", "Supermuestreo (Vulkan)", "Dibuja grande y lo achica: aspecto original sin dientes de sierra.",
           {{"disable", "Desactivado", "Se ve a la resolución elegida."},
            {"1/2", "A la mitad", "Más limpio."},
            {"1/4", "A un cuarto", nullptr},
            {"1/8", "A un octavo", "Aspecto original perfecto con la resolución más alta."}}}}},
        {"sueno",
         {{"flycast_widescreen_hack", "Pantalla ancha (16:9)", "Abre la vista de los juegos 3D para llenar una pantalla ancha.",
           {{"disabled", "Desactivada", "4:3, como la consola."},
            {"enabled", "Activada", "Ves más a los lados; en algunos juegos aparecen cosas que no debían verse."}}},
          {"flycast_anisotropic_filtering", "Filtrado anisotrópico", "Deja nítidas las texturas del suelo y las paredes que se ven de lado.",
           {{"off", "Apagado", "Como la consola: el suelo se ve borroso a lo lejos."},
            {"2", "2×", "Un poco más nítido."},
            {"4", "4×", "Nítido, casi sin costo."},
            {"8", "8×", "Muy nítido."},
            {"16", "16×", "Lo más nítido; con tarjeta dedicada no se nota el costo."}}},
          {"flycast_texupscale", "Agrandar texturas (xBRZ)", "Agranda las texturas con un filtro para que se vean menos pixeladas.",
           {{"1", "Apagado", "Las texturas originales."},
            {"2", "2×", "Más suaves."},
            {"4", "4×", "Muy suaves; pide más a la tarjeta."},
            {"6", "6×", "Lo más suave; solo para tarjetas potentes."}}},
          {"flycast_alpha_sorting", "Transparencias", "Cómo se ordenan las cosas transparentes (humo, vidrio, agua).",
           {{"per-strip (fast, least accurate)", "Rápido", "Lo más liviano; a veces una transparencia queda delante de lo que no debe."},
            {"per-triangle (normal)", "Normal", "El equilibrio de siempre."},
            {"per-pixel (accurate)", "Exacto", "Como la consola; pide más a la tarjeta."}}},
          {"flycast_mipmapping", "Mipmapping", "Suaviza las texturas lejanas para que no tiemblen.",
           {{"enabled", "Activado", "Sin temblor a lo lejos."}, {"disabled", "Desactivado", "Más nítido, pero lo lejano parpadea."}}},
          {"flycast_fog", "Niebla", "Algunos juegos usan niebla para esconder lo lejano.",
           {{"enabled", "Activada", "Como la consola."}, {"disabled", "Desactivada", "Ves más lejos (a veces aparecen cosas de golpe)."}}}}},
        {"portatil",
         {{"ppsspp_texture_scaling_level", "Agrandar texturas", "Agranda las texturas con un filtro para que se vean menos pixeladas.",
           {{"disabled", "Apagado", "Las texturas originales."},
            {"1", "Apagado", "Las texturas originales."},
            {"2x", "2×", "Más suaves."},
            {"3x", "3×", nullptr},
            {"4x", "4×", "Muy suaves; pide más al procesador."},
            {"5x", "5×", "Lo más suave."}}},
          {"ppsspp_texture_scaling_type", "Tipo de agrandado", "El filtro que se usa al agrandar texturas.",
           {{"xbrz", "xBRZ", "Bordes limpios, estilo dibujo."},
            {"hybrid", "Híbrido", "Mezcla xBRZ con suavizado: bueno para fotos y fondos."},
            {"bicubic", "Bicúbico", "Suave y parejo."},
            {"hybrid_bicubic", "Híbrido + bicúbico", "Lo más suave."}}},
          {"ppsspp_texture_filtering", "Filtrado de texturas", "Cómo se ven las texturas al agrandarse.",
           {{"auto", "Automático", "Lo que pide cada juego."},
            {"nearest", "Píxel exacto", "Texturas nítidas y pixeladas."},
            {"linear", "Suave", "Texturas suaves."},
            {"auto_max_quality", "Máxima calidad", "Automático, con la mejor calidad posible."}}},
          {"ppsspp_texture_anisotropic_filtering", "Filtrado anisotrópico", "Deja nítidas las texturas del suelo y las paredes que se ven de lado.",
           {{"disabled", "Apagado", "Como la consola."},
            {"off", "Apagado", "Como la consola."},
            {"2x", "2×", nullptr},
            {"4x", "4×", "Nítido, casi sin costo."},
            {"8x", "8×", nullptr},
            {"16x", "16×", "Lo más nítido."}}},
          {"ppsspp_texture_deposterize", "Suavizar bandas de color", "Quita los escalones de color en los degradados de las texturas.",
           {{"disabled", "Desactivado", nullptr}, {"enabled", "Activado", "Degradados más suaves."}}}}},
        {"anillo",
         {{"beetle_saturn_horizontal_blend", "Suavizado horizontal", "Algunos juegos de Saturn usaban el desenfoque de la tele para hacer transparencias.",
           {{"disabled", "Desactivado", "Imagen nítida."}, {"enabled", "Activado", "Mezcla píxeles vecinos: transparencias como en la tele."}}},
          {"beetle_saturn_mesh_transparency", "Transparencias mejoradas", "La Saturn dibujaba las transparencias como una malla de puntos.",
           {{"disabled", "Como la consola", "Se ve la malla de puntos."}, {"enabled", "Transparencias reales", "La malla se convierte en una transparencia suave."}}},
          {"beetle_saturn_deinterlacer", "Desentrelazado", "Algunos juegos usan modo entrelazado (menús en alta resolución).",
           {{"weave", "Tejido", "Nítido en imágenes quietas; peinado al moverse."},
            {"bob", "Bob", "Sin peinado, con un leve parpadeo."},
            {"bob_offset", "Bob con ajuste", "Como Bob, más estable."},
            {"fastmad", "Adaptativo", "Lo mejor de los dos: nítido quieto y limpio en movimiento."},
            {"progressive", "Progresivo", "Para juegos que no lo necesitan."},
            {"off", "Apagado", nullptr}}},
          {"beetle_saturn_horizontal_overscan", "Recortar los bordes laterales", "Quita columnas de los bordes, que algunas teles escondían.", {}},
          {"beetle_saturn_initial_scanline", "Primera línea visible", "Recorta líneas de arriba (la tele escondía algunas).", {}},
          {"beetle_saturn_last_scanline", "Última línea visible", "Recorta líneas de abajo.", {}}}},
    };
    static const std::vector<OptionInfo> none;
    auto it = m.find(sys);
    return it == m.end() ? none : it->second;
}

const ValueInfo* FindValue(const OptionInfo& o, const std::string& v) {
    for (const auto& vi : o.values)
        if (v == vi.value) return &vi;
    return nullptr;
}

std::string CommonValueLabel(const std::string& v, const std::string& label) {
    std::string low = label.empty() ? v : label;
    for (auto& c : low) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    static const std::map<std::string, const char*> names{
        {"enabled", "Activado"}, {"disabled", "Desactivado"}, {"on", "Activado"}, {"off", "Desactivado"}, {"true", "Sí"},
        {"false", "No"}, {"auto", "Automático"}, {"none", "Ninguno"}};
    auto it = names.find(low);
    return it != names.end() ? Tr(it->second) : (label.empty() ? v : label);
}

// Imagen de Satronho: explicación de cada valor.
const char* kScaleHelp[] = {"La imagen llena la pantalla sin deformarse (puede quedar una franja a los lados).",
                            "Solo tamaños exactos (2×, 3×…): píxeles perfectos, con un borde alrededor.",
                            "Llena toda la pantalla aunque se deforme.",
                            "Pantalla ancha en las consolas 3D que la tienen; en las demás, lo más grande sin deformarse, con los costados llenos."};
const char* kFilterHelp[] = {"Píxeles duros y cuadrados. Perfecto con tamaño entero; en otros tamaños algunos píxeles salen más gruesos.",
                             "Píxeles cuadrados y parejos a cualquier tamaño, sin bordes temblorosos. El recomendado.",
                             "Todo suavizado: sin píxeles visibles, pero más borroso."};
const char* kEffectHelp[] = {"Imagen limpia, sin efectos.",
                             "Líneas horizontales finas, como el barrido de un televisor viejo.",
                             "Televisor de tubo: líneas, máscara de colores y bordes oscurecidos.",
                             "La rejilla de la pantalla de las portátiles.",
                             "CRT en las consolas de TV en 2D, LCD en las portátiles y limpio en las 3D."};
} // namespace

// ---------------------------------------------------------------- opciones del núcleo por consola

std::string App::CoreOptionValue(const SystemDef& sys, const CoreOption& def) const {
    const std::string saved = ini_.Get("nucleo." + sys.id + "." + def.key);
    if (!saved.empty()) return saved;
    for (const auto& [k, v] : sys.core_defaults)
        if (k == def.key) return v;
    return def.core_def.empty() ? def.def : def.core_def;
}

void App::SetCoreOptionFor(const SystemDef& sys, const std::string& key, const std::string& value) {
    const std::string prefix = "nucleo." + sys.id + ".";
    ini_.Set(prefix + key, value);
    ini_.SetBool(prefix + "@migrado", true);
    if (core_.CoreLoaded() && core_file_ == CoreFile(sys) && options_sys_ == sys.id) core_.SetOption(key, value);
    if (tab_options_sys_ == sys.id) tab_options_sys_.clear(); // la pestaña Núcleo vuelve a leerlas
    SaveConfig();
}

void App::MigrateGfxOptions() {
    // Build 16 guardaba aparte las opciones de gráficos («gfx.<consola>.<opción>») y no se aplicaban:
    // se pasan al lugar de las opciones del núcleo.
    for (const auto& [sys, opts] : settings_.gfx)
        for (const auto& [k, v] : opts) {
            const std::string key = "nucleo." + sys + "." + k;
            if (ini_.Get(key).empty()) {
                ini_.Set(key, v);
                ini_.SetBool("nucleo." + sys + ".@migrado", true);
            }
        }
    settings_.gfx.clear();
    std::vector<std::string> old;
    for (const auto& [k, v] : ini_.Values())
        if (k.rfind("graficos.", 0) == 0) old.push_back(k);
    for (const auto& k : old) ini_.Erase(k);
}

const std::vector<CoreOption>* App::CoreDefsFor(const SystemDef& sys) {
    if (core_.CoreLoaded() && core_file_ == CoreFile(sys)) return &core_.Options();
    if (auto it = core_cache_.find(CoreFile(sys)); it != core_cache_.end()) return &it->second.options;
    // Sin abrir un juego no se descarga el núcleo de la memoria solo por mirar: se carga una vez.
    if (!in_game_ && gfx_probe_tried_.insert(sys.id).second) {
        std::string err;
        if (EnsureCore(sys, &err))
            if (auto it = core_cache_.find(CoreFile(sys)); it != core_cache_.end()) return &it->second.options;
    }
    return nullptr;
}

// ---------------------------------------------------------------- la página

void App::DrawSettingsVideo() {
    bool changed = false;
    if (!compact_) {
        Muted("Elige la consola que quieres ajustar. Lo que cambies vale solo para ella.", 13);
        ImGui::Dummy({0, S(4)});
    }

    // ---------------- Barra de consolas (fija arriba; en pantallas angostas, en dos o más filas)
    if (gfx_sys_.empty()) gfx_sys_ = in_game_ ? sys_->id : "cd";
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const auto& systems = Systems();
        const float gap = S(8);
        const float avail = ImGui::GetContentRegionAvail().x;
        const float min_w = compact_ ? S(96) : S(104);
        const int per_row = std::max(1, std::min(static_cast<int>(systems.size()), static_cast<int>((avail + gap) / (min_w + gap))));
        const float w = (avail - gap * (per_row - 1)) / per_row;
        const float h = compact_ ? S(36) : S(64);
        for (size_t i = 0; i < systems.size(); ++i) {
            const auto& sd = systems[i];
            ImGui::PushID(sd.id.c_str());
            FontScope f(GetFonts().semibold, compact_ ? 12.0f : 13.0f);
            if (i % per_row != 0) ImGui::SameLine(0, gap);
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const bool sel = gfx_sys_ == sd.id;
            if (ImGui::InvisibleButton("##c", {w, h})) gfx_sys_ = sd.id;
            const bool hov = ImGui::IsItemHovered();
            dl->AddRectFilled(p, {p.x + w, p.y + h}, U32(sel ? sd.color : Color::Surface2, sel ? 0.22f : hov ? 1.0f : 0.7f), S(12));
            if (sel) dl->AddRect(p, {p.x + w, p.y + h}, U32(sd.color, 0.9f), S(12), S(2));
            const float tw = ImGui::CalcTextSize(sd.name.c_str()).x;
            ImGui::PushClipRect(p, {p.x + w, p.y + h}, true);
            if (compact_) {
                // Celular: botón bajito con el mando chiquito a la izquierda y el nombre.
                const float icon = sd.handheld ? S(13) : S(22);
                DrawControllerMini(dl, sd.layout, {p.x + S(18), p.y + h * 0.5f}, icon, sel || hov ? 1.0f : 0.7f);
                dl->AddText({p.x + S(34), p.y + (h - ImGui::GetFontSize()) * 0.5f}, U32(sel ? Color::Text : Color::Muted), sd.name.c_str());
            } else {
                const float icon = sd.handheld ? S(20) : S(32);
                DrawControllerMini(dl, sd.layout, {p.x + w * 0.5f, p.y + S(22)}, icon, sel || hov ? 1.0f : 0.7f);
                dl->AddText({p.x + std::max(S(4), (w - tw) * 0.5f), p.y + h - S(22)}, U32(sel ? Color::Text : Color::Muted), sd.name.c_str());
            }
            ImGui::PopClipRect();
            ImGui::PopID();
        }
    }
    const SystemDef* sys = SystemById(gfx_sys_);
    if (!sys) sys = &Systems().front();
    ImGui::Dummy({0, S(8)});
    // Desde aquí, lo que se desplaza: la barra de consolas queda siempre a la vista.
    ImGui::BeginChild("##gfx_opciones", {0, 0}, 0, ImGuiWindowFlags_NoBackground);
    if (qa_.count("bajar") && qa_frames_ > 5) ImGui::SetScrollY(S(static_cast<float>(std::atof(qa_["bajar"].c_str()))));
    if (!compact_) {
        GuideSays(GuideId::Troya, "Aquí ajustas cómo se ve cada consola. Elige una en la barra y cambia su imagen, su resolución y las opciones de "
                                  "su emulador; debajo de cada opción te digo qué hace lo que elegiste. Toca un título para plegarlo.");
        ImGui::Dummy({0, S(10)});
    }

    // Encabezado de la consola elegida.
    {
        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float hh = compact_ ? S(38) : S(50);
        dl->AddRectFilled(p, {p.x + ImGui::GetContentRegionAvail().x, p.y + hh}, U32(sys->color, 0.12f), S(12));
        DrawControllerMini(dl, sys->layout, {p.x + S(28), p.y + hh * 0.5f}, (sys->handheld ? S(24) : S(38)) * (compact_ ? 0.75f : 1.0f), 1.0f);
        FontScope f(GetFonts().bold, compact_ ? 15.0f : 18.0f);
        dl->AddText({p.x + S(56), p.y + (hh - ImGui::GetFontSize()) * 0.5f}, U32(Color::Text), (sys->full + "  ·  " + sys->hint).c_str());
        ImGui::Dummy({0, hh + S(8)});
    }

    // ---------------- Imagen
    if (Section("Imagen", &gfx_open_[0])) {
    VideoOverride& v = settings_.video_for[sys->id];
    RowBegin(Fmt("Imagen propia para %s", sys->name.c_str()).c_str(),
             "Apagado: usa la imagen general (la misma para todas las consolas que no tengan la suya).");
    if (Toggle("##propia", &v.on)) {
        if (v.on) {
            v.scale_mode = settings_.scale_mode;
            v.filter = settings_.filter;
            v.effect = settings_.effect;
            v.effect_strength = settings_.effect_strength;
        }
        changed = true;
    }
    RowEnd();
    int& scale = v.on ? v.scale_mode : settings_.scale_mode;
    int& filter = v.on ? v.filter : settings_.filter;
    int& effect = v.on ? v.effect : settings_.effect;
    float& strength = v.on ? v.effect_strength : settings_.effect_strength;
    if (!v.on) {
        ChoiceNote("Estás cambiando la imagen general: vale para esta consola y para todas las que no tengan la suya.");
        ImGui::Dummy({0, S(4)});
    }
    RowBegin("Tamaño", nullptr);
    changed |= Segmented("##escala", &scale, {"Ajustar", "Entero", "Estirar todo", "Inteligente"}, S(420));
    ChoiceNote(kScaleHelp[std::clamp(scale, 0, 3)]);
    RowEnd();
    RowBegin("Costados", "Lo que llena los lados de la pantalla cuando el juego no la ocupa entera.");
    changed |= Segmented("##costados", &settings_.side_fill, {"Colores del juego", "Marco de Satronho"}, S(340));
    RowEnd();
    RowBegin("Filtro", "Cómo se agrandan los píxeles.");
    changed |= Segmented("##filtro", &filter, {"Píxel exacto", "Nítido suave", "Suave"}, S(340));
    ChoiceNote(kFilterHelp[std::clamp(filter, 0, 2)]);
    RowEnd();
    RowBegin("Efecto de pantalla", nullptr);
    changed |= Segmented("##efecto", &effect, {"Ninguno", "Líneas", "CRT", "LCD", "Auto"}, S(360));
    ChoiceNote(kEffectHelp[std::clamp(effect, 0, 4)]);
    RowEnd();
    if (effect != 0) {
        RowBegin("Intensidad del efecto", nullptr);
        changed |= SliderPercent("##intensidad", &strength, S(340));
        RowEnd();
    }
    }

    // ---------------- Motor y resolución (consolas que dibujan con la tarjeta gráfica)
    const bool gpu_sys = sys->gpu;
    if (gpu_sys) {
        ImGui::Dummy({0, S(6)});
        if (Section("Motor y resolución", &gfx_open_[1])) {
        if (!gl_) {
            Muted("Con el motor clásico, estas consolas no usan la tarjeta gráfica. Elige OpenGL abajo, en «Para toda la app».", 13.5f);
        } else {
            const int api_now = GpuApiFor(*sys);
            const int res_now = GpuResFor(*sys, api_now);
            RowBegin("Motor del juego", nullptr);
            int api = settings_.GpuApi(sys->id);
            if (!sys->gpu_cpu && api == 3) api = 0;
            const bool changed_api = sys->gpu_cpu ? Segmented("##api", &api, {"Automático", "OpenGL", "Vulkan", "Procesador"}, S(420))
                                                  : Segmented("##api", &api, {"Automático", "OpenGL", "Vulkan"}, S(330));
            if (changed_api) {
                if (api == 2 && !hw_render_.VulkanAvailable()) Toast("Este equipo no tiene Vulkan; se usará OpenGL.", GuideId::Troya, 4);
                settings_.gpu_api[sys->id] = api;
                changed = true;
                if (in_game_ && sys_ == sys) Toast("Se aplica la próxima vez que abras el juego.", GuideId::Troya, 3);
            }
            const char* api_help = sys->id == "sueno"
                ? (api_now == 2 ? "Ahora: Vulkan. Suele rendir más en tarjetas NVIDIA y AMD; si un juego se ve raro, prueba OpenGL."
                                : "Ahora: OpenGL. El más compatible con los juegos de Dreamcast. Esta consola no puede dibujar con el procesador.")
                : sys->id == "portatil"
                ? (api_now == 1 ? "Ahora: OpenGL. Funciona en cualquier tarjeta." : api_now == 2 ? "Ahora: Vulkan. Más rápido en tarjetas NVIDIA y AMD."
                                                                : "Ahora: el procesador. El más fiel, pero muy pesado y solo a la resolución original.")
                : sys->id == "cd"
                ? (api_now == 1 ? "Ahora: OpenGL. Funciona en cualquier tarjeta." : api_now == 2 ? "Ahora: Vulkan. Más rápido en tarjetas NVIDIA y AMD."
                                                                : "Ahora: el procesador. Fiel, pero solo hasta 2× y más lento.")
                : (api_now == 1 ? "Ahora: GLideN64 con OpenGL. Acepta resoluciones exactas y tiene más filtros."
                   : api_now == 2 ? "Ahora: ParaLLEl-RDP con Vulkan. El más fiel a la consola; sube de 2 en 2."
                                  : "Ahora: Angrylion con el procesador. Muy fiel, pero pesado y sin subir la resolución.");
            ChoiceNote(api_help);
            RowEnd();
            RowBegin("Resolución", hw_.display_h > 0 ? Fmt("Tu pantalla tiene %d píxeles de alto.", hw_.display_h).c_str() : nullptr);
            {
                const std::vector<int> ks = GpuResSteps(*sys, api_now);
                const int nearest = NearestResStep(*sys, api_now);
                std::vector<std::string> res_names{std::string(Tr("Automática")) + "  ·  " + ResLabel(res_now, sys)};
                int sel = 0;
                const int stored = settings_.GpuRes(sys->id);
                for (size_t i = 0; i < ks.size(); ++i) {
                    std::string l = ResLabel(ks[i], sys);
                    if (ks[i] == nearest && l.find(Tr("como tu pantalla")) == std::string::npos) l += std::string("  · ") + Tr("la más cercana a tu pantalla");
                    res_names.push_back(l);
                    if (stored > 0 && ResHeight(ks[i]) <= ResHeight(stored)) sel = static_cast<int>(i) + 1;
                }
                if (ComboStyled("##res", &sel, res_names, ImGui::GetContentRegionAvail().x)) {
                    settings_.gpu_res[sys->id] = sel == 0 ? 0 : ks[sel - 1];
                    changed = true;
                    if (in_game_ && sys_ == sys) Toast("Se aplica la próxima vez que abras el juego.", GuideId::Troya, 3);
                }
                const bool exact = sys->id == "n64" && api_now == 1;
                if (sys->id == "sueno")
                    ChoiceNote("La Dreamcast dibuja a 640 × 480. Flycast la sube al tamaño exacto que elijas; la marcada «como tu pantalla» es la ideal.");
                else if (sys->id == "portatil")
                    ChoiceNote("La pantalla de la PSP es de 480 × 272 y PPSSPP la sube en múltiplos (2×, 3×…). Si eliges una más grande que tu "
                               "pantalla, se achica y se ve más nítida.");
                else if (!exact)
                    ChoiceNote(sys->id == "cd"
                        ? "La PS1 solo sube de 2 en 2 (1×, 2×, 4×, 8× y 16×), así que no siempre hay una igual a tu pantalla. Si eliges una más "
                          "grande, se dibuja grande y se achica a tu pantalla: se ve más nítida. Automática elige la más cercana."
                        : "Con Vulkan la 64 sube de 2 en 2 (1×, 2×, 4× y 8×). Si quieres una resolución exacta, usa OpenGL.");
                else
                    ChoiceNote("Con OpenGL la 64 dibuja exactamente a la resolución que elijas. La marcada «como tu pantalla» es la ideal.");
#ifdef __ANDROID__
                ChoiceNote("En el celular, la automática llega como mucho a 2× para que los juegos 3D vayan fluidos. Si tu teléfono puede "
                           "más, súbela tú; si un juego va lento, Ñoño te ofrece bajarla.");
#endif
            }
            RowEnd();
        }
        }
    }

    // ---------------- Opciones de su emulador
    ImGui::Dummy({0, S(6)});
    const std::vector<CoreOption>* defs = Section("Opciones de su emulador", &gfx_open_[2]) ? CoreDefsFor(*sys) : nullptr;
    if (!gfx_open_[2]) {
    } else if (!defs) {
        Muted("Estas opciones aparecen cuando el emulador de esta consola está instalado.", 13);
    } else {
        int shown = 0;
        // Primero las conocidas (explicadas), después las demás de imagen que traiga el emulador.
        std::vector<std::pair<const CoreOption*, const OptionInfo*>> rows;
        for (const auto& info : KnownOptions(sys->id)) {
            auto d = std::find_if(defs->begin(), defs->end(), [&](const CoreOption& o) { return o.key == info.key; });
            if (d != defs->end() && !d->values.empty()) rows.emplace_back(&*d, &info);
        }
        static const char* kSkip[] = {"renderer", "internal_resolution", "rdp-plugin", "rsp-plugin", "upscaling", "NativeResFactor", "43screensize",
                                       "169screensize", "renderer_software_fb", "frame_duping", "skip",
                                       // HDR: Satronho dibuja en 8 bits por canal, así que no hacen nada.
                                       "hdr", "aces", "tonemap", "tone_map", "luminance", "color_format", "gamut", "paper"};
        for (const auto& d : *defs) {
            if (d.values.size() < 2 || !d.visible) continue;
            if (std::any_of(rows.begin(), rows.end(), [&](const auto& r) { return r.first->key == d.key; })) continue;
            std::string cat = d.category;
            for (auto& c : cat) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            const bool video = cat.find("video") != std::string::npos || cat.find("graphic") != std::string::npos ||
                               cat.find("screen") != std::string::npos || cat.find("display") != std::string::npos;
            if (!video) continue;
            std::string lk = d.key + " " + d.label;
            for (auto& c : lk) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (std::any_of(std::begin(kSkip), std::end(kSkip), [&](const char* s) { return lk.find(s) != std::string::npos; })) continue;
            rows.emplace_back(&d, nullptr);
        }
        const size_t known = static_cast<size_t>(std::count_if(rows.begin(), rows.end(), [](const auto& r) { return r.second != nullptr; }));
        for (size_t ri = 0; ri < rows.size(); ++ri) {
            const auto& [d, info] = rows[ri];
            if (ri == known) {
                // Las demás del emulador: plegadas, para no llenar la página.
                ImGui::Dummy({0, S(6)});
                const std::string t = Fmt("Más opciones de imagen del emulador (%d)", static_cast<int>(rows.size() - known));
                RowBegin(t.c_str(), "Las trae el propio emulador; su explicación es la suya. Pasa el mouse por el nombre para leerla completa.");
                Toggle("##mas", &gfx_more_);
                RowEnd();
                if (!gfx_more_) break;
            }
            const std::string cur = CoreOptionValue(*sys, *d);
            std::vector<std::string> labels;
            int sel = 0;
            for (size_t i = 0; i < d->values.size(); ++i) {
                const auto& val = d->values[i];
                const ValueInfo* vi = info ? FindValue(*info, val.value) : nullptr;
                std::string l = vi && vi->label ? std::string(Tr(vi->label)) : CommonValueLabel(val.value, val.label);
                // Recortes de bordes: «N líneas».
                if (!vi && info && info->values.empty() && std::all_of(val.value.begin(), val.value.end(), ::isdigit))
                    l = val.value == "0" ? std::string(Tr("Nada")) : val.value + " " + Tr("líneas");
                if (val.value == d->core_def) l += std::string("  ") + Tr("(del emulador)");
                labels.push_back(l);
                if (val.value == cur) sel = static_cast<int>(i);
            }
            // Explicación del emulador: solo la primera frase (la completa, al pasar el mouse).
            std::string short_help = d->info;
            if (!info) {
                const size_t dot = short_help.find(". ");
                if (dot != std::string::npos && dot < 180) short_help.resize(dot + 1);
                else if (short_help.size() > 180) short_help = short_help.substr(0, 177) + "…";
            }
            RowBegin(info ? info->label : d->label.c_str(), info ? info->help : (short_help.empty() ? nullptr : short_help.c_str()));
            if (!info && short_help.size() < d->info.size() && ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(S(520));
                ImGui::TextUnformatted(d->info.c_str());
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
            ImGui::PushID(d->key.c_str());
            if (ComboStyled("##o", &sel, labels, ImGui::GetContentRegionAvail().x)) {
                SetCoreOptionFor(*sys, d->key, d->values[sel].value);
                changed = true;
            }
            if (info) {
                const ValueInfo* vi = FindValue(*info, d->values[sel].value);
                if (vi && vi->help) ChoiceNote(vi->help);
            }
            ImGui::PopID();
            RowEnd();
            ++shown;
        }
        if (shown == 0) Muted("El emulador de esta consola no tiene más opciones de imagen: usa las de arriba.", 13);
        else Muted("Las opciones del emulador se aplican al momento o al volver a abrir el juego. También están en Ajustes » Núcleo.", 12.5f);
    }

    ImGui::Dummy({0, S(14)});
    // ---------------- Para toda la app
    if (Section("Para toda la app (todas las consolas)", &gfx_open_[3])) {
#ifndef __ANDROID__
    RowBegin("Pantalla completa", "También con F11 o Alt+Enter.");
    bool fs = settings_.fullscreen;
    if (Toggle("##fs", &fs)) {
        SetFullscreen(fs);
        changed = true;
    }
    RowEnd();
    RowBegin("Pantalla completa a 60 Hz", "Si tu pantalla es de 120 o 144 Hz, al jugar en pantalla completa la cambio a 60 Hz: las consolas "
                                          "van a 60 cuadros y así el movimiento queda perfectamente parejo.");
    if (Toggle("##fs60", &settings_.fullscreen_60hz)) {
        if (settings_.fullscreen) SetFullscreen(true);
        changed = true;
    }
    RowEnd();
#endif
    RowBegin("Fondo ambiente", "Llena los bordes con los colores del juego, suaves y desenfocados.");
    changed |= Toggle("##amb", &settings_.ambient);
    RowEnd();
    RowBegin("Sincronización vertical", "Evita que la imagen se corte. Déjala activada salvo que sepas lo que haces.");
    if (Toggle("##vsync", &settings_.vsync)) {
        ApplyVsync();
        changed = true;
    }
    RowEnd();
    {
        std::vector<std::string> names{"Automático (OpenGL)", "OpenGL"};
        std::vector<std::string> values{"auto", "opengl"};
        for (int i = 0; i < SDL_GetNumRenderDrivers(); ++i) {
            const std::string d = SDL_GetRenderDriver(i);
            if (d == "software" || d == "gpu" || d == "opengl" || d == "opengles2") continue;
            std::string n = d == "direct3d11" ? "Direct3D 11" : d == "direct3d12" ? "Direct3D 12" : d == "direct3d" ? "Direct3D 9" : d == "vulkan" ? "Vulkan" : d;
            names.push_back(std::string(Tr("Clásico · ")) + n);
            values.push_back(d);
        }
        int cur = 0;
        for (size_t i = 0; i < values.size(); ++i)
            if (values[i] == settings_.renderer) cur = static_cast<int>(i);
        const std::string now = gl_ ? hw_.renderer : std::string(Tr("clásico (")) + (renderer_ ? SDL_GetRendererName(renderer_) : "?") + ")";
        RowBegin("Motor gráfico", Fmt("OpenGL dibuja todo con la tarjeta gráfica y deja que CD y 64 usen OpenGL o Vulkan. El clásico es el de "
                                      "respaldo. Se aplica al volver a abrir la app. Ahora: %s.", now.c_str()).c_str());
        if (ComboStyled("##motor", &cur, names, ImGui::GetContentRegionAvail().x)) {
            settings_.renderer = values[cur];
            changed = true;
            Toast("El motor gráfico cambia la próxima vez que abras Satronho Emulation.", GuideId::Troya, 4);
        }
        RowEnd();
        RowBegin("Tarjeta gráfica", hw_render_.VulkanAvailable() ? ("Vulkan: " + hw_render_.VulkanGpu()).c_str()
                                    : hw_render_.VulkanProbed() ? "Este equipo no tiene Vulkan: se usa OpenGL." : "Revisando…");
        {
            FontScope f(GetFonts().semibold, 14);
            ImGui::TextUnformatted(hw_.gpu.empty() ? "?" : hw_.gpu.c_str());
        }
        RowEnd();
    }

    static const float scales[] = {0.0f, 0.9f, 1.0f, 1.15f, 1.3f, 1.5f};
    int si = 0;
    for (int i = 0; i < 6; ++i)
        if (std::fabs(settings_.ui_scale - scales[i]) < 0.02f) si = i;
    RowBegin("Tamaño de la interfaz", "«Auto» la agranda en pantallas 1440p y 4K para que todo se lea cómodo.");
    if (Segmented("##ui", &si, {"Auto", "90 %", "100 %", "115 %", "130 %", "150 %"}, S(400))) {
        settings_.ui_scale = scales[si];
        UpdateScale();
        changed = true;
    }
    RowEnd();
    }
    BackToTop();
    ImGui::EndChild();
    if (changed) SaveConfig();
}


// ---------------------------------------------------------------- interfaz nativa (Android)

std::vector<std::pair<std::string, std::pair<std::string, std::string>>> NativeSystemList(bool only_gpu);

namespace {
// Lista de opciones del emulador como «opciones» de NPage (las conocidas, con su explicación).
void NativeCoreOptionRow(App* app, NPage& p, const SystemDef& sys, const CoreOption& d, const OptionInfo* info, const std::string& cur,
                         std::function<void(const std::string&)> set) {
    std::vector<std::string> labels;
    int sel = 0;
    for (size_t i = 0; i < d.values.size(); ++i) {
        const auto& val = d.values[i];
        const ValueInfo* vi = info ? FindValue(*info, val.value) : nullptr;
        std::string l = vi && vi->label ? std::string(Tr(vi->label)) : CommonValueLabel(val.value, val.label);
        if (val.value == d.core_def) l += std::string("  ") + Tr("(del emulador)");
        labels.push_back(l);
        if (val.value == cur) sel = static_cast<int>(i);
    }
    std::string help = info && info->help ? std::string(Tr(info->help)) : d.info;
    if (!info) {
        const size_t dot = help.find(". ");
        if (dot != std::string::npos && dot < 220) help.resize(dot + 1);
    }
    std::string note;
    if (info) {
        const ValueInfo* vi = FindValue(*info, d.values[sel].value);
        if (vi && vi->help) note = Tr(vi->help);
    }
    const std::vector<CoreOption::Value> values = d.values;
    p.Choice("op_" + d.key, info ? Tr(info->label) : d.label, help, sel, labels, [set, values](int v) {
        if (v >= 0 && v < static_cast<int>(values.size())) set(values[v].value);
    }, "", note);
    (void)app;
    (void)sys;
}
} // namespace

void App::NativePageGraphics(NPage& p) {
    if (gfx_sys_.empty()) gfx_sys_ = in_game_ && sys_ ? sys_->id : "cd";
    const SystemDef* sys = SystemById(gfx_sys_);
    if (!sys) sys = &Systems().front();
    p.Section("", "Elige la consola que quieres ajustar. Lo que cambies vale solo para ella.");
    p.Systems("consola", sys->id, NativeSystemList(false), [this](const std::string& v) {
        if (SystemById(v)) gfx_sys_ = v;
    });

    // Imagen.
    VideoOverride& vo = settings_.video_for[sys->id];
    const std::string sid = sys->id;
    p.Section("Imagen");
    p.Toggle("propia", Fmt("Imagen propia para %s", sys->name.c_str()), "Si está apagado, usa la imagen general (la de todas las consolas).", vo.on,
             [this, sid](bool v) {
                 VideoOverride& o = settings_.video_for[sid];
                 if (v && !o.on) {
                     o.scale_mode = settings_.scale_mode;
                     o.filter = settings_.filter;
                     o.effect = settings_.effect;
                     o.effect_strength = settings_.effect_strength;
                 }
                 o.on = v;
                 SaveConfig();
             }, "graficos");
    auto ref_int = [this, sid](int which) -> int& {
        VideoOverride& o = settings_.video_for[sid];
        if (which == 0) return o.on ? o.scale_mode : settings_.scale_mode;
        if (which == 1) return o.on ? o.filter : settings_.filter;
        return o.on ? o.effect : settings_.effect;
    };
    p.Choice("escala", "Tamaño", "Inteligente: pantalla ancha en las 3D y lo más grande sin deformar en las demás. Estirar todo: llena la pantalla aunque se deforme.",
             ref_int(0), {"Ajustar", "Entero", "Estirar todo", "Inteligente"}, [this, ref_int](int v) { ref_int(0) = std::clamp(v, 0, 3); SaveConfig(); });
    p.Choice("costados", "Costados", "Lo que llena los lados cuando el juego no ocupa toda la pantalla.", settings_.side_fill,
             {"Colores del juego", "Marco de Satronho"}, [this](int v) { settings_.side_fill = std::clamp(v, 0, 1); SaveConfig(); });
    p.Choice("filtro", "Filtro", "Cómo se agrandan los píxeles.", ref_int(1), {"Píxel exacto", "Nítido suave", "Suave"},
             [this, ref_int](int v) { ref_int(1) = std::clamp(v, 0, 2); SaveConfig(); });
    p.Choice("efecto", "Efecto de pantalla", "Auto: CRT en las de TV, LCD en portátiles y limpio en las 3D.", ref_int(2),
             {"Ninguno", "Líneas", "CRT", "LCD", "Auto"}, [this, ref_int](int v) { ref_int(2) = std::clamp(v, 0, 4); SaveConfig(); });
    if (ref_int(2) != 0) {
        const float st = vo.on ? vo.effect_strength : settings_.effect_strength;
        p.Slider("intensidad", "Intensidad del efecto", "", st * 100, 0, 100, 5, "%.0f %%", [this, sid](float v) {
            VideoOverride& o = settings_.video_for[sid];
            (o.on ? o.effect_strength : settings_.effect_strength) = std::clamp(v / 100.0f, 0.0f, 1.0f);
            SaveConfig();
        });
    }

    // Motor y resolución (consolas 3D).
    if (sys->gpu) {
        const int api_now = GpuApiFor(*sys);
        const int res_now = GpuResFor(*sys, api_now);
        p.Section("Motor y resolución", "Se aplica la próxima vez que abras un juego de esta consola.");
        int api = settings_.GpuApi(sys->id);
        if (!sys->gpu_cpu && api == 3) api = 0;
        std::vector<std::string> apis{"Automático", "OpenGL", "Vulkan"};
        if (sys->gpu_cpu) apis.push_back("Procesador");
        const char* api_name = api_now == 1 ? "OpenGL" : api_now == 2 ? "Vulkan" : "el procesador";
        p.Choice("api", "Motor del juego", TrFmt("Ahora: %s.", Tr(api_name)), api, apis, [this, sid](int v) {
            if (v == 2 && !hw_render_.VulkanAvailable()) Toast("Este equipo no tiene Vulkan; se usará OpenGL.", GuideId::Troya, 4);
            settings_.gpu_api[sid] = v;
            SaveConfig();
        }, "chip");
        const std::vector<int> ks = GpuResSteps(*sys, api_now);
        const int stored = settings_.GpuRes(sys->id);
        // La automática dice exactamente cuál usa (antes decía una y usaba otra).
        std::vector<std::string> names{TrFmt("Automática · %s", ResLabel(res_now, sys).c_str())};
        int sel = 0;
        for (size_t i = 0; i < ks.size(); ++i) {
            names.push_back(ResLabel(ks[i], sys));
            if (stored > 0 && ResHeight(ks[i]) <= ResHeight(stored)) sel = static_cast<int>(i) + 1;
        }
        std::string help = "La automática elige la que va fluida en tu equipo. Si un juego va lento, Ñoño te ofrece bajarla y la recuerda solo para ese juego.";
#ifdef __ANDROID__
        help = "En el celular la automática llega como mucho a 2× para que los juegos 3D vayan fluidos. Si un juego va lento, Ñoño te ofrece bajarla y la recuerda solo para ese juego.";
#endif
        p.Choice("res", "Resolución", help, sel, names, [this, sid, ks](int v) {
            settings_.gpu_res[sid] = v <= 0 || v > static_cast<int>(ks.size()) ? 0 : ks[v - 1];
            SaveConfig();
        }, "resolucion");
    }

    // Opciones de su emulador que tocan la imagen.
    if (const std::vector<CoreOption>* defs = CoreDefsFor(*sys)) {
        bool header = false;
        for (const auto& info : KnownOptions(sys->id)) {
            auto d = std::find_if(defs->begin(), defs->end(), [&](const CoreOption& o) { return o.key == info.key; });
            if (d == defs->end() || d->values.empty()) continue;
            if (!header) {
                p.Section("Opciones de su emulador", "Se aplican al momento o al volver a abrir el juego.");
                header = true;
            }
            const SystemDef* sp = sys;
            NativeCoreOptionRow(this, p, *sys, *d, &info, CoreOptionValue(*sys, *d),
                                [this, sp, key = d->key](const std::string& v) { SetCoreOptionFor(*sp, key, v); });
        }
    }

    p.Section("Para toda la app (todas las consolas)");
    p.Toggle("ambiente", "Fondo ambiente", "Llena los bordes con los colores del juego, suaves y desenfocados.", settings_.ambient,
             [this](bool v) { settings_.ambient = v; SaveConfig(); });
    p.Toggle("vsync", "Sincronización vertical", "Evita que la imagen se corte. Déjala activada salvo que sepas lo que haces.", settings_.vsync,
             [this](bool v) { settings_.vsync = v; ApplyVsync(); SaveConfig(); });
}

void App::NativePageCore(NPage& p) {
    const SystemDef* sys = SystemById(core_tab_system_);
    if (!sys) sys = &Systems().front();
    p.Section("");
    p.Systems("consola", sys->id, NativeSystemList(false), [this](const std::string& v) {
        if (SystemById(v)) core_tab_system_ = v;
    });
    const std::vector<CoreOption>* defs = CoreDefsFor(*sys);
    std::string name, version;
    if (core_.CoreLoaded() && core_file_ == CoreFile(*sys)) {
        name = core_.SystemInfo().library_name ? core_.SystemInfo().library_name : "";
        version = core_.SystemInfo().library_version ? core_.SystemInfo().library_version : "";
    } else if (auto it = core_cache_.find(CoreFile(*sys)); it != core_cache_.end()) {
        name = it->second.name;
        version = it->second.version;
    }
    p.Guide("samy", TrFmt("El corazón de %s es %s %s. Estas opciones son suyas; la mayoría se nota al momento y otras al reiniciar el juego.",
                          Tr(sys->full).c_str(), name.c_str(), version.c_str()));
    if (!sys->bios.empty()) {
        const std::vector<std::string> have = BiosFound(*sys);
        std::string status;
        if (have.empty()) status = Tr(sys->bios_required ? "Falta. " : "No hay (es opcional). ");
        else {
            status = Tr("Tienes: ");
            for (size_t i = 0; i < have.size(); ++i) status += (i ? ", " : "") + have[i];
            status += ". ";
        }
        std::string names;
        for (size_t i = 0; i < sys->bios.size(); ++i) names += std::string(i ? ", " : "") + sys->bios[i].name;
        p.Info(TrFmt("BIOS de %s", Tr(sys->full).c_str()), status + Tr(sys->bios_note) + " " + Tr("Nombres que reconoce: ") + names + ".", "chip");
        p.Info("Cómo agregarla", "Compártela desde WhatsApp, Drive o Archivos con «Abrir con» Satronho Emulation, o ponla en una carpeta de juegos: la reconozco sola.",
               "info");
    }
    if (!defs) {
        p.Info("Estas opciones aparecen cuando el emulador de esta consola está instalado.", "", "info");
        return;
    }
    std::vector<CoreCategory> cats;
    if (core_.CoreLoaded() && core_file_ == CoreFile(*sys)) cats = core_.Categories();
    else if (auto it = core_cache_.find(CoreFile(*sys)); it != core_cache_.end()) cats = it->second.categories;
    const SystemDef* sp = sys;
    auto row = [&](const CoreOption& o) {
        if (!o.visible || o.values.size() < 2) return;
        NativeCoreOptionRow(this, p, *sys, o, nullptr, CoreOptionValue(*sys, o),
                            [this, sp, key = o.key](const std::string& v) { SetCoreOptionFor(*sp, key, v); });
    };
    if (cats.empty()) {
        p.Section("Opciones");
        for (const auto& o : *defs) row(o);
    } else {
        for (const auto& c : cats) {
            bool any = false;
            for (const auto& o : *defs) any |= o.category == c.key && o.visible && o.values.size() >= 2;
            if (!any) continue;
            p.Section(c.label);
            for (const auto& o : *defs)
                if (o.category == c.key) row(o);
        }
        bool header = false;
        for (const auto& o : *defs) {
            if (!o.category.empty() || !o.visible || o.values.size() < 2) continue;
            if (!header) {
                p.Section("General");
                header = true;
            }
            row(o);
        }
    }
    p.Section("");
    std::vector<std::pair<std::string, std::string>> defaults;
    for (const auto& o : *defs) defaults.push_back({o.key, o.def});
    p.Action("restablecer", "Restablecer opciones del núcleo", "", "refrescar", [this, sp, defaults] {
        for (const auto& [k, v] : defaults) SetCoreOptionFor(*sp, k, v);
    }, "", "¿Volver a las opciones de fábrica de este emulador?");
}

} // namespace Satronho
