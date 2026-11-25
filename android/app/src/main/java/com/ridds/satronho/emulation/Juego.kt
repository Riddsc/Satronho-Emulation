// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
package com.ridds.satronho.emulation

import android.view.HapticFeedbackConstants
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationDrawerItem
import androidx.compose.material3.NavigationDrawerItemDefaults
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.input.pointer.PointerId
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.atan2
import kotlin.math.hypot

// Botones del RetroPad (libretro.h).
private const val ARRIBA = 4
private const val ABAJO = 5
private const val IZQUIERDA = 6
private const val DERECHA = 7

@Composable
fun PantallaJuego(e: Estado) {
    val juego = e.juego ?: return
    Box(Modifier.fillMaxSize()) {
        if (e.tactil.visible && !juego.menu) ControlTactil(e.tactil)
        CajonJuego(juego, e.tactil)
    }
}

// ------------------------------------------------------------------ control en pantalla

private class Dedo(var dueno: Int, var x: Float, var y: Float, var dir: Int = -1)

/**
 * El control en pantalla nativo, encima del juego y semitransparente (como NX). Los botones y su
 * lugar vienen de C++ (el mismo diseño de cada consola, con lo que la persona movió en el editor).
 */
@Composable
private fun ControlTactil(tc: Tactil) {
    val vista = LocalView.current
    val medidor = rememberTextMeasurer()
    val densidad = LocalDensity.current
    val tcActual by rememberUpdatedState(tc)
    // Lo que se dibuja apretado (se actualiza con cada toque).
    var apretados by remember { mutableStateOf(0) }
    var palanca by remember { mutableStateOf(Offset.Zero) }
    var palancaId by remember { mutableStateOf(-99) }

    Canvas(
        Modifier.fillMaxSize().pointerInput(Unit) {
            val dedos = HashMap<PointerId, Dedo>()
            var antes = 0
            var antesL = Offset.Zero
            var antesR = Offset.Zero
            awaitPointerEventScope {
                while (true) {
                    val ev = awaitPointerEvent()
                    val W = size.width.toFloat()
                    val H = size.height.toFloat()
                    val bs = tcActual.botones
                    for (c in ev.changes) {
                        if (c.pressed && !dedos.containsKey(c.id)) {
                            // Un dedo nuevo toma el botón más cercano dentro de su zona.
                            var mejor = -1
                            var mejorD = Float.MAX_VALUE
                            bs.forEachIndexed { i, b ->
                                val bx = b.x * W
                                val by = b.y * H
                                val r = b.r * H
                                val zona = r * (if (b.id <= -2) 1.25f else 1.35f) * (if (b.grupo.startsWith("gat") || b.grupo == "centro") 1.3f else 1f)
                                val d = hypot(c.position.x - bx, c.position.y - by) / zona
                                if (d < 1f && d < mejorD) {
                                    mejorD = d
                                    mejor = i
                                }
                            }
                            dedos[c.id] = Dedo(mejor, c.position.x, c.position.y)
                            if (mejor >= 0 && bs[mejor].id == -1) SatronhoUi.orden("menu|1")
                        } else if (c.pressed) {
                            dedos[c.id]?.let { it.x = c.position.x; it.y = c.position.y }
                        } else {
                            dedos.remove(c.id)
                        }
                        c.consume()
                    }
                    // Qué está apretado ahora.
                    var bits = 0
                    var l = Offset.Zero
                    var r = Offset.Zero
                    var stickDueno = -99
                    for (d in dedos.values) {
                        if (d.dueno < 0 || d.dueno >= bs.size) continue
                        val b = bs[d.dueno]
                        val bx = b.x * W
                        val by = b.y * H
                        val rad = b.r * H
                        val dx = d.x - bx
                        val dy = d.y - by
                        when (b.id) {
                            -1 -> {}
                            -2 -> bits = bits or cruceta(d, dx, dy, rad, tcActual.solo4)
                            -3 -> {
                                var x = (dx / rad).coerceIn(-1f, 1f)
                                var y = (dy / rad).coerceIn(-1f, 1f)
                                val mag = hypot(x, y)
                                if (mag < 0.15f) { x = 0f; y = 0f } else {
                                    if (abs(x) < mag * 0.38f) x = 0f
                                    if (abs(y) < mag * 0.38f) y = 0f
                                }
                                if (b.grupo == "der") r = Offset(x, y) else l = Offset(x, y)
                                stickDueno = d.dueno
                            }
                            else -> if (b.id in 0..15) bits = bits or (1 shl b.id)
                        }
                    }
                    if (bits != antes || l != antesL || r != antesR) {
                        // Vibración corta al apretar algo nuevo.
                        if (tcActual.vibrar && (bits and antes.inv()) != 0) {
                            vista.performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
                        }
                        antes = bits
                        antesL = l
                        antesR = r
                        try { Puente.tactil(bits, l.x, l.y, r.x, r.y) } catch (_: Throwable) {}
                        apretados = bits
                    }
                    palanca = if (l != Offset.Zero) l else r
                    palancaId = stickDueno
                }
            }
        },
    ) {
        val op = tc.opacidad.coerceIn(0.15f, 1f)
        tc.botones.forEachIndexed { i, b ->
            dibujarBoton(b, apretados, if (palancaId == i) palanca else Offset.Zero, op, medidor, densidad.density)
        }
    }
    // Al salir del juego o esconder el control, nada queda apretado.
    DisposableEffect(Unit) {
        onDispose { try { Puente.tactil(0, 0f, 0f, 0f, 0f) } catch (_: Throwable) {} }
    }
}

