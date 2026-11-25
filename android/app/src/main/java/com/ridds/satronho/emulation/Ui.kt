// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
package com.ridds.satronho.emulation

import android.content.pm.ActivityInfo
import android.util.Log
import android.view.Choreographer
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.activity.ComponentActivity
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.ViewCompositionStrategy

/**
 * Las pantallas nativas de Satronho Emulation (build 25), encima de la superficie de SDL donde se
 * dibuja el juego. En la biblioteca tapan todo; en el juego son transparentes (solo el control en
 * pantalla, el cajón y las tarjetas). Si C++ necesita mostrar algo de la interfaz de siempre
 * (Ajustes, el explorador…), se esconden y los toques van a SDL.
 */
object SatronhoUi {
    private const val TAG = "SatronhoUi"

    var estado by mutableStateOf(Estado())
        private set
    var biblioteca by mutableStateOf(Biblioteca())
        private set
    val avisos = mutableStateListOf<Aviso>()
    var tarjeta by mutableStateOf<Tarjeta?>(null)

    /** Hay algo abierto en la biblioteca que «atrás» debe cerrar (menú, búsqueda, consola…). */
    var cerrarAlgo: (() -> Boolean)? = null

    @Volatile var activa = false
        private set

    private var actividad: ComponentActivity? = null
    private var capa: FrameLayout? = null
    private var superficie: View? = null
    private var verEstado = -1
    private var verBiblioteca = -1
    private var modoAnterior = ""
    private var imguiAnterior = false

    fun instalar(act: ComponentActivity, layout: ViewGroup, sdlSurface: View?) {
        try {
            Puente.activar(true)
        } catch (e: Throwable) {
            Log.e(TAG, "Sin el puente con C++: queda la interfaz de siempre", e)
            return
        }
        actividad = act
        superficie = sdlSurface
        Imagenes.assets = act.assets
        val vista = ComposeView(act).apply {
            setViewCompositionStrategy(ViewCompositionStrategy.DisposeOnDetachedFromWindow)
            setContent { Raiz() }
        }
        val marco = FrameLayout(act)
        marco.addView(vista, FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT))
        layout.addView(marco, ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT))
        capa = marco
        activa = true
        aplicarModo("biblioteca", false)
        Choreographer.getInstance().postFrameCallback(cuadro)
    }

    // Cada cuadro de la pantalla se mira si C++ cambió algo (es barato: dos números).
    private val cuadro = object : Choreographer.FrameCallback {
        override fun doFrame(frameTimeNanos: Long) {
            try {
                leer()
            } catch (e: Throwable) {
                Log.e(TAG, "Error leyendo el estado", e)
            }
            if (activa) Choreographer.getInstance().postFrameCallback(this)
        }
    }

    private fun leer() {
        val ve = Puente.versionEstado()
        if (ve != verEstado) {
            verEstado = ve
            val nuevo = Leer.estado(Puente.estado())
            Puente.cambioIdioma(nuevo.idioma)
            estado = nuevo
            if (nuevo.modo != modoAnterior || nuevo.imgui != imguiAnterior) aplicarModo(nuevo.modo, nuevo.imgui)
        }
        val vb = Puente.versionBiblioteca()
        if (vb != verBiblioteca) {
            verBiblioteca = vb
            biblioteca = Leer.biblioteca(Puente.biblioteca())
        }
        val sueltos = Puente.avisos()
        if (sueltos.isNotEmpty()) {
            for (linea in sueltos.split('\n')) {
                when (val a = try { Leer.aviso(linea) } catch (e: Exception) { null }) {
                    is Aviso -> {
                        avisos.removeAll { it.texto == a.texto }
                        avisos.add(a)
                        while (avisos.size > 3) avisos.removeAt(0)
                    }
                    is Tarjeta -> tarjeta = a
                }
            }
        }
    }

    /** Biblioteca: gira con el celular (como NX). Juego: siempre acostado. */
    private fun aplicarModo(modo: String, imgui: Boolean) {
        val act = actividad ?: return
        val marco = capa ?: return
        if (modo != modoAnterior) {
            act.requestedOrientation = if (modo == "juego") ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
            else ActivityInfo.SCREEN_ORIENTATION_FULL_USER
            if (modo == "juego") tarjeta = null
        }
        // En el juego los botones del mando van a SDL: lo nativo no se queda con el foco.
        if (modo == "juego" || imgui) {
            marco.descendantFocusability = ViewGroup.FOCUS_BLOCK_DESCENDANTS
            marco.clearFocus()
            superficie?.requestFocus()
        } else {
            marco.descendantFocusability = ViewGroup.FOCUS_AFTER_DESCENDANTS
        }
        marco.visibility = if (imgui) View.GONE else View.VISIBLE
        modoAnterior = modo
        imguiAnterior = imgui
    }

    /** Tarjetas que arma lo nativo (consejos de los guías): su botón lo atiende quien la mostró. */
    var alTocarLocal: ((id: String, boton: String) -> Unit)? = null

    fun accionLocal(id: String, boton: String) {
        alTocarLocal?.invoke(id, boton)
    }

    fun orden(texto: String) {
        try { Puente.orden(texto) } catch (e: Throwable) { Log.e(TAG, "Orden perdida: $texto", e) }
    }

    /** Botón «atrás». true si lo atendió lo nativo; false para que siga a SDL. */
    fun atras(): Boolean {
        if (!activa || estado.imgui) return false
        tarjeta?.let {
            // La tarjeta se cierra con su primer botón (el «no, gracias»).
            if (!it.id.startsWith("local:") && it.botones.isNotEmpty()) orden("tarjeta|${it.id}|${it.botones.first().first}")
            tarjeta = null
            return true
        }
        if (estado.enJuego) {
            orden(if (estado.juego?.menu == true) "menu|0" else "menu|1")
            return true
        }
        if (cerrarAlgo?.invoke() == true) return true
        return false // en la biblioteca, C++ pregunta si cerrar la app
    }
}

@Composable
private fun Raiz() {
    val e = SatronhoUi.estado
    val act = androidx.compose.ui.platform.LocalContext.current
    SatronhoTema(e.tema, act.assets) {
        Box(Modifier.fillMaxSize().background(if (e.enJuego) Color.Transparent else e.tema.fondo)) {
            if (e.enJuego) PantallaJuego(e) else PantallaBiblioteca(e, SatronhoUi.biblioteca)
            e.cargando?.let { TarjetaCarga(it) }
            CapaAvisos(e.enJuego)
            SatronhoUi.tarjeta?.let { TarjetaGuia(it, e.enJuego) }
        }
    }
}
