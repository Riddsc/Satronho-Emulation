// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
package com.ridds.satronho.emulation

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInVertically
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
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.delay

/** «Cargando…»: una tarjeta chica con la portada, el nombre y una barra (como NX). */
@Composable
fun TarjetaCarga(c: Cargando) {
    Box(Modifier.fillMaxSize().background(Color.Black.copy(alpha = 0.55f)), contentAlignment = Alignment.Center) {
        Surface(
            shape = RoundedCornerShape(14.dp),
            color = MaterialTheme.colorScheme.surface,
            tonalElevation = 3.dp,
            modifier = Modifier.width(300.dp),
        ) {
            Row(Modifier.padding(12.dp), verticalAlignment = Alignment.CenterVertically) {
                Box {
                    Portada(c.portada, c.titulo, c.color, Modifier.size(64.dp), 10)
                    Logo(22.dp, Modifier.align(Alignment.BottomEnd).padding(2.dp))
                }
                Spacer(Modifier.width(12.dp))
                Column(Modifier.weight(1f)) {
                    Text(c.titulo, style = MaterialTheme.typography.titleSmall, maxLines = 1, overflow = TextOverflow.Ellipsis)
                    Text(c.consola, style = MaterialTheme.typography.bodySmall, color = c.color, maxLines = 1)
                    Text(t("Cargando…"), style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                    Spacer(Modifier.height(8.dp))
                    if (c.avance >= 0f) {
                        LinearProgressIndicator(progress = { c.avance }, modifier = Modifier.fillMaxWidth().clip(RoundedCornerShape(4.dp)))
                        TextButton(onClick = { SatronhoUi.orden("cancelar_descompresion") }) { Text(t("Cancelar")) }
                    } else {
                        LinearProgressIndicator(modifier = Modifier.fillMaxWidth().clip(RoundedCornerShape(4.dp)))
                    }
                }
            }
        }
    }
}

/** Portada de un juego, o un recuadro con su color y su nombre mientras no hay carátula. */
@Composable
fun Portada(ruta: String, titulo: String, color: Color, modifier: Modifier, radio: Int = 12, lado: Int = 360) {
    val img = Imagenes.cargar(ruta, lado)
    Box(
        modifier.clip(RoundedCornerShape(radio.dp)).background(color.copy(alpha = 0.22f)),
        contentAlignment = Alignment.Center,
    ) {
        if (img != null) {
            Image(img, contentDescription = titulo, contentScale = ContentScale.Crop, modifier = Modifier.fillMaxSize())
        } else {
            Text(
                titulo, color = color, fontWeight = FontWeight.Bold, fontSize = 12.sp, maxLines = 3,
                overflow = TextOverflow.Ellipsis, modifier = Modifier.padding(8.dp),
            )
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
