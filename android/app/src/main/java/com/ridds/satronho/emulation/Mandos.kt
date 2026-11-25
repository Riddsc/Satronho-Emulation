// SPDX-FileCopyrightText: Copyright 2026 Ridds (Satronho Emulation)
// SPDX-License-Identifier: GPL-3.0-or-later
package com.ridds.satronho.emulation

import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.max
import kotlin.math.min
import kotlin.math.sin

// El dibujo de los controles de Satronho, igual que en PC (src/ui/controllers.cpp y el control en
// pantalla de src/app_touch.cpp), pasado a Kotlin. No usa nada de Android: dibuja sobre un «Pintor»
// (Compose le da uno en Lienzo.kt). Así el mismo dibujo sirve en Ajustes » Controles y en el juego.

/** Un punto. */
class V(@JvmField val x: Float, @JvmField val y: Float)

/** Lo mínimo para dibujar (como el ImDrawList de ImGui). Los colores son 0xAARRGGBB. */
interface Pintor {
    fun rectFilled(a: V, b: V, col: Int, radio: Float = 0f, esquinas: Int = ESQ_TODAS)
    fun rect(a: V, b: V, col: Int, radio: Float, grosor: Float)
    fun circleFilled(c: V, r: Float, col: Int)
    fun circle(c: V, r: Float, col: Int, grosor: Float)
    fun ellipseFilled(c: V, rx: Float, ry: Float, col: Int)
    fun line(a: V, b: V, col: Int, grosor: Float)
    fun triFilled(a: V, b: V, c: V, col: Int)
    fun tri(a: V, b: V, c: V, col: Int, grosor: Float)
    fun rectMulti(a: V, b: V, arribaIzq: Int, arribaDer: Int, abajoDer: Int, abajoIzq: Int)
    fun poly(puntos: List<V>, col: Int)
    fun text(t: String, centro: V, tam: Float, col: Int, centrado: Boolean = true, negrita: Boolean = true)
}

const val ESQ_ARRIBA_IZQ = 1
const val ESQ_ARRIBA_DER = 2
const val ESQ_ABAJO_IZQ = 4
const val ESQ_ABAJO_DER = 8
const val ESQ_ARRIBA = ESQ_ARRIBA_IZQ or ESQ_ARRIBA_DER
const val ESQ_TODAS = 15

/** IM_COL32 de ImGui → 0xAARRGGBB. */
fun col(r: Int, g: Int, b: Int, a: Int = 255): Int = (a shl 24) or (r shl 16) or (g shl 8) or b
fun rgb(rgb: Int, alfa: Float = 1f): Int = (((alfa.coerceIn(0f, 1f) * 255).toInt()) shl 24) or (rgb and 0xFFFFFF)

/** Color aclarado (k > 1) u oscurecido (k < 1), con transparencia (como Shade de PC). */
fun sombra(rgb: Int, k: Float, alfa: Float): Int {
    fun ch(s: Int): Int {
        val v = ((rgb shr s) and 0xFF).toFloat()
        val o = if (k >= 1f) v + (255f - v) * (k - 1f) else v * k
        return o.coerceIn(0f, 255f).toInt()
    }
    return col(ch(16), ch(8), ch(0), (alfa.coerceIn(0f, 1f) * 255).toInt())
}

// Botones del RetroPad (libretro.h).
object Rp {
    const val B = 0; const val Y = 1; const val SELECT = 2; const val START = 3
    const val UP = 4; const val DOWN = 5; const val LEFT = 6; const val RIGHT = 7
    const val A = 8; const val X = 9; const val L = 10; const val R = 11
    const val L2 = 12; const val R2 = 13; const val L3 = 14; const val R3 = 15
}

private const val ORO = 0xFFD166

/** Puntos de un arco (para formas redondeadas). */
private fun arco(out: MutableList<V>, c: V, r: Float, a0: Float, a1: Float, pasos: Int = 12) {
    for (i in 0..pasos) {
        val a = a0 + (a1 - a0) * i / pasos
        out.add(V(c.x + cos(a) * r, c.y + sin(a) * r))
    }
}

// ================================================================== controles de Ajustes » Controles

/** Contexto del dibujo: origen, escala y qué está apretado o esperando asignación. */
class Ctx(
    val dl: Pintor, private val ox: Float, private val oy: Float, val k: Float,
    private val apretados: Int, val capturando: Int, val pulso: Float, private val acento: Int,
) {
    /** Zonas que se pueden tocar (botón, x, y, radio en píxeles). */
    val zonas = ArrayList<FloatArray>()

    fun P(x: Number, y: Number) = V(ox + x.toFloat() * k, oy + y.toFloat() * k)
    fun L(v: Number) = v.toFloat() * k
    fun down(id: Int) = id >= 0 && (apretados shr id) and 1 == 1
    fun hit(id: Int, x: Number, y: Number, r: Number) {
        val p = P(x, y)
        zonas.add(floatArrayOf(id.toFloat(), p.x, p.y, L(r)))
    }
    fun acento(a: Float = 1f) = rgb(acento, a)
    fun oro(a: Float = 1f) = rgb(ORO, a)
    fun ring(id: Int, c: V, r: Float) {
        if (capturando == id) dl.circle(c, r + L(6), acento(0.4f + 0.6f * pulso), L(4))
        if (down(id)) {
            dl.circleFilled(c, r + L(9), oro(0.18f))
            dl.circle(c, r + L(5), oro(), L(3.5f))
        }
    }
    fun marco(id: Int, a: V, b: V, r: Float) {
        if (capturando == id) dl.rect(V(a.x - L(3), a.y - L(3)), V(b.x + L(3), b.y + L(3)), acento(0.4f + 0.6f * pulso), L(r), L(2.5f))
    }
    fun text(t: String, x: Number, y: Number, size: Number, c: Int, centro: Boolean = true) = dl.text(t, P(x, y), L(size), c, centro)
}

private fun dpad(c: Ctx, cx: Float, cy: Float, size: Float, body: Int, shade: Int) {
    val a = size * 0.5f
    val w = size * 0.17f
    val dl = c.dl
    dl.rectFilled(c.P(cx - a + 3, cy - w + 4), c.P(cx + a + 3, cy + w + 4), col(0, 0, 0, 60), c.L(4))
    dl.rectFilled(c.P(cx - w + 3, cy - a + 4), c.P(cx + w + 3, cy + a + 4), col(0, 0, 0, 60), c.L(4))
    dl.rectFilled(c.P(cx - a, cy - w), c.P(cx + a, cy + w), body, c.L(4))
    dl.rectFilled(c.P(cx - w, cy - a), c.P(cx + w, cy + a), body, c.L(4))
    dl.circleFilled(c.P(cx, cy), c.L(w * 0.55f), shade)
    val brazos = listOf(Triple(Rp.UP, 0f, -1f), Triple(Rp.DOWN, 0f, 1f), Triple(Rp.LEFT, -1f, 0f), Triple(Rp.RIGHT, 1f, 0f))
    for ((id, dx, dy) in brazos) {
        val mx = cx + dx * (a - w * 1.1f)
        val my = cy + dy * (a - w * 1.1f)
        c.hit(id, mx, my, max(w * 1.3f, 18f))
        val tip = c.P(mx + dx * w * 0.45f, my + dy * w * 0.45f)
        val b1 = c.P(mx - dx * w * 0.25f + dy * w * 0.45f, my - dy * w * 0.25f + dx * w * 0.45f)
        val b2 = c.P(mx - dx * w * 0.25f - dy * w * 0.45f, my - dy * w * 0.25f - dx * w * 0.45f)
        dl.triFilled(tip, b1, b2, shade)
        val r0 = c.P(if (dx != 0f) (if (dx < 0) cx - a else cx + w) else cx - w, if (dy != 0f) (if (dy < 0) cy - a else cy + w) else cy - w)
        val r1 = c.P(if (dx != 0f) (if (dx < 0) cx - w else cx + a) else cx + w, if (dy != 0f) (if (dy < 0) cy - w else cy + a) else cy + w)
        if (c.down(id)) dl.rectFilled(r0, r1, c.oro(0.85f), c.L(3))
        if (c.capturando == id) dl.rect(r0, r1, c.acento(0.4f + 0.6f * c.pulso), c.L(3), c.L(2.5f))
    }
}

private fun round(c: Ctx, id: Int, x: Float, y: Float, r: Float, color: Int, shade: Int) {
    c.hit(id, x, y, max(r * 1.3f, 22f))
    val p = c.P(x, y)
    c.dl.circleFilled(V(p.x + c.L(2), p.y + c.L(4)), c.L(r), col(0, 0, 0, 70))
    c.dl.circleFilled(p, c.L(r), if (c.down(id)) c.oro() else color)
    c.dl.circleFilled(V(p.x - c.L(r * 0.25f), p.y - c.L(r * 0.3f)), c.L(r * 0.45f), shade)
    c.ring(id, p, c.L(r))
}