/**
 * Cruceta: arriba, abajo, izquierda y derecha tienen zonas anchas (72°) y las diagonales angostas
 * (18°), así apretar «arriba» no marca también un lado. Centro muerto más grande (30 % del radio) y,
 * para cambiar de dirección, hay que pasarse un poco del borde (no tiembla entre dos vecinas).
 * Con «Solo 4 direcciones» no hay diagonales.
 */
private fun cruceta(d: Dedo, dx: Float, dy: Float, radio: Float, solo4: Boolean): Int {
    if (hypot(dx, dy) < radio * 0.30f) {
        d.dir = -1
        return 0
    }
    var grados = (atan2(dy, dx) * 180.0 / PI).toFloat() // 0 derecha, 90 abajo
    if (grados < 0) grados += 360f
    fun mitad(dir: Int) = if (solo4) (if (dir % 2 == 0) 45f else 0f) else (if (dir % 2 == 0) 36f else 9f)
    fun distancia(a: Float, b: Float): Float {
        val x = abs(a - b)
        return if (x > 180f) 360f - x else x
    }
    var dir = -1
    if (d.dir >= 0 && mitad(d.dir) > 0f && distancia(grados, d.dir * 45f) <= mitad(d.dir) + 6f) dir = d.dir
    var k = 0
    while (dir < 0 && k < 8) {
        if (mitad(k) > 0f && distancia(grados, k * 45f) <= mitad(k)) dir = k
        k++
    }
    if (dir < 0) dir = ((Math.round(grados / 90f) * 2) and 7)
    d.dir = dir
    return when (dir) {
        0 -> 1 shl DERECHA
        1 -> (1 shl DERECHA) or (1 shl ABAJO)
        2 -> 1 shl ABAJO
        3 -> (1 shl ABAJO) or (1 shl IZQUIERDA)
        4 -> 1 shl IZQUIERDA
        5 -> (1 shl IZQUIERDA) or (1 shl ARRIBA)
        6 -> 1 shl ARRIBA
        else -> (1 shl ARRIBA) or (1 shl DERECHA)
    }
}

/** Figuras de PlayStation y nombres largos en corto. */
private fun rotulo(s: String): String = when (s) {
    "Triángulo" -> "△"
    "Círculo" -> "○"
    "Cruz" -> "✕"
    "Cuadrado" -> "□"
    "Select" -> "SELECT"
    "Start" -> "START"
    "C arriba" -> "C▲"
    "C abajo" -> "C▼"
    "C izquierda" -> "C◀"
    "C derecha" -> "C▶"
    "A turbo" -> "A⚡"
    "B turbo" -> "B⚡"
    else -> s
}

