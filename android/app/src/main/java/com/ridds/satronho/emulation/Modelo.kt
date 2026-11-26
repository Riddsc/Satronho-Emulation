// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
package com.ridds.satronho.emulation

import androidx.compose.ui.graphics.Color
import org.json.JSONArray
import org.json.JSONObject

// Lo que C++ le cuenta a las pantallas nativas (ver NativeStateJson y NativeLibraryJson en
// src/app_native.cpp). Todo llega en JSON y aquí se pasa a clases de Kotlin.

data class Tema(
    val fondo: Color = Color(0xFF0F0D17),
    val superficie: Color = Color(0xFF1B1829),
    val superficie2: Color = Color(0xFF241F38),
    val borde: Color = Color(0xFF2E2848),
    val texto: Color = Color(0xFFEBE8F5),
    val tenue: Color = Color(0xFFA9A4C2),
    val acento: Color = Color(0xFF9B8CFF),
    val claro: Boolean = false,
    val marca: Float = 0.18f,
)

data class Cargando(
    val titulo: String, val consola: String, val color: Color, val portada: String, val avance: Float,
    val tono: Int = 0, val forma: String = "nes", val etiqueta: String = "",
)

data class JuegoActual(
    val titulo: String, val clave: String, val consola: String, val sistema: String, val color: Color,
    val pausa: Boolean, val menu: Boolean, val nucleo: String, val ranura: Int,
)

/** Un botón del control en pantalla. x en proporción del ancho; y y r en proporción del alto. */
data class BotonTactil(val id: Int, val etiqueta: String, val grupo: String, val x: Float, val y: Float, val r: Float)

data class Tactil(
    val visible: Boolean = false, val modo: Int = 0, val opacidad: Float = 0.6f, val vibrar: Boolean = true,
    val solo4: Boolean = false, val botones: List<BotonTactil> = emptyList(), val editar: Boolean = false,
)

/** Una pantalla de Ajustes armada por C++ (NativeSettingsJson). Los elementos quedan en JSON. */
data class Seccion(val titulo: String, val descripcion: String, val items: List<JSONObject>)
data class Pagina(val pagina: String, val titulo: String, val subtitulo: String, val icono: String, val secciones: List<Seccion>)

data class Confirmar(val tipo: Int, val guia: String, val titulo: String, val texto: String, val si: String, val no: String, val noPreguntar: String)
data class Bienvenida(val paso: Int, val guia: String, val titulo: String, val texto: String)
data class Progreso(
    val id: String, val guia: String, val texto: String, val avance: Float, val trabajando: Boolean,
    val detalles: List<String>, val botones: List<Pair<String, String>>, val nota: String,
)
data class Zip(val titulo: String, val opciones: List<Pair<String, String>>)
data class Medidor(val texto: String, val esquina: Int, val color: Color, val lento: Boolean)

data class Estado(
    val modo: String = "biblioteca",
    val imgui: Boolean = false,
    val build: Int = 0,
    val guias: Boolean = true,
    val idioma: String = "",
    val tema: Tema = Tema(),
    val cargando: Cargando? = null,
    val juego: JuegoActual? = null,
    val tactil: Tactil = Tactil(),
    val ajustes: Pagina? = null,
    val confirmar: Confirmar? = null,
    val bienvenida: Bienvenida? = null,
    val novedad: String = "",
    val zip: Zip? = null,
    val progreso: Progreso? = null,
    val capturando: Boolean = false,
    val medidor: Medidor? = null,
    val chip: String = "",
    val avisoCalor: Boolean = true,
    val novedadTitulo: String = "",
    val actualizacion: Actualizacion? = null,
) {
    val enJuego get() = modo == "juego"
}

/** Una build nueva del hub: «disponible», «bajando» (con su avance) o «instalando». */
data class Actualizacion(val build: Int, val actual: Int, val estado: String, val avance: Float)

data class Juego(
    val clave: String, val titulo: String, val sistema: String, val etiquetas: String, val portada: String,
    val favorito: Boolean, val oculto: Boolean, val tiempo: Long, val tiempoTexto: String, val ultima: Long,
    val ultimaTexto: String, val continuar: Boolean, val tono: Int,
    /** Juegos de Satronho NX: la ruta del archivo (NX lo abre) y su programa (lo dice NX). */
    val ruta: String = "", val programa: String = "",
    /** Sello de la última sesión («60 fps»), como en Satronho NX. */
    val sello: String = "",
)

data class Consola(
    val id: String, val nombre: String, val completo: String, val pista: String, val juegosDe: String,
    val color: Color, val cuantos: Int, val forma: String = "nes", val etiqueta: String = "",
)

data class Consejo(val guia: String, val texto: String)

data class Biblioteca(
    val juegos: List<Juego> = emptyList(),
    val consolas: List<Consola> = emptyList(),
    val consejos: List<Consejo> = emptyList(),
    val buscando: Boolean = false,
    val carpetas: Int = 0,
    /** Claves y firmware de la Switch que el hub encontró en el teléfono (para el asistente de NX). */
    val nxClaves: List<String> = emptyList(),
    val nxFirmware: List<String> = emptyList(),
) {
    fun consola(id: String) = consolas.firstOrNull { it.id == id }
}

