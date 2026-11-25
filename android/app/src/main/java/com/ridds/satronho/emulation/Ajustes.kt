// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
package com.ridds.satronho.emulation

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material.icons.filled.KeyboardArrowRight
import androidx.compose.material.icons.filled.Send
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.FilterChip
import androidx.compose.material3.FilterChipDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.RadioButton
import androidx.compose.material3.SegmentedButton
import androidx.compose.material3.SegmentedButtonDefaults
import androidx.compose.material3.SingleChoiceSegmentedButtonRow
import androidx.compose.material3.Slider
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.json.JSONArray
import org.json.JSONObject

// Ajustes nativos con el estilo de Satronho NX: una lista de tarjetas con ícono, título, explicación
// y estado. Lo que hay en cada pantalla lo decide C++ (NativeSettingsJson); aquí solo se dibuja.

private fun JSONObject.lista(clave: String): List<String> {
    val a = optJSONArray(clave) ?: return emptyList()
    return List(a.length()) { a.optString(it) }
}

@Composable
fun PantallaAjustes(p: Pagina, e: Estado) {
    val cs = MaterialTheme.colorScheme
    Surface(color = cs.background, modifier = Modifier.fillMaxSize()) {
        Column(Modifier.fillMaxSize().windowInsetsPadding(WindowInsets.safeDrawing)) {
            // Encabezado: flecha atrás y el título (como «← Settings» de NX).
            Row(Modifier.fillMaxWidth().padding(start = 4.dp, end = 16.dp, top = 6.dp, bottom = 2.dp), verticalAlignment = Alignment.CenterVertically) {
                IconButton(onClick = { SatronhoUi.atrasAjustes() }) { Icon(Icons.Filled.ArrowBack, t("Volver"), tint = cs.onBackground) }
                Column(Modifier.weight(1f)) {
                    Text(p.titulo, style = MaterialTheme.typography.titleLarge, maxLines = 1, overflow = TextOverflow.Ellipsis)
                    if (p.subtitulo.isNotEmpty() && p.pagina != "inicio") {
                        Text(p.subtitulo, style = MaterialTheme.typography.bodySmall, color = cs.onSurfaceVariant, maxLines = 1,
                            overflow = TextOverflow.Ellipsis)
                    }
                }
                if (p.pagina != "inicio") Logo(30.dp)
            }
            if (p.pagina == "chat") {
                val chat = p.secciones.firstOrNull()?.items?.firstOrNull()
                if (chat != null) Chat(chat)
                return@Column
            }
            val estadoLista = rememberLazyListState()
            LaunchedEffect(p.pagina) { estadoLista.scrollToItem(0) }
            LazyColumn(
                state = estadoLista,
                contentPadding = PaddingValues(start = 14.dp, end = 14.dp, top = 6.dp, bottom = 40.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp),
                modifier = Modifier.fillMaxSize(),
            ) {
                if (p.pagina == "inicio") item(key = "hero") { HeroSatronho(p.subtitulo) }
                p.secciones.forEachIndexed { si, s ->
                    if (s.titulo.isNotEmpty() || s.descripcion.isNotEmpty()) {
                        item(key = "s$si") {
                            Column(Modifier.padding(start = 6.dp, top = if (si == 0) 2.dp else 12.dp, bottom = 2.dp)) {
                                if (s.titulo.isNotEmpty()) Text(s.titulo, style = MaterialTheme.typography.titleSmall, color = cs.primary)
                                if (s.descripcion.isNotEmpty()) Text(s.descripcion, style = MaterialTheme.typography.bodySmall, color = cs.onSurfaceVariant)
                            }
                        }
                    }
                    s.items.forEachIndexed { ii, it ->
                        item(key = "i$si-$ii-${it.optString("id", it.optString("destino", it.optString("titulo")))}") { Elemento(it) }
                    }
                }
            }
        }
    }
}

/** Arriba de Ajustes: el logo grande de Satronho Emulation (el sello de la app). */
@Composable
private fun HeroSatronho(subtitulo: String) {
    val cs = MaterialTheme.colorScheme
    Surface(
        shape = RoundedCornerShape(22.dp), color = cs.surface,
        modifier = Modifier.fillMaxWidth().border(1.dp, cs.primary.copy(alpha = 0.35f), RoundedCornerShape(22.dp)),
    ) {
        Row(Modifier.padding(16.dp), verticalAlignment = Alignment.CenterVertically) {
            Logo(84.dp)
            Spacer(Modifier.width(14.dp))
            Column {
                Text("Satronho Emulation", style = MaterialTheme.typography.titleLarge)
                Text(subtitulo, style = MaterialTheme.typography.bodySmall, color = cs.primary)
                Spacer(Modifier.height(4.dp))
                Text(t("Por Ridds"), style = MaterialTheme.typography.bodySmall, color = cs.onSurfaceVariant)
            }
        }
    }
}

