package com.projectrevivesolutions.layertime.controller.ui

import android.graphics.Paint
import android.graphics.Typeface
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.graphics.nativeCanvas
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.res.imageResource
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.IntSize
import com.projectrevivesolutions.layertime.controller.R
import com.projectrevivesolutions.layertime.controller.link.ReconText
import com.projectrevivesolutions.layertime.controller.link.StatusSnapshot
import com.projectrevivesolutions.layertime.controller.model.ConnState
import com.projectrevivesolutions.layertime.controller.ui.theme.LtColors
import kotlin.math.cos
import kotlin.math.min
import kotlin.math.sin

/**
 * The LayerTime circular face, ported from the Garmin face
 * (devices/garmin-tactix/source/HomeView.mc) at its 454 px design scale, drawn
 * in the left column of the landscape console. The owl and clock positions are
 * mirrored from the watch so the time sits above the owl. A brushed-steel ring
 * rims the face.
 *
 * Honest phone adaptation: TEMP and SUN have no source on this phone, so their
 * ring icon, label, and value are greyed and read N/A. The two ear glyphs show
 * the controller's real radios (BLE and the wand link); commands live on the
 * right.
 */
@Composable
fun FaceView(
    conn: ConnState,
    status: StatusSnapshot?,
    bluetoothOn: Boolean,
    batteryPct: Int,
    hourText: String,
    minText: String,
    dateText: String,
    tempValue: String? = null,
    tempUnit: String? = null,
    sunLabel: String = "SUN",
    sunValue: String = "--",
    sunAmPm: String? = null,
    sunTag: String? = null,
    onCounter: () -> Unit,
    onRecon: () -> Unit,
    onOutsideCircle: () -> Unit = {},
    modifier: Modifier = Modifier,
) {
    val owl = ImageBitmap.imageResource(R.drawable.layertime_owl)
    BoxWithConstraints(modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
        val side = maxWidth.coerceAtMost(maxHeight)
        Canvas(
            Modifier.fillMaxSize().pointerInput(Unit) {
                detectTapGestures { pos ->
                    val (lb, rb) = faceButtonRects(size.width.toFloat(), size.height.toFloat())
                    val r = min(size.width, size.height) / 2f
                    val dx = pos.x - size.width / 2f; val dy = pos.y - size.height / 2f
                    if (dx * dx + dy * dy > r * r) onOutsideCircle()
                    else if (lb.contains(pos)) onCounter() else if (rb.contains(pos)) onRecon()
                }
            }
        ) {
            val d = side.toPx()
            val cx = size.width / 2f
            val cy = size.height / 2f
            // Garmin design pixels (454 px display, origin top-left) mapped into the
            // circle inside the bezel: x(v), y(v) are positions, k(v) are lengths.
            val g = FaceGeom(cx, cy, d)
            fun k(v: Float) = g.k(v)
            fun sr(v: Float) = v * d / 454f      // full-circle scale, for the rim only

            // Left of the vertical bisector the background is black; right of it the face is
            // transparent outside the circle, so the console layer shows through.
            drawRect(LtColors.Bg, topLeft = Offset(0f, 0f), size = Size(cx, size.height))
            drawBronzeDial(cx, cy, d / 2f)

            drawBattery(g, batteryPct)
            drawTitle(g)

            // Owl 96..186 on the Garmin, grown to 100 as asked; ear icons beside its head.
            val owlSize = k(100f)
            drawImage(owl, dstOffset = IntOffset((cx - owlSize / 2f).toInt(), (g.y(141f) - owlSize / 2f).toInt()),
                dstSize = IntSize(owlSize.toInt(), owlSize.toInt()))
            val wandColor = when (conn) {
                ConnState.CONNECTED -> LtColors.Green
                ConnState.DISCONNECTED -> LtColors.Amber
                else -> LtColors.Red
            }
            drawBle(g.x(227f - 72f), g.y(118f), k(15f), if (bluetoothOn) LtColors.Green else LtColors.Red, ::k)
            drawAntenna(g.x(227f + 72f), g.y(118f), k(15f), wandColor, ::k)

            // Clock: Garmin FONT_NUMBER_MEDIUM, 59 px digits; hour ink rows 192..251,
            // minute ink rows 258..317.
            ink(hourText, cx, g.y(251f), k(59f), LtColors.Ivory, digits = true)
            ink(minText, cx, g.y(317f), k(59f), LtColors.Amber, digits = true)

            // Complications: RING_Y 190, RING_R 62, x 95 and 359.
            val ry = g.y(190f); val rr = k(62f)
            val lx = g.x(95f); val rx = g.x(359f)
            drawRing(lx, ry, rr, LtColors.Cyan, ::k)
            drawWeatherGlyph(lx, ry - k(37f), k(10f), LtColors.Cyan)
            bands(g, lx, "TEMP", LtColors.Cyan, tempValue ?: "N/A", if (tempValue != null && tempValue != "--") Color.White else LtColors.Grey,
                tempUnit, LtColors.Ivory)
            drawRing(rx, ry, rr, LtColors.Green, ::k)
            if (sunTag == null) drawSolarGlyph(rx, ry - k(37f), k(10f), LtColors.Green)
            bands(g, rx, sunLabel, if (sunValue != "--") LtColors.Green else LtColors.Grey, sunValue,
                if (sunValue != "--") Color.White else LtColors.Grey, sunAmPm, LtColors.Ivory)
            // MANUAL or the satellite count rides on a small riveted plate in the glyph's place at
            // the top of the ring (the bottom row only has room for AM or PM). Its row inside the
            // ring is about 84 px wide, so the plate is held to 80.
            sunTag?.let { drawRingTag(rx, ry - k(37f), it, if (it.startsWith("SAT")) LtColors.Ivory else LtColors.Amber, g, k(80f)) }

            val (lb, rb) = faceButtonRects(size.width, size.height)
            drawFaceButton(lb, "COUNTER", "shield", g)
            drawFaceButton(rb, "RECON", "radar", g)

            drawRecon(g, conn, status)
            // Date: XTINY, ink rows 401..419.
            ink(dateText, cx, g.y(419f), k(19f), Color.White, maxW = k(210f))

            // The bezel is drawn last: the battered rim holds a crystal above the metrics.
            drawSteampunkRim(cx, cy, d, ::sr)
            drawCrystalSheen(cx, cy, d, ::sr)
        }
    }
}

