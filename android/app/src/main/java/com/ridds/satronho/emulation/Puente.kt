// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
package com.ridds.satronho.emulation

/**
 * Puente con el código en C++ (src/sys/native_ui.cpp). Las pantallas nativas no llaman directo a la
 * app: dejan órdenes de texto («jugar|clave») que C++ atiende en su hilo, y leen el estado en JSON
 * cuando cambia su número de versión. Así nadie se pisa entre hilos.
 */
object Puente {
    @JvmStatic external fun activar(on: Boolean)
    @JvmStatic external fun orden(texto: String)
    @JvmStatic external fun versionEstado(): Int
    @JvmStatic external fun estado(): String
    @JvmStatic external fun versionBiblioteca(): Int
    @JvmStatic external fun biblioteca(): String
    @JvmStatic external fun avisos(): String
    @JvmStatic external fun tactil(botones: Int, lx: Float, ly: Float, rx: Float, ry: Float)
    @JvmStatic external fun tr(es: String): String

    private val traducciones = HashMap<String, String>()
    private var idioma = ""

    /** Texto en el idioma de la app (los textos se escriben en español, como en C++). */
    fun t(es: String): String = traducciones.getOrPut(es) {
        try { tr(es) } catch (e: Throwable) { es }
    }

    fun cambioIdioma(nuevo: String) {
        if (nuevo != idioma) {
            idioma = nuevo
            traducciones.clear()
        }
    }
}

/** Atajo para los textos: t("Buscar juegos"). */
fun t(es: String): String = Puente.t(es)
