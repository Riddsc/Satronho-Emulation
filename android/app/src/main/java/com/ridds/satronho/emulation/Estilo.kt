// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
package com.ridds.satronho.emulation

import android.content.res.AssetManager
import android.graphics.BitmapFactory
import android.util.LruCache
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.Notifications
import androidx.compose.material.icons.filled.Star
import androidx.compose.material.icons.filled.Build
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.Share
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.Lock
import androidx.compose.material.icons.filled.Check
import androidx.compose.material.icons.filled.KeyboardArrowRight
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Typography
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.graphics.vector.addPathNodes
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.text.ExperimentalTextApi
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.File

// ------------------------------------------------------------------ colores y letras

/** Colores fijos de los guías (los mismos de PC, en todos los temas). */
object Guias {
    fun color(clave: String): Color = when (clave) {
        "samy" -> Color(0xFFFFB23E)
        "troya" -> Color(0xFF9B8CFF)
        "nono" -> Color(0xFF4FD1C5)
        "juana" -> Color(0xFFFF8FB5)
        else -> Color(0xFF74D14C) // MechaRidds
    }

    fun nombre(clave: String): String = when (clave) {
        "samy" -> "Samy"
        "troya" -> "Troya"
        "nono" -> "Ñoño"
        "juana" -> "Juana"
        else -> "MechaRidds"
    }
}

private var poppins: FontFamily? = null

@OptIn(ExperimentalTextApi::class)
fun letras(assets: AssetManager): FontFamily {
    poppins?.let { return it }
    val f = try {
        FontFamily(
            Font("fonts/Poppins-Regular.ttf", assets, FontWeight.Normal),
            Font("fonts/Poppins-Medium.ttf", assets, FontWeight.Medium),
            Font("fonts/Poppins-SemiBold.ttf", assets, FontWeight.SemiBold),
            Font("fonts/Poppins-Bold.ttf", assets, FontWeight.Bold),
        )
    } catch (e: Throwable) {
        FontFamily.Default
    }
    poppins = f
    return f
}

@Composable
fun SatronhoTema(tema: Tema, assets: AssetManager, contenido: @Composable () -> Unit) {
    val base = if (tema.claro) lightColorScheme() else darkColorScheme()
    val colores = base.copy(
        primary = tema.acento,
        onPrimary = if (tema.claro) Color.White else Color(0xFF15121F),
        secondary = tema.acento,
        background = tema.fondo,
        onBackground = tema.texto,
        surface = tema.superficie,
        onSurface = tema.texto,
        surfaceVariant = tema.superficie2,
        onSurfaceVariant = tema.tenue,
        surfaceContainer = tema.superficie,
        surfaceContainerHigh = tema.superficie2,
        surfaceContainerHighest = tema.superficie2,
        surfaceContainerLow = tema.superficie,
        outline = tema.borde,
        outlineVariant = tema.borde,
    )
    val f = letras(assets)
    val t = Typography()
    fun TextStyle.p() = copy(fontFamily = f)
    val tipos = Typography(
        displayLarge = t.displayLarge.p(), displayMedium = t.displayMedium.p(), displaySmall = t.displaySmall.p(),
        headlineLarge = t.headlineLarge.p(), headlineMedium = t.headlineMedium.p(), headlineSmall = t.headlineSmall.p(),
        titleLarge = t.titleLarge.p().copy(fontWeight = FontWeight.Bold, fontSize = 22.sp),
        titleMedium = t.titleMedium.p().copy(fontWeight = FontWeight.SemiBold),
        titleSmall = t.titleSmall.p().copy(fontWeight = FontWeight.SemiBold),
        bodyLarge = t.bodyLarge.p(), bodyMedium = t.bodyMedium.p(), bodySmall = t.bodySmall.p(),
        labelLarge = t.labelLarge.p().copy(fontWeight = FontWeight.SemiBold),
        labelMedium = t.labelMedium.p(), labelSmall = t.labelSmall.p(),
    )
    MaterialTheme(colorScheme = colores, typography = tipos, content = contenido)
}

// ------------------------------------------------------------------ íconos
// Los que no trae el paquete básico de Material (mismos trazos de Material Icons, licencia Apache 2.0).

private fun icono(nombre: String, d: String): ImageVector =
    ImageVector.Builder(name = nombre, defaultWidth = 24.dp, defaultHeight = 24.dp, viewportWidth = 24f, viewportHeight = 24f)
        .addPath(pathData = addPathNodes(d), fill = SolidColor(Color.Black))
        .build()