private fun pill(c: Ctx, id: Int, x: Float, y: Float, len: Float, thick: Float, angulo: Float, color: Int) {
    c.hit(id, x, y, len * 0.6f)
    val ar = angulo * PI.toFloat() / 180f
    val dx = cos(ar) * len * 0.5f
    val dy = sin(ar) * len * 0.5f
    val a = c.P(x - dx, y - dy)
    val b = c.P(x + dx, y + dy)
    val cc = if (c.down(id)) c.oro() else color
    c.dl.line(V(a.x + c.L(1.5f), a.y + c.L(3)), V(b.x + c.L(1.5f), b.y + c.L(3)), col(0, 0, 0, 70), c.L(thick))
    c.dl.line(a, b, cc, c.L(thick))
    c.dl.circleFilled(a, c.L(thick * 0.5f), cc)
    c.dl.circleFilled(b, c.L(thick * 0.5f), cc)
    if (c.capturando == id) {
        val pad = thick * 0.5f + 5
        c.dl.rect(c.P(x - len * 0.5f - pad, y - pad), c.P(x + len * 0.5f + pad, y + pad), c.acento(0.4f + 0.6f * c.pulso), c.L(pad), c.L(2.5f))
    }
}

private fun letra(c: Ctx, id: Int, t: String, x: Float, y: Float, size: Float, color: Int) =
    c.text(t, x, y, size, if (c.down(id)) col(70, 50, 0) else color)

private fun hombro(c: Ctx, id: Int, x0: Float, x1: Float, y0: Float, y1: Float, color: Int, esq: Int, radio: Float, t: String, ty: Float, tcol: Int, ts: Float) {
    c.hit(id, (x0 + x1) * 0.5f, (y0 + y1) * 0.5f, max(36f, (x1 - x0) * 0.35f))
    val a = c.P(x0, y0)
    val b = c.P(x1, y1)
    c.dl.rectFilled(a, b, if (c.down(id)) c.oro() else color, c.L(radio), esq)
    c.marco(id, a, b, radio)
    if (t.isNotEmpty()) c.text(t, (x0 + x1) * 0.5f, ty, ts, if (c.down(id)) col(60, 40, 0) else tcol)
}

private fun drawNes(c: Ctx) {
    val dl = c.dl
    dl.rectFilled(c.P(6, 12), c.P(686, 312), col(0, 0, 0, 80), c.L(16))
    dl.rectFilled(c.P(0, 0), c.P(680, 300), col(203, 201, 198), c.L(16))
    dl.rectFilled(c.P(0, 0), c.P(680, 22), col(220, 218, 215), c.L(16), ESQ_ARRIBA)
    dl.rect(c.P(0, 0), c.P(680, 300), col(150, 148, 146), c.L(16), c.L(2))
    dl.rectFilled(c.P(22, 62), c.P(658, 280), col(28, 27, 30), c.L(10))
    for (i in 0 until 4) {
        val y = 86f + i * 24
        dl.rectFilled(c.P(232, y), c.P(448, y + 14), col(150, 148, 146), c.L(3))
    }
    dl.rectFilled(c.P(232, 196), c.P(448, 256), col(176, 174, 171), c.L(8))
    c.text("SELECT", 290, 184, 13, col(214, 40, 48))
    c.text("START", 390, 184, 13, col(214, 40, 48))
    pill(c, Rp.SELECT, 290f, 226f, 44f, 16f, 0f, col(36, 35, 38))
    pill(c, Rp.START, 390f, 226f, 44f, 16f, 0f, col(36, 35, 38))
    dl.rectFilled(c.P(58, 108), c.P(208, 258), col(40, 39, 42), c.L(12))
    dpad(c, 133f, 183f, 128f, col(18, 18, 20), col(48, 47, 52))
    for (i in 0 until 2) {
        val x = if (i == 1) 572f else 484f
        dl.rectFilled(c.P(x - 38, 168), c.P(x + 38, 244), col(176, 174, 171), c.L(10))
    }
    round(c, Rp.B, 484f, 206f, 30f, col(206, 34, 44), col(230, 82, 90))
    round(c, Rp.A, 572f, 206f, 30f, col(206, 34, 44), col(230, 82, 90))
    c.text("B", 525, 262, 15, col(214, 40, 48))
    c.text("A", 613, 262, 15, col(214, 40, 48))
}

private fun drawGameBoy(c: Ctx) {
    val dl = c.dl
    fun body(dx: Float, dy: Float, color: Int) {
        val pts = ArrayList<V>()
        arco(pts, c.P(14 + dx, 14 + dy), c.L(14), PI.toFloat(), PI.toFloat() * 1.5f)
        arco(pts, c.P(286 + dx, 14 + dy), c.L(14), PI.toFloat() * 1.5f, PI.toFloat() * 2f)
        arco(pts, c.P(230 + dx, 434 + dy), c.L(56), 0f, PI.toFloat() * 0.5f)
        arco(pts, c.P(14 + dx, 476 + dy), c.L(14), PI.toFloat() * 0.5f, PI.toFloat())
        dl.poly(pts, color)
    }
    body(5f, 10f, col(0, 0, 0, 80))
    body(0f, 0f, col(200, 197, 189))
    for (i in 0 until 2) dl.line(c.P(16, 22 + i * 6), c.P(284, 22 + i * 6), col(170, 167, 160), c.L(2))
    val marco = ArrayList<V>()
    marco.add(c.P(28, 44)); marco.add(c.P(272, 44))
    arco(marco, c.P(250, 210), c.L(22), 0f, PI.toFloat() * 0.5f)
    marco.add(c.P(28, 232))
    dl.poly(marco, col(88, 86, 102))
    dl.rectMulti(c.P(72, 66), c.P(232, 210), col(155, 188, 15), col(139, 172, 15), col(110, 145, 12), col(120, 155, 14))
    dl.circleFilled(c.P(48, 122), c.L(5), col(220, 40, 40))
    dpad(c, 76f, 318f, 78f, col(24, 24, 28), col(56, 56, 62))
    round(c, Rp.B, 194f, 334f, 20f, col(156, 37, 89), col(186, 70, 120))
    round(c, Rp.A, 252f, 306f, 20f, col(156, 37, 89), col(186, 70, 120))
    c.text("B", 194, 366, 13, col(60, 60, 140))
    c.text("A", 252, 338, 13, col(60, 60, 140))
    pill(c, Rp.SELECT, 118f, 404f, 30f, 9f, -25f, col(128, 126, 134))
    pill(c, Rp.START, 168f, 404f, 30f, 9f, -25f, col(128, 126, 134))
    c.text("SELECT", 116, 428, 9, col(60, 60, 140))
    c.text("START", 168, 428, 9, col(60, 60, 140))
    for (i in 0 until 6) {
        val x = 210f + i * 12
        dl.line(c.P(x, 470), c.P(x + 24, 410), col(150, 147, 140), c.L(5))
    }
}

private fun drawGba(c: Ctx) {
    val dl = c.dl
    val shell = col(90, 74, 176)
    val shellDark = col(66, 52, 140)
    hombro(c, Rp.L, 30f, 180f, -6f, 44f, col(150, 140, 200), ESQ_ARRIBA_IZQ, 18f, "", 0f, 0, 0f)
    hombro(c, Rp.R, 380f, 530f, -6f, 44f, col(150, 140, 200), ESQ_ARRIBA_DER, 18f, "", 0f, 0, 0f)
    dl.rectFilled(c.P(6, 40), c.P(566, 320), col(0, 0, 0, 80), c.L(130))
    dl.rectFilled(c.P(0, 30), c.P(560, 310), shell, c.L(130))
    dl.rectFilled(c.P(150, 30), c.P(410, 310), shellDark, c.L(28))
    c.text("L", 105, 12, 18, if (c.down(Rp.L)) col(60, 40, 0) else col(50, 40, 100))
    c.text("R", 455, 12, 18, if (c.down(Rp.R)) col(60, 40, 0) else col(50, 40, 100))
    dl.rectFilled(c.P(166, 58), c.P(394, 236), col(40, 38, 52), c.L(14))
    dl.rectMulti(c.P(190, 78), c.P(370, 198), col(60, 70, 80), col(50, 58, 70), col(32, 38, 48), col(40, 46, 58))
    dpad(c, 92f, 140f, 84f, col(40, 34, 70), col(70, 62, 110))
    round(c, Rp.A, 482f, 122f, 22f, col(170, 160, 220), col(200, 192, 240))
    round(c, Rp.B, 436f, 160f, 22f, col(170, 160, 220), col(200, 192, 240))
    c.text("A", 482, 156, 13, col(210, 205, 240))
    c.text("B", 436, 194, 13, col(210, 205, 240))
    round(c, Rp.START, 108f, 232f, 12f, col(40, 34, 70), col(70, 62, 110))
    round(c, Rp.SELECT, 108f, 272f, 12f, col(40, 34, 70), col(70, 62, 110))
    c.text("START", 128, 232, 12, col(210, 205, 240), false)
    c.text("SELECT", 128, 272, 12, col(210, 205, 240), false)
    for (r in 0 until 3) for (i in 0 until 3) dl.circleFilled(c.P(462 + i * 14, 236 + r * 14), c.L(3), shellDark)
}