/**
 * Maps the Garmin face's 454 px design coordinates into this circle. The Garmin
 * draws edge to edge on a bare display; here a bezel takes the outer 22/454, so
 * the whole layout is scaled by [CONTENT] about the centre. Its outermost element,
 * the battery arc, then sits just inside the bezel's inner edge.
 */
private class FaceGeom(val cx: Float, val cy: Float, d: Float) {
    val unit = d / 454f * CONTENT
    fun x(v: Float) = cx + (v - 227f) * unit
    fun y(v: Float) = cy + (v - 227f) * unit
    fun k(v: Float) = v * unit
}

/** Share of the circle the Garmin layout is scaled to, so it fits inside the bezel (inner edge 205 of 227). */
private const val CONTENT = 0.93f

/** The Garmin strokes are 2 to 11 px; ours are 1.7 times that (the asked-for +25% on top of the earlier thickening). */
private const val PEN = 1.7f

// ---- bronze dial ----

private val BronzeGlaze = Color(0x40B87333)
private val DialCentre = Color(0xFF3C2916)
private val DialMid = Color(0xFF281B0E)
private val DialEdge = Color(0xFF140D07)

/**
 * The dial behind the metrics: a very faint burnished bronze, warmer and a little
 * brighter toward the upper left as if lit, darkening to the bezel, with fine
 * concentric polishing marks. Kept dark so the amber, ivory, cyan, and green
 * elements stay at full contrast.
 */
private fun DrawScope.drawBronzeDial(cx: Float, cy: Float, r: Float) {
    val c = Offset(cx, cy)
    drawCircle(Brush.radialGradient(listOf(DialCentre, DialMid, DialEdge),
        center = Offset(cx - r * 0.18f, cy - r * 0.22f), radius = r * 1.15f), radius = r, center = c)
    // burnishing: thin concentric arcs of slightly lighter and darker bronze, seeded
    val rnd = Lcg(29)
    var rr = r * 0.12f
    while (rr < r) {
        val start = rnd.range(0f, 360f); val sweep = rnd.range(40f, 160f)
        val light = rnd.next() < 0.5f
        drawArc(if (light) Color(0x18E0A060) else Color(0x1C000000), start, sweep, false,
            Offset(cx - rr, cy - rr), Size(2 * rr, 2 * rr), style = Stroke(1f))
        rr += rnd.range(2.5f, 6f)
    }
    // a soft sheen band across the upper left, like light caught on polished metal
    drawCircle(Brush.linearGradient(listOf(Color.Transparent, Color(0x1CFFC890), Color.Transparent),
        start = Offset(cx - r, cy - r), end = Offset(cx + r * 0.2f, cy + r * 0.2f)), radius = r, center = c)
}

// ---- steampunk rim ----

/** Small deterministic generator so the dents, rivets, and scratches are the same every frame. */
private class Lcg(seed: Int) {
    private var x = seed.toLong() and 0xffffffffL
    fun next(): Float { x = (x * 1664525L + 1013904223L) and 0xffffffffL; return (x ushr 8) / 16777216f }
    fun range(a: Float, b: Float) = a + (b - a) * next()
}

private val RimDark = Color(0xFF1C1F23)
private val RimSeam = Color(0xFF2B2F34)
private val RimLip = Color(0xFFDDE3E8)
private val RivetBody = Color(0xFF9AA3AC)
private val RivetRim = Color(0xFF3A3F45)
private val RivetShine = Color(0xFFEEF2F5)
private val Rust = Color(0x668B4A1F)
private val RustDark = Color(0x806B3A14)
private val Scratch = Color(0x7A202428)

/**
 * A thick, battered metal rim: a brushed-steel band about 20/454 of the face
 * wide, cut into plates by dark seams, riveted at each seam, with a mangled
 * outer lip that never reaches more than 2 px beyond the band's edge (and stays
 * inside the face's own circle), hammer dents, scratches, and rust. All of it is
 * vector drawing seeded with a fixed generator, so it costs no bitmap memory
 * and looks identical every frame.
 */
