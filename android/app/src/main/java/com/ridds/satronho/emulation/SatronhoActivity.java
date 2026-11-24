// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
package com.ridds.satronho.emulation;

import android.Manifest;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInstaller;
import android.content.pm.PackageManager;
import android.content.pm.ActivityInfo;
import android.content.res.AssetManager;
import android.database.Cursor;
import android.net.Uri;
import android.os.StatFs;
import android.provider.OpenableColumns;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.provider.Settings;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;

/**
 * Satronho Emulation en Android: la misma app de PC (C++ y SDL3). Esta clase solo prepara lo que
 * Android pide aparte: copia los assets la primera vez, dice dónde quedan los núcleos y ofrece al
 * código en C++ unas pocas cosas del sistema (permiso de archivos, descargas, vibración e instalar
 * una build nueva).
 */
public class SatronhoActivity extends SDLActivity {
    private static final String TAG = "Satronho";
    private static SatronhoActivity sInstance;

    @Override
    protected String[] getLibraries() {
        return new String[] {"c++_shared", "SDL3", "main"};
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        sInstance = this;
        prepareFiles();
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        recibir(getIntent());
    }

    /** La orientación la elige Satronho (Ajustes), no SDL según el tamaño de la ventana. */
    @Override
    public void setOrientationBis(int w, int h, boolean resizable, String hint) {}

    // ------------------------------------------------------------ juegos que llegan de otras apps
    // «Abrir con» y «Compartir» desde WhatsApp, Telegram, Drive, Archivos… Android solo deja leer
    // el archivo un rato, así que se copia ya a la carpeta de la app y el código en C++ lo acomoda.

    private static final Object sLock = new Object();
    private static final ArrayList<String> sRecibidos = new ArrayList<>(); // líneas «ok|ruta» o «error|nombre|motivo»
    private static volatile String sEstado = "";                         // «nombre|copiados|total» mientras copia
    private static int sPendientes = 0;

    private void recibir(Intent intent) {
        if (intent == null) return;
        final ArrayList<Uri> uris = new ArrayList<>();
        final String action = intent.getAction();
        try {
            if (Intent.ACTION_VIEW.equals(action) && intent.getData() != null) {
                uris.add(intent.getData());
            } else if (Intent.ACTION_SEND.equals(action)) {
                Uri u = intent.getParcelableExtra(Intent.EXTRA_STREAM);
                if (u != null) uris.add(u);
            } else if (Intent.ACTION_SEND_MULTIPLE.equals(action)) {
                ArrayList<Uri> list = intent.getParcelableArrayListExtra(Intent.EXTRA_STREAM);
                if (list != null) uris.addAll(list);
            }
        } catch (Exception e) {
            Log.w(TAG, "No se pudo leer lo que llegó: " + e);
        }
        if (uris.isEmpty()) return;
        // Que no se procese otra vez al volver a la app.
        intent.setAction(Intent.ACTION_MAIN);
        synchronized (sLock) { sPendientes += uris.size(); }
        new Thread(() -> {
            for (Uri u : uris) copiarRecibido(u);
        }, "Satronho-recibir").start();
    }

    private void copiarRecibido(Uri uri) {
        String nombre = "juego";
        long total = -1;
        try (Cursor c = getContentResolver().query(uri, new String[] {OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE}, null, null, null)) {
            if (c != null && c.moveToFirst()) {
                int n = c.getColumnIndex(OpenableColumns.DISPLAY_NAME), z = c.getColumnIndex(OpenableColumns.SIZE);
                if (n >= 0 && !c.isNull(n)) nombre = c.getString(n);
                if (z >= 0 && !c.isNull(z)) total = c.getLong(z);
            }
        } catch (Exception e) {
            if (uri.getLastPathSegment() != null) nombre = uri.getLastPathSegment();
        }
        nombre = nombre.replaceAll("[\\\\/:*?\"<>|]", "_");
        try {
            File dir = new File(getExternalFilesDir(null), "recibidos");
            dir.mkdirs();
            if (total > 0) {
                long libre = new StatFs(dir.getAbsolutePath()).getAvailableBytes();
                if (libre < total + (64L << 20)) {
                    terminar("sinespacio|" + nombre + "|" + total + "|" + libre);
                    return;
                }
            }
            File out = new File(dir, nombre);
            File part = new File(dir, nombre + ".part");
            long copiados = 0;
            try (InputStream in = getContentResolver().openInputStream(uri); OutputStream o = new FileOutputStream(part)) {
                if (in == null) throw new Exception("sin datos");
                byte[] buf = new byte[1 << 20];
                int n;
                while ((n = in.read(buf)) > 0) {
                    o.write(buf, 0, n);
                    copiados += n;
                    sEstado = nombre + "|" + copiados + "|" + total;
                }
            }
            out.delete();
            if (!part.renameTo(out)) throw new Exception("no se pudo guardar");
            terminar("ok|" + out.getAbsolutePath());
        } catch (Exception e) {
            Log.w(TAG, "No se pudo copiar " + nombre + ": " + e);
            terminar("error|" + nombre + "|" + e.getMessage());
        }
    }

