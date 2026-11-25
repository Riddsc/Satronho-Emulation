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