private fun drawGenesis(c: Ctx) {
    val dl = c.dl
    val shell = col(36, 35, 41)
    val rim = col(58, 57, 66)
    fun body(dx: Float, dy: Float, color: Int) {
        dl.circleFilled(c.P(170 + dx, 182 + dy), c.L(150), color)
        dl.circleFilled(c.P(470 + dx, 182 + dy), c.L(150), color)
        dl.rectFilled(c.P(170 + dx, 58 + dy), c.P(470 + dx, 318 + dy), color, c.L(40))
    }
    run {
        val id = Rp.SELECT
        c.hit(id, 520, 30, 30)
        dl.rectFilled(c.P(496, 18), c.P(544, 44), if (c.down(id)) c.oro() else col(70, 69, 78), c.L(8))
        c.marco(id, c.P(490, 12), c.P(550, 50), 10f)
    }
    body(5f, 10f, col(0, 0, 0, 90))
    body(0f, 0f, rim)
    dl.circleFilled(c.P(170, 182), c.L(142), shell)
    dl.circleFilled(c.P(470, 182), c.L(142), shell)
    dl.rectFilled(c.P(170, 66), c.P(470, 310), shell, c.L(36))
    c.text("MODE", 520, 66, 12, col(150, 148, 160))
    dl.circleFilled(c.P(170, 186), c.L(82), col(26, 25, 30))
    dl.circle(c.P(170, 186), c.L(82), col(70, 69, 78), c.L(2))
    dpad(c, 170f, 186f, 132f, col(52, 51, 59), col(80, 79, 90))
    run {
        val id = Rp.START
        c.hit(id, 320, 150, 34)
        dl.rectFilled(c.P(298, 143), c.P(346, 163), col(0, 0, 0, 80), c.L(10))
        dl.rectFilled(c.P(296, 140), c.P(344, 160), if (c.down(id)) c.oro() else col(64, 63, 72), c.L(10))
        c.marco(id, c.P(288, 132), c.P(352, 168), 14f)
        c.text("START", 320, 182, 12, col(150, 148, 160))
    }
    val btn = col(62, 61, 70)
    val hi = col(92, 91, 104)
    val letter = col(190, 188, 200)
    data class B(val id: Int, val t: String, val x: Float, val y: Float, val r: Float)
    for (b in listOf(B(Rp.Y, "X", 404f, 132f, 19f), B(Rp.X, "Y", 462f, 116f, 19f), B(Rp.L, "Z", 520f, 100f, 19f),
        B(Rp.B, "A", 398f, 214f, 28f), B(Rp.A, "B", 466f, 196f, 28f), B(Rp.R, "C", 534f, 178f, 28f))) {
        round(c, b.id, b.x, b.y, b.r, btn, hi)
        letra(c, b.id, b.t, b.x, b.y, b.r * 0.85f, letter)
    }
}

private fun drawSnes(c: Ctx) {
    val dl = c.dl
    val shell = col(200, 199, 208)
    val shellDark = col(176, 175, 186)
    hombro(c, Rp.L, 56f, 236f, -8f, 60f, col(150, 149, 160), ESQ_ARRIBA_IZQ, 24f, "L", 10f, col(90, 89, 100), 16f)
    hombro(c, Rp.R, 404f, 584f, -8f, 60f, col(150, 149, 160), ESQ_ARRIBA_DER, 24f, "R", 10f, col(90, 89, 100), 16f)
    fun body(dx: Float, dy: Float, color: Int) {
        dl.circleFilled(c.P(160 + dx, 168 + dy), c.L(140), color)
        dl.circleFilled(c.P(480 + dx, 168 + dy), c.L(140), color)
        dl.rectFilled(c.P(160 + dx, 50 + dy), c.P(480 + dx, 286 + dy), color)
    }
    body(5f, 10f, col(0, 0, 0, 80))
    body(0f, 0f, shell)
    dl.circleFilled(c.P(160, 168), c.L(76), shellDark)
    dpad(c, 160f, 168f, 120f, col(64, 64, 72), col(92, 92, 102))
    pill(c, Rp.SELECT, 278f, 192f, 42f, 14f, -28f, col(118, 117, 128))
    pill(c, Rp.START, 340f, 192f, 42f, 14f, -28f, col(118, 117, 128))
    c.text("SELECT", 274, 228, 11, col(110, 109, 122))
    c.text("START", 338, 228, 11, col(110, 109, 122))
    dl.circleFilled(c.P(482, 168), c.L(100), shellDark)
    data class B(val id: Int, val t: String, val x: Float, val y: Float, val c1: Int, val hi: Int)
    for (b in listOf(B(Rp.X, "X", 480f, 104f, col(70, 98, 196), col(110, 136, 226)), B(Rp.Y, "Y", 416f, 168f, col(48, 150, 92), col(90, 186, 128)),
        B(Rp.A, "A", 544f, 168f, col(204, 52, 60), col(232, 98, 104)), B(Rp.B, "B", 480f, 232f, col(222, 178, 40), col(244, 210, 96)))) {
        round(c, b.id, b.x, b.y, 28f, b.c1, b.hi)
        letra(c, b.id, b.t, b.x, b.y, 22f, col(255, 255, 255, 220))
    }
}

private fun figurasPs(c: Ctx, caras: List<Triple<Int, Float, Float>>, radio: Float, figura: Float, grosor: Float) {
    val dl = c.dl
    val btn = col(58, 58, 66)
    val hi = col(84, 84, 94)
    for ((id, x, y) in caras) {
        round(c, id, x, y, radio, btn, hi)
        val down = c.down(id)
        val t = c.L(grosor)
        val m = c.P(x, y)
        val r = c.L(figura)
        val oscuro = col(60, 40, 0)
        when (id) {
            Rp.X -> dl.tri(V(m.x, m.y - r), V(m.x - r * 1.05f, m.y + r * 0.75f), V(m.x + r * 1.05f, m.y + r * 0.75f), if (down) oscuro else col(64, 200, 150), t)
            Rp.A -> dl.circle(m, r, if (down) oscuro else col(240, 90, 96), t)
            Rp.B -> {
                val cc = if (down) oscuro else col(110, 150, 240)
                dl.line(V(m.x - r, m.y - r), V(m.x + r, m.y + r), cc, t)
                dl.line(V(m.x + r, m.y - r), V(m.x - r, m.y + r), cc, t)
            }
            else -> dl.rect(V(m.x - r * 0.85f, m.y - r * 0.85f), V(m.x + r * 0.85f, m.y + r * 0.85f), if (down) oscuro else col(236, 130, 200), 0f, t)
        }
    }
}

private fun stick(c: Ctx, id: Int, x: Float, y: Float, baseR: Float, knobR: Float, base: Int, knob: Int, sx: Float = 0f, sy: Float = 0f) {
    val dl = c.dl
    if (id >= 0) c.hit(id, x, y, baseR * 0.9f)
    dl.circleFilled(c.P(x, y), c.L(baseR), base)
    dl.circleFilled(c.P(x + sx + 2, y + sy + 4), c.L(knobR), col(0, 0, 0, 80))
    dl.circleFilled(c.P(x + sx, y + sy), c.L(knobR), if (id >= 0 && c.down(id)) c.oro() else knob)
    dl.circle(c.P(x + sx, y + sy), c.L(knobR * 0.75f), base, c.L(2))
    if (id >= 0) c.ring(id, c.P(x + sx, y + sy), c.L(knobR))
}