private fun DrawScope.drawSteampunkRim(cx: Float, cy: Float, d: Float, s: (Float) -> Float) {
    val rOut = d / 2f - s(2f)          // nominal outer edge of the band
    val rIn = d / 2f - s(22f)          // inner edge of the band
    val rMid = (rOut + rIn) / 2f
    val band = rOut - rIn
    val c = Offset(cx, cy)
    val rnd = Lcg(7)

    // 1. Brushed-steel band.
    drawCircle(Brush.sweepGradient(LtColors.Steel, c), radius = rMid, center = c, style = Stroke(band))
    // warm bronze glaze over the steel so the whole circle reads as one warm, salvaged piece
    drawCircle(BronzeGlaze, radius = rMid, center = c, style = Stroke(band))
    // Subtle bevel: darker toward the inner edge, lighter toward the outer.
    drawCircle(RimDark.copy(alpha = 0.35f), radius = rIn + s(3f), center = c, style = Stroke(s(6f)))
    drawCircle(RimLip.copy(alpha = 0.25f), radius = rOut - s(3f), center = c, style = Stroke(s(5f)))

    // 2. Mangled outer lip: the band's outer edge wanders between a dent of up to 5/454
    //    inward and 2 px outward. The gap between that edge and the true circle is black,
    //    so the lip reads as hammered metal, and a bright line follows it.
    val steps = 180
    val lip = androidx.compose.ui.graphics.Path()
    val outer = FloatArray(steps + 1)
    for (i in 0..steps) {
        val a = Math.toRadians(i * 360.0 / steps)
        val dent = if (rnd.next() < 0.18f) -rnd.range(s(2f), s(5f)) else rnd.range(-s(1f), 2.5f)
        outer[i] = (rOut + dent).coerceAtMost(d / 2f - 0.5f)
        val px = cx + outer[i] * cos(a).toFloat(); val py = cy + outer[i] * sin(a).toFloat()
        if (i == 0) lip.moveTo(px, py) else lip.lineTo(px, py)
    }
    lip.close()
    // black outside the lip, inside the face circle
    val hole = androidx.compose.ui.graphics.Path().apply { addOval(Rect(cx - d / 2f, cy - d / 2f, cx + d / 2f, cy + d / 2f)) }
    val cut = androidx.compose.ui.graphics.Path.combine(androidx.compose.ui.graphics.PathOperation.Difference, hole, lip)
    drawPath(cut, LtColors.Bg)
    drawPath(lip, RimLip.copy(alpha = 0.85f), style = Stroke(s(1.2f)))

    // 3. Inner lip, slightly uneven, with a dark seam against the face.
    val inner = androidx.compose.ui.graphics.Path()
    for (i in 0..steps) {
        val a = Math.toRadians(i * 360.0 / steps)
        val r = rIn + rnd.range(-s(1f), s(1.5f))
        val px = cx + r * cos(a).toFloat(); val py = cy + r * sin(a).toFloat()
        if (i == 0) inner.moveTo(px, py) else inner.lineTo(px, py)
    }
    inner.close()
    drawPath(inner, RimSeam, style = Stroke(s(2.2f)))

    // 4. Plates and seams. Fourteen plates with slightly uneven widths, dark radial seams
    //    with a light bevel line beside each, and two rivets per plate near its seams.
    val plates = 14
    var ang = rnd.range(0f, 20f)
    val seams = ArrayList<Float>()
    for (i in 0 until plates) { seams.add(ang); ang += 360f / plates + rnd.range(-4f, 4f) }
    fun polar(r: Float, deg: Float): Offset {
        val a = Math.toRadians(deg.toDouble()); return Offset(cx + r * cos(a).toFloat(), cy + r * sin(a).toFloat())
    }
    for (deg in seams) {
        val tilt = rnd.range(-3f, 3f)
        drawLine(RimDark, polar(rIn + s(1f), deg), polar(rOut - s(1f), deg + tilt), strokeWidth = s(2.4f))
        drawLine(RimLip.copy(alpha = 0.45f), polar(rIn + s(1f), deg + 1.2f), polar(rOut - s(1f), deg + tilt + 1.2f), strokeWidth = s(0.9f))
        // rivets either side of the seam
        for (side in listOf(-1f, 1f)) {
            val p = polar(rMid + rnd.range(-s(1.5f), s(1.5f)), deg + side * 5.5f)
            drawCircle(RivetRim, s(3.4f), p)
            drawCircle(RivetBody, s(2.5f), p)
            drawCircle(RivetShine, s(0.9f), Offset(p.x - s(0.8f), p.y - s(0.8f)))
            drawCircle(RimDark.copy(alpha = 0.5f), s(0.9f), Offset(p.x + s(0.9f), p.y + s(0.9f)))
        }
    }

    // 5. Hammer dents, scratches, and rust on the plates.
    for (i in 0 until plates) {
        val mid = seams[i] + 360f / plates / 2f
        // one shallow dent
        val dp = polar(rMid + rnd.range(-s(4f), s(4f)), mid + rnd.range(-6f, 6f))
        drawCircle(RimDark.copy(alpha = 0.22f), s(rnd.range(3f, 6f)), dp)
        drawCircle(RimLip.copy(alpha = 0.18f), s(rnd.range(1.5f, 3f)), Offset(dp.x - s(1f), dp.y - s(1f)))
        // two scratches
        repeat(2) {
            val a0 = mid + rnd.range(-8f, 8f); val r0 = rMid + rnd.range(-s(7f), s(7f))
            drawLine(Scratch, polar(r0, a0), polar(r0 + rnd.range(-s(3f), s(3f)), a0 + rnd.range(1f, 5f)), strokeWidth = s(0.8f))
        }
        // rust on every other plate
        if (i % 2 == 0) {
            val rp = polar(rMid + rnd.range(-s(5f), s(5f)), mid + rnd.range(-9f, 9f))
            drawCircle(Rust, s(rnd.range(4f, 7f)), rp)
            drawCircle(RustDark, s(rnd.range(1.5f, 3f)), Offset(rp.x + s(1.5f), rp.y + s(1f)))
        }
    }
}

