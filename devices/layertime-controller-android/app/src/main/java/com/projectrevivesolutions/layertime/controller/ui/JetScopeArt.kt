package com.projectrevivesolutions.layertime.controller.ui

import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RadialGradient
import android.graphics.RectF
import android.graphics.Shader
import android.graphics.Typeface
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asImageBitmap
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.sin
import kotlin.random.Random

/**
 * The hunt screen's radar (Michael, 2026-11-01): a scope somebody yanked out of an old fighter
 * jet and bolted onto the salvaged ship console. Mocked up and approved first (Claude outputs:
 * hunt_screen_mockup.png).
 *
 * Everything here is static, so it is drawn once per screen size off the main thread:
 *  - [ScopeImages.decor]: the instrument's cast shadow and side wall (it overlaps the console
 *    slightly and stands proud of it, lit from the upper left like the main face), the worn
 *    gunmetal plate with chipped paint, quarter-turn fasteners, stencils, dead knobs, cut wires,
 *    the bearing ring, the phosphor tube's base glow, the straps riveted onto the console frame,
 *    and the junk: duct tape with marker on it, baling wire, a zip tie, a bottle cap, a C-clamp,
 *    a hose clamp, and a blob of JB Weld.
 *  - [ScopeImages.glass]: the tube's glare and scratches, drawn over the live radar.
 *
 * The live radar (rings, sweep, step trail) is drawn by HuntScreen inside [ScopeGeom.rIn].
 * Marker writing is hand-printed block capitals with crooked letters, so it needs no
 * handwriting font on the device.
 */
internal data class ScopeGeom(
    val w: Int, val h: Int, val dp: Float,
    /** The instrument plate: left, top, and side, px. */
    val px: Float, val py: Float, val s: Float,
) {
    /** One unit of the 316-unit mockup plate. */
    val u: Float get() = s / 316f
    val cx: Float get() = px + s / 2f
    val cy: Float get() = py + s / 2f - 6f * u
    /** Outer edge of the bearing ring, and the phosphor tube's radius. */
    val rOut: Float get() = 124f * u
    val rIn: Float get() = 104f * u
}

internal class ScopeImages(val decor: ImageBitmap, val glass: ImageBitmap)

internal object JetScopeArt {
    private const val STENCIL = 0xD9EBE6D2.toInt()

    fun render(g: ScopeGeom, font: Typeface?): ScopeImages =
        ScopeImages(renderDecor(g, font).asImageBitmap(), renderGlass(g).asImageBitmap())

    private fun rr(l: Float, t: Float, r: Float, b: Float) = RectF(l, t, r, b)

