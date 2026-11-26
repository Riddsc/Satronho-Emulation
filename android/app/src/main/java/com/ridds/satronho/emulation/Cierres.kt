// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
package com.ridds.satronho.emulation

import android.app.ActivityManager
import android.app.ApplicationExitInfo
import android.content.Context
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.util.Log
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * Cierres inesperados. Si la app se cierra de golpe (un error en Kotlin/Java, en C++ o porque se
 * trabó), al abrirla otra vez Ñoño lo avisa y ofrece mandar el registro, que ya trae lo que anotó
 * Android. Así no hace falta conectar el teléfono a un computador para saber qué pasó.
 *
 *  - Errores de Kotlin/Java: se escriben al momento en `cierre-java.txt`.
 *  - Errores de C++ y apps trabadas: Android los guarda (getHistoricalProcessExitReasons, Android 11+).
 *  Todo se junta en `ultimo-cierre.txt` (carpeta interna), que «Enviar el registro» incluye.
 */
object Cierres {
    private const val TAG = "SatronhoCierres"
    private const val PREFS = "satronho_cierres"
    private const val JAVA = "cierre-java.txt"
    const val REPORTE = "ultimo-cierre.txt"

    data class Cierre(val cuando: String, val texto: String)

    var pendiente by mutableStateOf<Cierre?>(null)
        private set

    private var instalado = false

    /** Al abrir la app: empieza a anotar errores y revisa si la vez anterior terminó mal. */
    fun instalar(ctx: Context) {
        if (instalado) return
        instalado = true
        val app = ctx.applicationContext
        val previo = Thread.getDefaultUncaughtExceptionHandler()
        Thread.setDefaultUncaughtExceptionHandler { hilo, error ->
            try {
                File(app.filesDir, JAVA).writeText(
                    "Fecha: ${fecha(System.currentTimeMillis())}\nHilo: ${hilo.name}\n\n${Log.getStackTraceString(error)}"
                )
            } catch (_: Throwable) {}
            previo?.uncaughtException(hilo, error)
        }
        Thread {
            try {
                revisar(app)
            } catch (e: Throwable) {
                Log.w(TAG, "No pude revisar el último cierre", e)
            }
        }.start()
    }

    private fun fecha(ms: Long) = SimpleDateFormat("d MMM HH:mm", Locale.getDefault()).format(Date(ms))

    private fun revisar(ctx: Context) {
        val prefs = ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        val visto = prefs.getLong("visto", 0L)
        val texto = StringBuilder()
        var cuando = 0L

        val java = File(ctx.filesDir, JAVA)
        if (java.exists()) {
            cuando = java.lastModified()
            texto.append("— Error en Kotlin/Java —\n").append(java.readText().take(20_000)).append("\n\n")
            java.delete()
        }

        var ultimaSalida = visto
        if (Build.VERSION.SDK_INT >= 30) {
            val am = ctx.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager
            val salidas = try { am.getHistoricalProcessExitReasons(ctx.packageName, 0, 5) } catch (_: Throwable) { emptyList() }
            for (s in salidas) ultimaSalida = maxOf(ultimaSalida, s.timestamp)
            val mala = salidas.firstOrNull {
                it.timestamp > visto && it.reason in listOf(
                    ApplicationExitInfo.REASON_CRASH, ApplicationExitInfo.REASON_CRASH_NATIVE, ApplicationExitInfo.REASON_ANR,
                )
            }
            // La primera vez que corre esta versión no se avisa de cierres viejos (visto == 0).
            if (mala != null && visto > 0L) {
                cuando = maxOf(cuando, mala.timestamp)
                val tipo = when (mala.reason) {
                    ApplicationExitInfo.REASON_CRASH -> "error en Kotlin/Java"
                    ApplicationExitInfo.REASON_CRASH_NATIVE -> "error en C++ (núcleo o interfaz)"
                    else -> "la app se trabó (ANR)"
                }
                texto.append("— Android —\nFecha: ${fecha(mala.timestamp)}\nMotivo: $tipo\n")
                if (!mala.description.isNullOrEmpty()) texto.append("Detalle: ${mala.description}\n")
                texto.append("Memoria: ${mala.pss / 1024} MB (pss), ${mala.rss / 1024} MB (rss)\n")
                // Las trabadas traen texto con lo que hacía cada hilo; los cierres de C++ vienen en binario.
                if (mala.reason == ApplicationExitInfo.REASON_ANR) {
                    try {
                        mala.traceInputStream?.bufferedReader()?.use { r -> texto.append("\n").append(r.readText().take(20_000)) }
                    } catch (_: Throwable) {}
                }
                texto.append("\n")
            }
        }
        prefs.edit().putLong("visto", maxOf(ultimaSalida, 1L)).apply()

        if (texto.isEmpty()) return
        try {
            File(ctx.filesDir, REPORTE).writeText(texto.toString())
        } catch (_: Throwable) {}
        val c = Cierre(fecha(if (cuando > 0) cuando else System.currentTimeMillis()), texto.toString())
        Handler(Looper.getMainLooper()).post { pendiente = c }
    }

    /** Ñoño ya avisó (se cerró la tarjeta o se mandó el registro). */
    fun visto() {
        pendiente = null
    }
}
