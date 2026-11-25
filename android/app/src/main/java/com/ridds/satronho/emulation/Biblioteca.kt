// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
package com.ridds.satronho.emulation

import android.content.Context
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.GridItemSpan
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Clear
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Search
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.Star
import androidx.compose.material3.Checkbox
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExtendedFloatingActionButton
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.FilterChip
import androidx.compose.material3.FilterChipDefaults
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.blur
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.delay

// Preferencias de la vista (solo del celular): cuadrícula/compacta/lista, orden y ocultos.
private class Vista(context: Context) {
    private val prefs = context.getSharedPreferences("satronho_biblioteca", Context.MODE_PRIVATE)
    var modo by mutableIntStateOf(prefs.getInt("vista", 0))
    var orden by mutableIntStateOf(prefs.getInt("orden", 0))
    var ocultos by mutableStateOf(prefs.getBoolean("ocultos", false))
    var favoritos by mutableStateOf(prefs.getBoolean("favoritos", false))
    fun guardar() {
        prefs.edit().putInt("vista", modo).putInt("orden", orden).putBoolean("ocultos", ocultos)
            .putBoolean("favoritos", favoritos).apply()
    }
}

@Composable
fun PantallaBiblioteca(e: Estado, b: Biblioteca) {
    val context = LocalContext.current
    val vista = remember { Vista(context) }
    var busqueda by remember { mutableStateOf("") }
    var consola by remember { mutableStateOf("") }
    var menuFiltro by remember { mutableStateOf(false) }
    var menuJuego by remember { mutableStateOf("") }
    var consejo by remember { mutableIntStateOf(0) }
    val bActual by rememberUpdatedState(b)

    // «Atrás» cierra primero lo que esté abierto aquí.
    DisposableEffect(Unit) {
        SatronhoUi.cerrarAlgo = {
            when {
                menuFiltro -> { menuFiltro = false; true }
                menuJuego.isNotEmpty() -> { menuJuego = ""; true }
                busqueda.isNotEmpty() -> { busqueda = ""; true }
                consola.isNotEmpty() -> { consola = ""; true }
                else -> false
            }
        }
        SatronhoUi.alTocarLocal = { id, boton ->
            if (id == "local:consejo") when (boton) {
                "otro" -> {
                    consejo = elegirConsejo(bActual, consejo)
                    mostrarConsejo(bActual, consejo)
                }
                "preguntar" -> SatronhoUi.abrirAjustes("chat")
            }
        }
        onDispose {
            SatronhoUi.cerrarAlgo = null
            SatronhoUi.alTocarLocal = null
        }
    }

    // Un guía pidió mostrar una consola.
    LaunchedEffect(SatronhoUi.consolaPedida) {
        if (SatronhoUi.consolaPedida.isNotEmpty()) {
            consola = SatronhoUi.consolaPedida
            SatronhoUi.consolaPedida = ""
        }
    }
    // El guía de abajo va cambiando: Samy y Troya salen más (Samy un poco más), luego Ñoño, Juana y MechaRidds.
    LaunchedEffect(b.consejos.size) {
        while (b.consejos.isNotEmpty()) {
            consejo = elegirConsejo(b, consejo)
            delay(20_000)
        }
    }
    val filtro = busqueda.trim().lowercase()
    val lista = remember(b, filtro, consola, vista.orden, vista.ocultos, vista.favoritos) {
        b.juegos.asSequence()
            .filter { vista.ocultos || !it.oculto }
            .filter { !vista.favoritos || it.favorito }
            .filter { consola.isEmpty() || it.sistema == consola }
            .filter { filtro.isEmpty() || it.titulo.lowercase().contains(filtro) }
            .sortedWith(
                compareByDescending<Juego> { it.favorito }.then(
                    when (vista.orden) {
                        1 -> compareByDescending<Juego> { it.ultima }.thenBy { it.titulo.lowercase() }
                        2 -> compareByDescending<Juego> { it.tiempo }.thenBy { it.titulo.lowercase() }
                        else -> compareBy { it.titulo.lowercase() }
                    }
                )
            ).toList()
    }
    // «Seguir jugando»: el último juego con partida guardada al salir.
    val seguir = remember(b, consola) {
        b.juegos.filter { it.continuar && !it.oculto && (consola.isEmpty() || it.sistema == consola) }.maxByOrNull { it.ultima }
    }

    Box(Modifier.fillMaxSize()) {
        // Logo de Satronho de fondo, muy suave (como el planeta de NX).
        val logo = Imagenes.cargar("asset:logo.png", 512)
        if (logo != null) {
            Image(logo, null, modifier = Modifier.align(Alignment.Center).size(300.dp).alpha((e.tema.marca * 0.6f).coerceIn(0.05f, 0.6f)),
                contentScale = ContentScale.Fit)
        }

        Column(Modifier.fillMaxSize().windowInsetsPadding(WindowInsets.safeDrawing)) {
            // Encabezado: nombre y los tres botones (ocultos, filtro, ajustes), como NX.
            Row(Modifier.fillMaxWidth().padding(start = 18.dp, end = 8.dp, top = 10.dp), verticalAlignment = Alignment.CenterVertically) {
                Logo(38.dp)
                Spacer(Modifier.width(8.dp))
                Column(Modifier.weight(1f)) {
                    Text("Satronho Emulation", style = MaterialTheme.typography.titleLarge.copy(fontSize = 20.sp), maxLines = 1,
                        overflow = TextOverflow.Ellipsis)
                    Text(
                        if (b.juegos.isEmpty()) t("Por Ridds") else TrJuegos(b.juegos.count { !it.oculto }),
                        style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.primary, maxLines = 1,
                    )
                }
                BotonRedondo(Iconos.ojo, t(if (vista.ocultos) "Esconder los juegos ocultos" else "Mostrar los juegos ocultos"),
                    activo = vista.ocultos) {
                    vista.ocultos = !vista.ocultos
                    vista.guardar()
                }
                Box {
                    BotonRedondo(Iconos.filtro, t("Vista y orden"), activo = menuFiltro) { menuFiltro = true }
                    MenuFiltro(menuFiltro, vista) { menuFiltro = false }
                }
                BotonRedondo(Icons.Filled.Settings, t("Ajustes")) { SatronhoUi.abrirAjustes() }
            }

            // Búsqueda.
            CampoBusqueda(busqueda, { busqueda = it }, Modifier.padding(horizontal = 14.dp, vertical = 10.dp))

            // Consolas con juegos (Todas, Octa, CD…).
            val conJuegos = b.consolas.filter { it.cuantos > 0 }
            if (conJuegos.size > 1) {
                LazyRow(contentPadding = PaddingValues(horizontal = 14.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    item {
                        FilterChip(selected = consola.isEmpty(), onClick = { consola = "" }, label = { Text(t("Todas")) })
                    }
                    items(conJuegos, key = { it.id }) { c ->
                        FilterChip(
                            selected = consola == c.id,
                            onClick = { consola = if (consola == c.id) "" else c.id },
                            label = { Text(c.nombre) },
                            leadingIcon = { Box(Modifier.size(8.dp).clip(CircleShape).background(c.color)) },
                            colors = FilterChipDefaults.filterChipColors(selectedContainerColor = c.color.copy(alpha = 0.22f)),
                        )
                    }
                }
                Spacer(Modifier.height(6.dp))
            }

            if (b.juegos.isEmpty()) {
                SinJuegos(b)
            } else if (lista.isEmpty()) {
                Box(Modifier.fillMaxSize().padding(32.dp), contentAlignment = Alignment.TopCenter) {
                    Text(t("No hay juegos que coincidan."), color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
            } else if (vista.modo == 3) {
                Carrusel(lista, b, menuJuego, { menuJuego = it })
            } else {
                val celdas = when (vista.modo) {
                    1 -> GridCells.Adaptive(86.dp)
                    2 -> GridCells.Fixed(1)
                    else -> GridCells.Adaptive(112.dp)
                }
                LazyVerticalGrid(
                    columns = celdas,
                    contentPadding = PaddingValues(start = 14.dp, end = 14.dp, top = 6.dp, bottom = 120.dp),
                    horizontalArrangement = Arrangement.spacedBy(if (vista.modo == 2) 0.dp else 12.dp),
                    verticalArrangement = Arrangement.spacedBy(if (vista.modo == 2) 4.dp else 14.dp),
                    modifier = Modifier.fillMaxSize(),
                ) {
                    if (seguir != null && filtro.isEmpty()) {
                        item(span = { GridItemSpan(maxLineSpan) }, key = "seguir") { SeguirJugando(seguir, b) }
                    }
                    items(lista, key = { it.clave }) { g ->
                        val c = b.consola(g.sistema)
                        Box {
                            when (vista.modo) {
                                2 -> FilaJuego(g, c, { SatronhoUi.orden("jugar|${g.clave}") }, { menuJuego = g.clave })
                                else -> TarjetaJuego(g, c, vista.modo == 1, { SatronhoUi.orden("jugar|${g.clave}") }, { menuJuego = g.clave })
                            }
                            MenuJuego(menuJuego == g.clave, g) { menuJuego = "" }
                        }
                    }
                }
            }
        }

        // Abajo a la derecha: el guía (consejos) y «Carpeta», como en NX.
        Column(
            Modifier.align(Alignment.BottomEnd).windowInsetsPadding(WindowInsets.safeDrawing).padding(18.dp),
            horizontalAlignment = Alignment.End,
        ) {
            if (e.guias && b.consejos.isNotEmpty()) {
                val guia = b.consejos[consejo.mod(b.consejos.size)].guia
                Box(
                    Modifier.size(52.dp).clip(CircleShape).border(2.dp, Guias.color(guia), CircleShape)
                        .clickable { mostrarConsejo(b, consejo) },
                    contentAlignment = Alignment.Center,
                ) { Avatar(guia, 48.dp) }
                Spacer(Modifier.height(12.dp))
            }
            ExtendedFloatingActionButton(
                onClick = { SatronhoUi.orden("carpeta") },
                icon = { Icon(Iconos.carpeta, null) },
                text = { Text(t("Carpeta")) },
                containerColor = MaterialTheme.colorScheme.primary,
                contentColor = MaterialTheme.colorScheme.onPrimary,
            )
        }
    }
}

// Reparto de las apariciones de los guías (de 100): Samy 32, Troya 26, Ñoño 16, Juana 14, MechaRidds 12.
private val repartoGuias = listOf("samy" to 32, "troya" to 26, "nono" to 16, "juana" to 14, "mecharidds" to 12)

/** Elige el próximo consejo: primero el guía (según el reparto) y luego uno de sus consejos. */
private fun elegirConsejo(b: Biblioteca, actual: Int): Int {
    if (b.consejos.isEmpty()) return 0
    var r = kotlin.random.Random.nextInt(100)
    var guia = repartoGuias.first().first
    for ((g, peso) in repartoGuias) {
        if (r < peso) { guia = g; break }
        r -= peso
    }
    val suyos = b.consejos.indices.filter { b.consejos[it].guia == guia && it != actual.mod(b.consejos.size) }
    return if (suyos.isNotEmpty()) suyos.random() else kotlin.random.Random.nextInt(b.consejos.size)
}

private fun TrJuegos(n: Int): String = if (n == 1) t("1 juego") else n.toString() + " " + t("juegos")

private fun mostrarConsejo(b: Biblioteca, i: Int) {
    if (b.consejos.isEmpty()) return
    val c = b.consejos[i.mod(b.consejos.size)]
    SatronhoUi.tarjeta = Tarjeta(
        "local:consejo", c.guia, Guias.nombre(c.guia), c.texto,
        listOf("preguntar" to t("Preguntar"), "otro" to t("Otro consejo"), "cerrar" to t("Cerrar")),
    )
}

@Composable
private fun BotonRedondo(icono: ImageVector, descripcion: String, activo: Boolean = false, alTocar: () -> Unit) {
    IconButton(onClick = alTocar) {
        Box(
            Modifier.size(34.dp).clip(CircleShape)
                .background(if (activo) MaterialTheme.colorScheme.primary.copy(alpha = 0.25f) else MaterialTheme.colorScheme.surfaceVariant),
            contentAlignment = Alignment.Center,
        ) {
            Icon(icono, descripcion, tint = MaterialTheme.colorScheme.primary, modifier = Modifier.size(18.dp))
        }
    }
}

@Composable
private fun CampoBusqueda(texto: String, cambiar: (String) -> Unit, modifier: Modifier) {
    val cs = MaterialTheme.colorScheme
    Row(
        modifier.fillMaxWidth().height(46.dp).clip(RoundedCornerShape(23.dp)).border(1.dp, cs.outline, RoundedCornerShape(23.dp))
            .padding(horizontal = 14.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Icon(Icons.Filled.Search, null, tint = cs.primary, modifier = Modifier.size(20.dp))
        Spacer(Modifier.width(10.dp))
        Box(Modifier.weight(1f)) {
            if (texto.isEmpty()) Text(t("Buscar juegos"), color = cs.onSurfaceVariant, style = MaterialTheme.typography.bodyLarge)
            BasicTextField(
                value = texto, onValueChange = cambiar, singleLine = true,
                textStyle = MaterialTheme.typography.bodyLarge.copy(color = cs.onSurface),
                cursorBrush = SolidColor(cs.primary), modifier = Modifier.fillMaxWidth(),
            )
        }
        if (texto.isNotEmpty()) {
            Icon(Icons.Filled.Clear, t("Borrar"), tint = cs.onSurfaceVariant, modifier = Modifier.size(20.dp).clickable { cambiar("") })
        }
    }
}

@Composable
private fun MenuFiltro(abierto: Boolean, vista: Vista, cerrar: () -> Unit) {
    DropdownMenu(expanded = abierto, onDismissRequest = cerrar) {
        @Composable
        fun opcion(texto: String, elegida: Boolean, alTocar: () -> Unit) {
            DropdownMenuItem(
                text = { Text(texto) },
                trailingIcon = { RadioButton(selected = elegida, onClick = null) },
                onClick = { alTocar(); vista.guardar() },
            )
        }
        opcion(t("Cuadrícula"), vista.modo == 0) { vista.modo = 0 }
        opcion(t("Cuadrícula compacta"), vista.modo == 1) { vista.modo = 1 }
        opcion(t("En lista"), vista.modo == 2) { vista.modo = 2 }
        opcion(t("Carrusel"), vista.modo == 3) { vista.modo = 3 }
        HorizontalDivider()
        opcion(t("Alfabético"), vista.orden == 0) { vista.orden = 0 }
        opcion(t("Jugados hace poco"), vista.orden == 1) { vista.orden = 1 }
        opcion(t("Más jugados"), vista.orden == 2) { vista.orden = 2 }
        HorizontalDivider()
        DropdownMenuItem(
            text = { Text(t("Solo favoritos")) },
            trailingIcon = { Checkbox(checked = vista.favoritos, onCheckedChange = null) },
            onClick = { vista.favoritos = !vista.favoritos; vista.guardar() },
        )
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun TarjetaJuego(g: Juego, c: Consola?, compacta: Boolean, jugar: () -> Unit, menu: () -> Unit) {
    val color = c?.color ?: MaterialTheme.colorScheme.primary
    Column(
        Modifier.combinedClickable(onClick = jugar, onLongClick = menu).alpha(if (g.oculto) 0.45f else 1f),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Box {
            Portada(g.portada, g.titulo, color, Modifier.fillMaxWidth().aspectRatio(1f), radio = 14, tono = g.tono,
                forma = c?.forma ?: "nes", etiqueta = c?.etiqueta ?: "")
            if (g.favorito) {
                Icon(Icons.Filled.Star, null, tint = Color(0xFFFFD166),
                    modifier = Modifier.align(Alignment.TopEnd).padding(6.dp).size(18.dp))
            }
        }
        if (!compacta) {
            Spacer(Modifier.height(6.dp))
            Text(g.titulo, style = MaterialTheme.typography.labelMedium.copy(fontWeight = FontWeight.SemiBold), maxLines = 2,
                overflow = TextOverflow.Ellipsis, textAlign = TextAlign.Center)
        }
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun FilaJuego(g: Juego, c: Consola?, jugar: () -> Unit, menu: () -> Unit) {
    val color = c?.color ?: MaterialTheme.colorScheme.primary
    Row(
        Modifier.fillMaxWidth().clip(RoundedCornerShape(12.dp)).combinedClickable(onClick = jugar, onLongClick = menu)
            .padding(vertical = 6.dp, horizontal = 4.dp).alpha(if (g.oculto) 0.45f else 1f),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Portada(g.portada, g.titulo, color, Modifier.size(52.dp), radio = 10, lado = 160, tono = g.tono, forma = c?.forma ?: "nes",
            etiqueta = c?.etiqueta ?: "")
        Spacer(Modifier.width(12.dp))
        Column(Modifier.weight(1f)) {
            Text(g.titulo, style = MaterialTheme.typography.titleSmall, maxLines = 1, overflow = TextOverflow.Ellipsis)
            val detalle = listOfNotNull(c?.completo, g.tiempoTexto.takeIf { it.isNotEmpty() }).joinToString(" · ")
            Text(detalle, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant, maxLines = 1)
        }
        if (g.favorito) Icon(Icons.Filled.Star, null, tint = Color(0xFFFFD166), modifier = Modifier.size(18.dp))
    }
}

@Composable
private fun SeguirJugando(g: Juego, b: Biblioteca) {
    val c = b.consola(g.sistema)
    val color = c?.color ?: MaterialTheme.colorScheme.primary
    Surface(
        shape = RoundedCornerShape(16.dp),
        color = MaterialTheme.colorScheme.surface,
        modifier = Modifier.fillMaxWidth().padding(bottom = 4.dp).border(1.dp, color.copy(alpha = 0.35f), RoundedCornerShape(16.dp)),
    ) {
        Row(Modifier.padding(10.dp), verticalAlignment = Alignment.CenterVertically) {
            Portada(g.portada, g.titulo, color, Modifier.size(64.dp), radio = 12, lado = 200, tono = g.tono, forma = c?.forma ?: "nes",
                etiqueta = c?.etiqueta ?: "")
            Spacer(Modifier.width(12.dp))
            Column(Modifier.weight(1f)) {
                Text(t("Seguir jugando"), style = MaterialTheme.typography.labelMedium, color = color)
                Text(g.titulo, style = MaterialTheme.typography.titleSmall, maxLines = 1, overflow = TextOverflow.Ellipsis)
                if (g.ultimaTexto.isNotEmpty()) {
                    Text(g.ultimaTexto, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
            }
            FilledTonalButton(onClick = { SatronhoUi.orden("continuar|${g.clave}") }) {
                Icon(Icons.Filled.PlayArrow, null, modifier = Modifier.size(18.dp))
                Spacer(Modifier.width(4.dp))
                Text(t("Continuar"))
            }
        }
    }
}

@Composable
private fun MenuJuego(abierto: Boolean, g: Juego, cerrar: () -> Unit) {
    DropdownMenu(expanded = abierto, onDismissRequest = cerrar) {
        DropdownMenuItem(text = { Text(t("Jugar")) }, leadingIcon = { Icon(Icons.Filled.PlayArrow, null) },
            onClick = { cerrar(); SatronhoUi.orden("jugar|${g.clave}") })
        if (g.continuar) {
            DropdownMenuItem(text = { Text(t("Continuar donde quedé")) }, leadingIcon = { Icon(Iconos.cargar, null) },
                onClick = { cerrar(); SatronhoUi.orden("continuar|${g.clave}") })
        }
        DropdownMenuItem(
            text = { Text(t(if (g.favorito) "Quitar de favoritos" else "Agregar a favoritos")) },
            leadingIcon = { Icon(Icons.Filled.Star, null) },
            onClick = { cerrar(); SatronhoUi.orden("favorito|${g.clave}") },
        )
        DropdownMenuItem(
            text = { Text(t(if (g.oculto) "Mostrar en la biblioteca" else "Ocultar de la biblioteca")) },
            leadingIcon = { Icon(Iconos.ojo, null) },
            onClick = { cerrar(); SatronhoUi.orden("ocultar|${g.clave}|${if (g.oculto) 0 else 1}") },
        )
        DropdownMenuItem(text = { Text(t("Ajustes de este juego…")) }, leadingIcon = { Icon(Iconos.ajustesJuego, null) },
            onClick = { cerrar(); SatronhoUi.orden("ajustes_juego|${g.clave}") })
        if (g.tiempoTexto.isNotEmpty()) {
            HorizontalDivider()
            Text(
                t("Tiempo de juego") + ": " + g.tiempoTexto, style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant, modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp),
            )
        }
    }
}

@Composable
private fun SinJuegos(b: Biblioteca) {
    Column(Modifier.fillMaxSize().padding(28.dp), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.Center) {
        Avatar("samy", 72.dp)
        Spacer(Modifier.height(14.dp))
        Text(
            t(if (b.buscando) "Buscando tus juegos…" else "Todavía no hay juegos"),
            style = MaterialTheme.typography.titleMedium, textAlign = TextAlign.Center,
        )
        Spacer(Modifier.height(6.dp))
        Text(
            t("Toca «Carpeta» y elige dónde guardas tus juegos. También puedes abrirlos desde WhatsApp, Drive o Archivos con «Abrir con»."),
            style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant, textAlign = TextAlign.Center,
        )
    }
}

/**
 * Vista carrusel (como en PC): una portada grande a la vez, con las vecinas asomando a los lados, el
 * fondo con los colores de la carátula y los datos del juego abajo. Se desliza con el dedo.
 */
@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun Carrusel(lista: List<Juego>, b: Biblioteca, menuJuego: String, abrirMenu: (String) -> Unit) {
    val cs = MaterialTheme.colorScheme
    val estado = androidx.compose.foundation.pager.rememberPagerState(pageCount = { lista.size })
    // El fondo y los datos siguen a la portada donde el carrusel se detiene, no a cada una que pasa
    // mientras se desliza (así no se recalcula el desenfoque a cada rato y va fluido).
    val actual = lista.getOrNull(estado.settledPage) ?: lista.getOrNull(estado.currentPage) ?: return
    val c = b.consola(actual.sistema)
    val color = c?.color ?: cs.primary
    // Un deslizón rápido puede pasar varias portadas de una vez, no solo una.
    val lanzar = androidx.compose.foundation.pager.PagerDefaults.flingBehavior(
        state = estado,
        pagerSnapDistance = androidx.compose.foundation.pager.PagerSnapDistance.atMost(12),
    )
    androidx.compose.foundation.layout.BoxWithConstraints(Modifier.fillMaxSize()) {
        val horizontal = maxWidth > maxHeight
        // Fondo: la carátula del juego elegido, grande, borrosa y oscura.
        val fondo = Imagenes.cargar(actual.portada, 120)
        if (fondo != null) {
            Image(
                fondo, null, contentScale = ContentScale.Crop,
                modifier = Modifier.fillMaxSize().blur(28.dp).alpha(0.35f),
            )
        }
        Box(Modifier.fillMaxSize().background(androidx.compose.ui.graphics.Brush.verticalGradient(
            listOf(Color.Transparent, cs.background.copy(alpha = 0.85f), cs.background))))

        // Tamaño de la portada: que quepa a lo alto y a lo ancho, en vertical y en horizontal.
        val altoDisponible = if (horizontal) maxHeight - 24.dp else maxHeight - 230.dp
        val anchoDisponible = if (horizontal) maxWidth * 0.55f else maxWidth
        val alto = minOf(altoDisponible, (anchoDisponible - 120.dp) / 0.78f).coerceAtLeast(120.dp)
        val ancho = alto * 0.78f

        val pasarela: @Composable (Modifier) -> Unit = { mod ->
            androidx.compose.foundation.layout.BoxWithConstraints(mod) {
                val lado = ((maxWidth - ancho) / 2).coerceAtLeast(0.dp)
                androidx.compose.foundation.pager.HorizontalPager(
                    state = estado,
                    contentPadding = PaddingValues(horizontal = lado),
                    pageSpacing = 14.dp,
                    flingBehavior = lanzar,
                    beyondViewportPageCount = 2,
                    modifier = Modifier.fillMaxSize(),
                    key = { lista[it].clave },
                ) { i ->
                    val g = lista[i]
                    val gc = b.consola(g.sistema)
                    val tinte = gc?.color ?: cs.primary
                    Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                        Box {
                            Box(
                                Modifier.size(ancho, alto)
                                    // La distancia al centro se lee al dibujar, no al armar la pantalla:
                                    // deslizar no vuelve a construir cada portada en cada cuadro.
                                    .graphicsLayer {
                                        val d = kotlin.math.abs((estado.currentPage - i) + estado.currentPageOffsetFraction)
                                            .coerceIn(0f, 1f)
                                        val e = 1f - 0.14f * d
                                        scaleX = e; scaleY = e; alpha = 1f - 0.45f * d
                                    }
                                    .border(2.dp, tinte.copy(alpha = 0.55f), RoundedCornerShape(20.dp))
                                    .combinedClickable(onClick = { SatronhoUi.orden("jugar|${g.clave}") }, onLongClick = { abrirMenu(g.clave) }),
                            ) {
                                Portada(g.portada, g.titulo, tinte, Modifier.fillMaxSize(), radio = 20, lado = 480,
                                    tono = g.tono, forma = gc?.forma ?: "nes", etiqueta = gc?.etiqueta ?: "")
                                if (g.favorito) Icon(Icons.Filled.Star, null, tint = Color(0xFFFFD166),
                                    modifier = Modifier.align(Alignment.TopEnd).padding(10.dp).size(24.dp))
                            }
                            MenuJuego(menuJuego == g.clave, g) { abrirMenu("") }
                        }
                    }
                }
            }
        }

        val datos: @Composable (Modifier, Boolean) -> Unit = { mod, lado ->
            Column(mod, horizontalAlignment = if (lado) Alignment.Start else Alignment.CenterHorizontally) {
                Text(actual.titulo, style = MaterialTheme.typography.titleLarge,
                    textAlign = if (lado) TextAlign.Start else TextAlign.Center, maxLines = 2,
                    overflow = TextOverflow.Ellipsis)
                Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.padding(top = 4.dp)) {
                    Box(Modifier.size(8.dp).clip(CircleShape).background(color))
                    Spacer(Modifier.width(6.dp))
                    Text(c?.completo ?: "", style = MaterialTheme.typography.labelLarge, color = color, maxLines = 1,
                        overflow = TextOverflow.Ellipsis)
                }
                if (actual.tiempoTexto.isNotEmpty()) {
                    Text(actual.tiempoTexto, style = MaterialTheme.typography.labelMedium, color = cs.onSurfaceVariant,
                        modifier = Modifier.padding(top = 2.dp))
                }
                Spacer(Modifier.height(12.dp))
                Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    if (actual.continuar) {
                        androidx.compose.material3.OutlinedButton(onClick = { SatronhoUi.orden("continuar|${actual.clave}") }) {
                            Icon(Iconos.cargar, null, modifier = Modifier.size(18.dp))
                            Spacer(Modifier.width(6.dp))
                            Text(t("Continuar"))
                        }
                    }
                    androidx.compose.material3.Button(onClick = { SatronhoUi.orden("jugar|${actual.clave}") }) {
                        Icon(Icons.Filled.PlayArrow, null, modifier = Modifier.size(18.dp))
                        Spacer(Modifier.width(6.dp))
                        Text(t("Jugar"))
                    }
                }
                Spacer(Modifier.height(6.dp))
                Text("${estado.currentPage + 1} / ${lista.size}", style = MaterialTheme.typography.labelSmall, color = cs.onSurfaceVariant)
            }
        }

        if (horizontal) {
            // En horizontal: portadas a la izquierda y los datos a la derecha, sin taparse.
            Row(Modifier.fillMaxSize(), verticalAlignment = Alignment.CenterVertically) {
                pasarela(Modifier.weight(0.58f).fillMaxSize())
                datos(Modifier.weight(0.42f).padding(start = 8.dp, end = 104.dp, bottom = 40.dp), true)
            }
        } else {
            Column(Modifier.fillMaxSize(), horizontalAlignment = Alignment.CenterHorizontally) {
                pasarela(Modifier.fillMaxWidth().weight(1f))
                datos(Modifier.fillMaxWidth().padding(start = 24.dp, end = 24.dp, top = 8.dp, bottom = 96.dp), false)
            }
        }
    }
}