private fun DrawScope.dibujarBoton(
    b: BotonTactil, apretados: Int, palanca: Offset, op: Float,
    medidor: androidx.compose.ui.text.TextMeasurer, dens: Float,
) {
    val W = size.width
    val H = size.height
    val c = Offset(b.x * W, b.y * H)
    val r = b.r * H
    val relleno = Color(0xFF1E1E24).copy(alpha = 0.42f * op)
    val rellenoApretado = Color.White.copy(alpha = 0.30f * op)
    val borde = Color.White.copy(alpha = 0.38f * op)
    val textoColor = Color.White.copy(alpha = 0.85f * op)
    val trazo = Stroke(width = 1.5f * dens)
    fun texto(s: String, centro: Offset, alto: Float) {
        if (s.isEmpty()) return
        val estilo = TextStyle(color = textoColor, fontSize = (alto / dens / 1.25f).sp, fontWeight = FontWeight.SemiBold)
        val m = medidor.measure(s, estilo, maxLines = 1, overflow = TextOverflow.Clip)
        drawText(m, topLeft = Offset(centro.x - m.size.width / 2f, centro.y - m.size.height / 2f))
    }
    when {
        b.id == -2 -> {
            // Cruceta en forma de cruz (como NX), con el brazo apretado iluminado.
            val brazo = r * 0.62f
            val largo = r * 1.9f
            val cr = CornerRadius(brazo * 0.25f)
            drawRoundRect(relleno, Offset(c.x - largo / 2, c.y - brazo / 2), Size(largo, brazo), cr)
            drawRoundRect(relleno, Offset(c.x - brazo / 2, c.y - largo / 2), Size(brazo, largo), cr)
            drawRoundRect(borde, Offset(c.x - largo / 2, c.y - brazo / 2), Size(largo, brazo), cr, style = trazo)
            drawRoundRect(borde, Offset(c.x - brazo / 2, c.y - largo / 2), Size(brazo, largo), cr, style = trazo)
            fun luz(bit: Int, o: Offset, s: Size) {
                if (apretados and (1 shl bit) != 0) drawRoundRect(rellenoApretado, o, s, cr)
            }
            luz(ARRIBA, Offset(c.x - brazo / 2, c.y - largo / 2), Size(brazo, largo / 2 - brazo / 2))
            luz(ABAJO, Offset(c.x - brazo / 2, c.y + brazo / 2), Size(brazo, largo / 2 - brazo / 2))
            luz(IZQUIERDA, Offset(c.x - largo / 2, c.y - brazo / 2), Size(largo / 2 - brazo / 2, brazo))
            luz(DERECHA, Offset(c.x + brazo / 2, c.y - brazo / 2), Size(largo / 2 - brazo / 2, brazo))
        }
        b.id == -3 -> {
            drawCircle(relleno, r, c)
            drawCircle(borde, r, c, style = trazo)
            val cabeza = Offset(c.x + palanca.x * r * 0.5f, c.y + palanca.y * r * 0.5f)
            drawCircle(Color.White.copy(alpha = (if (palanca != Offset.Zero) 0.35f else 0.22f) * op), r * 0.55f, cabeza)
            drawCircle(borde, r * 0.55f, cabeza, style = trazo)
        }
        b.id == -1 -> {
            // Menú: tres rayitas.
            drawCircle(relleno, r, c)
            drawCircle(borde, r, c, style = trazo)
            val w = r * 0.9f
            for (k in -1..1) drawLine(textoColor, Offset(c.x - w / 2, c.y + k * r * 0.32f), Offset(c.x + w / 2, c.y + k * r * 0.32f), 2f * dens)
        }
        b.grupo.startsWith("gat") || b.grupo == "centro" -> {
            // Gatillos y Select/Start: pastillas.
            val ancho = r * (if (b.grupo == "centro") 2.4f else 2.8f)
            val alto = r * (if (b.grupo == "centro") 1.1f else 1.35f)
            val o = Offset(c.x - ancho / 2, c.y - alto / 2)
            val cr = CornerRadius(alto / 2)
            val on = b.id in 0..15 && (apretados and (1 shl b.id)) != 0
            drawRoundRect(if (on) rellenoApretado else relleno, o, Size(ancho, alto), cr)
            drawRoundRect(borde, o, Size(ancho, alto), cr, style = trazo)
            texto(rotulo(b.etiqueta), c, alto * (if (b.grupo == "centro") 0.42f else 0.6f))
        }
        else -> {
            val on = b.id in 0..15 && (apretados and (1 shl b.id)) != 0
            drawCircle(if (on) rellenoApretado else relleno, r, c)
            drawCircle(borde, r, c, style = trazo)
            val s = rotulo(b.etiqueta)
            texto(s, c, r * (if (s.length > 2) 0.62f else 0.95f))
        }
    }
}

// ------------------------------------------------------------------ cajón del juego (como NX)

