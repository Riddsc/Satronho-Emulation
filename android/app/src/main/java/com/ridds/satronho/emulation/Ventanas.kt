// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
package com.ridds.satronho.emulation

import android.os.Build
import android.os.Environment
import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material.icons.filled.Check
import androidx.compose.material.icons.filled.Close
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Checkbox
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.scale
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.delay
import java.io.File

/** Velo oscuro que no deja tocar lo de atrás. */
@Composable
internal fun Velo(alpha: Float = 0.6f, contenido: @Composable () -> Unit) {
    Box(
        Modifier.fillMaxSize().background(Color.Black.copy(alpha = alpha))
            .clickable(interactionSource = remember { MutableInteractionSource() }, indication = null) {}
            .windowInsetsPadding(WindowInsets.safeDrawing).padding(18.dp),
        contentAlignment = Alignment.Center,
    ) { contenido() }
}

@Composable
internal fun Ventana(ancho: Int = 460, contenido: @Composable ColumnScope.() -> Unit) {
    Surface(
        shape = RoundedCornerShape(22.dp), color = MaterialTheme.colorScheme.surface, tonalElevation = 4.dp, shadowElevation = 10.dp,
        modifier = Modifier.widthIn(max = ancho.dp).fillMaxWidth(),
    ) { Column(Modifier.padding(18.dp)) { contenido() } }
}

// ------------------------------------------------------------------ presentación con el logo

/** Al abrir la app: el logo de Satronho Emulation se agranda y aparece el nombre (como una consola). */
@Composable
fun Presentacion() {
    val escala = remember { Animatable(0.6f) }
    val brillo = remember { Animatable(0f) }
    LaunchedEffect(Unit) {
        brillo.animateTo(1f, tween(450))
        escala.animateTo(1f, tween(650, easing = FastOutSlowInEasing))
        delay(700)
        brillo.animateTo(0f, tween(350))
        SatronhoUi.presentacion = false
    }
    Box(
        Modifier.fillMaxSize().background(MaterialTheme.colorScheme.background)
            .clickable(interactionSource = remember { MutableInteractionSource() }, indication = null) { SatronhoUi.presentacion = false },
        contentAlignment = Alignment.Center,
    ) {
        Column(horizontalAlignment = Alignment.CenterHorizontally, modifier = Modifier.alpha(brillo.value)) {
            Logo(180.dp, Modifier.scale(escala.value))
            Spacer(Modifier.height(18.dp))
            Text("Satronho Emulation", style = MaterialTheme.typography.headlineSmall.copy(fontWeight = FontWeight.Bold))
            Text(t("Por Ridds"), style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.primary)
        }
    }
}

// ------------------------------------------------------------------ bienvenida (primera vez)

@Composable
fun PantallaBienvenida(b: Bienvenida) {
    val cs = MaterialTheme.colorScheme
    Box(Modifier.fillMaxSize().background(cs.background).windowInsetsPadding(WindowInsets.safeDrawing).padding(22.dp), contentAlignment = Alignment.Center) {
        Column(Modifier.widthIn(max = 480.dp).verticalScroll(rememberScrollState()), horizontalAlignment = Alignment.CenterHorizontally) {
            Logo(if (b.paso == 0) 150.dp else 90.dp)
            Spacer(Modifier.height(14.dp))
            Text(b.titulo, style = MaterialTheme.typography.headlineSmall.copy(fontWeight = FontWeight.Bold), textAlign = TextAlign.Center)
            Spacer(Modifier.height(16.dp))
            GuiaDice(b.guia, b.texto)
            Spacer(Modifier.height(18.dp))
            // Tres puntitos del avance.
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                for (i in 0..2) Box(Modifier.size(width = if (i == b.paso) 22.dp else 8.dp, height = 8.dp).clip(RoundedCornerShape(4.dp))
                    .background(if (i <= b.paso) cs.primary else cs.surfaceVariant))
            }
            Spacer(Modifier.height(18.dp))
            when (b.paso) {
                0 -> Button(onClick = { SatronhoUi.orden("bienvenida|empezar") }, modifier = Modifier.fillMaxWidth().height(50.dp)) { Text(t("Empezar")) }
                1 -> Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    OutlinedButton(onClick = { SatronhoUi.orden("bienvenida|despues") }, modifier = Modifier.weight(1f).height(50.dp)) { Text(t("Lo reviso después")) }
                    Button(onClick = { SatronhoUi.orden("bienvenida|aplicar") }, modifier = Modifier.weight(1f).height(50.dp)) { Text(t("Aplícalo por mí")) }
                }
                else -> Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    OutlinedButton(onClick = { SatronhoUi.orden("bienvenida|fin") }, modifier = Modifier.weight(1f).height(50.dp)) { Text(t("Más tarde")) }
                    Button(onClick = { SatronhoUi.orden("bienvenida|carpeta") }, modifier = Modifier.weight(1f).height(50.dp)) {
                        Icon(Iconos.carpeta, null, modifier = Modifier.size(18.dp))
                        Spacer(Modifier.width(6.dp))
                        Text(t("Agregar carpeta"))
                    }
                }
            }
        }
    }
}