private fun drawPlayStation(c: Ctx) {
    val dl = c.dl
    val shell = col(190, 192, 201)
    val shellDark = col(160, 162, 173)
    val ink = col(110, 112, 124)
    hombro(c, Rp.L2, 100f, 230f, 0f, 52f, col(140, 142, 154), ESQ_ARRIBA, 12f, "L2", 13f, col(80, 82, 94), 13f)
    hombro(c, Rp.R2, 430f, 560f, 0f, 52f, col(140, 142, 154), ESQ_ARRIBA, 12f, "R2", 13f, col(80, 82, 94), 13f)
    hombro(c, Rp.L, 104f, 226f, 30f, 80f, col(165, 167, 178), ESQ_ARRIBA, 12f, "L1", 43f, col(80, 82, 94), 13f)
    hombro(c, Rp.R, 434f, 556f, 30f, 80f, col(165, 167, 178), ESQ_ARRIBA, 12f, "R1", 43f, col(80, 82, 94), 13f)
    fun body(dx: Float, dy: Float, color: Int) {
        dl.rectFilled(c.P(120 + dx, 62 + dy), c.P(540 + dx, 250 + dy), color, c.L(60))
        for (side in 0 until 2) {
            val x0 = if (side == 1) 512f else 148f
            val x1 = if (side == 1) 560f else 100f
            for (i in 0..10) {
                val t = i / 10f
                dl.circleFilled(c.P(x0 + (x1 - x0) * t + dx, 165 + 150 * t + dy), c.L(108 - 44 * t), color)
            }
        }
    }
    body(5f, 10f, col(0, 0, 0, 80))
    body(0f, 0f, shell)
    dl.circleFilled(c.P(150, 160), c.L(66), shellDark)
    dpad(c, 150f, 160f, 104f, col(205, 207, 215), col(150, 152, 163))
    fun small(id: Int, x: Float, t: String, tri: Boolean) {
        c.hit(id, x, 150, 24)
        val cc = if (c.down(id)) c.oro() else col(96, 98, 110)
        if (tri) dl.triFilled(c.P(x - 16, 141), c.P(x - 16, 159), c.P(x + 16, 150), cc)
        else dl.rectFilled(c.P(x - 16, 143), c.P(x + 16, 157), cc, c.L(3))
        c.marco(id, c.P(x - 24, 134), c.P(x + 24, 166), 8f)
        c.text(t, x, 176, 11, ink)
    }
    small(Rp.SELECT, 290f, "SELECT", false)
    small(Rp.START, 370f, "START", true)
    dl.circleFilled(c.P(510, 160), c.L(84), shellDark)
    figurasPs(c, listOf(Triple(Rp.X, 510f, 110f), Triple(Rp.Y, 460f, 160f), Triple(Rp.A, 560f, 160f), Triple(Rp.B, 510f, 210f)), 24f, 10f, 2.5f)
    stick(c, Rp.L3, 240f, 250f, 44f, 31f, col(70, 72, 82), col(44, 45, 52))
    stick(c, Rp.R3, 420f, 250f, 44f, 31f, col(70, 72, 82), col(44, 45, 52))
}

private fun drawN64(c: Ctx) {
    val dl = c.dl
    val shell = col(150, 152, 162)
    val shellDark = col(120, 122, 134)
    hombro(c, Rp.SELECT, 70f, 200f, 6f, 52f, col(118, 120, 132), ESQ_ARRIBA, 16f, "L", 22f, col(60, 62, 74), 14f)
    hombro(c, Rp.R2, 440f, 570f, 6f, 52f, col(118, 120, 132), ESQ_ARRIBA, 16f, "R", 22f, col(60, 62, 74), 14f)
    fun capsule(x0: Float, y0: Float, x1: Float, y1: Float, r0: Float, r1: Float, dx: Float, dy: Float, color: Int) {
        for (i in 0..12) {
            val t = i / 12f
            dl.circleFilled(c.P(x0 + (x1 - x0) * t + dx, y0 + (y1 - y0) * t + dy), c.L(r0 + (r1 - r0) * t), color)
        }
    }
    fun body(dx: Float, dy: Float, color: Int) {
        dl.rectFilled(c.P(40 + dx, 40 + dy), c.P(600 + dx, 190 + dy), color, c.L(70))
        capsule(130f, 150f, 116f, 340f, 88f, 44f, dx, dy, color)
        capsule(320f, 170f, 320f, 380f, 62f, 40f, dx, dy, color)
        capsule(510f, 150f, 524f, 340f, 88f, 44f, dx, dy, color)
    }
    body(5f, 10f, col(0, 0, 0, 80))
    body(0f, 0f, shell)
    dl.circleFilled(c.P(130, 120), c.L(60), shellDark)
    dpad(c, 130f, 120f, 96f, col(58, 58, 66), col(88, 88, 98))
    round(c, Rp.START, 320f, 100f, 17f, col(206, 48, 54), col(236, 100, 104))
    c.text("START", 320, 136, 11, col(70, 72, 84))
    dl.circleFilled(c.P(320, 214), c.L(42), shellDark)
    dl.circleFilled(c.P(320, 214), c.L(26), col(80, 82, 94))
    dl.circleFilled(c.P(320, 210), c.L(20), col(104, 106, 118))
    run {
        val id = Rp.L2
        c.hit(id, 320, 330, 34)
        val a = c.P(296, 304)
        val b = c.P(344, 356)
        dl.rectFilled(a, b, if (c.down(id)) c.oro() else col(74, 76, 88), c.L(12))
        c.marco(id, a, b, 12f)
        c.text("Z", 320, 330, 16, if (c.down(id)) col(60, 40, 0) else col(190, 192, 202))
    }
    round(c, Rp.B, 470f, 178f, 24f, col(58, 98, 210), col(104, 140, 236))
    letra(c, Rp.B, "A", 470f, 178f, 18f, col(255, 255, 255, 220))
    round(c, Rp.Y, 424f, 140f, 22f, col(52, 160, 90), col(96, 196, 130))
    letra(c, Rp.Y, "B", 424f, 140f, 17f, col(255, 255, 255, 220))
    data class Cb(val id: Int, val x: Float, val y: Float, val ax: Float, val ay: Float)
    for (cb in listOf(Cb(Rp.X, 540f, 74f, 0f, -1f), Cb(Rp.A, 540f, 140f, 0f, 1f), Cb(Rp.L, 507f, 107f, -1f, 0f), Cb(Rp.R, 573f, 107f, 1f, 0f))) {
        round(c, cb.id, cb.x, cb.y, 15f, col(236, 190, 40), col(250, 220, 110))
        val m = c.P(cb.x, cb.y)
        val r = c.L(6)
        val cc = if (c.down(cb.id)) col(60, 40, 0) else col(120, 84, 0)
        dl.triFilled(V(m.x + cb.ax * r, m.y + cb.ay * r), V(m.x - cb.ax * r * 0.4f + cb.ay * r * 0.8f, m.y - cb.ay * r * 0.4f + cb.ax * r * 0.8f),
            V(m.x - cb.ax * r * 0.4f - cb.ay * r * 0.8f, m.y - cb.ay * r * 0.4f - cb.ax * r * 0.8f), cc)
    }
}

private fun drawDs(c: Ctx) {
    val dl = c.dl
    val shell = col(64, 70, 86)
    val shellHi = col(82, 90, 108)
    dl.rectFilled(c.P(36, 16), c.P(430, 266), col(0, 0, 0, 80), c.L(22))
    dl.rectFilled(c.P(30, 6), c.P(424, 256), shellHi, c.L(22))
    dl.rectFilled(c.P(110, 40), c.P(344, 222), col(30, 32, 42), c.L(8))
    dl.rectMulti(c.P(124, 52), c.P(330, 208), col(60, 78, 100), col(50, 66, 88), col(34, 44, 60), col(42, 54, 72))
    hombro(c, Rp.L, 20f, 120f, 262f, 292f, col(110, 118, 138), ESQ_TODAS, 10f, "L", 277f, col(220, 224, 236), 14f)
    hombro(c, Rp.R, 340f, 440f, 262f, 292f, col(110, 118, 138), ESQ_TODAS, 10f, "R", 277f, col(220, 224, 236), 14f)
    dl.rectFilled(c.P(26, 300), c.P(436, 556), col(0, 0, 0, 80), c.L(22))
    dl.rectFilled(c.P(20, 290), c.P(430, 546), shell, c.L(22))
    dl.rectFilled(c.P(118, 312), c.P(336, 492), col(26, 28, 36), c.L(6))
    dl.rectMulti(c.P(128, 322), c.P(326, 482), col(70, 92, 116), col(58, 78, 102), col(40, 52, 70), col(48, 62, 82))
    c.text(t("TÁCTIL"), 227, 402, 12, col(150, 170, 196, 160))
    dpad(c, 68f, 390f, 78f, col(32, 34, 42), col(62, 66, 78))
    for ((id, tt, xy) in listOf(Triple(Rp.X, "X", 388f to 354f), Triple(Rp.Y, "Y", 358f to 388f), Triple(Rp.A, "A", 418f to 388f), Triple(Rp.B, "B", 388f to 422f))) {
        round(c, id, xy.first, xy.second, 15f, col(34, 36, 44), col(70, 74, 88))
        letra(c, id, tt, xy.first, xy.second, 12f, col(200, 206, 222))
    }
    round(c, Rp.START, 372f, 478f, 9f, col(34, 36, 44), col(70, 74, 88))
    round(c, Rp.SELECT, 372f, 512f, 9f, col(34, 36, 44), col(70, 74, 88))
    c.text("START", 388, 478, 10, col(190, 196, 212), false)
    c.text("SELECT", 388, 512, 10, col(190, 196, 212), false)
}

