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
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.border
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.Button
import androidx.compose.material3.TextButton
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
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.input.pointer.PointerId
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.graphics.toArgb
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
    val ajustes = SatronhoUi.pilaAjustes.isNotEmpty()
    Box(Modifier.fillMaxSize()) {
        e.medidor?.let { if (!ajustes) LineaMedidor(it) }
        if (e.chip.isNotEmpty() && !ajustes) ChipEstado(e.chip)
        if (e.tactil.editar) EditorTactil(e.tactil, juego)
        // «Automático» (modo 0): con un mando Bluetooth o USB conectado, el control en pantalla se esconde,
        // aunque el mando todavía no le haya llegado a SDL. Con «Siempre» (1) se sigue viendo.
        else if (e.tactil.visible && !(e.tactil.modo == 0 && SatronhoUi.mandoFisico) && !juego.menu && !ajustes) ControlTactil(e.tactil, juego)
        CajonJuego(juego, e.tactil)
    }
}

// ------------------------------------------------------------------ medidor y estado

/** El medidor: una línea fina en una esquina (como el de NX), con el color elegido. */
@Composable
private fun LineaMedidor(m: Medidor) {
    val alinear = when (m.esquina) {
        0 -> Alignment.TopStart
        2 -> Alignment.BottomStart
        3 -> Alignment.BottomEnd
        else -> Alignment.TopEnd
    }
    Box(Modifier.fillMaxSize().windowInsetsPadding(WindowInsets.safeDrawing).padding(horizontal = 10.dp, vertical = 4.dp), contentAlignment = alinear) {
        Row(
            Modifier.clip(RoundedCornerShape(10.dp)).background(Color.Black.copy(alpha = 0.45f)).padding(horizontal = 8.dp, vertical = 2.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Box(Modifier.size(7.dp).clip(CircleShape).background(if (m.lento) Color(0xFFFF9F43) else m.color))
            Spacer(Modifier.width(6.dp))
            Text(m.texto, color = if (m.lento) Color(0xFFFF9F43) else Color.White.copy(alpha = 0.9f), fontSize = 11.sp,
                fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace)
        }
    }
}

/** «En pausa», «Rebobinando»… arriba al centro. */
@Composable
private fun ChipEstado(texto: String) {
    Box(Modifier.fillMaxSize().windowInsetsPadding(WindowInsets.safeDrawing).padding(top = 22.dp), contentAlignment = Alignment.TopCenter) {
        Surface(shape = RoundedCornerShape(18.dp), color = MaterialTheme.colorScheme.surface.copy(alpha = 0.92f),
            modifier = Modifier.border(1.dp, MaterialTheme.colorScheme.primary.copy(alpha = 0.6f), RoundedCornerShape(18.dp))) {
            Text(texto, style = MaterialTheme.typography.labelLarge, modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp))
        }
    }
}

// ------------------------------------------------------------------ mover los botones