// ------------------------------------------------------------------ tarjetas de Satronho (builds, NX, cierres)

/**
 * La tarjeta de Satronho para avisos importantes de la biblioteca: build nueva, NX más nueva, lo
 * nuevo de esta build o un cierre inesperado. Borde con el degradado del guía y el acento, el
 * logo de Satronho Emulation de fondo, la insignia («Build 27», «NX») y un guía que habla.
 */
@Composable
fun TarjetaSatronho(
    guia: String,
    titulo: String,
    texto: String,
    insignia: String = "",
    avance: Float? = null,
    contenido: (@Composable ColumnScope.() -> Unit)? = null,
    botones: @Composable () -> Unit,
) {
    val cs = MaterialTheme.colorScheme
    val color = Guias.color(guia)
    val forma = RoundedCornerShape(22.dp)
    // Entra deslizándose desde abajo, como las tarjetas de Satronho NX.
    val entrada = remember { Animatable(0f) }
    LaunchedEffect(Unit) { entrada.animateTo(1f, tween(380, easing = FastOutSlowInEasing)) }
    Box(Modifier.fillMaxSize().windowInsetsPadding(WindowInsets.safeDrawing).padding(16.dp), contentAlignment = Alignment.BottomCenter) {
        Surface(
            shape = forma, color = cs.surface, shadowElevation = 12.dp,
            modifier = Modifier.widthIn(max = 440.dp).fillMaxWidth().padding(bottom = 70.dp)
                .graphicsLayer { translationY = (1f - entrada.value) * 120f; alpha = entrada.value }
                .border(1.5.dp, Brush.linearGradient(listOf(color, cs.primary, color.copy(alpha = 0.4f))), forma),
        ) {
            Box {
                // Brillo del color del guía arriba y el logo de Satronho, tenue, en la esquina.
                Box(Modifier.matchParentSize().background(Brush.verticalGradient(listOf(color.copy(alpha = 0.16f), Color.Transparent))))
                Logo(96.dp, Modifier.align(Alignment.TopEnd).padding(top = 4.dp, end = 4.dp).alpha(0.16f))
                Column(Modifier.padding(16.dp)) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Avatar(guia, 46.dp, Modifier.border(2.dp, color, CircleShape))
                        Spacer(Modifier.width(12.dp))
                        Column(Modifier.weight(1f)) {
                            Row(verticalAlignment = Alignment.CenterVertically) {
                                Text(Guias.nombre(guia), style = MaterialTheme.typography.labelLarge.copy(fontWeight = FontWeight.Bold), color = color)
                                if (insignia.isNotEmpty()) {
                                    Spacer(Modifier.width(8.dp))
                                    Text(
                                        insignia, style = MaterialTheme.typography.labelSmall.copy(fontWeight = FontWeight.Bold), color = Color.White,
                                        modifier = Modifier.clip(RoundedCornerShape(50)).background(Brush.horizontalGradient(listOf(color, cs.primary)))
                                            .padding(horizontal = 8.dp, vertical = 2.dp),
                                    )
                                }
                            }
                            Text(titulo, style = MaterialTheme.typography.titleMedium.copy(fontWeight = FontWeight.Bold), maxLines = 2, overflow = TextOverflow.Ellipsis)
                        }
                    }
                    if (texto.isNotEmpty()) {
                        Spacer(Modifier.height(8.dp))
                        Text(texto, style = MaterialTheme.typography.bodyMedium, color = cs.onSurfaceVariant)
                    }
                    contenido?.invoke(this)
                    if (avance != null) {
                        Spacer(Modifier.height(10.dp))
                        if (avance >= 0f) LinearProgressIndicator(progress = { avance }, color = color, modifier = Modifier.fillMaxWidth().height(6.dp).clip(RoundedCornerShape(3.dp)))
                        else LinearProgressIndicator(color = color, modifier = Modifier.fillMaxWidth().height(6.dp).clip(RoundedCornerShape(3.dp)))
                    }
                    Spacer(Modifier.height(10.dp))
                    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.End, verticalAlignment = Alignment.CenterVertically) { botones() }
                }
            }
        }
    }
}