/** La tarjeta de cada elemento (el estilo de NX: ícono, título, explicación y estado). */
@Composable
private fun TarjetaItem(
    icono: String, titulo: String, descripcion: String, valor: String = "", color: Color? = null,
    alTocar: (() -> Unit)? = null, peligro: Boolean = false, primario: Boolean = false, derecha: @Composable (() -> Unit)? = null,
    abajo: @Composable (() -> Unit)? = null, valorDerecha: String = "", nota: String = "",
) {
    val cs = MaterialTheme.colorScheme
    val acento = color ?: if (peligro) Color(0xFFFF5D73) else cs.primary
    Surface(
        shape = RoundedCornerShape(14.dp),
        color = if (primario) cs.primary.copy(alpha = 0.16f) else cs.surface,
        modifier = Modifier.fillMaxWidth().border(1.dp, if (primario) cs.primary.copy(alpha = 0.5f) else cs.outline.copy(alpha = 0.7f),
            RoundedCornerShape(14.dp)).clip(RoundedCornerShape(14.dp)).let { if (alTocar != null) it.clickable { alTocar() } else it },
    ) {
        Column(Modifier.padding(horizontal = 14.dp, vertical = 12.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                val ic = if (icono == "logo") null else iconoPorNombre(icono)
                if (icono == "logo") {
                    Logo(26.dp)
                    Spacer(Modifier.width(14.dp))
                } else if (ic != null) {
                    Icon(ic, null, tint = acento, modifier = Modifier.size(22.dp))
                    Spacer(Modifier.width(14.dp))
                }
                Column(Modifier.weight(1f)) {
                    if (titulo.isNotEmpty()) {
                        Text(titulo, style = MaterialTheme.typography.titleSmall, color = if (peligro) acento else cs.onSurface)
                    }
                    if (descripcion.isNotEmpty()) {
                        Text(descripcion, style = MaterialTheme.typography.bodySmall, color = cs.onSurfaceVariant)
                    }
                    if (valor.isNotEmpty() && derecha == null && valorDerecha.isEmpty()) {
                        Text(valor, style = MaterialTheme.typography.labelMedium.copy(fontWeight = FontWeight.SemiBold), color = acento)
                    }
                }
                if (derecha != null) {
                    Spacer(Modifier.width(10.dp))
                    derecha()
                } else if (valorDerecha.isNotEmpty()) {
                    Spacer(Modifier.width(10.dp))
                    Text(valorDerecha, style = MaterialTheme.typography.labelLarge, color = acento)
                }
            }
            if (nota.isNotEmpty()) {
                Text(nota, style = MaterialTheme.typography.bodySmall, color = cs.primary, modifier = Modifier.padding(top = 6.dp))
            }
            if (abajo != null) {
                Spacer(Modifier.height(8.dp))
                abajo()
            }
        }
    }
}

