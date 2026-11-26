// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
package com.ridds.satronho.emulation

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.core.animateFloat
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.delay

/** «Cargando…»: una tarjeta chica con la portada, el nombre y una barra (como NX). */
/**
 * Pantalla de carga (build 27): el cartucho (o el disco) del juego baja y entra en la ranura de su
 * consola Satronho, se ilumina con el color de la consola y un guía (según el reparto) acompaña con
 * un consejo. Abajo, el nombre, la consola y el avance.
 */
@Composable
fun TarjetaCarga(c: Cargando) {
    val cs = MaterialTheme.colorScheme
    val b = SatronhoUi.biblioteca
    val consejo = remember(c.titulo) { if (b.consejos.isEmpty()) null else b.consejos.getOrNull(elegirConsejo(b, -1)) }
    // Entrada del cartucho: baja desde arriba y rebota un poquito al entrar; después, un brillo.
    val baja = remember(c.titulo) { androidx.compose.animation.core.Animatable(-1f) }
    val brillo = remember(c.titulo) { androidx.compose.animation.core.Animatable(0f) }
    LaunchedEffect(c.titulo) {
        baja.animateTo(0f, androidx.compose.animation.core.spring(dampingRatio = 0.55f, stiffness = 220f))
        brillo.animateTo(1f, androidx.compose.animation.core.tween(260))
        brillo.animateTo(0.35f, androidx.compose.animation.core.tween(700))
    }
    val respira = androidx.compose.animation.core.rememberInfiniteTransition(label = "carga")
    val salto by respira.animateFloat(
        0f, 1f, androidx.compose.animation.core.infiniteRepeatable(
            androidx.compose.animation.core.tween(900), androidx.compose.animation.core.RepeatMode.Reverse,
        ), label = "salto",
    )
    Box(
        Modifier.fillMaxSize().background(
            androidx.compose.ui.graphics.Brush.radialGradient(listOf(c.color.copy(alpha = 0.28f), Color.Black.copy(alpha = 0.86f)))
        ),
        contentAlignment = Alignment.Center,
    ) {
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            Box(Modifier.size(width = 260.dp, height = 190.dp), contentAlignment = Alignment.BottomCenter) {
                // El cartucho, que entra en la ranura.
                Box(
                    Modifier.align(Alignment.TopCenter).padding(top = 6.dp).size(width = 104.dp, height = 128.dp)
                        .graphicsLayer { translationY = baja.value * 170.dp.toPx() },
                ) {
                    Portada(c.portada, c.titulo, c.color, Modifier.fillMaxSize(), 12, tono = c.tono, forma = c.forma, etiqueta = c.etiqueta)
                }
                // La consola: una ranura con el color de la consola y su nombre Satronho.
                Box(
                    Modifier.size(width = 240.dp, height = 74.dp).clip(RoundedCornerShape(18.dp))
                        .background(androidx.compose.ui.graphics.Brush.verticalGradient(listOf(cs.surface, cs.surfaceVariant)))
                        .border(2.dp, c.color.copy(alpha = 0.35f + 0.65f * brillo.value), RoundedCornerShape(18.dp)),
                    contentAlignment = Alignment.Center,
                ) {
                    Box(
                        Modifier.align(Alignment.TopCenter).padding(top = 8.dp).size(width = 128.dp, height = 8.dp)
                            .clip(RoundedCornerShape(4.dp)).background(c.color.copy(alpha = 0.25f + 0.75f * brillo.value)),
                    )
                    Row(Modifier.padding(top = 14.dp), verticalAlignment = Alignment.CenterVertically) {
                        Logo(26.dp)
                        Spacer(Modifier.width(6.dp))
                        Text(c.consola, style = MaterialTheme.typography.labelLarge.copy(fontWeight = FontWeight.Bold), color = c.color, maxLines = 1)
                    }
                }
                // El guía, al lado de la consola, saltando un poquito.
                if (consejo != null) {
                    Avatar(
                        consejo.guia, 54.dp,
                        Modifier.align(Alignment.BottomEnd).graphicsLayer { translationY = -salto * 6.dp.toPx() }
                            .border(2.dp, Guias.color(consejo.guia), androidx.compose.foundation.shape.CircleShape),
                    )
                }
            }
            Spacer(Modifier.height(14.dp))
            Text(c.titulo, style = MaterialTheme.typography.titleMedium.copy(fontWeight = FontWeight.Bold), maxLines = 1, overflow = TextOverflow.Ellipsis,
                modifier = Modifier.widthIn(max = 420.dp))
            Text(t("Cargando…"), style = MaterialTheme.typography.labelSmall, color = cs.onSurfaceVariant)
            Spacer(Modifier.height(8.dp))
            Box(Modifier.width(240.dp)) {
                if (c.avance >= 0f) {
                    Column(horizontalAlignment = Alignment.CenterHorizontally) {
                        LinearProgressIndicator(progress = { c.avance }, color = c.color, modifier = Modifier.fillMaxWidth().height(6.dp).clip(RoundedCornerShape(3.dp)))
                        TextButton(onClick = { SatronhoUi.orden("cancelar_descompresion") }) { Text(t("Cancelar")) }
                    }
                } else {
                    LinearProgressIndicator(color = c.color, modifier = Modifier.fillMaxWidth().height(6.dp).clip(RoundedCornerShape(3.dp)))
                }
            }
        }
        // El consejo del guía mientras carga (como la pantalla de carga de Satronho NX).
        if (consejo != null) {
            Surface(
                shape = RoundedCornerShape(14.dp), color = cs.surface.copy(alpha = 0.94f), tonalElevation = 2.dp,
                modifier = Modifier.align(Alignment.BottomCenter).padding(18.dp).widthIn(max = 520.dp).fillMaxWidth(),
            ) { Box(Modifier.padding(12.dp)) { GuiaDice(consejo.guia, consejo.texto) } }
        }
    }
}