/** Juana avisa que llegó una build nueva (ya instalada): qué trae y dónde verlo. */
@Composable
fun TarjetaNovedad(texto: String, tituloBuild: String, build: Int) {
    TarjetaSatronho("juana", texto, tituloBuild, insignia = if (build > 0) TrFmtKt("Build %d", build) else "") {
        TextButton(onClick = { SatronhoUi.orden("novedades_visto") }) { Text(t("Cerrar")) }
        Spacer(Modifier.width(6.dp))
        Button(onClick = { SatronhoUi.orden("novedades_visto"); SatronhoUi.abrirAjustes("novedades") }) { Text(t("Ver novedades")) }
    }
}

/** Salió una build nueva del hub: Juana la baja e instala con un toque (con su avance). */
@Composable
fun TarjetaActualizacion(a: Actualizacion) {
    val texto = when (a.estado) {
        "bajando" -> t("Bajándola… no cierres la app. Al terminar, Android te pregunta si la instalas: dile que sí.")
        "instalando" -> t("Confirma la instalación en la ventana de Android. Satronho se vuelve a abrir solo.")
        else -> t("Se instala encima: tus juegos, partidas y ajustes no se tocan.")
    }
    TarjetaSatronho(
        "juana", TrFmtKt("¡Ya salió la build %d!", a.build), texto, insignia = TrFmtKt("Build %d", a.build),
        avance = if (a.estado == "disponible") null else a.avance,
    ) {
        if (a.estado == "disponible") {
            TextButton(onClick = { SatronhoUi.abrirAjustes("novedades") }) { Text(t("Novedades")) }
            TextButton(onClick = { SatronhoUi.orden("actualizacion_despues") }) { Text(t("Después")) }
            Spacer(Modifier.width(6.dp))
            Button(onClick = { SatronhoUi.orden("actualizar") }) { Text(t("Actualizar")) }
        }
    }
}

/** Hay una Satronho NX más nueva que la del teléfono: MechaRidds la instala encima. */
@Composable
fun TarjetaNxNueva() {
    val ctx = LocalContext.current
    TarjetaSatronho(
        "mecharidds", t("Hay una Satronho NX más nueva"),
        t("Trae arreglos para tus juegos de Switch. Se instala encima; no pierdes partidas, claves ni firmware."), insignia = "NX",
    ) {
        TextButton(onClick = { Nx.despuesNueva(ctx) }) { Text(t("Después")) }
        Spacer(Modifier.width(6.dp))
        Button(onClick = { Nx.actualizarDesdeAviso(ctx) }) { Text(t("Actualizar")) }
    }
}

/** La última vez la app se cerró de golpe: Ñoño ofrece mandar el registro para arreglarlo. */
@Composable
fun TarjetaCierre(c: Cierres.Cierre) {
    TarjetaSatronho(
        "nono", t("La última vez Satronho se cerró de golpe"),
        t("Ya guardé lo que pasó. Si mandas el registro a Ridds, lo arregla más rápido."),
        insignia = c.cuando,
    ) {
        TextButton(onClick = { Cierres.visto() }) { Text(t("Cerrar")) }
        Spacer(Modifier.width(6.dp))
        Button(onClick = { Cierres.visto(); SatronhoUi.orden("enviar_registro") }) { Text(t("Enviar el registro")) }
    }
}

// ------------------------------------------------------------------ preguntas