private fun drawSaturn(c: Ctx) {
    val dl = c.dl
    val shell = col(52, 52, 60)
    val rim = col(72, 72, 82)
    hombro(c, Rp.L2, 70f, 210f, 8f, 70f, col(96, 96, 108), ESQ_ARRIBA_IZQ, 30f, "L", 28f, col(200, 200, 212), 15f)
    hombro(c, Rp.R2, 430f, 570f, 8f, 70f, col(96, 96, 108), ESQ_ARRIBA_DER, 30f, "R", 28f, col(200, 200, 212), 15f)
    dl.rectFilled(c.P(26, 56), c.P(626, 334), col(0, 0, 0, 90), c.L(135))
    dl.rectFilled(c.P(20, 46), c.P(620, 324), rim, c.L(135))
    dl.rectFilled(c.P(28, 54), c.P(612, 316), shell, c.L(128))
    dl.circleFilled(c.P(160, 186), c.L(76), col(36, 36, 42))
    dpad(c, 160f, 186f, 120f, col(84, 84, 96), col(110, 110, 124))
    round(c, Rp.START, 320f, 214f, 15f, col(90, 90, 102), col(120, 120, 134))
    c.text("START", 320, 248, 11, col(150, 150, 164))
    val small = col(110, 110, 124)
    val smallHi = col(140, 140, 156)
    data class B(val id: Int, val t: String, val x: Float, val y: Float, val r: Float, val c1: Int, val hi: Int)
    for (b in listOf(B(Rp.Y, "X", 418f, 140f, 18f, small, smallHi), B(Rp.X, "Y", 474f, 124f, 18f, small, smallHi),
        B(Rp.L, "Z", 530f, 108f, 18f, small, smallHi), B(Rp.B, "A", 414f, 214f, 27f, col(88, 104, 190), col(120, 136, 220)),
        B(Rp.A, "B", 478f, 196f, 27f, col(64, 150, 100), col(100, 186, 136)), B(Rp.R, "C", 542f, 178f, 27f, col(200, 160, 50), col(230, 196, 100)))) {
        round(c, b.id, b.x, b.y, b.r, b.c1, b.hi)
        letra(c, b.id, b.t, b.x, b.y, b.r * 0.8f, col(255, 255, 255, 220))
    }
}

private fun drawDreamcast(c: Ctx) {
    val dl = c.dl
    val shell = col(232, 232, 236)
    val shellDark = col(200, 200, 208)
    val ink = col(130, 130, 142)
    hombro(c, Rp.L2, 90f, 220f, 8f, 74f, col(176, 176, 186), ESQ_ARRIBA, 26f, "L", 30f, col(110, 110, 122), 15f)
    hombro(c, Rp.R2, 380f, 510f, 8f, 74f, col(176, 176, 186), ESQ_ARRIBA, 26f, "R", 30f, col(110, 110, 122), 15f)
    fun body(dx: Float, dy: Float, color: Int) {
        dl.rectFilled(c.P(40 + dx, 54 + dy), c.P(560 + dx, 260 + dy), color, c.L(90))
        for (side in 0 until 2) {
            val x0 = if (side == 1) 460f else 140f
            val x1 = if (side == 1) 500f else 100f
            for (i in 0..10) {
                val t = i / 10f
                dl.circleFilled(c.P(x0 + (x1 - x0) * t + dx, 190 + 170 * t + dy), c.L(100 - 40 * t), color)
            }
        }
    }
    body(5f, 10f, col(0, 0, 0, 70))
    body(0f, 0f, shell)
    dl.rectFilled(c.P(232, 64), c.P(368, 172), shellDark, c.L(14))
    dl.rectFilled(c.P(248, 78), c.P(352, 158), col(150, 150, 160), c.L(8))
    stick(c, -1, 140f, 128f, 50f, 32f, col(190, 190, 198), col(150, 150, 160))
    dpad(c, 196f, 250f, 96f, col(90, 90, 100), col(130, 130, 142))
    round(c, Rp.START, 300f, 232f, 13f, col(120, 120, 132), col(160, 160, 172))
    c.text("START", 300, 262, 11, ink)
    data class B(val id: Int, val t: String, val x: Float, val y: Float, val c1: Int, val hi: Int)
    for (b in listOf(B(Rp.X, "Y", 456f, 112f, col(70, 170, 90), col(110, 200, 130)), B(Rp.Y, "X", 408f, 158f, col(230, 190, 60), col(245, 215, 110)),
        B(Rp.A, "B", 504f, 158f, col(70, 110, 210), col(110, 150, 235)), B(Rp.B, "A", 456f, 204f, col(220, 70, 70), col(240, 120, 120)))) {
        round(c, b.id, b.x, b.y, 24f, b.c1, b.hi)
        letra(c, b.id, b.t, b.x, b.y, 16f, col(255, 255, 255, 230))
    }
}

private fun drawPsp(c: Ctx) {
    val dl = c.dl
    val shell = col(150, 156, 172)
    val shellHi = col(186, 192, 206)
    hombro(c, Rp.L, 40f, 160f, 6f, 48f, col(84, 86, 98), ESQ_ARRIBA_IZQ, 20f, "L", 22f, col(200, 202, 214), 14f)
    hombro(c, Rp.R, 540f, 660f, 6f, 48f, col(84, 86, 98), ESQ_ARRIBA_DER, 20f, "R", 22f, col(200, 202, 214), 14f)
    dl.rectFilled(c.P(16, 34), c.P(694, 294), col(0, 0, 0, 90), c.L(120))
    dl.rectFilled(c.P(10, 24), c.P(688, 284), shellHi, c.L(120))
    dl.rectFilled(c.P(16, 30), c.P(682, 278), shell, c.L(114))
    dl.rectFilled(c.P(196, 58), c.P(504, 232), col(20, 21, 28), c.L(8))
    dl.rectMulti(c.P(206, 68), c.P(494, 222), col(60, 78, 104), col(48, 64, 90), col(30, 40, 58), col(38, 50, 70))
    dpad(c, 104f, 122f, 92f, col(30, 31, 38), col(70, 72, 84))
    stick(c, -1, 92f, 214f, 28f, 20f, col(30, 31, 38), col(120, 122, 134))
    figurasPs(c, listOf(Triple(Rp.X, 596f, 84f), Triple(Rp.Y, 552f, 126f), Triple(Rp.A, 640f, 126f), Triple(Rp.B, 596f, 168f)), 19f, 8f, 2.2f)
    pill(c, Rp.SELECT, 404f, 256f, 30f, 9f, 0f, col(90, 92, 104))
    pill(c, Rp.START, 456f, 256f, 30f, 9f, 0f, col(90, 92, 104))
    c.text("SELECT", 404, 274, 9, col(70, 72, 86))
    c.text("START", 456, 274, 9, col(70, 72, 86))
}

/** Tamaño de diseño de cada control (ancho, alto), como DesignOf en PC. */
fun disenoControl(forma: String): Pair<Float, Float> = when (forma) {
    "gameboy" -> 300f to 490f
    "gba" -> 572f to 336f
    "genesis" -> 650f to 350f
    "snes" -> 650f to 336f
    "playstation" -> 660f to 390f
    "n64" -> 640f to 420f
    "ds" -> 460f to 560f
    "saturn" -> 640f to 340f
    "dreamcast" -> 600f to 440f
    "psp" -> 700f to 300f
    else -> 692f to 318f
}

/**
 * Dibuja el control de una consola en el rectángulo (x, y, ancho, alto). Devuelve las zonas que se
 * pueden tocar: [botón, x, y, radio] en píxeles.
 */
fun dibujarControl(p: Pintor, forma: String, x: Float, y: Float, ancho: Float, alto: Float, apretados: Int, capturando: Int, pulso: Float, acento: Int): List<FloatArray> {
    val (w, h) = disenoControl(forma)
    val k = min(ancho / w, alto / h)
    var oy = y + (alto - h * k) * 0.5f
    if (forma == "gba") oy += 10 * k
    if (forma == "snes") oy += 12 * k
    val c = Ctx(p, x + (ancho - w * k) * 0.5f, oy, k, apretados, capturando, pulso, acento)
    when (forma) {
        "gameboy" -> drawGameBoy(c)
        "gba" -> drawGba(c)
        "genesis" -> drawGenesis(c)
        "snes" -> drawSnes(c)
        "playstation" -> drawPlayStation(c)
        "n64" -> drawN64(c)
        "ds" -> drawDs(c)
        "saturn" -> drawSaturn(c)
        "dreamcast" -> drawDreamcast(c)
        "psp" -> drawPsp(c)
        else -> drawNes(c)
    }
    return c.zonas
}

// ================================================================== control en pantalla (juego)

/** Color y figura de cada botón de acción según la consola (como LookFor en PC). */
class Cara(var base: Int = 0x2A2B35, var glifo: Int = 0xF4F2FA, var tipo: Int = LETRA, var dx: Float = 0f, var dy: Float = 0f) {
    companion object {
        const val LETRA = 0; const val TRIANGULO = 1; const val CIRCULO = 2; const val CRUZ = 3; const val CUADRADO = 4; const val FLECHA = 5
    }
}

