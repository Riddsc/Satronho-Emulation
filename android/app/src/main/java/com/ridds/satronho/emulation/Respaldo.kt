// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
package com.ridds.satronho.emulation

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Handler
import android.os.Looper
import android.util.Log
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.platform.LocalContext
import androidx.core.content.FileProvider
import java.io.BufferedOutputStream
import java.io.File
import java.io.FileOutputStream
import java.util.concurrent.Executors
import java.util.zip.ZipEntry
import java.util.zip.ZipFile
import java.util.zip.ZipOutputStream

/** C++ terminó un respaldo de partidas (o no había nada) y pide sumarle lo de NX y compartirlo. */
data class PedidoRespaldo(val ruta: String, val todos: Boolean, val hecho: Boolean)
/** C++ pide elegir un respaldo para restaurar. */
object PedidoRestaurar

/**
 * Respaldos de partidas en el celular (build 26). El .zip lo arma C++ (los juegos del hub); aquí se le
 * suman las partidas de Satronho NX y se abre «Compartir» (Drive, WhatsApp, el PC…). Al restaurar,
 * lo de NX vuelve a NX y lo demás lo acomoda C++.
 */
object Respaldo {
    private const val TAG = "SatronhoRespaldo"
    private const val NX_EN_ZIP = "nx/partidas-nx.zip"
    private val hilo = Executors.newSingleThreadExecutor()
    private val principal = Handler(Looper.getMainLooper())
    var pedirArchivo by mutableStateOf(false)

    private fun aviso(texto: String, guia: String = "juana") = SatronhoUi.orden("aviso|$guia|$texto")

    private fun partidasNx(ctx: Context): ByteArray? {
        if (!Nx.instalado(ctx)) return null
        val b = try { ctx.contentResolver.call(Uri.parse("content://${Nx.PAQUETE}.hub"), "partidas", null, null) } catch (e: Throwable) { null }
        val u = b?.getString("zip") ?: return null
        return try { ctx.contentResolver.openInputStream(Uri.parse(u))?.use { it.readBytes() } } catch (e: Throwable) { null }
    }

    fun terminar(ctx: Context, p: PedidoRespaldo) = hilo.execute {
        try {
            val zip = File(p.ruta)
            val nx = if (p.todos) partidasNx(ctx) else null
            if (!p.hecho && nx == null) {
                aviso("Todavía no hay partidas ni estados para respaldar.")
                return@execute
            }
            if (nx != null) {
                // Se rehace el .zip con todo lo de antes más las partidas de NX.
                val nuevo = File(zip.parentFile, zip.name + ".tmp")
                ZipOutputStream(BufferedOutputStream(FileOutputStream(nuevo))).use { out ->
                    if (zip.exists()) ZipFile(zip).use { z ->
                        for (e in z.entries()) {
                            out.putNextEntry(ZipEntry(e.name))
                            z.getInputStream(e).use { it.copyTo(out) }
                            out.closeEntry()
                        }
                    } else {
                        out.putNextEntry(ZipEntry("satronho-respaldo.json"))
                        out.write("{\"version\":1,\"app\":\"Satronho Emulation\",\"juegos\":[]}".toByteArray())
                        out.closeEntry()
                    }
                    out.putNextEntry(ZipEntry(NX_EN_ZIP))
                    out.write(nx)
                    out.closeEntry()
                }
                zip.parentFile?.mkdirs()
                nuevo.renameTo(zip)
            }
            principal.post { compartir(ctx, zip) }
        } catch (e: Throwable) {
            Log.e(TAG, "No se pudo terminar el respaldo", e)
            aviso("No pude armar el respaldo. Revisa que haya espacio en el teléfono.", "nono")
        }
    }

    private fun compartir(ctx: Context, zip: File) {
        try {
            val u = FileProvider.getUriForFile(ctx, ctx.packageName + ".archivos", zip)
            val i = Intent(Intent.ACTION_SEND).setType("application/zip").putExtra(Intent.EXTRA_STREAM, u)
                .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            ctx.startActivity(Intent.createChooser(i, t("Respaldo de partidas")).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
            aviso("Listo: el respaldo quedó en «Satronho Games/Respaldos». Guárdalo donde quieras.")
        } catch (e: Throwable) {
            Log.e(TAG, "No se pudo compartir", e)
        }
    }

    fun restaurar(ctx: Context, uri: Uri) = hilo.execute {
        try {
            val copia = File(ctx.cacheDir, "restaurar.zip")
            ctx.contentResolver.openInputStream(uri)?.use { inp -> FileOutputStream(copia).use { inp.copyTo(it) } }
            // Lo de NX, a NX.
            var nxOk = false
            ZipFile(copia).use { z ->
                val e = z.getEntry(NX_EN_ZIP)
                if (e != null && Nx.instalado(ctx)) {
                    val interno = File(ctx.cacheDir, "partidas-nx.zip")
                    z.getInputStream(e).use { inp -> FileOutputStream(interno).use { inp.copyTo(it) } }
                    val u = FileProvider.getUriForFile(ctx, ctx.packageName + ".archivos", interno)
                    ctx.grantUriPermission(Nx.PAQUETE, u, Intent.FLAG_GRANT_READ_URI_PERMISSION)
                    val r = try { ctx.contentResolver.call(Uri.parse("content://${Nx.PAQUETE}.hub"), "restaurar_partidas", u.toString(), null) } catch (_: Throwable) { null }
                    nxOk = (r?.getInt("archivos") ?: 0) > 0
                    interno.delete()
                }
            }
            if (nxOk) aviso("Las partidas de Satronho NX volvieron a NX.")
            SatronhoUi.orden("restaurar|${copia.absolutePath}")
        } catch (e: Throwable) {
            Log.e(TAG, "No se pudo restaurar", e)
            aviso("No pude abrir ese archivo. Tiene que ser un .zip de respaldo de Satronho.", "nono")
        }
    }
}

/** Elige el .zip a restaurar con el selector de archivos de Android. */
@Composable
fun SelectorRespaldo() {
    val ctx = LocalContext.current
    val elegir = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) Respaldo.restaurar(ctx, uri)
    }
    LaunchedEffect(Respaldo.pedirArchivo) {
        if (Respaldo.pedirArchivo) {
            Respaldo.pedirArchivo = false
            elegir.launch(arrayOf("application/zip", "application/octet-stream"))
        }
    }
}

/**
 * Mientras se juega, cada medio minuto se mira la temperatura de la batería: si pasa de 42 °C, Ñoño
 * avisa una vez (como Satronho NX). Un celular muy caliente baja su velocidad solo.
 */
@Composable
fun VigilaCalor(activo: Boolean) {
    val ctx = LocalContext.current
    LaunchedEffect(activo) {
        if (!activo) return@LaunchedEffect
        var avisado = false
        while (!avisado) {
            kotlinx.coroutines.delay(30_000)
            val i = try { ctx.registerReceiver(null, android.content.IntentFilter(Intent.ACTION_BATTERY_CHANGED)) } catch (_: Throwable) { null }
            val decimas = i?.getIntExtra(android.os.BatteryManager.EXTRA_TEMPERATURE, 0) ?: 0
            if (decimas >= 420) {
                avisado = true
                val texto = TrFmtKt("El celular está a %d °C. Si se calienta más, se pone lento solo: dale un respiro o baja la resolución.", decimas / 10)
                SatronhoUi.avisos.add(Aviso("nono", texto, 8.0))
            }
        }
    }
}