/** A faint glass highlight just inside the bezel, so the rim reads as holding a crystal. */
private fun DrawScope.drawCrystalSheen(cx: Float, cy: Float, d: Float, s: (Float) -> Float) {
    val r = d / 2f - s(24f)
    val box = Size(2 * r, 2 * r); val tl = Offset(cx - r, cy - r)
    drawArc(Color(0x2EBFD8FF), 200f, 70f, false, tl, box, style = Stroke(s(3f)))
    drawArc(Color(0x14BFD8FF), 20f, 50f, false, tl, box, style = Stroke(s(2f)))
}

// ---- worn-metal treatment for the strokes inside the face ----
// Each element keeps its stroke width. Under it goes a dark shadow stroke offset by one
// pixel; on it go a thin bright bevel line, a few nicks and highlights, and rivets where
// a plate would be fastened. Seeded, so the wear is the same every frame.

private val WearShadow = Color(0xAA000000)
private val WearBevel = Color(0x66FFFFFF)
private val WearNick = Color(0xD9080A0C)
private val WeldBead = Color(0xFFB9C1C9)
private val WeldBeadEdge = Color(0xFF5C646C)
private val WeldHeat = Color(0x99000000)

/** Shadow + bevel + nicks along a straight stroke of the given width, drawn after the stroke. */
private fun DrawScope.wearLine(a: Offset, b: Offset, width: Float, rnd: Lcg, s: (Float) -> Float, shadowFirst: Boolean = false) {
    val dx = b.x - a.x; val dy = b.y - a.y
    val len = kotlin.math.sqrt(dx * dx + dy * dy); if (len < 1f) return
    val nx = -dy / len; val ny = dx / len           // unit normal
    if (shadowFirst) drawLine(WearShadow, Offset(a.x + 1f, a.y + 1f), Offset(b.x + 1f, b.y + 1f), strokeWidth = width)
    // bevel: bright hairline just inside one edge of the stroke
    val off = width * 0.3f
    drawLine(WearBevel, Offset(a.x - nx * off, a.y - ny * off), Offset(b.x - nx * off, b.y - ny * off), strokeWidth = maxOf(1f, s(1.0f)))
    // nicks across the stroke
    val n = (len / s(40f)).toInt().coerceIn(1, 4)
    repeat(n) {
        val t = rnd.range(0.1f, 0.9f)
        val px = a.x + dx * t; val py = a.y + dy * t
        val half = width * rnd.range(0.35f, 0.6f)
        drawLine(WearNick, Offset(px - nx * half, py - ny * half), Offset(px + nx * half, py + ny * half), strokeWidth = maxOf(1f, s(1.1f)))
    }
}

/**
 * A weld bead along a line: the "stack of dimes" pattern of overlapping scallops in
 * steel tones over a dark heat-affected band, with a thin line of the element's own
 * colour under it so a cyan button still reads as cyan and an amber bracket as amber.
 * The bead is about 3/454 of the face tall, one scaled pixel more than the old stroke.
 */
private fun DrawScope.weldLine(a: Offset, b: Offset, tint: Color, rnd: Lcg, s: (Float) -> Float) {
    val dx = b.x - a.x; val dy = b.y - a.y
    val len = kotlin.math.sqrt(dx * dx + dy * dy); if (len < 1f) return
    val ux = dx / len; val uy = dy / len; val nx = -uy; val ny = ux
    drawLine(WeldHeat, a, b, strokeWidth = s(8.1f))
    drawLine(tint.copy(alpha = 0.6f), a, b, strokeWidth = s(2.75f))
    val step = s(3.75f); val r = s(2.9f)
    var t = r
    var i = 0
    while (t < len - r * 0.5f) {
        val wob = rnd.range(-0.4f, 0.4f) * s(1f)
        val cx = a.x + ux * t + nx * wob; val cy = a.y + uy * t + ny * wob
        // each scallop is a disc with a darker trailing edge, overlapping the previous one
        drawCircle(WeldBeadEdge, r, Offset(cx + ux * s(0.5f), cy + uy * s(0.5f)))
        drawCircle(if (i % 3 == 0) WeldBead.copy(alpha = 0.92f) else WeldBead, r * 0.82f, Offset(cx, cy))
        drawCircle(RivetShine, maxOf(1f, r * 0.3f), Offset(cx - nx * r * 0.35f - ux * r * 0.2f, cy - ny * r * 0.35f - uy * r * 0.2f))
        t += step; i++
    }
}