fun caraDe(sistema: String, nombre: String): Cara {
    val l = Cara()
    if (sistema == "cd" || sistema == "portatil") {
        l.base = 0x2B2C36
        when (nombre) {
            "Triángulo" -> { l.tipo = Cara.TRIANGULO; l.glifo = 0x3FD9A2 }
            "Círculo" -> { l.tipo = Cara.CIRCULO; l.glifo = 0xFF5D70 }
            "Cruz" -> { l.tipo = Cara.CRUZ; l.glifo = 0x79A6FF }
            "Cuadrado" -> { l.tipo = Cara.CUADRADO; l.glifo = 0xF78AD2 }
        }
        return l
    }
    fun color4(a: Int, b: Int, x: Int, y: Int) {
        when (nombre) { "A" -> l.base = a; "B" -> l.base = b; "X" -> l.base = x; "Y" -> l.base = y }
    }
    when (sistema) {
        "hexa" -> color4(0xD3313B, 0xF0BE2C, 0x3B52C4, 0x2C9A50)
        "sueno" -> color4(0xDC3838, 0x3A6EDC, 0xF0BE2C, 0x33B055)
        "octa" -> l.base = 0xC8212B
        "bolsillo" -> l.base = 0x9A1E58
        "bolsillo_plus" -> l.base = 0x4C4F6E
        "dual" -> { l.base = 0xD8D9E0; l.glifo = 0x2B2B33 }
        "mega" -> { l.base = 0x232329; l.glifo = 0xD9D9E2 }
        "anillo" -> { l.base = 0x34343F; l.glifo = 0xE6E6EF }
        "n64" -> when {
            nombre == "A" -> l.base = 0x2D53D4
            nombre == "B" -> l.base = 0x2CA14F
            nombre.startsWith("C ") -> {
                l.base = 0xF0BE2C; l.glifo = 0x4A3500; l.tipo = Cara.FLECHA
                when (nombre) { "C arriba" -> l.dy = -1f; "C abajo" -> l.dy = 1f; "C izquierda" -> l.dx = -1f; else -> l.dx = 1f }
            }
        }
    }
    if (l.base == 0xF0BE2C && l.tipo == Cara.LETRA) l.glifo = 0x4A3500
    return l
}

/** Botón redondo con relieve: sombra, cuerpo, brillo y borde. Al apretarlo se hunde y brilla. */
fun disco(p: Pintor, c: V, r: Float, base: Int, a: Float, on: Boolean, brillo: Int) {
    val cuerpo = if (on) 1f else a
    if (!on) p.circleFilled(V(c.x, c.y + r * 0.10f), r * 1.02f, col(0, 0, 0, (90 * a).toInt()))
    p.circleFilled(c, r, sombra(base, if (on) 0.95f else 0.62f, 0.94f * cuerpo))
    p.circleFilled(V(c.x, c.y - r * 0.06f), r * 0.90f, sombra(base, if (on) 1.35f else 1f, 0.94f * cuerpo))
    p.ellipseFilled(V(c.x, c.y - r * 0.50f), r * 0.46f, r * 0.20f, col(255, 255, 255, (20 * cuerpo).toInt()))
    p.circle(c, r, col(255, 255, 255, (46 * cuerpo).toInt()), max(1f, r * 0.035f))
    if (on) p.circle(c, r + r * 0.12f, sombra(brillo, 1.2f, 0.75f), max(2f, r * 0.09f))
}

/** Píldora (gatillos, Select, Start) con el mismo relieve. */
fun pildora(p: Pintor, c: V, hw: Float, hh: Float, base: Int, a: Float, on: Boolean, brillo: Int) {
    val cuerpo = if (on) 1f else a
    val p0 = V(c.x - hw, c.y - hh)
    val p1 = V(c.x + hw, c.y + hh)
    if (!on) p.rectFilled(V(p0.x, p0.y + hh * 0.22f), V(p1.x, p1.y + hh * 0.22f), col(0, 0, 0, (90 * a).toInt()), hh)
    p.rectFilled(p0, p1, sombra(base, if (on) 0.95f else 0.62f, 0.94f * cuerpo), hh)
    p.rectFilled(V(p0.x + hh * 0.12f, p0.y), V(p1.x - hh * 0.12f, p1.y - hh * 0.22f), sombra(base, if (on) 1.35f else 1f, 0.94f * cuerpo), hh * 0.9f)
    p.rectFilled(V(p0.x + hh * 0.7f, p0.y + hh * 0.18f), V(p1.x - hh * 0.7f, p0.y + hh * 0.62f), col(255, 255, 255, (16 * cuerpo).toInt()), hh * 0.3f)
    p.rect(p0, p1, if (on) sombra(brillo, 1.2f, 0.85f) else col(255, 255, 255, (46 * cuerpo).toInt()), hh, max(1f, hh * (if (on) 0.12f else 0.06f)))
}

/** La figura o letra de un botón de acción. */
fun glifo(p: Pintor, c: V, r: Float, l: Cara, etiqueta: String, a: Float, on: Boolean) {
    val vis = if (on) 1f else max(a, 0.6f)
    val cc = sombra(l.glifo, if (on) 1.25f else 1f, vis)
    val sh = col(0, 0, 0, (70 * vis).toInt())
    val t = r * 0.15f
    val q = r * 0.42f
    when (l.tipo) {
        Cara.TRIANGULO -> {
            val p1 = V(c.x, c.y - q * 1.02f)
            val p2 = V(c.x + q, c.y + q * 0.72f)
            val p3 = V(c.x - q, c.y + q * 0.72f)
            p.tri(V(p1.x, p1.y + t * 0.4f), V(p2.x, p2.y + t * 0.4f), V(p3.x, p3.y + t * 0.4f), sh, t)
            p.tri(p1, p2, p3, cc, t)
        }
        Cara.CIRCULO -> {
            p.circle(V(c.x, c.y + t * 0.4f), q * 0.86f, sh, t)
            p.circle(c, q * 0.86f, cc, t)
        }
        Cara.CRUZ -> for (s in 0 until 2) {
            val k = if (s == 0) sh else cc
            val oy = if (s == 0) t * 0.4f else 0f
            p.line(V(c.x - q * 0.78f, c.y - q * 0.78f + oy), V(c.x + q * 0.78f, c.y + q * 0.78f + oy), k, t)
            p.line(V(c.x + q * 0.78f, c.y - q * 0.78f + oy), V(c.x - q * 0.78f, c.y + q * 0.78f + oy), k, t)
        }
        Cara.CUADRADO -> {
            p.rect(V(c.x - q * 0.74f, c.y - q * 0.74f + t * 0.4f), V(c.x + q * 0.74f, c.y + q * 0.74f + t * 0.4f), sh, 0f, t)
            p.rect(V(c.x - q * 0.74f, c.y - q * 0.74f), V(c.x + q * 0.74f, c.y + q * 0.74f), cc, 0f, t)
        }
        Cara.FLECHA -> {
            val k = q * 0.95f
            p.triFilled(V(c.x + l.dx * k, c.y + l.dy * k), V(c.x - l.dx * k * 0.55f - l.dy * k * 0.85f, c.y - l.dy * k * 0.55f + l.dx * k * 0.85f),
                V(c.x - l.dx * k * 0.55f + l.dy * k * 0.85f, c.y - l.dy * k * 0.55f - l.dx * k * 0.85f), cc)
        }
        else -> {
            p.text(etiqueta, V(c.x, c.y + r * 0.06f), r * 0.95f * (if (etiqueta.length > 2) 0.62f else 1f), sh)
            p.text(etiqueta, c, r * 0.95f * (if (etiqueta.length > 2) 0.62f else 1f), cc)
        }
    }
}

/**
 * Un botón del control en pantalla, con el estilo de la consola (como DrawTouchPad en PC).
 * id: botón del RetroPad, -1 menú, -2 cruceta, -3 palanca.
 */