object Iconos {
    val monitor by lazy {
        icono("monitor", "M21 2H3c-1.1 0-2 .9-2 2v12c0 1.1.9 2 2 2h7v2H8v2h8v-2h-2v-2h7c1.1 0 2-.9 2-2V4c0-1.1-.9-2-2-2zm0 14H3V4h18v12z")
    }
    val sonido by lazy {
        icono("sonido", "M3 9v6h4l5 5V4L7 9H3zm13.5 3c0-1.77-1.02-3.29-2.5-4.03v8.05c1.48-.73 2.5-2.25 2.5-4.02zM14 3.23v2.06c2.89.86 5 3.54 5 6.71s-2.11 5.85-5 6.71v2.06c4.01-.91 7-4.49 7-8.77s-2.99-7.86-7-8.77z")
    }
    val rapido by lazy { icono("rapido", "M4 18l8.5-6L4 6v12zm9-12v12l8.5-6L13 6z") }
    val medidor by lazy { icono("medidor", "M5 9.2h3V19H5zM10.6 5h2.8v14h-2.8zm5.6 8H19v6h-2.8z") }
    val chip by lazy {
        icono("chip", "M15 9H9v6h6V9zm-2 4h-2v-2h2v2zm8-2V9h-2V7c0-1.1-.9-2-2-2h-2V3h-2v2h-2V3H9v2H7c-1.1 0-2 .9-2 2v2H3v2h2v2H3v2h2v2c0 1.1.9 2 2 2h2v2h2v-2h2v2h2v-2h2c1.1 0 2-.9 2-2v-2h2v-2h-2v-2h2zm-4 6H7V7h10v10z")
    }
    val paleta by lazy {
        icono("paleta", "M12 3c-4.97 0-9 4.03-9 9s4.03 9 9 9c.83 0 1.5-.67 1.5-1.5 0-.39-.15-.74-.39-1.01-.23-.26-.38-.61-.38-.99 0-.83.67-1.5 1.5-1.5H16c2.76 0 5-2.24 5-5 0-4.42-4.03-8-9-8zm-5.5 9c-.83 0-1.5-.67-1.5-1.5S5.67 9 6.5 9 8 9.67 8 10.5 7.33 12 6.5 12zm3-4C8.67 8 8 7.33 8 6.5S8.67 5 9.5 5s1.5.67 1.5 1.5S10.33 8 9.5 8zm5 0c-.83 0-1.5-.67-1.5-1.5S13.67 5 14.5 5s1.5.67 1.5 1.5S15.33 8 14.5 8zm3 4c-.83 0-1.5-.67-1.5-1.5S16.67 9 17.5 9s1.5.67 1.5 1.5-.67 1.5-1.5 1.5z")
    }
    val tactil by lazy {
        icono("tactil", "M9 11.24V7.5C9 6.12 10.12 5 11.5 5S14 6.12 14 7.5v3.74c1.21-.81 2-2.18 2-3.74C16 5.01 13.99 3 11.5 3S7 5.01 7 7.5c0 1.56.79 2.93 2 3.74zm9.84 4.63l-4.54-2.26c-.17-.07-.35-.11-.54-.11H13v-6c0-.83-.67-1.5-1.5-1.5S10 6.67 10 7.5v10.74l-3.43-.72c-.08-.01-.15-.03-.24-.03-.31 0-.59.13-.79.33l-.79.8 4.94 4.94c.27.27.65.44 1.06.44h6.79c.75 0 1.33-.55 1.44-1.28l.75-5.27c.01-.07.02-.14.02-.2 0-.62-.38-1.16-.91-1.38z")
    }
    val chat by lazy {
        icono("chat", "M20 2H4c-1.1 0-1.99.9-1.99 2L2 22l4-4h14c1.1 0 2-.9 2-2V4c0-1.1-.9-2-2-2zM6 9h12v2H6V9zm8 5H6v-2h8v2zm4-6H6V6h12v2z")
    }
    val imagen by lazy {
        icono("imagen", "M21 19V5c0-1.1-.9-2-2-2H5c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h14c1.1 0 2-.9 2-2zM8.5 13.5l2.5 3.01L14.5 12l4.5 6H5l3.5-4.5z")
    }
    val idioma by lazy {
        icono("idioma", "M12.87 15.07l-2.54-2.51.03-.03c1.74-1.94 2.98-4.17 3.71-6.53H17V4h-7V2H8v2H1v1.99h11.17C11.5 7.92 10.44 9.75 9 11.35 8.07 10.32 7.3 9.19 6.69 8h-2c.73 1.63 1.73 3.17 2.98 4.56l-5.09 5.02L4 19l5-5 3.11 3.11.76-2.04zM18.5 10h-2L12 22h2l1.12-3h4.75L21 22h2l-4.5-12zm-2.62 7l1.62-4.33L19.12 17h-3.24z")
    }
    val memoria by lazy {
        icono("memoria", "M18 2h-8L4.02 8 4 20c0 1.1.9 2 2 2h12c1.1 0 2-.9 2-2V4c0-1.1-.9-2-2-2zm-6 6h-2V4h2v4zm3 0h-2V4h2v4zm3 0h-2V4h2v4z")
    }
    val carpetaArriba by lazy {
        icono("carpeta_arriba", "M20 6h-8l-2-2H4c-1.1 0-1.99.9-1.99 2L2 18c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2zm-8 3l4 4h-3v4h-2v-4H8l4-4z")
    }
    val ojo by lazy {
        icono("ojo", "M12 4.5C7 4.5 2.73 7.61 1 12c1.73 4.39 6 7.5 11 7.5s9.27-3.11 11-7.5c-1.73-4.39-6-7.5-11-7.5zM12 17c-2.76 0-5-2.24-5-5s2.24-5 5-5 5 2.24 5 5-2.24 5-5 5zm0-8c-1.66 0-3 1.34-3 3s1.34 3 3 3 3-1.34 3-3-1.34-3-3-3z")
    }
    val filtro by lazy { icono("filtro", "M10 18h4v-2h-4v2zM3 6v2h18V6H3zm3 7h12v-2H6v2z") }
    val carpeta by lazy { icono("carpeta", "M10 4H4c-1.1 0-1.99.9-1.99 2L2 18c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2h-8l-2-2z") }
    val pausa by lazy { icono("pausa", "M6 19h4V5H6v14zm8-14v14h4V5h-4z") }
    val guardar by lazy {
        icono("guardar", "M17 3H5c-1.11 0-2 .9-2 2v14c0 1.1.89 2 2 2h14c1.1 0 2-.9 2-2V7l-4-4zm-5 16c-1.66 0-3-1.34-3-3s1.34-3 3-3 3 1.34 3 3-1.34 3-3 3zm3-10H5V5h10v4z")
    }
    val cargar by lazy { icono("cargar", "M5 20h14v-2H5v2zM19 9h-4V3H9v6H5l7 7 7-7z") }
    val mando by lazy {
        icono("mando", "M21 6H3c-1.1 0-2 .9-2 2v8c0 1.1.9 2 2 2h18c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2zm-10 7H8v3H6v-3H3v-2h3V8h2v3h3v2zm4.5 2c-.83 0-1.5-.67-1.5-1.5s.67-1.5 1.5-1.5 1.5.67 1.5 1.5-.67 1.5-1.5 1.5zm4-3c-.83 0-1.5-.67-1.5-1.5S18.67 9 19.5 9s1.5.67 1.5 1.5-.67 1.5-1.5 1.5z")
    }
    val cuadricula by lazy { icono("cuadricula", "M4 11h5V5H4v6zm0 7h5v-6H4v6zm6 0h5v-6h-5v6zm6 0h5v-6h-5v6zm-6-7h5V5h-5v6zm6-6v6h5V5h-5z") }
    val lista by lazy { icono("lista", "M3 13h2v-2H3v2zm0 4h2v-2H3v2zm0-8h2V7H3v2zm4 4h14v-2H7v2zm0 4h14v-2H7v2zM7 7v2h14V7H7z") }
    val salir by lazy {
        icono("salir", "M10.09 15.59L11.5 17l5-5-5-5-1.41 1.41L12.67 11H3v2h9.67l-2.58 2.59zM19 3H5c-1.11 0-2 .9-2 2v4h2V5h14v14H5v-4H3v4c0 1.1.89 2 2 2h14c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2z")
    }
    val ajustesJuego by lazy {
        icono("ajustes_juego", "M3 17v2h6v-2H3zM3 5v2h10V5H3zm10 16v-2h8v-2h-8v-2h-2v6h2zM7 9v2H3v2h4v2h2V9H7zm14 4v-2H11v2h10zm-6-4h2V7h4V5h-4V3h-2v6z")
    }
    val patita by lazy {
        icono("patita", "M4.5 12a2.5 2.5 0 1 0 0-5 2.5 2.5 0 0 0 0 5zm4.5-4a2.5 2.5 0 1 0 0-5 2.5 2.5 0 0 0 0 5zm6 0a2.5 2.5 0 1 0 0-5 2.5 2.5 0 0 0 0 5zm4.5 4a2.5 2.5 0 1 0 0-5 2.5 2.5 0 0 0 0 5zm-2.83 2.86c-.87-1.02-1.6-1.89-2.48-2.91-.46-.54-1.05-1.08-1.75-1.32-.11-.04-.22-.07-.33-.09-.25-.04-.52-.04-.78-.04s-.53 0-.79.05c-.11.02-.22.05-.33.09-.7.24-1.28.78-1.75 1.32-.87 1.02-1.6 1.89-2.48 2.91-1.31 1.31-2.92 2.76-2.62 4.79.29 1.02 1.02 2.03 2.33 2.32.73.15 3.06-.44 5.54-.44h.18c2.48 0 4.81.58 5.54.44 1.31-.29 2.04-1.31 2.33-2.32.31-2.04-1.3-3.49-2.61-4.8z")
    }
}