/** A small rivet no wider than the stroke it sits on. */
private fun DrawScope.rivet(p: Offset, r: Float) {
    drawCircle(RivetRim, r, p)
    drawCircle(RivetBody, r * 0.7f, p)
    drawCircle(RivetShine, r * 0.28f, Offset(p.x - r * 0.3f, p.y - r * 0.3f))
}

/** Shadow, bevel, and nicks along an arc stroke (full circle when sweep is 360). */
private fun DrawScope.wearArc(cx: Float, cy: Float, r: Float, startDeg: Float, sweepDeg: Float, width: Float, rnd: Lcg, s: (Float) -> Float) {
    drawArc(WearBevel, startDeg, sweepDeg, false, Offset(cx - r + width * 0.3f, cy - r + width * 0.3f),
        Size(2 * (r - width * 0.3f), 2 * (r - width * 0.3f)), style = Stroke(maxOf(1f, s(1.0f))))
    val n = (sweepDeg / 30f).toInt().coerceIn(2, 12)
    repeat(n) {
        val a = Math.toRadians((startDeg + sweepDeg * rnd.range(0.05f, 0.95f)).toDouble())
        val c = cos(a).toFloat(); val sn = sin(a).toFloat()
        val half = width * rnd.range(0.35f, 0.6f)
        drawLine(WearNick, Offset(cx + (r - half) * c, cy + (r - half) * sn), Offset(cx + (r + half) * c, cy + (r + half) * sn), strokeWidth = maxOf(1f, s(1.1f)))
    }
}

// ---- text, sized and placed by ink like the Garmin fonts ----

private val LabelFace: Typeface = Typeface.create("sans-serif-condensed", Typeface.BOLD)
private val DigitFace: Typeface = Typeface.create(Typeface.DEFAULT, Typeface.BOLD)

/**
 * Draws [str] so its capital (or digit) ink is [capPx] tall with its bottom on
 * [baseline], the way the Garmin layout specifies every line by ink rows. Shrinks
 * to [maxW] when given. [align] is CENTER at [x], or LEFT from [x]. Returns the
 * drawn width.
 */
private fun DrawScope.ink(
    str: String, x: Float, baseline: Float, capPx: Float, color: Color,
    digits: Boolean = false, maxW: Float = 0f, align: Paint.Align = Paint.Align.CENTER, glow: Boolean = false,
): Float {
    val p = Paint().apply {
        isAntiAlias = true
        this.color = color.toArgb()
        textAlign = align
        typeface = if (digits) DigitFace else LabelFace
        letterSpacing = if (digits) 0.02f else 0.06f
        textSize = 100f
    }
    val b = android.graphics.Rect()
    p.getTextBounds(if (digits) "0" else "H", 0, 1, b)
    p.textSize = 100f * capPx / b.height().coerceAtLeast(1)
    var w = p.measureText(str)
    if (maxW > 0f && w > maxW) { p.textSize *= maxW / w; w = maxW }
    if (glow) p.setShadowLayer(capPx * 0.35f, 0f, 0f, color.copy(alpha = 0.55f).toArgb())
    val cv = drawContext.canvas.nativeCanvas
    // a one-pixel drop shadow keeps the text crisp over the worn metal
    val sh = Paint(p).apply { this.color = 0xCC000000.toInt(); clearShadowLayer() }
    cv.drawText(str, x + 1.2f, baseline + 1.2f, sh)
    cv.drawText(str, x, baseline, p)
    return w
}

// ---- face elements ----

/**
 * Battery as the Garmin draws it: a 120 degree segmented amber arc over the top at
 * r = 227 - 18, black ticks every 4 degrees, and under it a battery icon with
 * "BAT n%" whose capitals fill rows 42..60.
 */