/**
 * Portada de un juego. Si todavía no hay carátula, el cartucho o la caja de disco de su consola con
 * una etiqueta del color del juego (como en PC).
 */
@Composable
fun Portada(
    ruta: String, titulo: String, color: Color, modifier: Modifier, radio: Int = 12, lado: Int = 360,
    tono: Int = 0, forma: String = "nes", etiqueta: String = "",
) {
    val img = Imagenes.cargar(ruta, lado)
    val medidor = rememberTextMeasurer()
    val fuente = letras(LocalContext.current.assets)
    Box(modifier.clip(RoundedCornerShape(radio.dp)).background(color.copy(alpha = 0.22f)), contentAlignment = Alignment.Center) {
        if (img != null) {
            Image(img, contentDescription = titulo, contentScale = ContentScale.Crop, modifier = Modifier.fillMaxSize())
        } else {
            Canvas(Modifier.fillMaxSize()) {
                cartucho(Lienzo(this, medidor, fuente), 0f, 0f, size.width, size.height, tono, titulo, forma, etiqueta, radio.dp.toPx())
            }
        }
    }
}

/** Avisos cortos de los guías: de a uno, arriba al centro, y se van solos. */
@Composable
fun CapaAvisos(enJuego: Boolean) {
    val lista = SatronhoUi.avisos
    var ahora by remember { mutableLongStateOf(System.currentTimeMillis()) }
    LaunchedEffect(lista.size) {
        while (lista.isNotEmpty()) {
            ahora = System.currentTimeMillis()
            lista.removeAll { ahora - it.nacido > (it.segundos * 1000).toLong() + 300 }
            delay(250)
        }
    }
    val actual = lista.lastOrNull()
    Box(Modifier.fillMaxSize().windowInsetsPadding(WindowInsets.safeDrawing), contentAlignment = Alignment.TopCenter) {
        AnimatedVisibility(visible = actual != null, enter = fadeIn() + slideInVertically(), exit = fadeOut()) {
            if (actual != null) {
                Surface(
                    shape = RoundedCornerShape(22.dp),
                    color = MaterialTheme.colorScheme.surface.copy(alpha = 0.96f),
                    modifier = Modifier
                        .padding(top = if (enJuego) 28.dp else 12.dp, start = 16.dp, end = 16.dp)
                        .widthIn(max = 460.dp)
                        .border(1.dp, Guias.color(actual.guia).copy(alpha = 0.5f), RoundedCornerShape(22.dp)),
                ) {
                    Row(Modifier.padding(horizontal = 12.dp, vertical = 8.dp), verticalAlignment = Alignment.CenterVertically) {
                        Avatar(actual.guia, 30.dp)
                        Spacer(Modifier.width(10.dp))
                        Column {
                            Text(Guias.nombre(actual.guia), style = MaterialTheme.typography.labelMedium.copy(fontWeight = FontWeight.Bold),
                                color = Guias.color(actual.guia))
                            Text(actual.texto, style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurface)
                        }
                    }
                }
            }
        }
    }
}

/**
 * Tarjeta de un guía con botones («Ñoño tiene una idea»). Una sola a la vez: en la biblioteca, al
 * centro; en el juego, en la esquina de arriba a la izquierda para no tapar la imagen.
 */
@Composable
fun TarjetaGuia(t: Tarjeta, enJuego: Boolean) {
    val alinear = if (enJuego) Alignment.TopStart else Alignment.Center
    Box(
        Modifier.fillMaxSize().windowInsetsPadding(WindowInsets.safeDrawing).padding(16.dp),
        contentAlignment = alinear,
    ) {
        Surface(
            shape = RoundedCornerShape(18.dp),
            color = MaterialTheme.colorScheme.surface,
            tonalElevation = 4.dp,
            shadowElevation = 8.dp,
            modifier = Modifier.widthIn(max = if (enJuego) 360.dp else 420.dp)
                .border(1.dp, Guias.color(t.guia).copy(alpha = 0.5f), RoundedCornerShape(18.dp)),
        ) {
            Column(Modifier.padding(16.dp)) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Avatar(t.guia, 36.dp)
                    Spacer(Modifier.width(10.dp))
                    Text(t.titulo, style = MaterialTheme.typography.titleMedium.copy(fontWeight = FontWeight.Bold), color = Guias.color(t.guia))
                }
                Spacer(Modifier.height(10.dp))
                Text(t.texto, style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurface)
                Spacer(Modifier.height(6.dp))
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.End) {
                    for ((id, texto) in t.botones) {
                        TextButton(onClick = {
                            SatronhoUi.tarjeta = null
                            if (t.id.startsWith("local:")) SatronhoUi.accionLocal(t.id, id)
                            else SatronhoUi.orden("tarjeta|${t.id}|$id")
                        }) { Text(texto) }
                    }
                }
            }
        }
    }
}
