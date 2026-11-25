// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
package com.ridds.satronho.emulation

import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.RoundRect
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.StrokeJoin
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.TextMeasurer
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.sp
import kotlin.math.min

/** El «Pintor» de Mandos.kt sobre un Canvas de Compose. */
class Lienzo(private val d: DrawScope, private val medidor: TextMeasurer, private val letra: FontFamily?) : Pintor {
    private fun cc(argb: Int) = Color(argb)
    private fun o(v: V) = Offset(v.x, v.y)

    override fun rectFilled(a: V, b: V, col: Int, radio: Float, esquinas: Int) {
        val w = b.x - a.x
        val h = b.y - a.y
        if (w <= 0f || h <= 0f) return
        if (radio <= 0f) {
            d.drawRect(cc(col), o(a), Size(w, h))
            return
        }
        val r = min(radio, min(w, h) * 0.5f)
        if (esquinas == ESQ_TODAS) {
            d.drawRoundRect(cc(col), o(a), Size(w, h), CornerRadius(r))
            return
        }
        fun cr(bit: Int) = if (esquinas and bit != 0) CornerRadius(r) else CornerRadius.Zero
        val path = Path()
        path.addRoundRect(RoundRect(a.x, a.y, b.x, b.y, cr(ESQ_ARRIBA_IZQ), cr(ESQ_ARRIBA_DER), cr(ESQ_ABAJO_DER), cr(ESQ_ABAJO_IZQ)))
        d.drawPath(path, cc(col))
    }

    override fun rect(a: V, b: V, col: Int, radio: Float, grosor: Float) {
        val w = b.x - a.x
        val h = b.y - a.y
        if (w <= 0f || h <= 0f) return
        d.drawRoundRect(cc(col), o(a), Size(w, h), CornerRadius(min(radio, min(w, h) * 0.5f)), style = Stroke(grosor))
    }

    override fun circleFilled(c: V, r: Float, col: Int) {
        if (r > 0f) d.drawCircle(cc(col), r, o(c))
    }

    override fun circle(c: V, r: Float, col: Int, grosor: Float) {
        if (r > 0f) d.drawCircle(cc(col), r, o(c), style = Stroke(grosor))
    }

    override fun ellipseFilled(c: V, rx: Float, ry: Float, col: Int) {
        d.drawOval(cc(col), Offset(c.x - rx, c.y - ry), Size(rx * 2, ry * 2))
    }

    override fun line(a: V, b: V, col: Int, grosor: Float) {
        d.drawLine(cc(col), o(a), o(b), grosor, cap = StrokeCap.Round)
    }

    override fun triFilled(a: V, b: V, c: V, col: Int) = poly(listOf(a, b, c), col)

    override fun tri(a: V, b: V, c: V, col: Int, grosor: Float) {
        val p = Path()
        p.moveTo(a.x, a.y); p.lineTo(b.x, b.y); p.lineTo(c.x, c.y); p.close()
        d.drawPath(p, cc(col), style = Stroke(grosor, join = StrokeJoin.Round))
    }

    override fun rectMulti(a: V, b: V, arribaIzq: Int, arribaDer: Int, abajoDer: Int, abajoIzq: Int) {
        val w = b.x - a.x
        val h = b.y - a.y
        if (w <= 0f || h <= 0f) return
        // De arriba a abajo (como lo usan las pantallas y las etiquetas).
        d.drawRect(Brush.verticalGradient(listOf(cc(arribaIzq), cc(abajoIzq)), a.y, b.y), o(a), Size(w, h))
        d.drawRect(Brush.horizontalGradient(listOf(Color.Transparent, cc(arribaDer).copy(alpha = 0.35f)), a.x, b.x), o(a), Size(w, h))
    }

    override fun poly(puntos: List<V>, col: Int) {
        if (puntos.size < 3) return
        val p = Path()
        p.moveTo(puntos[0].x, puntos[0].y)
        for (i in 1 until puntos.size) p.lineTo(puntos[i].x, puntos[i].y)
        p.close()
        d.drawPath(p, cc(col))
    }

    override fun text(t: String, centro: V, tam: Float, col: Int, centrado: Boolean, negrita: Boolean) {
        if (t.isEmpty() || tam < 2f) return
        val estilo = TextStyle(
            color = cc(col), fontSize = (tam / d.density / d.fontScale).sp, fontFamily = letra,
            fontWeight = if (negrita) FontWeight.Bold else FontWeight.SemiBold,
        )
        val m = medidor.measure(t, estilo, maxLines = 1, softWrap = false)
        val x = if (centrado) centro.x - m.size.width / 2f else centro.x
        d.drawText(m, topLeft = Offset(x, centro.y - m.size.height / 2f))
    }
}