    private fun renderDecor(g: ScopeGeom, font: Typeface?): Bitmap {
        val bm = Bitmap.createBitmap(g.w, g.h, Bitmap.Config.ARGB_8888)
        val c = Canvas(bm)
        val u = g.u; val px = g.px; val py = g.py; val s = g.s
        val rnd = Random(7)
        val p = Paint(Paint.ANTI_ALIAS_FLAG)
        val corner = 18f * u

        // cast shadow on the hull and console: the light is upper left, as on the main face
        p.color = android.graphics.Color.BLACK
        p.setShadowLayer(22f * u, 12f * u, 14f * u, 0xBF000000.toInt())
        c.drawRoundRect(rr(px, py, px + s, py + s), corner, corner, p)
        p.clearShadowLayer()
        // side wall: the instrument's thickness, seen along the lower right
        for (d in 9 downTo 1) {
            p.color = android.graphics.Color.rgb(18 + d * 2, 20 + d * 2, 17 + d * 2)
            c.drawRoundRect(rr(px + d * u, py + d * u, px + s + d * u, py + s + d * u), corner, corner, p)
        }
        // the plate: worn gunmetal
        p.shader = LinearGradient(px, py, px + s, py + s, intArrayOf(0xFF4B5048.toInt(), 0xFF33372F.toInt(), 0xFF22251F.toInt()),
            floatArrayOf(0f, 0.5f, 1f), Shader.TileMode.CLAMP)
        c.drawRoundRect(rr(px, py, px + s, py + s), corner, corner, p)
        p.shader = null
        // bevel: lit outer lip, dark inner groove
        p.style = Paint.Style.STROKE
        p.strokeWidth = 3f * u; p.color = 0x38FFFFFF
        c.drawRoundRect(rr(px + 2 * u, py + 2 * u, px + s - 2 * u, py + s - 2 * u), 16f * u, 16f * u, p)
        p.strokeWidth = 2f * u; p.color = 0x99000000.toInt()
        c.drawRoundRect(rr(px + 6 * u, py + 6 * u, px + s - 6 * u, py + s - 6 * u), 14f * u, 14f * u, p)
        p.style = Paint.Style.FILL
        // chipped paint along the edges
        repeat(70) {
            val t = rnd.nextFloat(); val side = rnd.nextInt(4)
            val x: Float; val y: Float
            when (side) {
                0 -> { x = px + 10 * u + t * (s - 20 * u); y = py + 3 * u + rnd.nextFloat() * 6 * u }
                1 -> { x = px + 10 * u + t * (s - 20 * u); y = py + s - 9 * u + rnd.nextFloat() * 6 * u }
                2 -> { x = px + 3 * u + rnd.nextFloat() * 6 * u; y = py + 10 * u + t * (s - 20 * u) }
                else -> { x = px + s - 9 * u + rnd.nextFloat() * 6 * u; y = py + 10 * u + t * (s - 20 * u) }
            }
            p.color = android.graphics.Color.argb((64 + rnd.nextFloat() * 90).toInt(), 190, 195, 190)
            c.drawRect(x, y, x + (1 + rnd.nextFloat() * 4) * u, y + (1 + rnd.nextFloat() * 2) * u, p)
        }
        // scratches across the plate
        p.style = Paint.Style.STROKE; p.strokeWidth = 1f * u; p.color = 0x2EC8CDC8
        repeat(14) {
            val x = px + 20 * u + rnd.nextFloat() * (s - 40 * u); val y = py + 20 * u + rnd.nextFloat() * (s - 40 * u)
            c.drawLine(x, y, x + (rnd.nextFloat() - 0.5f) * 40 * u, y + (rnd.nextFloat() - 0.5f) * 12 * u, p)
        }
        p.style = Paint.Style.FILL
        // quarter-turn fasteners in the corners
        for ((fx, fy) in listOf(px + 20 * u to py + 20 * u, px + s - 20 * u to py + 20 * u,
                px + 20 * u to py + s - 20 * u, px + s - 20 * u to py + s - 20 * u)) {
            p.shader = RadialGradient(fx - 2 * u, fy - 2 * u, 8 * u, 0xFFD8DCD8.toInt(), 0xFF5A5E5A.toInt(), Shader.TileMode.CLAMP)
            c.drawCircle(fx, fy, 8 * u, p); p.shader = null
            val a = rnd.nextFloat() * 3f
            p.style = Paint.Style.STROKE; p.strokeWidth = 2.5f * u; p.color = 0xFF111111.toInt()
            c.drawLine(fx - 6 * u * cos(a), fy - 6 * u * sin(a), fx + 6 * u * cos(a), fy + 6 * u * sin(a), p)
            p.style = Paint.Style.FILL
        }
        // stencils
        val tp = Paint(Paint.ANTI_ALIAS_FLAG).apply { typeface = font; color = STENCIL; textAlign = Paint.Align.CENTER; isFakeBoldText = font == null }
        tp.textSize = 12f * u; c.drawText("RDR SCOPE", px + s / 2f, py + 22f * u, tp)
        tp.textSize = 11f * u
        c.drawText("BRT", px + 62f * u, py + s - 46f * u, tp)
        c.drawText("GAIN", px + s - 62f * u, py + s - 46f * u, tp)
        // knobs that don't do anything
        for (kx in listOf(px + 62f * u, px + s - 62f * u)) {
            val ky = py + s - 26f * u
            p.color = 0xFF151515.toInt(); c.drawCircle(kx, ky, 13f * u, p)
            p.style = Paint.Style.STROKE; p.strokeWidth = 1f * u; p.color = 0xFF444444.toInt()
            var a = 0.0
            while (a < 2 * PI) {
                c.drawLine(kx + 11 * u * cos(a).toFloat(), ky + 11 * u * sin(a).toFloat(), kx + 13 * u * cos(a).toFloat(), ky + 13 * u * sin(a).toFloat(), p)
                a += 0.35
            }
            p.strokeWidth = 2f * u; p.color = 0xFFDDDDDD.toInt(); c.drawLine(kx, ky, kx + 7 * u, ky - 8 * u, p)
            p.style = Paint.Style.FILL
        }
        // the wires it was cut from, still hanging out the top
        val wires = listOf(0xFFBB3333.toInt() to 0.62f, 0xFFCCCC33.toInt() to 0.66f, 0xFF333333.toInt() to 0.70f)
        wires.forEachIndexed { i, (col, f) ->
            val x = px + s * f
            p.style = Paint.Style.STROKE; p.strokeWidth = 4f * u; p.color = col
            c.drawPath(Path().apply { moveTo(x, py + 4 * u); quadTo(x + 6 * u, py - 10 * u, x + (10 + i * 3) * u, py + (-18 + i * 2) * u) }, p)
            p.style = Paint.Style.FILL; p.color = 0xFFC87533.toInt()
            c.drawRect(x + (8 + i * 3) * u, py + (-21 + i * 2) * u, x + (12 + i * 3) * u, py + (-17 + i * 2) * u, p)
        }
        // bearing ring around the tube
        val cx = g.cx; val cy = g.cy; val rOut = g.rOut; val rIn = g.rIn
        p.color = 0xFF0C0C0C.toInt(); c.drawCircle(cx, cy, rOut, p)
        p.style = Paint.Style.STROKE; p.strokeWidth = 2f * u; p.color = 0x2EFFFFFF
        c.drawArc(rr(cx - rOut + u, cy - rOut + u, cx + rOut - u, cy + rOut - u), 180f, 108f, false, p)
        p.color = 0xFFD8D4C4.toInt()
        for (d in 0 until 360 step 10) {
            val a = Math.toRadians((d - 90).toDouble()); val long = d % 30 == 0
            p.strokeWidth = (if (long) 2f else 1f) * u
            val r0 = rIn + 3 * u; val r1 = r0 + (if (long) 10f else 5f) * u
            c.drawLine(cx + r0 * cos(a).toFloat(), cy + r0 * sin(a).toFloat(), cx + r1 * cos(a).toFloat(), cy + r1 * sin(a).toFloat(), p)
        }
        p.style = Paint.Style.FILL
        tp.color = 0xFFD8D4C4.toInt(); tp.textSize = 10f * u
        c.drawText("0", cx, cy - rOut + 8 * u, tp)
        c.drawText("18", cx, cy + rOut - 2 * u, tp)
        c.drawText("9", cx + rOut - 6 * u, cy + 4 * u, tp)
        c.drawText("27", cx - rOut + 6 * u, cy + 4 * u, tp)
        // the phosphor tube's base glow, with scanlines
        p.shader = RadialGradient(cx, cy, rIn, 0xFF0B3A16.toInt(), 0xFF020C05.toInt(), Shader.TileMode.CLAMP)
        c.drawCircle(cx, cy, rIn, p); p.shader = null
        c.save()
        c.clipPath(Path().apply { addCircle(cx, cy, rIn, Path.Direction.CW) })
        p.color = 0x2E000000
        var sy = cy - rIn
        while (sy < cy + rIn) { c.drawRect(cx - rIn, sy, cx + rIn, sy + u, p); sy += 3 * u }
        c.restore()

        // straps riveted from the plate onto the console frame (they stop on the frame band)
        for (sy2 in listOf(py + 60 * u, py + s - 74 * u)) strap(c, px + s - 30 * u, sy2, 34 * u, 14 * u, u)

        // ---- the junk ----
        tape(c, px + 18 * u, py - 58 * u, 190 * u, 26 * u, -0.06f, "RADAR. WORKS GOOD.", 17 * u, font)
        // baling wire twisted from the plate to a hull bolt
        p.style = Paint.Style.STROKE; p.strokeWidth = 1.6f * u; p.color = 0xFF8A8F86.toInt()
        c.drawPath(Path().apply {
            moveTo(px + s - 30 * u, py + 8 * u)
            for (i in 0 until 14) lineTo(px + s - 26 * u + i * 3 * u, py - 6 * u - i * 3 * u + (if (i % 2 == 1) 3 else -3) * u)
        }, p)
        p.style = Paint.Style.FILL; p.color = 0xFF6B6E66.toInt(); c.drawCircle(px + s + 16 * u, py - 48 * u, 6 * u, p)
        p.color = 0xFF333333.toInt(); c.drawRect(px + s + 13 * u, py - 49 * u, px + s + 19 * u, py - 47 * u, p)
        // zip tie around the cut wires
        c.save(); c.translate(px + s * 0.665f, py - 6 * u); c.rotate(20f)
        p.color = 0xFFE8E8E0.toInt(); c.drawRect(-10 * u, -2 * u, 12 * u, 2 * u, p); c.drawRect(10 * u, -2 * u, 36 * u, u, p)
        c.restore()
        // a bottle cap, because of course
        val bx = px + 232 * u; val by = py - 44 * u
        p.shader = RadialGradient(bx - 2 * u, by - 2 * u, 9 * u, 0xFFCC3333.toInt(), 0xFF661111.toInt(), Shader.TileMode.CLAMP)
        c.drawPath(Path().apply {
            for (i in 0..21) {
                val a = i / 21.0 * 2 * PI; val r = (if (i % 2 == 1) 9f else 7.5f) * u
                val x = bx + r * cos(a).toFloat(); val y = by + r * sin(a).toFloat()
                if (i == 0) moveTo(x, y) else lineTo(x, y)
            }
            close()
        }, p)
        p.shader = null
        // a C-clamp gripping the plate's bottom edge
        c.save(); c.translate(px + 90 * u, py + s - 4 * u)
        p.style = Paint.Style.STROKE; p.strokeWidth = 9 * u; p.strokeJoin = Paint.Join.MITER
        p.shader = LinearGradient(0f, 0f, 40 * u, 0f, 0xFF6B2A1E.toInt(), 0xFFA8432B.toInt(), Shader.TileMode.CLAMP)
        c.drawPath(Path().apply { moveTo(-6 * u, -14 * u); lineTo(-6 * u, 40 * u); lineTo(30 * u, 40 * u); lineTo(30 * u, 26 * u) }, p)
        p.shader = null; p.strokeWidth = 5 * u; p.color = 0xFF9DA1A5.toInt()
        c.drawLine(-6 * u, -20 * u, -6 * u, -6 * u, p)
        p.style = Paint.Style.FILL; p.color = 0xFFB8BCC0.toInt()
        c.drawRect(24 * u, 6 * u, 36 * u, 10 * u, p); c.drawRect(28 * u, 10 * u, 32 * u, 28 * u, p); c.drawRect(20 * u, 28 * u, 40 * u, 32 * u, p)
        c.restore()
        tape(c, px + 160 * u, py + s + 26 * u, 150 * u, 24 * u, 0.05f, "DONT KICK IT", 16 * u, font)
        // JB Weld blob on the lower strap
        p.color = 0xFF5C5C58.toInt()
        c.save(); c.rotate(17f, px + s + 8 * u, py + s - 60 * u)
        c.drawOval(rr(px + s - 7 * u, py + s - 69 * u, px + s + 23 * u, py + s - 51 * u), p)
        p.color = 0x1FFFFFFF; c.drawOval(rr(px + s - 2 * u, py + s - 66 * u, px + s + 10 * u, py + s - 60 * u), p)
        c.restore()
        // a hose clamp screwed to the hull, holding nothing
        p.style = Paint.Style.STROKE; p.strokeWidth = 3 * u; p.color = 0xFFC0C4C8.toInt()
        c.drawOval(rr(px - 2 * u, py + s + 36 * u, px + 30 * u, py + s + 56 * u), p)
        p.style = Paint.Style.FILL; p.color = 0xFFD0D4D8.toInt()
        c.drawRect(px + 26 * u, py + s + 40 * u, px + 36 * u, py + s + 48 * u, p)
        return bm
    }