/** Ícono por nombre (los que manda C++ en los Ajustes). */
fun iconoPorNombre(n: String): ImageVector? = when (n) {
    "ajustes" -> Icons.Filled.Settings
    "novedades" -> Icons.Filled.Notifications
    "sugerida", "estrella" -> Icons.Filled.Star
    "chat" -> Iconos.chat
    "medidor" -> Iconos.medidor
    "graficos", "resolucion" -> Iconos.monitor
    "sonido" -> Iconos.sonido
    "mando" -> Iconos.mando
    "tactil" -> Iconos.tactil
    "rendimiento" -> Iconos.rapido
    "nucleo", "chip" -> Iconos.chip
    "carpeta" -> Iconos.carpeta
    "patita" -> Iconos.patita
    "paleta" -> Iconos.paleta
    "herramientas" -> Icons.Filled.Build
    "info" -> Icons.Filled.Info
    "compartir" -> Icons.Filled.Share
    "refrescar" -> Icons.Filled.Refresh
    "agregar" -> Icons.Filled.Add
    "borrar" -> Icons.Filled.Delete
    "imagen" -> Iconos.imagen
    "ojo" -> Iconos.ojo
    "candado" -> Icons.Filled.Lock
    "cargar" -> Iconos.cargar
    "guardar" -> Iconos.memoria
    "check" -> Icons.Filled.Check
    "flecha" -> Icons.Filled.KeyboardArrowRight
    "idioma" -> Iconos.idioma
    "ajustes_juego" -> Iconos.ajustesJuego
    else -> null
}