fun botonTactil(p: Pintor, id: Int, etiqueta: String, grupo: String, c: V, r: Float, sistema: String, colorConsola: Int, apretados: Int,
                palancaX: Float, palancaY: Float, a: Float) {
    when {
        id == -1 -> {
            p.circleFilled(c, r, col(10, 8, 18, (150 * a).toInt()))
            p.circle(c, r, rgb(0xFFFFFF, 0.35f * a), 1.5f * (r / 20f).coerceAtLeast(1f))
            // Pausa: dos barras.
            val bw = r * 0.18f
            val bh = r * 0.55f
            p.rectFilled(V(c.x - bw * 2f, c.y - bh), V(c.x - bw * 0.6f, c.y + bh), rgb(0xFFFFFF, 0.85f * a), bw * 0.3f)
            p.rectFilled(V(c.x + bw * 0.6f, c.y - bh), V(c.x + bw * 2f, c.y + bh), rgb(0xFFFFFF, 0.85f * a), bw * 0.3f)
        }
        id == -2 -> {
            val arm = r * 0.34f
            val len = r * 0.96f
            val rad = arm * 0.32f
            val body = 0x272833
            p.circleFilled(c, r, col(10, 8, 18, (45 * a).toInt()))
            for (pass in 0 until 3) {
                val oy = if (pass == 0) r * 0.07f else if (pass == 2) -r * 0.03f else 0f
                val inn = if (pass == 2) arm * 0.16f else 0f
                val cc = when (pass) { 0 -> col(0, 0, 0, (95 * a).toInt()); 1 -> sombra(body, 0.75f, 0.92f * a); else -> sombra(body, 1.08f, 0.92f * a) }
                p.rectFilled(V(c.x - arm + inn, c.y - len + inn + oy), V(c.x + arm - inn, c.y + len - inn + oy), cc, rad)
                p.rectFilled(V(c.x - len + inn, c.y - arm + inn + oy), V(c.x + len - inn, c.y + arm - inn + oy), cc, rad)
            }
            val dirs = listOf(Triple(Rp.UP, 0f, -1f), Triple(Rp.DOWN, 0f, 1f), Triple(Rp.LEFT, -1f, 0f), Triple(Rp.RIGHT, 1f, 0f))
            for ((bit, dx, dy) in dirs) {
                val on = (apretados shr bit) and 1 == 1
                if (on) {
                    val q0 = V(c.x + (if (dx != 0f) (if (dx > 0) arm * 0.9f else -len) else -arm), c.y + (if (dy != 0f) (if (dy > 0) arm * 0.9f else -len) else -arm))
                    val q1 = V(c.x + (if (dx != 0f) (if (dx > 0) len else -arm * 0.9f) else arm), c.y + (if (dy != 0f) (if (dy > 0) len else -arm * 0.9f) else arm))
                    p.rectFilled(q0, q1, sombra(colorConsola, 1f, 0.85f), rad)
                }
                val tx = c.x + dx * len * 0.66f
                val ty = c.y + dy * len * 0.66f
                val k = arm * 0.42f
                p.triFilled(V(tx + dx * k, ty + dy * k), V(tx - dy * k - dx * k * 0.4f, ty + dx * k - dy * k * 0.4f),
                    V(tx + dy * k - dx * k * 0.4f, ty - dx * k - dy * k * 0.4f), col(255, 255, 255, ((if (on) 240 else 90) * (if (on) 1f else a)).toInt()))
            }
            p.circleFilled(c, arm * 0.62f, sombra(body, 0.55f, 0.9f * a))
            p.circle(c, arm * 0.62f, col(255, 255, 255, (28 * a).toInt()), max(1f, arm * 0.06f))
        }
        id == -3 -> {
            val on = palancaX != 0f || palancaY != 0f
            p.circleFilled(V(c.x, c.y + r * 0.06f), r * 1.02f, col(0, 0, 0, (70 * a).toInt()))
            p.circleFilled(c, r, sombra(0x1E1F28, 1f, 0.82f * a))
            p.circle(c, r, if (on) sombra(colorConsola, 1.1f, 0.7f) else col(255, 255, 255, (40 * a).toInt()), max(1.5f, r * 0.04f))
            p.circle(c, r * 0.72f, col(0, 0, 0, (70 * a).toInt()), max(1.5f, r * 0.05f))
            val h = V(c.x + palancaX * r * 0.55f, c.y + palancaY * r * 0.55f)
            disco(p, h, r * 0.5f, if (on) colorConsola else 0x3B3C4A, a, false, colorConsola)
            p.circle(V(h.x, h.y - r * 0.02f), r * 0.33f, col(0, 0, 0, (60 * (if (on) 1f else a)).toInt()), max(1f, r * 0.04f))
        }
        grupo.startsWith("gat") || grupo == "centro" -> {
            val on = id in 0..15 && (apretados shr id) and 1 == 1
            val hw = r * 1.45f
            val hh = r * 0.62f
            val cc = V(c.x, c.y + (if (on) hh * 0.08f else 0f))
            pildora(p, cc, hw, hh, 0x2C2D38, a, on, colorConsola)
            if (etiqueta.isNotEmpty()) {
                val tam = min(hh * 1.05f, hw * 1.6f / max(1, etiqueta.length) * 1.7f)
                p.text(etiqueta, V(cc.x, cc.y + tam * 0.06f), tam, col(0, 0, 0, (70 * (if (on) 1f else a)).toInt()))
                p.text(etiqueta, cc, tam, col(244, 242, 250, (255 * (if (on) 1f else max(a, 0.6f))).toInt()))
            }
        }
        else -> {
            val on = id in 0..15 && (apretados shr id) and 1 == 1
            val l = caraDe(sistema, etiqueta)
            val cc = V(c.x, c.y + (if (on) r * 0.06f else 0f))
            val lum = ((l.base shr 16) and 0xFF) + ((l.base shr 8) and 0xFF) + (l.base and 0xFF)
            val brillo = if (l.tipo != Cara.LETRA) l.glifo else if (lum < 200) colorConsola else l.base
            disco(p, cc, r, l.base, a, on, brillo)
            glifo(p, cc, r, l, etiqueta, a, on)
        }
    }
}

// ================================================================== portadas que faltan (cartucho o disco)

/** Color desde tono (0..360), saturación y brillo (0..1), como Hsv de PC. */
fun hsv(h: Float, s: Float, v: Float, a: Float = 1f): Int {
    val hh = ((h % 360f) + 360f) % 360f / 60f
    val i = hh.toInt()
    val f = hh - i
    val p = v * (1 - s)
    val q = v * (1 - s * f)
    val t = v * (1 - s * (1 - f))
    val (r, g, b) = when (i) { 0 -> Triple(v, t, p); 1 -> Triple(q, v, p); 2 -> Triple(p, v, t); 3 -> Triple(p, q, v); 4 -> Triple(t, p, v); else -> Triple(v, p, q) }
    return col((r * 255).toInt(), (g * 255).toInt(), (b * 255).toInt(), (a * 255).toInt())
}

fun iniciales(titulo: String): String {
    val out = StringBuilder()
    var inicio = true
    for (ch in titulo) {
        if (ch.isLetterOrDigit()) {
            if (inicio && out.length < 2) out.append(ch.uppercaseChar())
            inicio = false
        } else if (ch == ' ' || ch == '-' || ch == ':' || ch == '.') {
            inicio = true
        }
    }
    return if (out.isEmpty()) "?" else out.toString()
}

/**
 * La portada de un juego que todavía no tiene carátula: el cartucho o la caja de disco de su
 * consola, con una etiqueta del color del juego, sus iniciales y el nombre Satronho (como en PC).
 */