    /** The tube's glare and a few scratches in the glass, over the live radar. */
    private fun renderGlass(g: ScopeGeom): Bitmap {
        val bm = Bitmap.createBitmap(g.w, g.h, Bitmap.Config.ARGB_8888)
        val c = Canvas(bm)
        val u = g.u; val cx = g.cx; val cy = g.cy; val rIn = g.rIn
        val p = Paint(Paint.ANTI_ALIAS_FLAG)
        c.save()
        c.clipPath(Path().apply { addCircle(cx, cy, rIn, Path.Direction.CW) })
        p.shader = LinearGradient(cx - rIn, cy - rIn, cx, cy, 0x29FFFFFF, 0x00FFFFFF, Shader.TileMode.CLAMP)
        c.save(); c.rotate(-34f, cx - 30 * u, cy - 40 * u)
        c.drawOval(rr(cx - 100 * u, cy - 80 * u, cx + 40 * u, cy), p)
        c.restore(); p.shader = null
        val rnd = Random(3)
        p.style = Paint.Style.STROKE; p.strokeWidth = u; p.color = 0x1FDCFFDC
        repeat(6) {
            val x = cx + (rnd.nextFloat() - 0.5f) * 150 * u; val y = cy + (rnd.nextFloat() - 0.5f) * 150 * u
            c.drawLine(x, y, x + (rnd.nextFloat() - 0.5f) * 50 * u, y + (rnd.nextFloat() - 0.5f) * 20 * u, p)
        }
        c.restore()
        return bm
    }