private fun DrawScope.drawBattery(g: FaceGeom, pct: Int) {
    val low = pct in 1..20
    val col = if (low) LtColors.Red else LtColors.Amber
    val r = g.k(209f)
    val pen = g.k(11f * PEN)
    val box = Size(2 * r, 2 * r)
    val tl = Offset(g.cx - r, g.cy - r)
    // Garmin 150 -> 30 degrees counter-clockwise from 3 o'clock = Compose 210, sweep 120.
    drawArc(WearShadow, 210f, 120f, false, Offset(tl.x + 1.5f, tl.y + 1.5f), box, style = Stroke(pen))
    drawArc(LtColors.Dim, 210f, 120f, false, tl, box, style = Stroke(pen))
    val lit = 120f * (if (pct < 0) 0 else pct) / 100f
    if (lit > 0f) drawArc(col, 210f, lit, false, tl, box, style = Stroke(pen))
    wearArc(g.cx, g.cy, r, 210f, 120f, pen, Lcg(11), g::k)
    var a = 210f
    while (a <= 330.1f) {
        val rad = Math.toRadians(a.toDouble())
        val c = cos(rad).toFloat(); val sn = sin(rad).toFloat()
        val h = pen / 2f + 1.5f
        drawLine(Color.Black, Offset(g.cx + (r - h) * c, g.cy + (r - h) * sn),
            Offset(g.cx + (r + h) * c, g.cy + (r + h) * sn), strokeWidth = g.k(3f * 1.25f))
        a += 4f
    }
    // Icon (20 x 12 with a nub) then the text, centred together as on the Garmin.
    val text = "BAT ${if (pct < 0) "--" else "$pct%"}"
    val probe = Paint().apply { typeface = LabelFace; letterSpacing = 0.06f; textSize = 100f }
    val bb = android.graphics.Rect(); probe.getTextBounds("H", 0, 1, bb)
    probe.textSize = 100f * g.k(19f) / bb.height()
    val tw = probe.measureText(text)
    val left = g.cx - (tw + g.k(30f)) / 2f
    val iy = g.y(51f)
    val iw = g.k(20f); val ih = g.k(12f)
    drawRect(col, Offset(left, iy - ih / 2f), Size(iw, ih), style = Stroke(g.k(2.5f)))
    drawRect(col, Offset(left + iw, iy - g.k(3f)), Size(g.k(3f), g.k(6f)))
    val fillW = g.k(16f) * (if (pct < 0) 0 else pct) / 100f
    if (fillW > 0f) drawRect(col, Offset(left + g.k(2f), iy - g.k(4f)), Size(fillW, g.k(8f)))
    ink(text, left + g.k(30f), g.y(60f), g.k(19f), col, align = Paint.Align.LEFT)
}

/** A small dark plate with a coloured rim and two rivets, centred on (x, y), carrying [text]. */
private fun DrawScope.drawRingTag(x: Float, y: Float, text: String, color: Color, g: FaceGeom, maxW: Float) {
    var cap = g.k(12f)
    val probe = Paint().apply { typeface = LabelFace; letterSpacing = 0.06f; textSize = 100f }
    val b = android.graphics.Rect(); probe.getTextBounds("H", 0, 1, b)
    probe.textSize = 100f * cap / b.height()
    val pad = g.k(22f)
    var tw = probe.measureText(text)
    if (tw + pad > maxW) { val f = (maxW - pad) / tw; cap *= f; tw *= f }
    val w = tw + pad; val h = g.k(19f)
    val tl = Offset(x - w / 2f, y - h / 2f)
    drawRoundRect(Color(0xCC000000), Offset(tl.x + 1.5f, tl.y + 1.5f), Size(w, h), androidx.compose.ui.geometry.CornerRadius(h / 4f))
    drawRoundRect(Color(0xFF1E140A), tl, Size(w, h), androidx.compose.ui.geometry.CornerRadius(h / 4f))
    drawRoundRect(color, tl, Size(w, h), androidx.compose.ui.geometry.CornerRadius(h / 4f), style = Stroke(g.k(2f)))
    rivet(Offset(tl.x + g.k(5.5f), y), g.k(2.6f)); rivet(Offset(tl.x + w - g.k(5.5f), y), g.k(2.6f))
    ink(text, x, y + cap / 2f, cap, color)
}

/** LAYERTIME with capitals on rows 68..86 and the amber dashes either side. */
private fun DrawScope.drawTitle(g: FaceGeom) {
    val tw = ink("LAYERTIME", g.cx, g.y(86f), g.k(19f), LtColors.Ivory)
    val y = g.y(78f)
    val rnd = Lcg(13)
    val pen = g.k(2f * PEN)
    val l1 = Offset(g.cx - tw / 2f - g.k(34f), y) to Offset(g.cx - tw / 2f - g.k(10f), y)
    val l2 = Offset(g.cx + tw / 2f + g.k(10f), y) to Offset(g.cx + tw / 2f + g.k(34f), y)
    for ((a, b) in listOf(l1, l2)) {
        drawLine(WearShadow, Offset(a.x + 1f, a.y + 1f), Offset(b.x + 1f, b.y + 1f), strokeWidth = pen)
        drawLine(LtColors.Amber, a, b, strokeWidth = pen)
        wearLine(a, b, pen, rnd, g::k)
    }
}

/**
 * The four bands inside a complication ring, on the Garmin's rows relative to the
 * ring centre y: label capitals y-24..y-6, value digits y..y+31, unit y+36..y+54.
 */
private fun DrawScope.bands(
    g: FaceGeom, x: Float, label: String, labelColor: Color, value: String, valueColor: Color,
    unit: String?, unitColor: Color,
) {
    val y = g.y(190f)
    ink(label, x, y - g.k(6f), g.k(19f), labelColor, maxW = g.k(100f))
    ink(value, x, y + g.k(31f), g.k(31f), valueColor, digits = true, maxW = g.k(104f))
    // the unit row sits low in the ring, where its chord is about 60 px wide
    unit?.let { ink(it, x, y + g.k(54f), g.k(19f), unitColor, maxW = g.k(56f)) }
}

