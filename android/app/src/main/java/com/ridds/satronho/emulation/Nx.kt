// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
package com.ridds.satronho.emulation

import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.os.Handler
import android.os.Looper
import android.os.StatFs
import android.util.Log
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.core.content.FileProvider
import org.json.JSONObject
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream
import java.net.HttpURLConnection
import java.net.URL
import java.security.MessageDigest
import java.util.concurrent.Executors
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

/**
 * Satronho NX (juegos de Switch) desde el hub (build 26). NX es una app aparte: el hub la instala
 * (sin ícono propio: solo se abre desde aquí), le pasa las claves y el firmware que la persona ya
 * tiene en el teléfono, le abre el juego y, al salir, se vuelve a la biblioteca con el tiempo jugado.
 * Hablamos con NX por su HubProvider (content://dev.satronho.nx.hub).
 */
object Nx {
    private const val TAG = "SatronhoNx"
    const val PAQUETE = "dev.satronho.nx"
    private const val EMULACION = "org.yuzu.yuzu_emu.activities.EmulationActivity"
    private const val PRINCIPAL = "org.yuzu.yuzu_emu.ui.main.MainActivity"
    /** De dónde se baja NX. Se puede cambiar sin sacar otra build: el hub lee este archivo. */
    private const val ENLACE_REMOTO = "https://raw.githubusercontent.com/Riddsc/Satronho-Emulation/main/remoto/nx.json"
    private const val APK_RESPALDO =
        "https://github.com/Riddsc/satronho-nx/releases/download/android-vista-previa/satronho-nx-android-arm64-vista-previa.apk"
    private const val PREFS = "satronho_nx"

    /** Lo que se sabe de NX en este teléfono. */
    data class EstadoNx(
        val instalado: Boolean = false,
        val responde: Boolean = false,   // tiene el enlace con el hub (NX de la build 57 en adelante)
        val claves: Boolean = false,
        val firmware: String = "",
        val version: String = "",
    )

    /** El asistente en pantalla: para qué juego es (o null si se abrió desde Ajustes). */
    class Asistente(val pedido: PedidoNx?)

    var asistente by mutableStateOf<Asistente?>(null)
    var estadoNx by mutableStateOf(EstadoNx())
        private set
    var tarea by mutableStateOf("")       // lo que está haciendo ahora («Bajando Satronho NX…»)
        private set
    var avance by mutableStateOf(-1f)
        private set
    var problema by mutableStateOf("")
        private set
    var hayNueva by mutableStateOf(false)
        private set

    private val hilo = Executors.newSingleThreadExecutor()
    private val principal = Handler(Looper.getMainLooper())
    private var lanzado: Pair<String, Long>? = null
    private var esperandoInstalacion = false
    private var revisoNueva = false
    private val consultados = HashSet<String>()
    /**
     * Hasta cuándo no se le pregunta nada a NX: mientras abre un juego y un momento después de volver.
     * Las preguntas al HubProvider usan el núcleo de NX; si llegaban justo cuando NX revisaba el juego
     * que le mandamos abrir, el juego parecía inválido y NX salía con «No bootable game present!».
     */
    @Volatile private var calmaHasta = 0L

    fun androidSirve() = Build.VERSION.SDK_INT >= 33