/** El logo de Satronho Emulation (planeta con los guías). */
@Composable
fun Logo(tam: Dp, modifier: Modifier = Modifier) {
    val img = Imagenes.cargar("asset:logo.png", 512)
    if (img != null) Image(img, contentDescription = "Satronho Emulation", modifier = modifier.size(tam))
    else Box(modifier.size(tam))
}

// ------------------------------------------------------------------ imágenes

/**
 * Carátulas (archivos) y guías (assets), decodificadas en segundo plano y achicadas a lo que se
 * muestra, con una caché chica para que la lista vaya fluida.
 */
object Imagenes {
    private val cache = object : LruCache<String, ImageBitmap>(48 * 1024 * 1024) {
        override fun sizeOf(key: String, value: ImageBitmap) = value.width * value.height * 4
    }
    lateinit var assets: AssetManager

    private fun decodificar(ruta: String, lado: Int): ImageBitmap? {
        return try {
            val opts = BitmapFactory.Options().apply { inJustDecodeBounds = true }
            val asset = ruta.startsWith("asset:")
            if (asset) assets.open(ruta.removePrefix("asset:")).use { BitmapFactory.decodeStream(it, null, opts) }
            else BitmapFactory.decodeFile(ruta, opts)
            var muestra = 1
            while (opts.outWidth / (muestra * 2) >= lado && opts.outHeight / (muestra * 2) >= lado) muestra *= 2
            val o2 = BitmapFactory.Options().apply { inSampleSize = muestra }
            val bmp = if (asset) assets.open(ruta.removePrefix("asset:")).use { BitmapFactory.decodeStream(it, null, o2) }
            else BitmapFactory.decodeFile(ruta, o2)
            bmp?.asImageBitmap()
        } catch (e: Throwable) {
            null
        }
    }

    @Composable
    fun cargar(ruta: String, lado: Int = 360): ImageBitmap? {
        if (ruta.isEmpty()) return null
        val clave = "$ruta@$lado"
        var img by remember(clave) { mutableStateOf(cache.get(clave)) }
        LaunchedEffect(clave) {
            if (img == null) {
                val leida = withContext(Dispatchers.IO) {
                    if (!ruta.startsWith("asset:") && !File(ruta).exists()) null else decodificar(ruta, lado)
                }
                if (leida != null) {
                    cache.put(clave, leida)
                    img = leida
                }
            }
        }
        return img
    }
}

/** La carita redonda de un guía. */
@Composable
fun Avatar(guia: String, tam: Dp, modifier: Modifier = Modifier) {
    val img = Imagenes.cargar("asset:guias/$guia.png", 160)
    Box(
        modifier.clip(CircleShape).background(Guias.color(guia).copy(alpha = 0.25f)),
        contentAlignment = Alignment.Center,
    ) {
        if (img != null) {
            Image(img, contentDescription = Guias.nombre(guia), contentScale = ContentScale.Crop,
                modifier = Modifier.size(tam))
        } else {
            Box(Modifier.size(tam))
        }
    }
}