@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun Elemento(o: JSONObject) {
    val cs = MaterialTheme.colorScheme
    val id = o.optString("id")
    val titulo = o.optString("titulo")
    val desc = o.optString("descripcion")
    val icono = o.optString("icono")
    fun cambiar(v: String) = SatronhoUi.orden("ajuste|$id|$v")
    when (o.optString("tipo")) {
        "interruptor" -> {
            val v = o.optBoolean("valor")
            TarjetaItem(icono, titulo, desc, alTocar = { cambiar(if (v) "0" else "1") }, derecha = {
                Switch(checked = v, onCheckedChange = { cambiar(if (it) "1" else "0") })
            })
        }
        "opciones" -> {
            val ops = o.lista("opciones")
            val v = o.optInt("valor")
            val nota = o.optString("nota")
            val corto = ops.size <= 3 && ops.all { it.length <= 12 }
            if (corto) {
                TarjetaItem(icono, titulo, desc, abajo = {
                    Column {
                        SingleChoiceSegmentedButtonRow(Modifier.fillMaxWidth()) {
                            ops.forEachIndexed { i, s ->
                                SegmentedButton(
                                    selected = i == v, onClick = { cambiar(i.toString()) },
                                    shape = SegmentedButtonDefaults.itemShape(index = i, count = ops.size),
                                ) { Text(s, maxLines = 1, fontSize = 12.sp) }
                            }
                        }
                        if (nota.isNotEmpty()) Text(nota, style = MaterialTheme.typography.bodySmall, color = cs.primary, modifier = Modifier.padding(top = 6.dp))
                    }
                })
            } else {
                var abierto by remember { mutableStateOf(false) }
                TarjetaItem(icono, titulo, desc, valor = ops.getOrElse(v) { "" }, alTocar = { abierto = true }, derecha = {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Text(ops.getOrElse(v) { "" }, style = MaterialTheme.typography.labelMedium, color = cs.primary, maxLines = 2,
                            overflow = TextOverflow.Ellipsis, textAlign = TextAlign.End, modifier = Modifier.widthIn(max = 150.dp))
                        Icon(Icons.Filled.KeyboardArrowRight, null, tint = cs.onSurfaceVariant)
                    }
                }, nota = nota)
                if (abierto) {
                    AlertDialog(
                        onDismissRequest = { abierto = false },
                        confirmButton = { TextButton(onClick = { abierto = false }) { Text(t("Cerrar")) } },
                        title = { Text(titulo) },
                        text = {
                            Column(Modifier.heightIn(max = 420.dp).verticalScroll(rememberScrollState())) {
                                ops.forEachIndexed { i, s ->
                                    Row(
                                        Modifier.fillMaxWidth().clip(RoundedCornerShape(10.dp)).clickable { cambiar(i.toString()); abierto = false }
                                            .padding(vertical = 6.dp),
                                        verticalAlignment = Alignment.CenterVertically,
                                    ) {
                                        RadioButton(selected = i == v, onClick = { cambiar(i.toString()); abierto = false })
                                        Text(s, style = MaterialTheme.typography.bodyMedium)
                                    }
                                }
                            }
                        },
                    )
                }
            }
        }
        "deslizador" -> {
            val min = o.optDouble("min").toFloat()
            val max = o.optDouble("max").toFloat()
            val paso = o.optDouble("paso", 1.0).toFloat()
            val valor = o.optDouble("valor").toFloat()
            var local by remember(id) { mutableFloatStateOf(valor) }
            var arrastrando by remember { mutableStateOf(false) }
            LaunchedEffect(valor) { if (!arrastrando) local = valor }
            val fmt = o.optString("formato", "%.0f")
            val texto = try { String.format(java.util.Locale.ROOT, fmt, local) } catch (e: Exception) { local.toString() }
            val pasos = if (paso > 0) ((max - min) / paso).toInt() - 1 else 0
            TarjetaItem(icono, titulo, desc, derecha = {
                Text(texto, style = MaterialTheme.typography.labelLarge, color = cs.primary)
            }, abajo = {
                Slider(
                    value = local, valueRange = min..max, steps = pasos.coerceIn(0, 200),
                    onValueChange = { arrastrando = true; local = it },
                    onValueChangeFinished = { arrastrando = false; cambiar(local.toString()) },
                )
            })
        }
        "accion" -> {
            val confirmar = o.optString("confirmar")
            val estilo = o.optString("estilo")
            val valor = o.optString("valor")
            var preguntar by remember { mutableStateOf(false) }
            TarjetaItem(icono, titulo, desc, alTocar = { if (confirmar.isNotEmpty()) preguntar = true else cambiar("1") },
                peligro = estilo == "peligro", primario = estilo == "primario", valorDerecha = valor)
            if (preguntar) {
                AlertDialog(
                    onDismissRequest = { preguntar = false },
                    title = { Text(titulo) },
                    text = { Text(confirmar) },
                    confirmButton = { TextButton(onClick = { preguntar = false; cambiar("1") }) { Text(t("Sí")) } },
                    dismissButton = { TextButton(onClick = { preguntar = false }) { Text(t("No")) } },
                )
            }
        }
        "pagina" -> {
            val c = o.optString("color")
            TarjetaItem(icono, titulo, desc, valor = o.optString("valor"), color = if (c.isNotEmpty()) Leer.color(c, cs.primary) else null,
                alTocar = { SatronhoUi.abrirAjustes(o.optString("destino")) }, derecha = {
                    Icon(Icons.Filled.KeyboardArrowRight, null, tint = cs.onSurfaceVariant)
                })
        }
        "texto" -> {
            val c = o.optString("color")
            TarjetaItem(icono, titulo, desc, color = if (c.isNotEmpty()) Leer.color(c, cs.primary) else null)
        }
        "guia" -> GuiaDice(o.optString("guia"), o.optString("texto"))
        "consolas" -> {
            val arr = o.optJSONArray("opciones") ?: JSONArray()
            val actual = o.optString("valor")
            LazyRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                items(List(arr.length()) { arr.getJSONObject(it) }) { c ->
                    val col = Leer.color(c.optString("color"), cs.primary)
                    FilterChip(
                        selected = c.optString("id") == actual, onClick = { cambiar(c.optString("id")) },
                        label = { Text(c.optString("nombre")) },
                        leadingIcon = { Box(Modifier.size(8.dp).clip(CircleShape).background(col)) },
                        colors = FilterChipDefaults.filterChipColors(selectedContainerColor = col.copy(alpha = 0.24f)),
                    )
                }
            }
        }
        "pestanas" -> {
            val ops = o.lista("opciones")
            val v = o.optInt("valor")
            SingleChoiceSegmentedButtonRow(Modifier.fillMaxWidth()) {
                ops.forEachIndexed { i, s ->
                    SegmentedButton(selected = i == v, onClick = { cambiar(i.toString()) },
                        shape = SegmentedButtonDefaults.itemShape(index = i, count = ops.size)) { Text(s, maxLines = 1, fontSize = 12.sp) }
                }
            }
        }
        "control" -> ControlGrande(o)
        else -> {}
    }
}