    private fun prefs(ctx: Context) = ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE)

    fun instalado(ctx: Context): Boolean = try {
        ctx.packageManager.getPackageInfo(PAQUETE, 0)
        true
    } catch (e: PackageManager.NameNotFoundException) {
        false
    }

    /** Mientras NX abre un juego (y un momento después de volver) no se le pregunta nada. */
    private fun esperarCalma() {
        while (true) {
            val falta = calmaHasta - System.currentTimeMillis()
            if (falta <= 0) return
            Thread.sleep(minOf(falta, 500L))
        }
    }

    /**
     * Le pregunta algo a NX (en un hilo aparte, nunca en la pantalla). Si NX está ocupada
     * (arrancando o con un juego abierto) o no contestó por un tropiezo, se vuelve a intentar
     * unas veces antes de rendirse; así una NX que sí habla con el hub no pasa por «vieja».
     */
    internal fun llamar(ctx: Context, metodo: String, arg: String? = null): Bundle? =
        llamarTal(ctx, metodo, arg)?.takeIf { !it.getBoolean("ocupado") }

    /** Como [llamar], pero si NX siguió ocupada devuelve su respuesta «ocupado» en vez de null. */
    private fun llamarTal(ctx: Context, metodo: String, arg: String? = null): Bundle? {
        var ultima: Bundle? = null
        repeat(6) { intento ->
            esperarCalma()
            val b = try {
                ctx.contentResolver.call(Uri.parse("content://$PAQUETE.hub"), metodo, arg, null)
            } catch (e: Throwable) {
                Log.w(TAG, "NX no respondió a $metodo (intento ${intento + 1})", e)
                null
            }
            if (b != null && !b.getBoolean("ocupado")) return b
            // Una NX vieja (sin enlace) no tiene HubProvider: no tiene caso insistir tanto.
            if (b == null && intento >= 1) return null
            ultima = b
            Thread.sleep(1000)
        }
        return ultima
    }

    private fun consultar(ctx: Context): EstadoNx {
        if (!instalado(ctx)) return EstadoNx()
        val b = llamarTal(ctx, "estado") ?: return EstadoNx(instalado = true)
        // Ocupada (con un juego abierto): sí habla con el hub; se queda lo último que se supo.
        if (b.getBoolean("ocupado")) return estadoNx.copy(instalado = true, responde = true)
        return EstadoNx(true, true, b.getBoolean("claves"), b.getString("firmware") ?: "", b.getString("version") ?: "")
    }

    private fun enHilo(trabajo: () -> Unit) = hilo.execute {
        try { trabajo() } catch (e: Throwable) { Log.e(TAG, "Error en NX", e) }
    }

    private fun enPantalla(trabajo: () -> Unit) = principal.post(trabajo)

    fun refrescar(ctx: Context, despues: (() -> Unit)? = null) = enHilo {
        val e = consultar(ctx)
        enPantalla {
            estadoNx = e
            despues?.invoke()
        }
    }

    /** Un archivo del teléfono, en forma que NX pueda leer (le damos permiso solo a ese). */
    private fun uriDe(ctx: Context, archivo: File): Uri {
        val u = FileProvider.getUriForFile(ctx, ctx.packageName + ".archivos", archivo)
        ctx.grantUriPermission(PAQUETE, u, Intent.FLAG_GRANT_READ_URI_PERMISSION)
        return u
    }

    // ------------------------------------------------------------ jugar

    /** C++ pide jugar un juego de Switch. Si todo está listo se abre directo; si no, el asistente. */
    fun pedir(act: ComponentActivity, p: PedidoNx) {
        if (!androidSirve()) {
            asistente = Asistente(p)
            return
        }
        refrescar(act) {
            val e = estadoNx
            if (e.instalado && e.responde && e.claves) lanzar(act, p) else asistente = Asistente(p)
        }
    }

    fun lanzar(ctx: Context, p: PedidoNx) {
        val f = File(p.ruta)
        if (!f.exists()) {
            problema = t("No encuentro el archivo del juego. ¿Lo moviste o lo borraste?")
            return
        }
        try {
            val u = uriDe(ctx, f)
            val i = Intent(Intent.ACTION_VIEW)
                .setDataAndType(u, "application/octet-stream")
                .setClassName(PAQUETE, EMULACION)
                .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_ACTIVITY_NEW_TASK)
            // Mientras NX revisa y abre el juego, el hub no le pregunta nada (al volver se acorta).
            calmaHasta = System.currentTimeMillis() + 30_000
            ctx.startActivity(i)
            lanzado = p.clave to System.currentTimeMillis()
            asistente = null
        } catch (e: Throwable) {
            Log.e(TAG, "No se pudo abrir NX", e)
            calmaHasta = 0L
            problema = t("No pude abrir Satronho NX. Prueba instalarlo otra vez desde aquí.")
            asistente = Asistente(p)
        }
    }

    /** Ajustes de NX (mandos, gráficos): su pantalla principal, aunque no tenga ícono. */
    fun abrirAjustesNx(ctx: Context) {
        try {
            ctx.startActivity(Intent().setClassName(PAQUETE, PRINCIPAL).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
        } catch (e: Throwable) {
            Log.e(TAG, "No se pudo abrir NX", e)
        }
    }

    /** La actividad volvió: si veníamos de un juego de NX, se anota su tiempo. */
    fun alVolver(ctx: Context) {
        // De vuelta en el hub: se espera un momento antes de preguntarle algo a NX (que termine de cerrar el juego).
        if (lanzado != null) calmaHasta = System.currentTimeMillis() + 2000
        lanzado?.let { (clave, desde) ->
            val s = (System.currentTimeMillis() - desde) / 1000
            if (s >= 5) SatronhoUi.orden("nx_volvio|$clave|$s")
            lanzado = null
        }
        if (esperandoInstalacion && instalado(ctx)) {
            esperandoInstalacion = false
            enHilo {
                // NX se instaló desde el hub: queda sin ícono propio, solo se abre desde aquí.
                if (prefs(ctx).getBoolean("por_hub", false)) llamar(ctx, "ocultar")
                val e = consultar(ctx)
                enPantalla {
                    estadoNx = e
                    tarea = ""
                    avance = -1f
                }
            }
        } else if (asistente != null) {
            refrescar(ctx)
        }
    }

    fun cerrar() {
        asistente = null
        problema = ""
    }

    // ------------------------------------------------------------ instalar y actualizar NX

    private fun leerTexto(url: String): String? = try {
        val c = URL(url).openConnection() as HttpURLConnection
        c.connectTimeout = 15000
        c.readTimeout = 15000
        c.instanceFollowRedirects = true
        if (c.responseCode in 200..299) c.inputStream.bufferedReader().use { it.readText() } else null
    } catch (e: Throwable) {
        null
    }

    private fun enlaceApk(): String {
        val remoto = leerTexto(ENLACE_REMOTO)
        if (remoto != null) try {
            val u = JSONObject(remoto).optString("apk")
            if (u.startsWith("https://")) return u
        } catch (_: Throwable) {}
        return APK_RESPALDO
    }

    private fun sha256(f: File): String {
        val md = MessageDigest.getInstance("SHA-256")
        FileInputStream(f).use { inp ->
            val buf = ByteArray(1 shl 16)
            while (true) {
                val n = inp.read(buf)
                if (n <= 0) break
                md.update(buf, 0, n)
            }
        }
        return md.digest().joinToString("") { "%02x".format(it) }
    }

    /** La tarjeta de MechaRidds en la biblioteca («Hay una Satronho NX más nueva»). */
    var avisoNueva by mutableStateOf(false)
        private set

    /**
     * ¿Hay una NX más nueva que la del teléfono? Una vez por sesión, al abrir el hub. Se compara la
     * huella (sha256) del APK publicado con la del APK instalado de verdad: así sirve aunque NX no la
     * haya instalado el hub o se haya reinstalado todo.
     */
    fun revisarNueva(ctx: Context) {
        if (revisoNueva || !androidSirve()) return
        revisoNueva = true
        enHilo {
            if (!instalado(ctx)) return@enHilo
            val remoto = leerTexto(enlaceApk() + ".sha256")?.trim()?.take(64) ?: return@enHilo
            if (remoto.length != 64) return@enHilo
            val local = try {
                sha256(File(ctx.packageManager.getApplicationInfo(PAQUETE, 0).sourceDir))
            } catch (e: Throwable) {
                prefs(ctx).getString("sha", "") ?: ""
            }
            if (local.isEmpty() || local == remoto) return@enHilo
            // Ya se ofreció esta misma versión y dijo «Después»: no se insiste hasta que salga otra.
            val omitida = prefs(ctx).getString("omitida", "") ?: ""
            enPantalla {
                hayNueva = true
                if (omitida != remoto) avisoNueva = true
            }
            ultimaRemota = remoto
        }
    }

    @Volatile private var ultimaRemota = ""

    /** «Después» en la tarjeta: se esconde y no vuelve a salir para esta misma versión. */
    fun despuesNueva(ctx: Context) {
        avisoNueva = false
        if (ultimaRemota.isNotEmpty()) prefs(ctx).edit().putString("omitida", ultimaRemota).apply()
    }

    /** «Actualizar» en la tarjeta: abre el asistente de NX (que muestra el avance) y empieza a bajarla. */
    fun actualizarDesdeAviso(ctx: Context) {
        avisoNueva = false
        asistente = Asistente(null)
        instalarNx(ctx)
    }

    fun instalarNx(ctx: Context) {
        if (tarea.isNotEmpty()) return
        problema = ""
        tarea = t("Bajando Satronho NX…")
        avance = 0f
        enHilo {
            val destino = File(ctx.cacheDir, "satronho-nx.apk")
            try {
                val url = enlaceApk()
                val c = URL(url).openConnection() as HttpURLConnection
                c.connectTimeout = 20000
                c.readTimeout = 30000
                c.instanceFollowRedirects = true
                if (c.responseCode !in 200..299) throw IllegalStateException("HTTP ${c.responseCode}")
                val total = c.contentLengthLong
                var leido = 0L
                c.inputStream.use { inp ->
                    FileOutputStream(destino).use { out ->
                        val buf = ByteArray(1 shl 16)
                        var ultimo = 0L
                        while (true) {
                            val n = inp.read(buf)
                            if (n <= 0) break
                            out.write(buf, 0, n)
                            leido += n
                            if (total > 0 && leido - ultimo > 512 * 1024) {
                                ultimo = leido
                                val a = leido.toFloat() / total
                                enPantalla { avance = a }
                            }
                        }
                    }
                }
                val sha = sha256(destino)
                val esperado = leerTexto("$url.sha256")?.trim()?.take(64)
                if (esperado != null && esperado.length == 64 && esperado != sha) throw IllegalStateException("sha")
                prefs(ctx).edit().putString("sha", sha).putBoolean("por_hub", !instalado(ctx) || prefs(ctx).getBoolean("por_hub", false)).apply()
                enPantalla {
                    tarea = t("Android te va a preguntar si instalas Satronho NX: dile que sí.")
                    avance = -1f
                    hayNueva = false
                    esperandoInstalacion = true
                    if (!SatronhoActivity.instalarApk(destino.absolutePath)) {
                        tarea = ""
                        problema = t("Primero dale permiso a Satronho Emulation para instalar apps (te abrí la pantalla). Luego vuelve a tocar Instalar.")
                        esperandoInstalacion = false
                    }
                }
            } catch (e: Throwable) {
                Log.e(TAG, "No se pudo bajar NX", e)
                destino.delete()
                enPantalla {
                    tarea = ""
                    avance = -1f
                    problema = t("No pude bajar Satronho NX. Revisa tu internet y vuelve a intentarlo.")
                }
            }
        }
    }

    // ------------------------------------------------------------ claves y firmware

    fun pasarClaves(ctx: Context, uris: List<Uri>) {
        if (uris.isEmpty()) return
        problema = ""
        tarea = t("Pasándole tus claves a NX…")
        enHilo {
            var ok = false
            // prod.keys primero (las title.keys van después, si están).
            for (u in uris.sortedBy { if (it.toString().lowercase().contains("title")) 1 else 0 }) {
                try { ctx.grantUriPermission(PAQUETE, u, Intent.FLAG_GRANT_READ_URI_PERMISSION) } catch (_: Throwable) {}
                val r = llamar(ctx, "claves", u.toString())
                if (r != null && r.getBoolean("claves")) ok = true
            }
            val e = consultar(ctx)
            enPantalla {
                estadoNx = e
                tarea = ""
                if (!ok) problema = t("Esas claves no le sirvieron a NX. Tienen que ser las prod.keys sacadas de tu propia consola.")
            }
        }
    }

    fun pasarClavesEncontradas(ctx: Context, rutas: List<String>) =
        pasarClaves(ctx, rutas.map { File(it) }.filter { it.exists() }.map { uriDe(ctx, it) })

    /** Una carpeta con los .nca del firmware se empaca en un .zip (NX instala firmware desde .zip). */
    private fun zipDeCarpeta(ctx: Context, dir: File): File {
        val z = File(ctx.cacheDir, "firmware-nx.zip")
        ZipOutputStream(FileOutputStream(z)).use { out ->
            for (f in dir.listFiles()?.filter { it.isFile && it.name.endsWith(".nca", true) } ?: emptyList()) {
                out.putNextEntry(ZipEntry(f.name))
                FileInputStream(f).use { it.copyTo(out) }
                out.closeEntry()
            }
        }
        return z
    }

    fun instalarFirmware(ctx: Context, uri: Uri?, ruta: String?) {
        problema = ""
        tarea = t("Instalando el firmware en NX… puede tardar un minuto.")
        enHilo {
            var temporal: File? = null
            val u = when {
                uri != null -> { try { ctx.grantUriPermission(PAQUETE, uri, Intent.FLAG_GRANT_READ_URI_PERMISSION) } catch (_: Throwable) {}; uri }
                ruta != null && File(ruta).isDirectory -> { temporal = zipDeCarpeta(ctx, File(ruta)); uriDe(ctx, temporal) }
                ruta != null -> uriDe(ctx, File(ruta))
                else -> null
            }
            val r = if (u != null) llamar(ctx, "firmware", u.toString()) else null
            temporal?.delete()
            val e = consultar(ctx)
            enPantalla {
                estadoNx = e
                tarea = ""
                if (r == null || !r.getBoolean("ok")) problema = t("Ese archivo no es un firmware que NX entienda. Tiene que ser el .zip con los .nca, sacado de tu consola.")
            }
        }
    }

    // ------------------------------------------------------------ nombre e ícono reales

    /** Los juegos de Switch salen con su nombre e ícono de verdad: se los pide a NX (necesita las claves). */
    fun revisarJuegos(ctx: Context, b: Biblioteca) {
        val faltan = b.juegos.filter { it.sistema == "nx" && it.programa.isEmpty() && it.ruta.isNotEmpty() && consultados.add(it.clave) }
        if (faltan.isEmpty() || !androidSirve()) return
        enHilo {
            // Si mientras tanto se abrió un juego, esto espera a que vuelvas (esperarCalma en llamar).
            if (!instalado(ctx)) { faltan.forEach { consultados.remove(it.clave) }; return@enHilo }
            val e = consultar(ctx)
            if (!e.claves) { faltan.forEach { consultados.remove(it.clave) }; return@enHilo }
            val dir = File(ctx.filesDir, "nx_iconos").apply { mkdirs() }
            for (g in faltan) {
                val f = File(g.ruta)
                if (!f.exists()) continue
                val r = llamar(ctx, "info", uriDe(ctx, f).toString()) ?: continue
                if (!r.getBoolean("valido")) continue
                val titulo = (r.getString("titulo") ?: "").replace('|', ' ')
                val programa = (r.getString("programa") ?: "").replace('|', ' ')
                var icono = ""
                r.getByteArray("icono")?.let { png ->
                    val out = File(dir, "${g.clave}.png")
                    out.writeBytes(png)
                    icono = out.absolutePath
                }
                SatronhoUi.orden("nx_info|${g.clave}|$titulo|$programa|$icono")
            }
        }
    }

    // ------------------------------------------------------------ el teléfono

    fun libreGb(): Double = try {
        val s = StatFs(Environment.getExternalStorageDirectory().path)
        s.availableBytes / 1073741824.0
    } catch (_: Throwable) { -1.0 }

    fun vulkan(ctx: Context): Boolean =
        ctx.packageManager.hasSystemFeature(PackageManager.FEATURE_VULKAN_HARDWARE_VERSION, 0x401000)

    fun chip(): String = if (Build.VERSION.SDK_INT >= 31) listOf(Build.SOC_MANUFACTURER, Build.SOC_MODEL).filter { it.isNotBlank() && it != "unknown" }.joinToString(" ")
        else Build.HARDWARE
}