@Composable
private fun CajonJuego(j: JuegoActual, tc: Tactil) {
    val abierto = j.menu
    var confirmarSalir by remember { mutableStateOf(false) }
    LaunchedEffect(abierto) { if (!abierto) confirmarSalir = false }

    AnimatedVisibility(visible = abierto, enter = fadeIn(), exit = fadeOut()) {
        Box(
            Modifier.fillMaxSize().background(Color.Black.copy(alpha = 0.45f))
                .clickable(interactionSource = remember { MutableInteractionSource() }, indication = null) { SatronhoUi.orden("menu|0") },
        )
    }
    AnimatedVisibility(visible = abierto, enter = slideInHorizontally { -it }, exit = slideOutHorizontally { -it }) {
        Surface(
            color = MaterialTheme.colorScheme.surface,
            shape = RoundedCornerShape(topEnd = 18.dp, bottomEnd = 18.dp),
            modifier = Modifier.fillMaxHeight().width(300.dp),
        ) {
            Column(
                Modifier.windowInsetsPadding(WindowInsets.safeDrawing).verticalScroll(rememberScrollState())
                    .padding(horizontal = 12.dp, vertical = 16.dp),
            ) {
                Text(j.titulo, style = MaterialTheme.typography.titleLarge, maxLines = 2, overflow = TextOverflow.Ellipsis,
                    modifier = Modifier.padding(horizontal = 12.dp))
                Text(
                    listOf(j.consola, j.nucleo).filter { it.isNotEmpty() }.joinToString(" · "),
                    style = MaterialTheme.typography.bodySmall, color = j.color, modifier = Modifier.padding(horizontal = 12.dp),
                )
                Spacer(Modifier.height(12.dp))
                Item(if (j.pausa) Icons.Filled.PlayArrow else Iconos.pausa, t(if (j.pausa) "Continuar el juego" else "Pausar")) {
                    SatronhoUi.orden("pausa")
                }
                Item(Iconos.guardar, t("Guardar partida") + " · " + t("ranura") + " ${j.ranura}") {
                    SatronhoUi.orden("guardar|${j.ranura}")
                    SatronhoUi.orden("menu|0")
                }
                Item(Iconos.cargar, t("Cargar partida") + " · " + t("ranura") + " ${j.ranura}") {
                    SatronhoUi.orden("cargar|${j.ranura}")
                    SatronhoUi.orden("menu|0")
                }
                Item(Icons.Filled.Refresh, t("Reiniciar el juego")) {
                    SatronhoUi.orden("reiniciar")
                    SatronhoUi.orden("menu|0")
                }
                HorizontalDivider(Modifier.padding(vertical = 8.dp))
                val textoControl = when (tc.modo) {
                    1 -> t("Control en pantalla: siempre")
                    2 -> t("Control en pantalla: nunca")
                    else -> t("Control en pantalla: automático")
                }
                Item(Iconos.mando, textoControl) { SatronhoUi.orden("tactil|${(tc.modo + 1) % 3}") }
                Item(Iconos.mando, t("Mover los botones")) { SatronhoUi.orden("tactil_editar") }
                Item(Iconos.ajustesJuego, t("Ajustes de este juego")) {
                    SatronhoUi.orden("menu|0")
                    SatronhoUi.orden("ajustes_juego|${j.clave}")
                }
                Item(Icons.Filled.Settings, t("Ajustes")) {
                    SatronhoUi.orden("menu|0")
                    SatronhoUi.orden("pagina|ajustes")
                }
                HorizontalDivider(Modifier.padding(vertical = 8.dp))
                Item(Iconos.salir, if (confirmarSalir) t("Toca otra vez para salir") else t("Salir del juego"), peligro = true) {
                    if (confirmarSalir) SatronhoUi.orden("salir_juego") else confirmarSalir = true
                }
            }
        }
    }
}

@Composable
private fun Item(icono: ImageVector, texto: String, peligro: Boolean = false, alTocar: () -> Unit) {
    val color = if (peligro) Color(0xFFFF5D73) else MaterialTheme.colorScheme.onSurface
    NavigationDrawerItem(
        label = { Text(texto, color = color) },
        icon = { Icon(icono, null, tint = if (peligro) color else MaterialTheme.colorScheme.primary) },
        selected = false,
        onClick = alTocar,
        colors = NavigationDrawerItemDefaults.colors(unselectedContainerColor = Color.Transparent),
        modifier = Modifier.padding(vertical = 1.dp),
    )
}