/** Un guía habla: su carita, su nombre en negrita con su color y el texto en letra clara. */
@Composable
fun GuiaDice(guia: String, texto: String) {
    val color = Guias.color(guia)
    Surface(
        shape = RoundedCornerShape(16.dp), color = color.copy(alpha = 0.10f),
        modifier = Modifier.fillMaxWidth().border(1.dp, color.copy(alpha = 0.45f), RoundedCornerShape(16.dp)),
    ) {
        Row(Modifier.padding(12.dp), verticalAlignment = Alignment.Top) {
            Avatar(guia, 40.dp)
            Spacer(Modifier.width(12.dp))
            Column {
                Text(Guias.nombre(guia), style = MaterialTheme.typography.titleSmall.copy(fontWeight = FontWeight.Bold), color = color)
                Text(texto, style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurface)
            }
        }
    }
}

// ------------------------------------------------------------------ Controles: el control grande

/**
 * El control grande al centro y las asignaciones alrededor (como en NX): a la izquierda la cruceta y
 * los gatillos izquierdos; a la derecha los botones de acción y los gatillos derechos; abajo Select y
 * Start. Tocar una asignación y apretar el botón del mando la cambia. Lo que aprietas se ilumina.
 */
@Composable
private fun ControlGrande(o: JSONObject) {
    val cs = MaterialTheme.colorScheme
    val color = Leer.color(o.optString("color"), cs.primary)
    val arr = o.optJSONArray("botones") ?: JSONArray()
    val botones = List(arr.length()) { arr.getJSONObject(it) }
    fun zona(vararg z: String) = botones.filter { it.optString("zona") in z }
    Surface(
        shape = RoundedCornerShape(18.dp), color = cs.surface,
        modifier = Modifier.fillMaxWidth().border(1.dp, color.copy(alpha = 0.4f), RoundedCornerShape(18.dp)),
    ) {
        Column(Modifier.padding(12.dp), horizontalAlignment = Alignment.CenterHorizontally) {
            Text(o.optString("consola"), style = MaterialTheme.typography.titleSmall, color = color)
            Spacer(Modifier.height(10.dp))
            // Gatillos arriba.
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                Column(verticalArrangement = Arrangement.spacedBy(6.dp)) { zona("gatillo_izq").forEach { Asignacion(it, color) } }
                Column(verticalArrangement = Arrangement.spacedBy(6.dp), horizontalAlignment = Alignment.End) {
                    zona("gatillo_der").forEach { Asignacion(it, color) }
                }
            }
            Spacer(Modifier.height(8.dp))
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(6.dp)) { zona("cruceta").forEach { Asignacion(it, color) } }
                // El control dibujado al centro.
                DibujoControl(botones, color, Modifier.width(120.dp).height(86.dp))
                Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(6.dp), horizontalAlignment = Alignment.End) {
                    zona("accion").forEach { Asignacion(it, color) }
                }
            }
            Spacer(Modifier.height(8.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) { zona("centro").forEach { Asignacion(it, color) } }
            val ultimo = o.optString("ultimo")
            Spacer(Modifier.height(8.dp))
            Text(
                if (ultimo.isEmpty()) t("Aprieta cualquier botón para verlo en vivo.") else t("Último botón") + ": " + ultimo,
                style = MaterialTheme.typography.bodySmall, color = cs.onSurfaceVariant,
            )
        }
    }
}

