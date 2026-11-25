// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
package com.ridds.satronho.emulation

import android.app.PendingIntent
import android.appwidget.AppWidgetManager
import android.appwidget.AppWidgetProvider
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.graphics.BitmapFactory
import android.widget.RemoteViews
import java.io.File

/**
 * Widget «Continuar» para la pantalla de inicio (como el de Satronho NX): el último juego que jugaste,
 * con su portada; al tocarlo, el hub abre y sigue justo donde quedaste.
 */
class ContinuarWidget : AppWidgetProvider() {
    override fun onUpdate(context: Context, manager: AppWidgetManager, ids: IntArray) {
        for (id in ids) manager.updateAppWidget(id, vista(context))
    }

    companion object {
        const val ACCION = "com.ridds.satronho.emulation.CONTINUAR"
        private const val PREFS = "satronho_widget"

        private fun vista(ctx: Context): RemoteViews {
            val p = ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
            val clave = p.getString("clave", "") ?: ""
            val v = RemoteViews(ctx.packageName, R.layout.widget_continuar)
            v.setTextViewText(R.id.widget_titulo, p.getString("titulo", null) ?: "Satronho Emulation")
            v.setTextViewText(R.id.widget_consola, p.getString("consola", "") ?: "")
            v.setTextViewText(R.id.widget_boton, if (clave.isEmpty()) "▶" else "▶  " + (p.getString("boton", "") ?: ""))
            val portada = p.getString("portada", "") ?: ""
            val bmp = if (portada.isNotEmpty() && File(portada).exists()) try {
                BitmapFactory.decodeFile(portada, BitmapFactory.Options().apply { inSampleSize = 2 })
            } catch (_: Throwable) { null } else null
            if (bmp != null) v.setImageViewBitmap(R.id.widget_portada, bmp) else v.setImageViewResource(R.id.widget_portada, R.mipmap.ic_launcher)
            val i = Intent(ctx, SatronhoActivity::class.java).setAction(ACCION).putExtra("clave", clave)
            val pi = PendingIntent.getActivity(ctx, 7, i, PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT)
            v.setOnClickPendingIntent(R.id.widget_raiz, pi)
            return v
        }

        /** La biblioteca cambió: si el último juego es otro, el widget se actualiza. */
        fun actualizar(ctx: Context, b: Biblioteca) {
            val g = b.juegos.filter { it.ultima > 0 }.maxByOrNull { it.ultima } ?: return
            val p = ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
            val firma = g.clave + "|" + g.titulo + "|" + g.portada + "|" + t("Continuar")
            if (p.getString("firma", "") == firma) return
            p.edit().putString("firma", firma).putString("clave", g.clave).putString("titulo", g.titulo)
                .putString("consola", b.consola(g.sistema)?.completo ?: "").putString("portada", g.portada)
                .putString("boton", t("Continuar")).apply()
            val m = AppWidgetManager.getInstance(ctx)
            val ids = m.getAppWidgetIds(ComponentName(ctx, ContinuarWidget::class.java))
            if (ids.isNotEmpty()) for (id in ids) m.updateAppWidget(id, vista(ctx))
        }
    }
}