@Composable
fun VentanaConfirmar(c: Confirmar) {
    var noPreguntar by remember { mutableStateOf(false) }
    Velo {
        Ventana {
            GuiaDice(c.guia, c.texto)
            Spacer(Modifier.height(12.dp))
            Text(c.titulo, style = MaterialTheme.typography.titleMedium)
            Row(Modifier.fillMaxWidth().clip(RoundedCornerShape(8.dp)).clickable { noPreguntar = !noPreguntar }, verticalAlignment = Alignment.CenterVertically) {
                Checkbox(checked = noPreguntar, onCheckedChange = { noPreguntar = it })
                Text(c.noPreguntar, style = MaterialTheme.typography.bodyMedium)
            }
            Spacer(Modifier.height(6.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                OutlinedButton(onClick = { SatronhoUi.orden("confirmar|no|${if (noPreguntar) 1 else 0}") }, modifier = Modifier.weight(1f).height(48.dp)) {
                    Text(c.no)
                }
                Button(
                    onClick = { SatronhoUi.orden("confirmar|si|${if (noPreguntar) 1 else 0}") },
                    colors = ButtonDefaults.buttonColors(containerColor = Color(0xFFFF5D73), contentColor = Color.White),
                    modifier = Modifier.weight(1f).height(48.dp),
                ) { Text(c.si) }
            }
        }
    }
}

@Composable
fun VentanaZip(z: Zip) {
    Velo {
        Ventana {
            GuiaDice("samy", z.titulo)
            Spacer(Modifier.height(10.dp))
            LazyColumn(Modifier.heightIn(max = 340.dp)) {
                items(z.opciones) { (entrada, titulo) ->
                    Text(
                        titulo, style = MaterialTheme.typography.titleSmall, maxLines = 2, overflow = TextOverflow.Ellipsis,
                        modifier = Modifier.fillMaxWidth().clip(RoundedCornerShape(10.dp)).clickable { SatronhoUi.orden("zip|$entrada") }
                            .padding(horizontal = 10.dp, vertical = 12.dp),
                    )
                }
            }
            TextButton(onClick = { SatronhoUi.orden("zip_cancelar") }, modifier = Modifier.align(Alignment.End)) { Text(t("Cancelar")) }
        }
    }
}

/** Recibiendo juegos de otra app, mudanza de «Satronho Games»… con su barra de avance. */
@Composable
fun VentanaProgreso(p: Progreso) {
    Velo {
        Ventana {
            GuiaDice(p.guia, p.texto)
            if (p.trabajando) {
                Spacer(Modifier.height(12.dp))
                if (p.avance >= 0f) LinearProgressIndicator(progress = { p.avance }, modifier = Modifier.fillMaxWidth().clip(RoundedCornerShape(4.dp)))
                else LinearProgressIndicator(modifier = Modifier.fillMaxWidth().clip(RoundedCornerShape(4.dp)))
            }
            for (d in p.detalles) Text(d, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant, modifier = Modifier.padding(top = 6.dp))
            if (p.nota.isNotEmpty()) Text(p.nota, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant, modifier = Modifier.padding(top = 8.dp))
            if (p.botones.isNotEmpty()) {
                Spacer(Modifier.height(10.dp))
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.End) {
                    for ((id, texto) in p.botones) TextButton(onClick = { SatronhoUi.orden("progreso|$id") }) { Text(texto) }
                }
            }
        }
    }
}

// ------------------------------------------------------------------ explorador de carpetas

private fun raicesAlmacenamiento(context: android.content.Context): List<File> {
    val out = ArrayList<File>()
    out.add(Environment.getExternalStorageDirectory())
    // Tarjetas SD: la carpeta de la app en cada una, subiendo hasta la raíz del volumen.
    for (d in context.getExternalFilesDirs(null)) {
        if (d == null) continue
        val p = d.absolutePath
        val i = p.indexOf("/Android/data/")
        if (i > 0) {
            val r = File(p.substring(0, i))
            if (out.none { it.absolutePath == r.absolutePath }) out.add(r)
        }
    }
    return out
}

private fun tienePermiso(context: android.content.Context): Boolean =
    if (Build.VERSION.SDK_INT >= 30) Environment.isExternalStorageManager()
    else context.checkSelfPermission(android.Manifest.permission.READ_EXTERNAL_STORAGE) == android.content.pm.PackageManager.PERMISSION_GRANTED

/**
 * Elegir una carpeta (la de tus juegos, o dónde va «Satronho Games»). Nativo, con la memoria del
 * celular y las tarjetas SD arriba.
 */