private fun DrawScope.drawRing(x: Float, y: Float, r: Float, color: Color, s: (Float) -> Float) {
    val w = s(7f * PEN)
    val rnd = Lcg((x * 31 + y * 17).toInt())
    drawArc(WearShadow, 0f, 360f, false, Offset(x - r + 1f, y - r + 1f), Size(2 * r, 2 * r), style = Stroke(w))
    drawArc(color, 0f, 360f, false, Offset(x - r, y - r), Size(2 * r, 2 * r), style = Stroke(w))
    wearArc(x, y, r, 0f, 360f, w, rnd, s)
    var a = 0f
    while (a < 360f) {
        val rad = Math.toRadians(a.toDouble())
        val c = cos(rad).toFloat(); val sn = sin(rad).toFloat()
        drawLine(Color.Black, Offset(x + (r - w / 2f - 1.5f) * c, y + (r - w / 2f - 1.5f) * sn),
            Offset(x + (r + w / 2f + 1.5f) * c, y + (r + w / 2f + 1.5f) * sn), strokeWidth = s(3.75f))
        a += 30f
    }
    // rivets at the diagonals, no wider than the ring
    for (deg in listOf(45f, 135f, 225f, 315f)) {
        val rad = Math.toRadians(deg.toDouble())
        rivet(Offset(x + r * cos(rad).toFloat(), y + r * sin(rad).toFloat()), w * 0.45f)
    }
}

private fun DrawScope.drawBle(cx: Float, cy: Float, r: Float, color: Color, s: (Float) -> Float) {
    val w = s(2f)
    val top = Offset(cx, cy - r); val bot = Offset(cx, cy + r)
    val ur = Offset(cx + r * 0.6f, cy - r * 0.5f); val lr = Offset(cx + r * 0.6f, cy + r * 0.5f)
    drawLine(color, top, bot, strokeWidth = w)
    drawLine(color, top, ur, strokeWidth = w)
    drawLine(color, ur, Offset(cx - r * 0.6f, cy + r * 0.5f), strokeWidth = w)
    drawLine(color, bot, lr, strokeWidth = w)
    drawLine(color, lr, Offset(cx - r * 0.6f, cy - r * 0.5f), strokeWidth = w)
}

private fun DrawScope.drawAntenna(cx: Float, cy: Float, r: Float, color: Color, s: (Float) -> Float) {
    val tipY = cy - r * 0.5f
    val path = androidx.compose.ui.graphics.Path().apply {
        moveTo(cx, tipY); lineTo(cx + r * 0.45f, cy + r); lineTo(cx - r * 0.45f, cy + r); close()
    }
    drawPath(path, color)
    drawCircle(color, r * 0.22f, Offset(cx, tipY))
    val w = s(2f)
    for (rr in listOf(r * 0.6f, r)) {
        drawArc(color, -50f, 100f, false, Offset(cx - rr, tipY - rr), Size(2 * rr, 2 * rr), style = Stroke(w))
        drawArc(color, 130f, 100f, false, Offset(cx - rr, tipY - rr), Size(2 * rr, 2 * rr), style = Stroke(w))
    }
}

private fun DrawScope.drawWeatherGlyph(cx: Float, cy: Float, r: Float, color: Color = LtColors.Grey) {
    // a small sun: disc and eight rays
    drawCircle(color, r * 0.45f, Offset(cx, cy), style = Stroke(2.5f))
    for (k in 0 until 8) {
        val a = Math.toRadians(k * 45.0); val c = cos(a).toFloat(); val sn = sin(a).toFloat()
        drawLine(color, Offset(cx + c * r * 0.7f, cy + sn * r * 0.7f), Offset(cx + c * r, cy + sn * r), 2.5f)
    }
}

private fun DrawScope.drawSolarGlyph(cx: Float, cy: Float, r: Float, color: Color = LtColors.Grey) {
    val hy = cy + r * 0.3f
    drawArc(color, 180f, 180f, false, Offset(cx - r * 0.55f, hy - r * 0.55f),
        Size(r * 1.1f, r * 1.1f), style = Stroke(2.5f))
    drawLine(color, Offset(cx - r, hy), Offset(cx + r, hy), 2.5f)
    drawLine(color, Offset(cx, hy - r * 0.9f), Offset(cx, hy - r * 1.3f), 2.5f)
    drawLine(color, Offset(cx - r * 0.7f, hy - r * 0.6f), Offset(cx - r * 0.95f, hy - r * 0.85f), 2.5f)
    drawLine(color, Offset(cx + r * 0.7f, hy - r * 0.6f), Offset(cx + r * 0.95f, hy - r * 0.85f), 2.5f)
}

/**
 * The Recon entry as on the Garmin: chevron brackets from row 326 to 393, the mode
 * in 19 px capitals on rows 334..353 and the count in 27 px capitals on rows
 * 359..386; width is the wider text plus 44, at least 220, and inside the bezel.
 */