// ------------------------------------------------------------------ el asistente

@Composable
private fun Paso(listo: Boolean, titulo: String, detalle: String, boton: String = "", alTocar: (() -> Unit)? = null, segundo: String = "", alTocar2: (() -> Unit)? = null) {
    Row(Modifier.fillMaxWidth().padding(vertical = 6.dp), verticalAlignment = Alignment.CenterVertically) {
        Box(
            Modifier.size(26.dp).clip(CircleShape).background(if (listo) Color(0xFF3DDC97) else MaterialTheme.colorScheme.surfaceVariant),
            contentAlignment = Alignment.Center,
        ) { Text(if (listo) "✓" else "•", color = if (listo) Color(0xFF0B2B1E) else MaterialTheme.colorScheme.onSurfaceVariant, fontWeight = FontWeight.Bold) }
        Spacer(Modifier.width(12.dp))
        Column(Modifier.weight(1f)) {
            Text(titulo, style = MaterialTheme.typography.titleSmall)
            if (detalle.isNotEmpty()) Text(detalle, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            if (!listo && (alTocar != null || alTocar2 != null)) {
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.padding(top = 6.dp)) {
                    if (alTocar != null && boton.isNotEmpty()) FilledTonalButton(onClick = alTocar) { Text(boton) }
                    if (alTocar2 != null && segundo.isNotEmpty()) TextButton(onClick = alTocar2) { Text(segundo) }
                }
            }
        }
    }
}