    private static void terminar(String linea) {
        synchronized (sLock) {
            sRecibidos.add(linea);
            sPendientes = Math.max(0, sPendientes - 1);
            if (sPendientes == 0) sEstado = "";
        }
    }

    /** Lo siguiente que terminó de llegar («ok|ruta», «error|…», «sinespacio|…») o "" si nada. */
    public static String recibido() {
        synchronized (sLock) {
            return sRecibidos.isEmpty() ? "" : sRecibidos.remove(0);
        }
    }

    /** Mientras copia: «nombre|copiados|total|pendientes»; "" si no está copiando. */
    public static String estadoRecepcion() {
        synchronized (sLock) {
            return sPendientes == 0 ? "" : sEstado + "|" + sPendientes;
        }
    }

    /** 0 horizontal, 1 girar sola, 2 vertical. */
    public static void orientacion(int modo) {
        final SatronhoActivity a = sInstance;
        if (a == null) return;
        final int o = modo == 1 ? ActivityInfo.SCREEN_ORIENTATION_FULL_USER
                : modo == 2 ? ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT : ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE;
        a.runOnUiThread(() -> a.setRequestedOrientation(o));
    }

    /** Alto del teclado en pantalla, en píxeles (0 si está escondido o no se sabe). */
    public static int tecladoAlto() {
        return org.libsdl.app.SDLActivity.sImeBottom;
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            // Pantalla completa de verdad (sin barras), como una consola.
            getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
        }
    }

    /** Copia los assets del APK a files/assets (las fuentes y los textos se leen como archivos). */
    private void prepareFiles() {
        File base = getFilesDir();
        try {
            long version = getPackageManager().getPackageInfo(getPackageName(), 0).lastUpdateTime;
            File mark = new File(base, "assets/.version");
            String want = Long.toString(version);
            boolean fresh = mark.exists() && want.equals(readText(mark));
            if (!fresh) {
                deleteTree(new File(base, "assets"));
                copyAssets(getAssets(), "", new File(base, "assets"));
                writeText(mark, want);
            }
            writeText(new File(base, "nucleos.txt"), getApplicationInfo().nativeLibraryDir);
        } catch (Exception e) {
            Log.e(TAG, "No se pudieron preparar los archivos", e);
        }
    }

    private static void copyAssets(AssetManager am, String path, File out) throws Exception {
        String[] list = am.list(path);
        if (list == null || list.length == 0) {
            if (path.isEmpty()) return;
            out.getParentFile().mkdirs();
            try (InputStream in = am.open(path); OutputStream o = new FileOutputStream(out)) {
                pipe(in, o);
            }
            return;
        }
        out.mkdirs();
        for (String name : list) {
            // Lo que el sistema agrega al APK no es nuestro.
            if (path.isEmpty() && (name.equals("images") || name.equals("webkit") || name.equals("sounds"))) continue;
            copyAssets(am, path.isEmpty() ? name : path + "/" + name, new File(out, name));
        }
    }

    private static void pipe(InputStream in, OutputStream out) throws Exception {
        byte[] buf = new byte[1 << 16];
        int n;
        while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
    }

    private static String readText(File f) {
        try (InputStream in = new FileInputStream(f)) {
            byte[] b = new byte[(int) f.length()];
            int n = in.read(b);
            return new String(b, 0, Math.max(0, n), "UTF-8").trim();
        } catch (Exception e) {
            return "";
        }
    }

    private static void writeText(File f, String s) throws Exception {
        f.getParentFile().mkdirs();
        try (OutputStream o = new FileOutputStream(f)) {
            o.write(s.getBytes("UTF-8"));
        }
    }

    private static void deleteTree(File f) {
        File[] kids = f.listFiles();
        if (kids != null) for (File k : kids) deleteTree(k);
        f.delete();
    }

    // ------------------------------------------------------------ lo que llama el código en C++

    /** ¿Puede leer los juegos de cualquier carpeta (y escribir en «Satronho Games»)? */
    public static boolean tieneAccesoArchivos() {
        if (Build.VERSION.SDK_INT >= 30) return Environment.isExternalStorageManager();
        // Android 8, 9 y 10: el permiso de siempre, que se pide con una ventanita.
        final SatronhoActivity a = sInstance;
        return a != null && a.checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED;
    }

    /** Abre la pantalla de Android donde se da el permiso de «acceso a todos los archivos». */
    public static void pedirAccesoArchivos() {
        final SatronhoActivity a = sInstance;
        if (a == null) return;
        if (Build.VERSION.SDK_INT < 30) {
            a.runOnUiThread(() -> a.requestPermissions(new String[] {Manifest.permission.READ_EXTERNAL_STORAGE,
                    Manifest.permission.WRITE_EXTERNAL_STORAGE}, 7731)); // número propio (SDL usa los suyos)
            return;
        }
        a.runOnUiThread(() -> {
            try {
                Intent i = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
                i.setData(Uri.parse("package:" + a.getPackageName()));
                a.startActivity(i);
            } catch (Exception e) {
                a.startActivity(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION));
            }
        });
    }

    /** Carpetas raíz: la memoria interna y las tarjetas SD, una por línea. */
    public static String raices() {
        StringBuilder sb = new StringBuilder();
        sb.append(Environment.getExternalStorageDirectory().getAbsolutePath());
        SatronhoActivity a = sInstance;
        if (a != null) {
            File[] dirs = a.getExternalFilesDirs(null);
            for (File d : dirs) {
                if (d == null) continue;
                String p = d.getAbsolutePath();
                int cut = p.indexOf("/Android/");
                if (cut > 0) p = p.substring(0, cut);
                if (sb.indexOf(p) < 0) sb.append('\n').append(p);
            }
        }
        return sb.toString();
    }

    /** Descarga una dirección a un archivo (sigue redirecciones, como las de GitHub). */
    public static boolean descargar(String url, String path) {
        try {
            File out = new File(path);
            File part = new File(path + ".part");
            out.getParentFile().mkdirs();
            String current = url;
            for (int hop = 0; hop < 6; ++hop) {
                HttpURLConnection c = (HttpURLConnection) new URL(current).openConnection();
                c.setInstanceFollowRedirects(false);
                c.setConnectTimeout(15000);
                c.setReadTimeout(30000);
                c.setRequestProperty("User-Agent", "SatronhoEmulation");
                int code = c.getResponseCode();
                if (code >= 300 && code < 400) {
                    current = new URL(new URL(current), c.getHeaderField("Location")).toString();
                    c.disconnect();
                    continue;
                }
                if (code != 200) {
                    c.disconnect();
                    return false;
                }
                try (InputStream in = c.getInputStream(); OutputStream o = new FileOutputStream(part)) {
                    pipe(in, o);
                }
                c.disconnect();
                out.delete();
                return part.renameTo(out);
            }
        } catch (Exception e) {
            Log.w(TAG, "Descarga fallida: " + url + " (" + e + ")");
        }
        return false;
    }

    /** Vibración corta del control en pantalla. */
    public static void vibrar(int ms, int fuerza) {
        SatronhoActivity a = sInstance;
        if (a == null) return;
        Vibrator v = (Vibrator) a.getSystemService(Context.VIBRATOR_SERVICE);
        if (v == null || !v.hasVibrator()) return;
        v.vibrate(VibrationEffect.createOneShot(Math.max(1, ms), Math.max(1, Math.min(255, fuerza))));
    }

    /** Instala una build nueva: Android muestra su ventana de confirmación. */
    public static boolean instalarApk(String path) {
        final SatronhoActivity a = sInstance;
        if (a == null) return false;
        try {
            if (Build.VERSION.SDK_INT >= 26 && !a.getPackageManager().canRequestPackageInstalls()) {
                a.runOnUiThread(() -> a.startActivity(new Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES,
                        Uri.parse("package:" + a.getPackageName()))));
                return false;
            }
            PackageInstaller pi = a.getPackageManager().getPackageInstaller();
            PackageInstaller.SessionParams params = new PackageInstaller.SessionParams(PackageInstaller.SessionParams.MODE_FULL_INSTALL);
            int id = pi.createSession(params);
            try (PackageInstaller.Session s = pi.openSession(id)) {
                File f = new File(path);
                try (InputStream in = new FileInputStream(f); OutputStream o = s.openWrite("satronho.apk", 0, f.length())) {
                    pipe(in, o);
                    s.fsync(o);
                }
                Intent i = new Intent(a, SatronhoActivity.class);
                PendingIntent p = PendingIntent.getActivity(a, id, i, PendingIntent.FLAG_MUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
                s.commit(p.getIntentSender());
            }
            return true;
        } catch (Exception e) {
            Log.e(TAG, "No se pudo instalar", e);
            return false;
        }
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        recibir(intent);
        // El instalador pide confirmación con su propia ventana.
        Intent confirm = intent.getParcelableExtra(Intent.EXTRA_INTENT);
        if (confirm != null) startActivity(confirm);
    }
}