@Composable
private fun Asignacion(b: JSONObject, color: Color) {
    val cs = MaterialTheme.colorScheme
    val apretado = b.optBoolean("apretado")
    val capturando = b.optBoolean("capturando")
    Row(
        Modifier.clip(RoundedCornerShape(10.dp))
            .background(if (apretado) color.copy(alpha = 0.35f) else if (capturando) cs.primary.copy(alpha = 0.25f) else cs.surfaceVariant)
            .clickable { SatronhoUi.orden("ajuste|boton${b.optInt("id")}|1") }
            .padding(horizontal = 8.dp, vertical = 5.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(b.optString("nombre"), style = MaterialTheme.typography.labelSmall, color = cs.onSurfaceVariant)
        Spacer(Modifier.width(6.dp))
        Text(b.optString("asignado"), style = MaterialTheme.typography.labelMedium.copy(fontWeight = FontWeight.Bold),
            color = if (capturando) cs.primary else cs.onSurface, maxLines = 1)
    }
}

@Composable
private fun DibujoControl(botones: List<JSONObject>, color: Color, modifier: Modifier) {
    val apretados = botones.filter { it.optBoolean("apretado") }.map { it.optString("zona") }.toSet()
    androidx.compose.foundation.Canvas(modifier) {
        val w = size.width
        val h = size.height
        val cuerpo = androidx.compose.ui.geometry.Size(w, h * 0.78f)
        drawRoundRect(color.copy(alpha = 0.22f), topLeft = androidx.compose.ui.geometry.Offset(0f, h * 0.12f), size = cuerpo,
            cornerRadius = androidx.compose.ui.geometry.CornerRadius(h * 0.35f))
        drawRoundRect(color.copy(alpha = 0.6f), topLeft = androidx.compose.ui.geometry.Offset(0f, h * 0.12f), size = cuerpo,
            cornerRadius = androidx.compose.ui.geometry.CornerRadius(h * 0.35f),
            style = androidx.compose.ui.graphics.drawscope.Stroke(width = 2f))
        // Cruceta.
        val cx = w * 0.25f
        val cy = h * 0.5f
        val a = h * 0.09f
        val l = h * 0.26f
        val cruz = if ("cruceta" in apretados) Color.White else color.copy(alpha = 0.9f)
        drawRect(cruz, topLeft = androidx.compose.ui.geometry.Offset(cx - l, cy - a), size = androidx.compose.ui.geometry.Size(l * 2, a * 2))
        drawRect(cruz, topLeft = androidx.compose.ui.geometry.Offset(cx - a, cy - l), size = androidx.compose.ui.geometry.Size(a * 2, l * 2))
        // Botones de acción.
        val bx = w * 0.75f
        val r = h * 0.075f
        val accion = if ("accion" in apretados) Color.White else color.copy(alpha = 0.9f)
        drawCircle(accion, r, androidx.compose.ui.geometry.Offset(bx, cy - r * 2))
        drawCircle(accion, r, androidx.compose.ui.geometry.Offset(bx, cy + r * 2))
        drawCircle(accion, r, androidx.compose.ui.geometry.Offset(bx - r * 2, cy))
        drawCircle(accion, r, androidx.compose.ui.geometry.Offset(bx + r * 2, cy))
        // Select y Start.
        val centro = if ("centro" in apretados) Color.White else color.copy(alpha = 0.7f)
        drawRoundRect(centro, topLeft = androidx.compose.ui.geometry.Offset(w * 0.42f, h * 0.62f), size = androidx.compose.ui.geometry.Size(w * 0.07f, h * 0.05f))
        drawRoundRect(centro, topLeft = androidx.compose.ui.geometry.Offset(w * 0.51f, h * 0.62f), size = androidx.compose.ui.geometry.Size(w * 0.07f, h * 0.05f))
        // Gatillos.
        val gi = if ("gatillo_izq" in apretados) Color.White else color.copy(alpha = 0.7f)
        val gd = if ("gatillo_der" in apretados) Color.White else color.copy(alpha = 0.7f)
        drawRoundRect(gi, topLeft = androidx.compose.ui.geometry.Offset(w * 0.1f, 0f), size = androidx.compose.ui.geometry.Size(w * 0.22f, h * 0.1f),
            cornerRadius = androidx.compose.ui.geometry.CornerRadius(h * 0.05f))
        drawRoundRect(gd, topLeft = androidx.compose.ui.geometry.Offset(w * 0.68f, 0f), size = androidx.compose.ui.geometry.Size(w * 0.22f, h * 0.1f),
            cornerRadius = androidx.compose.ui.geometry.CornerRadius(h * 0.05f))
    }
}

// ------------------------------------------------------------------ conversación con los guías

@Composable
private fun Chat(o: JSONObject) {
    val cs = MaterialTheme.colorScheme
    val arr = o.optJSONArray("mensajes") ?: JSONArray()
    val mensajes = List(arr.length()) { arr.getJSONObject(it) }
    val lista = rememberLazyListState()
    LaunchedEffect(mensajes.size) { if (mensajes.isNotEmpty()) lista.animateScrollToItem(mensajes.size - 1) }
    var texto by remember { mutableStateOf("") }
    Column(Modifier.fillMaxSize().imePadding()) {
        LazyColumn(
            state = lista, modifier = Modifier.weight(1f).fillMaxWidth(),
            contentPadding = PaddingValues(14.dp), verticalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            items(mensajes.size) { i ->
                val m = mensajes[i]
                val guia = m.optString("guia")
                if (guia.isEmpty()) {
                    Box(Modifier.fillMaxWidth(), contentAlignment = Alignment.CenterEnd) {
                        Surface(shape = RoundedCornerShape(16.dp), color = cs.primary.copy(alpha = 0.22f)) {
                            Text(m.optString("texto"), modifier = Modifier.padding(horizontal = 12.dp, vertical = 8.dp))
                        }
                    }
                } else {
                    Column {
                        GuiaDice(guia, m.optString("texto"))
                        val bs = m.lista("botones")
                        if (bs.isNotEmpty()) {
                            @OptIn(ExperimentalLayoutApi::class)
                            FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.padding(top = 6.dp, start = 52.dp)) {
                                bs.forEachIndexed { j, b ->
                                    Surface(
                                        shape = RoundedCornerShape(18.dp), color = cs.surfaceVariant,
                                        modifier = Modifier.padding(bottom = 6.dp).clip(RoundedCornerShape(18.dp))
                                            .clickable { SatronhoUi.orden("chat_boton|$i|$j") },
                                    ) { Text(b, color = cs.primary, style = MaterialTheme.typography.labelLarge, modifier = Modifier.padding(horizontal = 12.dp, vertical = 7.dp)) }
                                }
                            }
                        }
                    }
                }
            }
        }
        Row(
            Modifier.fillMaxWidth().padding(12.dp).clip(RoundedCornerShape(24.dp)).border(1.dp, cs.outline, RoundedCornerShape(24.dp))
                .padding(start = 16.dp, end = 4.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            val enviar = {
                if (texto.isNotBlank()) {
                    SatronhoUi.orden("preguntar|${texto.replace('|', '/')}")
                    texto = ""
                }
            }
            Box(Modifier.weight(1f).padding(vertical = 12.dp)) {
                if (texto.isEmpty()) Text(t("Escribe tu pregunta…"), color = cs.onSurfaceVariant)
                BasicTextField(
                    value = texto, onValueChange = { texto = it },
                    textStyle = MaterialTheme.typography.bodyLarge.copy(color = cs.onSurface),
                    cursorBrush = androidx.compose.ui.graphics.SolidColor(cs.primary),
                    keyboardOptions = KeyboardOptions(imeAction = ImeAction.Send),
                    keyboardActions = KeyboardActions(onSend = { enviar() }),
                    modifier = Modifier.fillMaxWidth(),
                )
            }
            IconButton(onClick = enviar) { Icon(Icons.Filled.Send, t("Enviar"), tint = cs.primary) }
        }
    }
}