/** Antes del primer juego de Switch: ¿el teléfono sirve? ¿NX está? ¿tiene tus claves y tu firmware? */
@Composable
fun AsistenteNx(a: Nx.Asistente) {
    val ctx = androidx.compose.ui.platform.LocalContext.current
    val e = Nx.estadoNx
    val b = SatronhoUi.biblioteca
    androidx.compose.runtime.LaunchedEffect(a) {
        Nx.refrescar(ctx)
        Nx.revisarNueva(ctx)
    }
    val elegirClaves = rememberLauncherForActivityResult(ActivityResultContracts.OpenMultipleDocuments()) { uris ->
        if (uris.isNotEmpty()) Nx.pasarClaves(ctx, uris)
    }
    val elegirFirmware = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) Nx.instalarFirmware(ctx, uri, null)
    }
    Velo {
        Ventana(ancho = 540) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Avatar("mecharidds", 44.dp)
                Spacer(Modifier.width(10.dp))
                Column(Modifier.weight(1f)) {
                    Text(t("Satronho NX · para juegos de Switch"), style = MaterialTheme.typography.titleMedium)
                    Text(a.pedido?.titulo ?: t("Preparar los juegos de Switch"), style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.primary, maxLines = 1)
                }
            }
            Spacer(Modifier.height(8.dp))
            Column(Modifier.heightIn(max = 460.dp).verticalScroll(rememberScrollState())) {
                val android = Nx.androidSirve()
                val gb = Nx.libreGb()
                Paso(android && Nx.vulkan(ctx), t("Tu teléfono"),
                    if (!android) t("Satronho NX necesita Android 13 o más nuevo. En este teléfono no se puede, lo siento.")
                    else listOf(Nx.chip(), if (gb >= 0) TrFmtKt("%.0f GB libres", gb) else "").filter { it.isNotEmpty() }.joinToString(" · ") +
                        (if (!Nx.vulkan(ctx)) " · " + t("sin Vulkan 1.1: NX no va a arrancar") else "") +
                        (if (gb in 0.0..3.0) " · " + t("Deja al menos 3 GB libres para los juegos.") else ""))
                if (android) {
                    Paso(e.instalado && e.responde, t("Satronho NX instalado"),
                        when {
                            !e.instalado -> t("Lo bajo e instalo por ti. No pone ícono propio: se abre desde aquí.")
                            !e.responde -> t("Tienes una NX vieja que no habla con el hub. Instala la nueva encima.")
                            else -> e.version
                        },
                        t("Instalar"), if (Nx.tarea.isEmpty()) ({ Nx.instalarNx(ctx) }) else null)
                    if (e.instalado && e.responde && Nx.hayNueva)
                        Paso(false, t("Hay una Satronho NX más nueva"), t("Se instala encima; no pierdes nada."), t("Actualizar"),
                            if (Nx.tarea.isEmpty()) ({ Nx.instalarNx(ctx) }) else null)
                    if (e.instalado && e.responde) {
                        Paso(e.claves, t("Tus claves"),
                            if (e.claves) t("NX ya las tiene.")
                            else if (b.nxClaves.isNotEmpty()) t("Encontré tus prod.keys en el teléfono.")
                            else t("Elige tu prod.keys (y title.keys si la tienes), sacadas de tu propia consola."),
                            if (b.nxClaves.isNotEmpty()) t("Pasárselas") else t("Elegir archivo"),
                            if (Nx.tarea.isEmpty()) ({ if (b.nxClaves.isNotEmpty()) Nx.pasarClavesEncontradas(ctx, b.nxClaves) else elegirClaves.launch(arrayOf("*/*")) }) else null,
                            if (b.nxClaves.isNotEmpty()) t("Elegir otro") else "",
                            if (b.nxClaves.isNotEmpty() && Nx.tarea.isEmpty()) ({ elegirClaves.launch(arrayOf("*/*")) }) else null)
                        Paso(e.firmware.isNotEmpty(), t("Firmware"),
                            if (e.firmware.isNotEmpty()) TrFmtKt("Versión %s.", e.firmware)
                            else if (b.nxFirmware.isNotEmpty()) t("Encontré un firmware en el teléfono. Muchos juegos lo necesitan.")
                            else t("Muchos juegos lo necesitan. Elige el .zip del firmware sacado de tu consola."),
                            if (b.nxFirmware.isNotEmpty()) t("Instalarlo") else t("Elegir .zip"),
                            if (Nx.tarea.isEmpty() && e.claves) ({ if (b.nxFirmware.isNotEmpty()) Nx.instalarFirmware(ctx, null, b.nxFirmware.first())
                                else elegirFirmware.launch(arrayOf("application/zip", "application/octet-stream")) }) else null)
                    }
                }
                if (Nx.tarea.isNotEmpty()) {
                    Spacer(Modifier.height(8.dp))
                    Text(Nx.tarea, style = MaterialTheme.typography.bodySmall)
                    Spacer(Modifier.height(6.dp))
                    if (Nx.avance >= 0f) LinearProgressIndicator(progress = { Nx.avance }, modifier = Modifier.fillMaxWidth().clip(RoundedCornerShape(4.dp)))
                    else LinearProgressIndicator(modifier = Modifier.fillMaxWidth().clip(RoundedCornerShape(4.dp)))
                }
                if (Nx.problema.isNotEmpty()) {
                    Spacer(Modifier.height(8.dp))
                    GuiaDice("nono", Nx.problema)
                }
                Spacer(Modifier.height(4.dp))
                Text(t("Las claves y el firmware tienen que salir de tu propia Switch. Satronho no los incluye ni los baja de internet."),
                    style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            Spacer(Modifier.height(12.dp))
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.End, verticalAlignment = Alignment.CenterVertically) {
                if (e.instalado && e.responde) TextButton(onClick = { Nx.abrirAjustesNx(ctx) }) { Text(t("Ajustes de NX")) }
                Spacer(Modifier.weight(1f))
                TextButton(onClick = { Nx.cerrar() }) { Text(t("Cerrar")) }
                val p = a.pedido
                if (p != null) {
                    Spacer(Modifier.width(6.dp))
                    Button(onClick = { Nx.lanzar(ctx, p) }, enabled = e.instalado && e.responde && e.claves && Nx.tarea.isEmpty()) { Text(t("Jugar")) }
                }
            }
        }
    }
}

/** Texto con formato en el idioma de la app («%.0f GB libres»). */
fun TrFmtKt(es: String, vararg v: Any): String = try { String.format(t(es), *v) } catch (_: Throwable) { es }