fun cartucho(p: Pintor, x: Float, y: Float, w: Float, h: Float, tono: Int, titulo: String, forma: String, etiqueta: String, radio: Float) {
    val hf = tono.toFloat()
    val p0 = V(x, y)
    val p1 = V(x + w, y + h)
    p.rectFilled(p0, p1, hsv(hf, 0.55f, 0.18f), radio)
    for (i in 0 until 6) {
        val k = 1f - i * 0.15f
        p.circleFilled(V(x + w * 0.38f, y + h * 0.36f), w * 0.40f * k, hsv(hf, 0.6f, 0.45f, 0.07f))
        p.circleFilled(V(x + w * 0.72f, y + h * 0.78f), w * 0.26f * k, hsv(hf + 40, 0.6f, 0.45f, 0.06f))
    }
    for (i in 0 until 9) {
        val sx = ((tono * 7.3f + i * 53.1f) % 100f) / 100f
        val sy = ((tono * 3.1f + i * 31.7f) % 100f) / 100f
        p.circleFilled(V(x + 4 + sx * (w - 8), y + 4 + sy * (h - 8)), 1f + (i % 3) * 0.6f, col(255, 255, 255, 50 + (i % 3) * 30))
    }
    val ini = iniciales(titulo)
    fun label(l0: V, l1: V, bandK: Float) {
        p.rectMulti(l0, l1, hsv(hf, 0.62f, 0.95f), hsv(hf + 25, 0.62f, 0.95f), hsv(hf + 25, 0.72f, 0.62f), hsv(hf, 0.72f, 0.62f))
        p.rect(l0, l1, col(0, 0, 0, 60), 0f, 1f)
        val lh = l1.y - l0.y
        val lw = l1.x - l0.x
        val fs = min(lh * 0.42f, lw * 0.36f)
        val c = V(l0.x + lw * 0.5f, l0.y + lh * 0.40f)
        p.text(ini, V(c.x + 2, c.y + 2), fs, col(0, 0, 0, 90))
        p.text(ini, c, fs, col(255, 255, 255, 245))
        val band = lh * bandK
        p.rectFilled(V(l0.x, l1.y - band), l1, col(20, 16, 31, 200))
        val fs2 = min(band * 0.62f, lw * 0.9f / max(1, etiqueta.length) * 1.6f)
        p.text(etiqueta, V(l0.x + lw * 0.5f, l1.y - band * 0.5f), fs2, rgb(ORO), negrita = false)
    }
    fun sombraRect(c0: V, c1: V, r: Float, a: Int = 70) = p.rectFilled(V(c0.x + 3, c0.y + 6), V(c1.x + 3, c1.y + 6), col(0, 0, 0, a), r)
    when (forma) {
        "gameboy", "ds" -> {
            val cw = w * (if (forma == "ds") 0.5f else 0.56f)
            val ch = cw * (if (forma == "ds") 1.1f else 1.12f)
            val c0 = V(x + (w - cw) * 0.5f, y + (h - ch) * 0.55f)
            val c1 = V(c0.x + cw, c0.y + ch)
            val cut = cw * (if (forma == "ds") 0.12f else 0.14f)
            val forma5 = listOf(c0, V(c1.x - cut, c0.y), V(c1.x, c0.y + cut), c1, V(c0.x, c1.y))
            p.poly(forma5.map { V(it.x + 3, it.y + 6) }, col(0, 0, 0, 70))
            p.poly(forma5, if (forma == "ds") col(54, 56, 66) else col(168, 166, 172))
            if (forma == "gameboy") for (i in 0 until 4) {
                val yy = c0.y + ch * (0.05f + i * 0.03f)
                p.line(V(c0.x + cw * 0.08f, yy), V(c1.x - cut - cw * 0.02f, yy), col(130, 128, 136), max(1f, ch * 0.012f))
            }
            label(V(c0.x + cw * 0.1f, c0.y + ch * (if (forma == "ds") 0.12f else 0.2f)), V(c1.x - cw * 0.1f, c0.y + ch * (if (forma == "ds") 0.84f else 0.86f)), 0.2f)
        }
        "gba" -> {
            val cw = w * 0.74f
            val ch = cw * 0.66f
            val c0 = V(x + (w - cw) * 0.5f, y + (h - ch) * 0.55f)
            val c1 = V(c0.x + cw, c0.y + ch)
            sombraRect(c0, c1, cw * 0.05f)
            p.rectFilled(V(c0.x + cw * 0.32f, c0.y - ch * 0.06f), V(c1.x - cw * 0.32f, c0.y + ch * 0.05f), col(70, 68, 78), cw * 0.03f)
            p.rectFilled(c0, c1, col(86, 84, 96), cw * 0.05f)
            label(V(c0.x + cw * 0.08f, c0.y + ch * 0.12f), V(c1.x - cw * 0.08f, c0.y + ch * 0.80f), 0.22f)
        }
        "genesis" -> {
            val cw = w * 0.64f
            val ch = min(h * 0.78f, cw * 1.08f)
            val c0 = V(x + (w - cw) * 0.5f, y + (h - ch) * 0.55f)
            val c1 = V(c0.x + cw, c0.y + ch)
            sombraRect(c0, c1, cw * 0.04f, 80)
            p.rectFilled(c0, c1, col(34, 33, 38), cw * 0.04f)
            p.rectFilled(c0, V(c1.x, c0.y + ch * 0.07f), col(48, 47, 54), cw * 0.04f, ESQ_ARRIBA)
            for (i in 0 until 6) {
                val yy = c0.y + ch * (0.18f + i * 0.1f)
                val t = max(1f, ch * 0.014f)
                p.line(V(c0.x + cw * 0.025f, yy), V(c0.x + cw * 0.07f, yy), col(70, 68, 78), t)
                p.line(V(c1.x - cw * 0.07f, yy), V(c1.x - cw * 0.025f, yy), col(70, 68, 78), t)
            }
            label(V(c0.x + cw * 0.11f, c0.y + ch * 0.12f), V(c1.x - cw * 0.11f, c0.y + ch * 0.86f), 0.18f)
        }
        "playstation", "saturn", "dreamcast", "psp" -> {
            val cw = w * 0.62f
            val ch = min(h * 0.74f, cw * 0.95f)
            val c0 = V(x + (w - cw) * 0.5f - w * 0.04f, y + (h - ch) * 0.6f)
            val c1 = V(c0.x + cw, c0.y + ch)
            val dc = V(c1.x - cw * 0.08f, c0.y + ch * 0.12f)
            val dr = cw * 0.42f
            p.circleFilled(V(dc.x + 3, dc.y + 5), dr, col(0, 0, 0, 60))
            p.circleFilled(dc, dr, when (forma) { "saturn" -> col(214, 206, 230); "dreamcast" -> col(236, 220, 200); else -> col(196, 200, 214) })
            p.circle(dc, dr * 0.72f, col(170, 176, 196), max(1f, dr * 0.04f))
            p.circleFilled(dc, dr * 0.2f, col(150, 156, 176))
            p.circleFilled(dc, dr * 0.08f, col(30, 28, 40))
            sombraRect(c0, c1, cw * 0.03f, 80)
            p.rectFilled(c0, c1, col(40, 40, 50), cw * 0.03f)
            p.rectFilled(c0, V(c0.x + cw * 0.07f, c1.y), col(24, 24, 32), cw * 0.03f, ESQ_ARRIBA_IZQ or ESQ_ABAJO_IZQ)
            label(V(c0.x + cw * 0.1f, c0.y + ch * 0.05f), V(c1.x - cw * 0.04f, c1.y - ch * 0.05f), 0.16f)
            p.rect(c0, c1, col(255, 255, 255, 40), cw * 0.03f, 1f)
        }
        "n64" -> {
            val cw = w * 0.70f
            val ch = cw * 0.80f
            val c0 = V(x + (w - cw) * 0.5f, y + (h - ch) * 0.55f)
            val c1 = V(c0.x + cw, c0.y + ch)
            sombraRect(c0, c1, cw * 0.1f)
            p.rectFilled(c0, c1, col(120, 122, 132), cw * 0.12f, ESQ_ARRIBA)
            for (i in 0 until 5) {
                val yy = c0.y + ch * (0.2f + i * 0.12f)
                p.line(V(c0.x + cw * 0.03f, yy), V(c0.x + cw * 0.08f, yy), col(96, 98, 108), max(1f, ch * 0.016f))
                p.line(V(c1.x - cw * 0.08f, yy), V(c1.x - cw * 0.03f, yy), col(96, 98, 108), max(1f, ch * 0.016f))
            }
            label(V(c0.x + cw * 0.12f, c0.y + ch * 0.1f), V(c1.x - cw * 0.12f, c0.y + ch * 0.82f), 0.2f)
        }
        "snes" -> {
            val cw = w * 0.74f
            val ch = cw * 0.72f
            val c0 = V(x + (w - cw) * 0.5f, y + (h - ch) * 0.55f)
            val c1 = V(c0.x + cw, c0.y + ch)
            sombraRect(c0, c1, cw * 0.08f)
            p.rectFilled(c0, c1, col(176, 175, 184), cw * 0.08f, ESQ_ARRIBA)
            p.rectFilled(V(c0.x + cw * 0.12f, c0.y), V(c0.x + cw * 0.2f, c0.y + ch * 0.07f), col(150, 149, 158), 2f)
            p.rectFilled(V(c1.x - cw * 0.2f, c0.y), V(c1.x - cw * 0.12f, c0.y + ch * 0.07f), col(150, 149, 158), 2f)
            for (i in 0 until 3) {
                val yy = c1.y - ch * (0.06f + i * 0.045f)
                p.line(V(c0.x + cw * 0.06f, yy), V(c1.x - cw * 0.06f, yy), col(140, 139, 150), max(1f, ch * 0.014f))
            }
            label(V(c0.x + cw * 0.14f, c0.y + ch * 0.12f), V(c1.x - cw * 0.14f, c0.y + ch * 0.74f), 0.22f)
        }
        else -> {
            val cw = w * 0.60f
            val ch = h * 0.80f
            val c0 = V(x + (w - cw) * 0.5f, y + (h - ch) * 0.58f)
            val c1 = V(c0.x + cw, c0.y + ch)
            sombraRect(c0, c1, cw * 0.05f)
            p.rectFilled(c0, c1, col(150, 147, 158), cw * 0.05f)
            p.rectFilled(c0, V(c1.x, c0.y + ch * 0.06f), col(172, 169, 180), cw * 0.05f, ESQ_ARRIBA)
            for (i in 0 until 5) {
                val yy = c0.y + ch * (0.80f + i * 0.035f)
                p.line(V(c0.x + cw * 0.12f, yy), V(c1.x - cw * 0.12f, yy), col(110, 107, 118), max(1f, ch * 0.012f))
            }
            label(V(c0.x + cw * 0.09f, c0.y + ch * 0.11f), V(c1.x - cw * 0.09f, c0.y + ch * 0.72f), 0.2f)
        }
    }
}