@Composable
fun Explorador(x: Explorar) {
    val cs = MaterialTheme.colorScheme
    val context = LocalContext.current
    val raices = remember { raicesAlmacenamiento(context) }
    var dir by remember { mutableStateOf(raices.first()) }
    var permiso by remember { mutableStateOf(tienePermiso(context)) }
    // El permiso se da en otra pantalla de Android: se vuelve a mirar cada segundo.
    LaunchedEffect(permiso) {
        while (!permiso) {
            delay(1000)
            permiso = tienePermiso(context)
        }
    }
    val carpetas = remember(dir, permiso) {
        (dir.listFiles() ?: emptyArray()).filter { it.isDirectory && !it.name.startsWith(".") }.sortedBy { it.name.lowercase() }
    }
    fun cerrar() {
        SatronhoUi.explorador = null
        SatronhoUi.orden("explorador_cerrar")
    }
    Surface(color = cs.background, modifier = Modifier.fillMaxSize()) {
        Column(Modifier.fillMaxSize().windowInsetsPadding(WindowInsets.safeDrawing)) {
            Row(Modifier.fillMaxWidth().padding(start = 4.dp, end = 12.dp, top = 6.dp), verticalAlignment = Alignment.CenterVertically) {
                IconButton(onClick = { cerrar() }) { Icon(Icons.Filled.Close, t("Cancelar")) }
                Text(
                    t(if (x.raiz) "Elige dónde va «Satronho Games»" else "Elige la carpeta de tus juegos"),
                    style = MaterialTheme.typography.titleMedium, modifier = Modifier.weight(1f), maxLines = 2,
                )
                Logo(28.dp)
            }
            if (!permiso) {
                Column(Modifier.padding(18.dp)) {
                    GuiaDice("samy", t("Para ver tus carpetas necesito el permiso de «acceso a todos los archivos». Lo das en la pantalla que se abre y vuelves aquí."))
                    Spacer(Modifier.height(12.dp))
                    Button(onClick = { SatronhoUi.orden("permiso") }, modifier = Modifier.fillMaxWidth().height(48.dp)) { Text(t("Dar permiso")) }
                }
                return@Column
            }
            // Memoria del celular y tarjetas SD.
            if (raices.size > 1) {
                Row(Modifier.padding(horizontal = 14.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    raices.forEachIndexed { i, r ->
                        FilledTonalButton(onClick = { dir = r }) { Text(t(if (i == 0) "Memoria del celular" else "Tarjeta SD")) }
                    }
                }
            }
            // Dónde estoy.
            Row(Modifier.fillMaxWidth().padding(horizontal = 8.dp, vertical = 4.dp), verticalAlignment = Alignment.CenterVertically) {
                val arriba = dir.parentFile
                val enRaiz = raices.any { it.absolutePath == dir.absolutePath }
                IconButton(onClick = { if (!enRaiz && arriba != null) dir = arriba }, enabled = !enRaiz && arriba != null) {
                    Icon(Icons.Filled.ArrowBack, t("Subir"))
                }
                Text(dir.absolutePath, style = MaterialTheme.typography.bodySmall, color = cs.onSurfaceVariant, maxLines = 2, overflow = TextOverflow.Ellipsis)
            }
            LazyColumn(Modifier.weight(1f).fillMaxWidth(), contentPadding = androidx.compose.foundation.layout.PaddingValues(horizontal = 12.dp)) {
                if (carpetas.isEmpty()) item {
                    Text(t("No hay carpetas aquí."), color = cs.onSurfaceVariant, modifier = Modifier.padding(16.dp))
                }
                items(carpetas, key = { it.absolutePath }) { c ->
                    Row(
                        Modifier.fillMaxWidth().clip(RoundedCornerShape(12.dp)).clickable { dir = c }.padding(horizontal = 10.dp, vertical = 12.dp),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        Icon(Iconos.carpeta, null, tint = cs.primary, modifier = Modifier.size(22.dp))
                        Spacer(Modifier.width(12.dp))
                        Text(c.name, style = MaterialTheme.typography.bodyLarge, maxLines = 1, overflow = TextOverflow.Ellipsis)
                    }
                }
            }
            Row(Modifier.fillMaxWidth().padding(14.dp), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                OutlinedButton(onClick = { cerrar() }, modifier = Modifier.weight(1f).height(50.dp)) { Text(t("Cancelar")) }
                Button(onClick = {
                    SatronhoUi.explorador = null
                    SatronhoUi.orden((if (x.raiz) "raiz|" else "carpeta_agregar|") + dir.absolutePath)
                }, modifier = Modifier.weight(1f).height(50.dp)) {
                    Icon(Icons.Filled.Check, null, modifier = Modifier.size(18.dp))
                    Spacer(Modifier.width(6.dp))
                    Text(t("Usar esta carpeta"), fontSize = 14.sp)
                }
            }
        }
    }
}