/** Aviso corto de un guía («Juego reiniciado.»). */
data class Aviso(val guia: String, val texto: String, val segundos: Double, val nacido: Long = System.currentTimeMillis())

/** Tarjeta con botones (Ñoño ofrece bajar la resolución, un consejo…). */
data class Tarjeta(val id: String, val guia: String, val titulo: String, val texto: String, val botones: List<Pair<String, String>>)

object Leer {
    fun color(s: String?, def: Color): Color {
        if (s.isNullOrEmpty()) return def
        return try { Color(android.graphics.Color.parseColor(s)) } catch (e: Exception) { def }
    }

    fun estado(texto: String): Estado {
        if (texto.isBlank()) return Estado()
        val o = JSONObject(texto)
        val tm = o.optJSONObject("tema")
        val tema = if (tm == null) Tema() else Tema(
            fondo = color(tm.optString("fondo"), Tema().fondo),
            superficie = color(tm.optString("superficie"), Tema().superficie),
            superficie2 = color(tm.optString("superficie2"), Tema().superficie2),
            borde = color(tm.optString("borde"), Tema().borde),
            texto = color(tm.optString("texto"), Tema().texto),
            tenue = color(tm.optString("tenue"), Tema().tenue),
            acento = color(tm.optString("acento"), Tema().acento),
            claro = tm.optBoolean("claro"),
            marca = tm.optDouble("marca", 0.18).toFloat(),
        )
        val c = o.optJSONObject("cargando")
        val cargando = c?.let {
            Cargando(it.optString("titulo"), it.optString("consola"), color(it.optString("color"), tema.acento),
                it.optString("portada"), it.optDouble("avance", -1.0).toFloat(), it.optInt("tono"), it.optString("forma", "nes"),
                it.optString("etiqueta"))
        }
        val j = o.optJSONObject("juego")
        val juego = j?.let {
            JuegoActual(it.optString("titulo"), it.optString("clave"), it.optString("consola"), it.optString("sistema"),
                color(it.optString("color"), tema.acento), it.optBoolean("pausa"), it.optBoolean("menu"),
                it.optString("nucleo"), it.optInt("ranura", 1))
        }
        val tc = o.optJSONObject("tactil")
        val tactil = if (tc == null) Tactil() else {
            val lista = ArrayList<BotonTactil>()
            val arr = tc.optJSONArray("botones") ?: JSONArray()
            for (i in 0 until arr.length()) {
                val b = arr.getJSONObject(i)
                lista.add(BotonTactil(b.optInt("id"), b.optString("etiqueta"), b.optString("grupo"),
                    b.optDouble("x").toFloat(), b.optDouble("y").toFloat(), b.optDouble("r").toFloat()))
            }
            Tactil(tc.optBoolean("visible"), tc.optInt("modo"), tc.optDouble("opacidad", 0.6).toFloat(),
                tc.optBoolean("vibrar", true), tc.optBoolean("solo4"), lista, tc.optBoolean("editar"))
        }
        val aj = o.optJSONObject("ajustes")
        val pagina = aj?.let { a ->
            val secs = ArrayList<Seccion>()
            val sa = a.optJSONArray("secciones") ?: JSONArray()
            for (i in 0 until sa.length()) {
                val so = sa.getJSONObject(i)
                val items = ArrayList<JSONObject>()
                val ia = so.optJSONArray("items") ?: JSONArray()
                for (k in 0 until ia.length()) items.add(ia.getJSONObject(k))
                secs.add(Seccion(so.optString("titulo"), so.optString("descripcion"), items))
            }
            Pagina(a.optString("pagina"), a.optString("titulo"), a.optString("subtitulo"), a.optString("icono"), secs)
        }
        val cf = o.optJSONObject("confirmar")?.let {
            Confirmar(it.optInt("tipo"), it.optString("guia"), it.optString("titulo"), it.optString("texto"), it.optString("si"),
                it.optString("no"), it.optString("no_preguntar"))
        }
        val bv = o.optJSONObject("bienvenida")?.let { Bienvenida(it.optInt("paso"), it.optString("guia"), it.optString("titulo"), it.optString("texto")) }
        val zp = o.optJSONObject("zip")?.let { z ->
            val ops = ArrayList<Pair<String, String>>()
            val arr = z.optJSONArray("opciones") ?: JSONArray()
            for (i in 0 until arr.length()) { val p = arr.getJSONArray(i); ops.add(p.getString(0) to p.getString(1)) }
            Zip(z.optString("titulo"), ops)
        }
        val pr = o.optJSONObject("progreso")?.let { g ->
            val det = ArrayList<String>()
            val da = g.optJSONArray("detalles") ?: JSONArray()
            for (i in 0 until da.length()) det.add(da.getString(i))
            val bs = ArrayList<Pair<String, String>>()
            val ba = g.optJSONArray("botones") ?: JSONArray()
            for (i in 0 until ba.length()) { val b = ba.getJSONArray(i); bs.add(b.getString(0) to b.getString(1)) }
            Progreso(g.optString("id"), g.optString("guia"), g.optString("texto"), g.optDouble("avance", -1.0).toFloat(),
                g.optBoolean("trabajando", true), det, bs, g.optString("nota"))
        }
        val md = o.optJSONObject("medidor")?.let {
            Medidor(it.optString("texto"), it.optInt("esquina", 1), color(it.optString("color"), Color(0xFF4FD1C5)), it.optBoolean("lento"))
        }
        return Estado(o.optString("modo", "biblioteca"), o.optBoolean("imgui"), o.optInt("build"), o.optBoolean("guias", true),
            o.optString("idioma"), tema, cargando, juego, tactil, pagina, cf, bv, o.optString("novedad"), zp, pr,
            o.optBoolean("capturando"), md, o.optString("chip"), o.optBoolean("aviso_calor", true), o.optString("novedad_titulo"),
            o.optJSONObject("actualizacion")?.let {
                Actualizacion(it.optInt("build"), it.optInt("actual"), it.optString("estado"), it.optDouble("avance", -1.0).toFloat())
            })
    }