private fun DrawScope.drawRecon(g: FaceGeom, conn: ConnState, status: StatusSnapshot?) {
    val connected = conn == ConnState.CONNECTED
    var mode: String
    val count: String
    var color: Color
    if (!connected) {
        mode = if (conn == ConnState.DISCONNECTED) "RECONNECTING" else "NO LAYERWAND"
        count = if (conn == ConnState.FAILED) "LINK FAILED" else "SEARCHING"
        color = LtColors.Grey
    } else {
        mode = if (status != null) ReconText.modeText(status.flags, status.active) else "RECON OFF"
        count = ReconText.countText(status?.eventCount ?: 0)
        color = LtColors.Amber
    }
    val alert = connected && status?.alertPending == true
    if (alert) { mode = "ALERT  $mode"; color = LtColors.Red }

    val tB = g.y(326f); val bB = g.y(393f); val mid = (tB + bB) / 2f
    // widest text decides the bracket, clamped to the chord inside the bezel at the bottom row
    val maxW = 2f * kotlin.math.sqrt((g.k(218f) * g.k(218f) - (bB - g.cy) * (bB - g.cy)).coerceAtLeast(0f))
    val mw = ink(mode, -10000f, -10000f, g.k(19f), color)          // measure only (off-screen)
    val cw = ink(count, -10000f, -10000f, g.k(27f), color)
    val w = (maxOf(mw, cw) + g.k(44f)).coerceAtLeast(g.k(220f)).coerceAtMost(maxW)
    val lx = g.cx - w / 2f; val rx = g.cx + w / 2f
    val k = g.k(16f); val seg = g.k(60f)
    val rnd = Lcg(17)
    fun l(a: Offset, b: Offset) = weldLine(a, b, if (alert) LtColors.Red else LtColors.Amber, rnd, g::k)
    l(Offset(lx + k, tB), Offset(lx, mid)); l(Offset(lx, mid), Offset(lx + k, bB))
    l(Offset(rx - k, tB), Offset(rx, mid)); l(Offset(rx, mid), Offset(rx - k, bB))
    l(Offset(lx + k, tB), Offset(lx + seg, tB)); l(Offset(rx - seg, tB), Offset(rx - k, tB))
    l(Offset(lx + k, bB), Offset(lx + seg, bB)); l(Offset(rx - seg, bB), Offset(rx - k, bB))
    ink(mode, g.cx, g.y(353f), g.k(19f), color, maxW = w - g.k(40f))
    ink(count, g.cx, g.y(386f), g.k(27f), if (connected && !alert) Color.White else color, maxW = w - g.k(40f))
}

/** The two face button rectangles (COUNTER left, RECON right): Garmin [22,262,133,63] and [299,262,133,63]. */
private fun faceButtonRects(w: Float, h: Float): Pair<Rect, Rect> {
    val g = FaceGeom(w / 2f, h / 2f, min(w, h))
    val lb = Rect(g.x(22f), g.y(262f), g.x(155f), g.y(325f))
    val rb = Rect(g.x(299f), g.y(262f), g.x(432f), g.y(325f))
    return lb to rb
}

private fun DrawScope.drawFaceButton(r: Rect, label: String, icon: String, g: FaceGeom) {
    val c = g.k(12f)
    val x = r.left; val y = r.top; val w = r.width; val h = r.height
    // Octagonal (cut-corner) outline, as the Garmin MAPPING/MESH buttons draw it.
    val pts = listOf(
        Offset(x + c, y), Offset(x + w - c, y), Offset(x + w, y + c), Offset(x + w, y + h - c),
        Offset(x + w - c, y + h), Offset(x + c, y + h), Offset(x, y + h - c), Offset(x, y + c),
    )
    val rnd = Lcg((x * 7 + y * 3).toInt())
    for (i in pts.indices) weldLine(pts[i], pts[(i + 1) % pts.size], LtColors.Cyan, rnd, g::k)
    drawLine(Scratch, Offset(x + w * rnd.range(0.15f, 0.4f), y + h * rnd.range(0.2f, 0.8f)),
        Offset(x + w * rnd.range(0.5f, 0.85f), y + h * rnd.range(0.2f, 0.8f)), strokeWidth = maxOf(1f, g.k(1.0f)))
    val cx = x + w / 2f
    // icon band centre y+19 (size 10), label capitals y+35..y+54
    when (icon) {
        "shield" -> drawShieldGlyph(cx, y + g.k(19f), g.k(10f))
        "radar" -> drawRadarGlyph(cx, y + g.k(19f), g.k(10f))
    }
    ink(label, cx, y + g.k(54f), g.k(19f), LtColors.Cyan, maxW = w - g.k(20f))
}

private fun DrawScope.drawShieldGlyph(cx: Float, cy: Float, r: Float) {
    val path = androidx.compose.ui.graphics.Path().apply {
        moveTo(cx - r, cy - r * 0.85f)
        lineTo(cx + r, cy - r * 0.85f)
        lineTo(cx + r, cy + r * 0.2f)
        lineTo(cx, cy + r * 1.15f)
        lineTo(cx - r, cy + r * 0.2f)
        close()
    }
    drawPath(path, LtColors.Cyan, style = Stroke(2.5f))
}

private fun DrawScope.drawRadarGlyph(cx: Float, cy: Float, r: Float) {
    drawCircle(LtColors.Cyan, r, Offset(cx, cy), style = Stroke(2.5f))
    drawCircle(LtColors.Cyan, r * 0.5f, Offset(cx, cy), style = Stroke(1.5f))
    val tip = Offset(cx + r * 0.71f, cy - r * 0.71f)
    drawLine(LtColors.Cyan, Offset(cx, cy), tip, strokeWidth = 2f)
    drawCircle(LtColors.Cyan, 2.2f, tip)
}