    private fun strap(c: Canvas, x: Float, y: Float, w: Float, h: Float, u: Float) {
        val p = Paint(Paint.ANTI_ALIAS_FLAG)
        p.color = 0x59000000; c.drawRect(x + 2 * u, y + h, x + w + 2 * u, y + h + 3 * u, p)
        p.shader = LinearGradient(x, y, x, y + h, 0xFFA3A8AD.toInt(), 0xFF5D6166.toInt(), Shader.TileMode.CLAMP)
        c.drawRect(x, y, x + w, y + h, p); p.shader = null
        for (rx in listOf(x + 7 * u, x + w - 7 * u)) {
            p.shader = RadialGradient(rx - u, y + h / 2f - u, 4 * u, 0xFFEEEEEE.toInt(), 0xFF666666.toInt(), Shader.TileMode.CLAMP)
            c.drawCircle(rx, y + h / 2f, 3.5f * u, p); p.shader = null
        }
    }

    /** A torn strip of duct tape with marker on it: crooked hand-printed capitals. */
    private fun tape(c: Canvas, x: Float, y: Float, w: Float, h: Float, ang: Float, text: String, size: Float, font: Typeface?) {
        val u = size / 17f
        c.save(); c.translate(x, y); c.rotate(Math.toDegrees(ang.toDouble()).toFloat())
        val outline = Path().apply {
            moveTo(0f, 0f)
            for (i in 0..6) lineTo(-3 * u + (i % 2) * 5 * u, i * h / 6f)
            lineTo(w, h)
            for (i in 6 downTo 0) lineTo(w + 3 * u - (i % 2) * 5 * u, i * h / 6f)
            close()
        }
        val p = Paint(Paint.ANTI_ALIAS_FLAG)
        p.color = 0xFF000000.toInt(); p.setShadowLayer(6 * u, 0f, 3 * u, 0x80000000.toInt())
        c.drawPath(outline, p); p.clearShadowLayer()
        p.shader = LinearGradient(0f, 0f, 0f, h, intArrayOf(0xFFC9CCCE.toInt(), 0xFF9FA3A6.toInt(), 0xFF82868A.toInt()),
            floatArrayOf(0f, 0.5f, 1f), Shader.TileMode.CLAMP)
        c.drawPath(outline, p); p.shader = null
        p.style = Paint.Style.STROKE; p.strokeWidth = u; p.color = 0x1FFFFFFF
        var yy = 2 * u
        while (yy < h) { c.drawLine(2 * u, yy, w - 2 * u, yy, p); yy += 3 * u }
        // marker
        val tp = Paint(Paint.ANTI_ALIAS_FLAG).apply { typeface = font; textSize = size; color = 0xFF141414.toInt(); isFakeBoldText = true }
        val total = tp.measureText(text) * 1.08f
        var cx = w / 2f - total / 2f
        val mr = Random(text.length * 97)
        for (ch in text) {
            val str = ch.toString()
            c.save()
            c.translate(cx, h / 2f + size / 3f + (mr.nextFloat() - 0.5f) * 2.2f * u)
            c.rotate((mr.nextFloat() - 0.5f) * 9f)
            c.drawText(str, 0f, 0f, tp); c.drawText(str, 0.6f * u, 0.3f * u, tp)
            c.restore()
            cx += tp.measureText(str) * 1.08f
        }
        c.restore()
    }
}