/** Editor del control: arrastra cada grupo (cruceta, botones, gatillos…) a donde te quede cómodo. */
@Composable
private fun EditorTactil(tc: Tactil, j: JuegoActual) {
    val fuente = letras(LocalContext.current.assets)
    val medidor = rememberTextMeasurer()
    val densidad = LocalDensity.current
    val tcActual by rememberUpdatedState(tc)
    var grupo by remember { mutableStateOf("") }
    var corrido by remember { mutableStateOf(Offset.Zero) }
    Box(Modifier.fillMaxSize().background(Color.Black.copy(alpha = 0.35f))) {
        Canvas(
            Modifier.fillMaxSize().pointerInput(Unit) {
                detectDragGestures(
                    onDragStart = { p ->
                        val W = size.width.toFloat()
                        val H = size.height.toFloat()
                        var mejor = ""
                        var mejorD = Float.MAX_VALUE
                        for (b in tcActual.botones) {
                            val d = hypot(p.x - b.x * W, p.y - b.y * H) / (b.r * H * 1.6f)
                            if (d < 1f && d < mejorD) { mejorD = d; mejor = b.grupo }
                        }
                        grupo = mejor
                        corrido = Offset.Zero
                    },
                    onDrag = { c, d -> if (grupo.isNotEmpty()) { c.consume(); corrido += d } },
                    onDragEnd = {
                        if (grupo.isNotEmpty() && corrido != Offset.Zero) {
                            val W = size.width.toFloat()
                            val H = size.height.toFloat()
                            SatronhoUi.orden("tactil_mover|$grupo|${corrido.x / W}|${corrido.y / H}")
                        }
                        grupo = ""
                        corrido = Offset.Zero
                    },
                    onDragCancel = { grupo = ""; corrido = Offset.Zero },
                )
            },
        ) {
            val op = tc.opacidad.coerceIn(0.5f, 1f)
            for (b in tc.botones) {
                val mover = if (b.grupo == grupo) corrido else Offset.Zero
                val bm = b.copy(x = b.x + mover.x / size.width, y = b.y + mover.y / size.height)
                botonTactil(Lienzo(this, medidor, fuente), bm.id, bm.etiqueta, bm.grupo, V(bm.x * size.width, bm.y * size.height), bm.r * size.height,
                    j.sistema, j.color.toArgb(), if (b.grupo == grupo) -1 else 0, 0f, 0f, op)
            }
        }
        Column(Modifier.align(Alignment.TopCenter).windowInsetsPadding(WindowInsets.safeDrawing).padding(top = 12.dp),
            horizontalAlignment = Alignment.CenterHorizontally) {
            Surface(shape = RoundedCornerShape(16.dp), color = MaterialTheme.colorScheme.surface) {
                Row(Modifier.padding(horizontal = 12.dp, vertical = 6.dp), verticalAlignment = Alignment.CenterVertically) {
                    Text(t("Arrastra cada grupo de botones"), style = MaterialTheme.typography.bodyMedium)
                    Spacer(Modifier.width(10.dp))
                    TextButton(onClick = { SatronhoUi.orden("tactil_restablecer") }) { Text(t("Restablecer")) }
                    Button(onClick = { SatronhoUi.orden("tactil_listo") }) { Text(t("Listo")) }
                }
            }
        }
    }
}

// ------------------------------------------------------------------ control en pantalla

private class Dedo(var dueno: Int, var x: Float, var y: Float, var dir: Int = -1)

/**
 * El control en pantalla nativo, encima del juego y semitransparente (como NX). Los botones y su
 * lugar vienen de C++ (el mismo diseño de cada consola, con lo que la persona movió en el editor).
 */
@Composable
private fun ControlTactil(tc: Tactil, j: JuegoActual) {
    val fuente = letras(LocalContext.current.assets)
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
            val pal = if (palancaId == i) palanca else Offset.Zero
            botonTactil(Lienzo(this, medidor, fuente), b.id, b.etiqueta, b.grupo, V(b.x * size.width, b.y * size.height), b.r * size.height,
                j.sistema, j.color.toArgb(), apretados, pal.x, pal.y, op)
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

// ------------------------------------------------------------------ cajón del juego (como NX)

@Composable
private fun CajonJuego(j: JuegoActual, tc: Tactil) {
    val abierto = j.menu
    var confirmarSalir by remember { mutableStateOf(false) }
    LaunchedEffect(abierto) {
        if (!abierto) {
            confirmarSalir = false
            if (SatronhoUi.pilaAjustes.isEmpty()) SatronhoUi.permitirFoco(false)
        }
    }

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
                Row(Modifier.padding(horizontal = 12.dp), verticalAlignment = Alignment.CenterVertically) {
                    Logo(28.dp)
                    Spacer(Modifier.width(8.dp))
                    Text("Satronho Emulation", style = MaterialTheme.typography.labelMedium, color = MaterialTheme.colorScheme.primary)
                }
                Spacer(Modifier.height(10.dp))
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
                Item(Iconos.ajustesJuego, t("Ajustes de este juego")) { SatronhoUi.abrirAjustes("juego", j.clave) }
                Item(Iconos.tactil, t("Control en pantalla")) { SatronhoUi.abrirAjustes("tactil") }
                Item(Icons.Filled.Settings, t("Ajustes")) { SatronhoUi.abrirAjustes() }
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
