// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
package com.ridds.satronho.emulation

import android.content.pm.ActivityInfo
import android.util.Log
import android.view.Choreographer
import android.view.InputDevice
import org.libsdl.app.SDLControllerManager
import android.view.MotionEvent
import android.os.Looper
import android.os.Handler
import android.hardware.input.InputManager
import android.content.Context
import android.view.KeyEvent
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.activity.ComponentActivity
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.LocalContentColor
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.ViewCompositionStrategy

/**
 * Las pantallas nativas de Satronho Emulation (build 25), encima de la superficie de SDL donde se
 * dibuja el juego. En la biblioteca tapan todo; en el juego son transparentes (solo el control en
 * pantalla, el medidor, el cajón y las tarjetas). Desde la prueba 2 todo lo de la app es nativo en
 * el celular: Ajustes, Controles, explorador de carpetas, preguntas y la conversación con los guías.
 */
object SatronhoUi {
    private const val TAG = "SatronhoUi"

    var estado by mutableStateOf(Estado())
        private set
    var biblioteca by mutableStateOf(Biblioteca())
        private set
    val avisos = mutableStateListOf<Aviso>()
    var tarjeta by mutableStateOf<Tarjeta?>(null)

    /** Ajustes abiertos: pila de pantallas (página, argumento). Vacía = cerrados. */
    val pilaAjustes = mutableStateListOf<Pair<String, String>>()
    /** Explorador de carpetas abierto (null = cerrado). */
    var explorador by mutableStateOf<Explorar?>(null)
    /** Consola que un guía pidió mostrar en la biblioteca. */
    var consolaPedida by mutableStateOf("")
    /** Presentación con el logo al abrir la app. */
    var presentacion by mutableStateOf(true)

    /** Hay algo abierto en la biblioteca que «atrás» debe cerrar (menú, búsqueda, consola…). */
    var cerrarAlgo: (() -> Boolean)? = null
    /** Tarjetas que arma lo nativo (consejos de los guías): su botón lo atiende quien la mostró. */
    var alTocarLocal: ((id: String, boton: String) -> Unit)? = null

    @Volatile var activa = false
        private set

    private var actividad: ComponentActivity? = null
    private var capa: FrameLayout? = null
    private var vista: ComposeView? = null
    private var superficie: View? = null
    private var verEstado = -1
    private var verBiblioteca = -1
    private var modoAnterior = ""
    private var imguiAnterior = false
    private var esperaAjustes = 0