    fun biblioteca(texto: String): Biblioteca {
        if (texto.isBlank()) return Biblioteca()
        val o = JSONObject(texto)
        val juegos = ArrayList<Juego>()
        val ja = o.optJSONArray("juegos") ?: JSONArray()
        for (i in 0 until ja.length()) {
            val g = ja.getJSONObject(i)
            juegos.add(Juego(g.optString("clave"), g.optString("titulo"), g.optString("sistema"), g.optString("etiquetas"),
                g.optString("portada"), g.optBoolean("favorito"), g.optBoolean("oculto"), g.optLong("tiempo"),
                g.optString("tiempo_texto"), g.optLong("ultima"), g.optString("ultima_texto"), g.optBoolean("continuar"),
                g.optInt("tono"), g.optString("ruta"), g.optString("programa"), g.optString("sello")))
        }
        val consolas = ArrayList<Consola>()
        val ca = o.optJSONArray("consolas") ?: JSONArray()
        for (i in 0 until ca.length()) {
            val s = ca.getJSONObject(i)
            consolas.add(Consola(s.optString("id"), s.optString("nombre"), s.optString("completo"), s.optString("pista"),
                s.optString("juegos_de"), color(s.optString("color"), Color(0xFF9B8CFF)), s.optInt("cuantos"), s.optString("forma", "nes"),
                s.optString("etiqueta")))
        }
        val consejos = ArrayList<Consejo>()
        val ta = o.optJSONArray("consejos") ?: JSONArray()
        for (i in 0 until ta.length()) {
            val c = ta.getJSONObject(i)
            consejos.add(Consejo(c.optString("guia"), c.optString("texto")))
        }
        val nx = o.optJSONObject("nx")
        fun lista(a: JSONArray?): List<String> = if (a == null) emptyList() else (0 until a.length()).map { a.getString(it) }
        return Biblioteca(juegos, consolas, consejos, o.optBoolean("buscando"), o.optInt("carpetas"),
            lista(nx?.optJSONArray("claves")), lista(nx?.optJSONArray("firmware")))
    }

    /** Un aviso suelto: devuelve un Aviso, una Tarjeta o null. */
    fun aviso(linea: String): Any? {
        if (linea.isBlank()) return null
        val o = JSONObject(linea)
        return when (o.optString("t")) {
            "aviso" -> Aviso(o.optString("guia"), o.optString("texto"), o.optDouble("segundos", 3.5))
            "tarjeta" -> {
                val bs = ArrayList<Pair<String, String>>()
                val arr = o.optJSONArray("botones") ?: JSONArray()
                for (i in 0 until arr.length()) {
                    val b = arr.getJSONArray(i)
                    bs.add(b.getString(0) to b.getString(1))
                }
                Tarjeta(o.optString("id"), o.optString("guia"), o.optString("titulo"), o.optString("texto"), bs)
            }
            "explorador" -> Explorar(o.optBoolean("raiz"))
            "ir" -> Ir(o.optString("pagina"), o.optString("arg"))
            "consola" -> VerConsola(o.optString("id"))
            "nx" -> PedidoNx(o.optString("clave"), o.optString("ruta"), o.optString("titulo"))
            "respaldo" -> PedidoRespaldo(o.optString("ruta"), o.optBoolean("todos"), o.optBoolean("hecho"))
            "restaurar" -> PedidoRestaurar
            else -> null
        }
    }
}

/** C++ pide abrir el explorador de carpetas (para juegos o para mover «Satronho Games»). */
data class Explorar(val raiz: Boolean)
/** Un guía llevó a otra pantalla de Ajustes («ir»). */
data class Ir(val pagina: String, val arg: String = "")
/** C++ pide jugar un juego de Switch con Satronho NX. */
data class PedidoNx(val clave: String, val ruta: String, val titulo: String)
/** Un guía llevó a una consola de la biblioteca. */
data class VerConsola(val id: String)