    fun instalar(act: ComponentActivity, layout: ViewGroup, sdlSurface: View?) {
        try {
            Puente.activar(true)
        } catch (e: Throwable) {
            Log.e(TAG, "Sin el puente con C++: queda la interfaz de siempre", e)
            return
        }
        actividad = act
        superficie = sdlSurface
        // Mandos de verdad (Bluetooth o USB): con uno conectado se esconde el control en pantalla.
        try {
            (act.getSystemService(Context.INPUT_SERVICE) as InputManager).registerInputDeviceListener(escuchaMandos, Handler(Looper.getMainLooper()))
        } catch (e: Throwable) {
            Log.e(TAG, "Sin aviso de mandos", e)
        }
        revisarMandos()
        // ¿Hay una Satronho NX más nueva? (una vez por sesión, en segundo plano).
        Nx.revisarNueva(act)
        Imagenes.assets = act.assets
        val v = ComposeView(act).apply {
            setViewCompositionStrategy(ViewCompositionStrategy.DisposeOnDetachedFromWindow)
            setContent { Raiz() }
        }
        val marco = FrameLayout(act)
        marco.addView(v, FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT))
        layout.addView(marco, ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT))
        capa = marco
        vista = v
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
            // C++ cerró los Ajustes (por ejemplo, un juego que ya no existe): se cierran aquí también.
            if (pilaAjustes.isNotEmpty() && nuevo.ajustes == null) {
                esperaAjustes++ // la orden de abrir puede no haber llegado todavía
                if (esperaAjustes > 30) { pilaAjustes.clear(); esperaAjustes = 0 }
            } else {
                esperaAjustes = 0
            }
        }
        val vb = Puente.versionBiblioteca()
        if (vb != verBiblioteca) {
            verBiblioteca = vb
            biblioteca = Leer.biblioteca(Puente.biblioteca())
            actividad?.let {
                Nx.revisarJuegos(it, biblioteca)
                try { ContinuarWidget.actualizar(it, biblioteca) } catch (e: Throwable) { Log.e(TAG, "Widget", e) }
            }
            if (continuarPendiente.isNotEmpty() && biblioteca.juegos.any { it.clave == continuarPendiente } && !estado.enJuego) {
                orden("continuar|$continuarPendiente")
                continuarPendiente = ""
            }
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
                    is Explorar -> explorador = a
                    is Ir -> if (a.pagina.isEmpty()) pilaAjustes.clear()
                        else if (pilaAjustes.lastOrNull()?.first != a.pagina) pilaAjustes.add(a.pagina to a.arg)
                    is VerConsola -> { pilaAjustes.clear(); consolaPedida = a.id }
                    is PedidoRespaldo -> actividad?.let { Respaldo.terminar(it, a) }
                    is PedidoRestaurar -> Respaldo.pedirArchivo = true
                    is PedidoNx -> actividad?.let { if (a.clave.isEmpty()) Nx.asistente = Nx.Asistente(null) else Nx.pedir(it, a) }
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
            if (modo != "juego" && modoAnterior == "juego") pilaAjustes.clear()
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

    /** Juego que pidió el widget «Continuar» (se abre cuando la biblioteca esté lista). */
    @Volatile var continuarPendiente = ""

    /** La actividad volvió al frente (por ejemplo, al salir de un juego de Satronho NX). */
    fun alVolver() {
        actividad?.let { Nx.alVolver(it) }
    }

    fun orden(texto: String) {
        try { Puente.orden(texto) } catch (e: Throwable) { Log.e(TAG, "Orden perdida: $texto", e) }
    }

    // ------------------------------------------------------------ Ajustes

    fun abrirAjustes(pagina: String = "inicio", arg: String = "") {
        pilaAjustes.add(pagina to arg)
        orden("ajustes|$pagina|$arg")
        if (estado.enJuego && estado.juego?.menu == true) orden("menu|0")
        permitirFoco(true)
    }

    fun atrasAjustes() {
        if (pilaAjustes.isEmpty()) return
        pilaAjustes.removeAt(pilaAjustes.lastIndex)
        val prev = pilaAjustes.lastOrNull()
        if (prev == null) cerrarAjustes() else orden("ajustes|${prev.first}|${prev.second}")
    }

    fun cerrarAjustes() {
        pilaAjustes.clear()
        orden("ajustes_cerrar")
        if (estado.enJuego) permitirFoco(false)
    }

    /** En el juego, lo nativo toma el foco solo mientras hay Ajustes o el cajón (para escribir y el mando). */
    fun permitirFoco(si: Boolean) {
        val marco = capa ?: return
        if (si) {
            marco.descendantFocusability = ViewGroup.FOCUS_AFTER_DESCENDANTS
        } else if (estado.enJuego) {
            marco.descendantFocusability = ViewGroup.FOCUS_BLOCK_DESCENDANTS
            marco.clearFocus()
            superficie?.requestFocus()
        }
    }

    // ------------------------------------------------------------ «atrás» y el mando

    fun accionLocal(id: String, boton: String) {
        alTocarLocal?.invoke(id, boton)
    }

    /** Botón «atrás». true si lo atendió lo nativo; false para que siga a SDL. */
    fun atras(): Boolean {
        if (!activa || estado.imgui) return false
        val e = estado
        if (presentacion && !e.enJuego) { presentacion = false; return true }
        if (explorador != null) { explorador = null; orden("explorador_cerrar"); return true }
        if (Nx.asistente != null) { Nx.cerrar(); return true }
        if (e.confirmar != null) { orden("confirmar|no"); return true }
        if (e.zip != null) { orden("zip_cancelar"); return true }
        if (e.tactil.editar) { orden("tactil_listo"); return true }
        if (pilaAjustes.isNotEmpty()) { atrasAjustes(); return true }
        tarjeta?.let {
            // La tarjeta se cierra con su primer botón (el «no, gracias»).
            if (!it.id.startsWith("local:") && it.botones.isNotEmpty()) orden("tarjeta|${it.id}|${it.botones.first().first}")
            tarjeta = null
            return true
        }
        if (e.enJuego) {
            orden(if (e.juego?.menu == true) "menu|0" else "menu|1")
            return true
        }
        if (cerrarAlgo?.invoke() == true) return true
        return false // en la biblioteca, C++ pregunta si cerrar la app
    }

    /** Hay un mando físico conectado (Bluetooth o USB): el control en pantalla se esconde solo. */
    var mandoFisico by mutableStateOf(false)
        private set

    private val escuchaMandos = object : InputManager.InputDeviceListener {
        override fun onInputDeviceAdded(id: Int) = revisarMandos()
        override fun onInputDeviceRemoved(id: Int) = revisarMandos()
        override fun onInputDeviceChanged(id: Int) = revisarMandos()
    }

    /** Un mando de verdad: no virtual, con botones o palancas de mando (como lo revisa Satronho NX). */
    private fun esMandoFisico(d: InputDevice?): Boolean {
        if (d == null || d.isVirtual) return false
        // Mandos que se presentan como teclado (TERIOS T12 y parecidos): la misma regla que SDL.
        if (SDLControllerManager.isKeyboardGamepad(d)) return true
        val s = d.sources
        val fuente = (s and InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD ||
            (s and InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK
        if (!fuente) return false
        val botones = d.hasKeys(KeyEvent.KEYCODE_BUTTON_A, KeyEvent.KEYCODE_BUTTON_B, KeyEvent.KEYCODE_BUTTON_X,
            KeyEvent.KEYCODE_BUTTON_Y, KeyEvent.KEYCODE_BUTTON_START).any { it }
        val palancas = d.motionRanges.any { it.axis == MotionEvent.AXIS_X || it.axis == MotionEvent.AXIS_Z || it.axis == MotionEvent.AXIS_HAT_X }
        return botones || palancas
    }

    private var dispositivosAnotados = ""

    fun revisarMandos() {
        mandoFisico = try { InputDevice.getDeviceIds().any { esMandoFisico(InputDevice.getDevice(it)) } } catch (_: Throwable) { false }
        // Al registro (C++ anota cada orden): qué ve Android, para entender un mando que no funciona.
        try {
            val lista = InputDevice.getDeviceIds().toList().mapNotNull { InputDevice.getDevice(it) }.filter { !it.isVirtual }.joinToString(" ; ") {
                val b = it.hasKeys(KeyEvent.KEYCODE_BUTTON_A, KeyEvent.KEYCODE_BUTTON_B)
                "${it.name} [fuentes=0x${Integer.toHexString(it.sources)} A=${b[0]} B=${b[1]} mando=${esMandoFisico(it)}]"
            }
            if (lista != dispositivosAnotados) {
                dispositivosAnotados = lista
                orden("dispositivos|$lista")
            }
        } catch (_: Throwable) {}
    }

    /** ¿Los botones del mando son para moverse por lo nativo (y no para el juego)? */
    fun quiereMando(): Boolean {
        if (!activa) return false
        val e = estado
        if (e.imgui || e.capturando) return false
        if (!e.enJuego) return true
        return e.juego?.menu == true || pilaAjustes.isNotEmpty() || e.confirmar != null || tarjeta != null
    }

    /** Un botón del mando o del control remoto, convertido en flechas / aceptar / atrás. */
    fun teclaMando(ev: KeyEvent): Boolean {
        val fuente = ev.source
        val esMando = (fuente and InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD ||
            (fuente and InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK ||
            (fuente and InputDevice.SOURCE_DPAD) == InputDevice.SOURCE_DPAD ||
            SDLControllerManager.isKeyboardGamepad(ev.device)
        if (!esMando || !quiereMando()) return false
        val v = vista ?: return false
        val codigo = when (ev.keyCode) {
            KeyEvent.KEYCODE_BUTTON_A -> KeyEvent.KEYCODE_DPAD_CENTER
            KeyEvent.KEYCODE_BUTTON_B -> {
                if (ev.action == KeyEvent.ACTION_UP && ev.repeatCount == 0) {
                    if (!atras()) return false // nada nativo que cerrar: C++ pregunta si cerrar la app
                }
                return true
            }
            KeyEvent.KEYCODE_DPAD_UP, KeyEvent.KEYCODE_DPAD_DOWN, KeyEvent.KEYCODE_DPAD_LEFT, KeyEvent.KEYCODE_DPAD_RIGHT,
            KeyEvent.KEYCODE_DPAD_CENTER -> ev.keyCode
            else -> return false
        }
        permitirFoco(true)
        if (!v.hasFocus()) v.requestFocus()
        v.dispatchKeyEvent(KeyEvent(ev.downTime, ev.eventTime, ev.action, codigo, ev.repeatCount, ev.metaState))
        return true
    }
}

@Composable
private fun Raiz() {
    val e = SatronhoUi.estado
    val act = LocalContext.current
    SatronhoTema(e.tema, act.assets) {
        // Color de las letras: el del tema (sin esto, quedaban negras sobre el fondo oscuro).
        CompositionLocalProvider(LocalContentColor provides e.tema.texto) {
            Box(Modifier.fillMaxSize().background(if (e.enJuego) Color.Transparent else e.tema.fondo)) {
                if (e.enJuego) PantallaJuego(e) else PantallaBiblioteca(e, SatronhoUi.biblioteca)
                e.cargando?.let { TarjetaCarga(it) }
                val pagina = e.ajustes
                if (SatronhoUi.pilaAjustes.isNotEmpty() && pagina != null) PantallaAjustes(pagina, e)
                SatronhoUi.explorador?.let { Explorador(it) }
                e.zip?.let { VentanaZip(it) }
                e.progreso?.let { VentanaProgreso(it) }
                e.bienvenida?.let { PantallaBienvenida(it) }
                // Tarjetas de Satronho en la biblioteca, una a la vez: build nueva, NX más nueva, cierre, novedades.
                if (!e.enJuego && e.bienvenida == null && SatronhoUi.pilaAjustes.isEmpty() && Nx.asistente == null) {
                    val cierre = Cierres.pendiente
                    when {
                        e.actualizacion != null -> TarjetaActualizacion(e.actualizacion)
                        cierre != null -> TarjetaCierre(cierre)
                        Nx.avisoNueva -> TarjetaNxNueva()
                        e.novedad.isNotEmpty() -> TarjetaNovedad(e.novedad, e.novedadTitulo, e.build)
                    }
                }
                e.confirmar?.let { VentanaConfirmar(it) }
                CapaAvisos(e.enJuego)
                SatronhoUi.tarjeta?.let { TarjetaGuia(it, e.enJuego) }
                if (!e.enJuego) Nx.asistente?.let { AsistenteNx(it) }
                SelectorRespaldo()
                VigilaCalor(e.enJuego && e.avisoCalor)
                if (SatronhoUi.presentacion && !e.enJuego) Presentacion()
            }
        }
    }
}
