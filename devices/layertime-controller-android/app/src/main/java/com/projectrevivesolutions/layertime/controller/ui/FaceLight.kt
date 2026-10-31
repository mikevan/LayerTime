package com.projectrevivesolutions.layertime.controller.ui

import androidx.compose.foundation.Canvas
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathOperation
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.max
import kotlin.math.min
import kotlin.math.sin

/**
 * Light for the face (Tier 1 of the 3D look, Michael, 2026-10-31). The face reads
 * as flat because each part was lit its own way. Here one light, from the upper
 * left, shades every part the way a real object would be shaded:
 *  - the rim as a raised ring: its outer slope catches the light on the upper left,
 *    its inner slope on the lower right;
 *  - the dial as recessed: the rim shades the dial just inside its upper-left edge;
 *  - the crystal as a dome: a curved window reflection on the upper left, a small
 *    hot spot, and a faint bright edge;
 *  - the whole face as a thick puck resting on the console: a side wall on the right and
 *    lower right, a contact shadow where it meets the console, and a short soft shadow.
 *
 * The light and the shadow are their own layers so that, when the face rolls off,
 * the face turns under the light and the light stays where it is. A wheel turns;
 * the sun does not. Only the geometry is shared with FaceView: the rim radii in its
 * 454 px design units, and the face circle filling the smaller side of its box.
 */
internal object FaceLight {
    /** Where the light comes from, in degrees clockwise from 3 o'clock (screen axes). 225 is the upper left. */
    const val LIGHT_DEG = 225f
    // Rim radii in 454 px design units, as drawn by FaceView's drawSteampunkRim.
    const val RIM_OUT = 2f
    const val RIM_IN = 22f
    /** How thick the face looks: the height of its side wall, in 454 px design units. */
    const val THICKNESS = 12f
}

/** The non-rotating light layer over the face (the sliding watch layout). */
@Composable
fun FaceLightLayer(modifier: Modifier = Modifier) {
    Canvas(modifier) {
        val d = min(size.width, size.height)
        drawFaceLighting(size.width / 2f, size.height / 2f, d)
    }
}

/** Under the face: its side wall and its shadows on the console, also non-rotating. */
@Composable
fun FaceShadowLayer(modifier: Modifier = Modifier) {
    Canvas(modifier) {
        val d = min(size.width, size.height)
        drawFaceCastShadow(size.width / 2f, size.height / 2f, d)
    }
}

/**
 * A sweep brush whose strength follows the light: [lit] where a surface facing
 * [facingDeg] turns toward the light, [shade] where it turns away. [ambient] adds a
 * share of [shade] everywhere, for occlusion that does not depend on the light.
 */
private fun lightSweep(c: Offset, facingOffsetDeg: Float, lit: Color, shade: Color, ambient: Float = 0f, steps: Int = 48): Brush {
    val stops = Array(steps + 1) { i ->
        val deg = i * 360f / steps
        val f = cos(((deg + facingOffsetDeg - FaceLight.LIGHT_DEG) * PI / 180.0)).toFloat()
        val color = if (f >= 0f && lit.alpha > 0f) lit.copy(alpha = lit.alpha * f)
            else shade.copy(alpha = shade.alpha * (ambient + (1f - ambient) * max(0f, -f)))
        (i.toFloat() / steps) to color
    }
    return Brush.sweepGradient(*stops, center = c)
}

internal fun DrawScope.drawFaceLighting(cx: Float, cy: Float, d: Float) {
    fun s(v: Float) = v * d / 454f
    val c = Offset(cx, cy)
    val rOut = d / 2f - s(FaceLight.RIM_OUT)
    val rIn = d / 2f - s(FaceLight.RIM_IN)
    val rMid = (rOut + rIn) / 2f

    // 1. Raised rim. Outer slope (its surface faces outward): lit on the upper left.
    //    Held clear of the hammered lip so the black dents stay black.
    val outerW = rOut - s(4f) - rMid
    drawCircle(lightSweep(c, 0f, Color.White.copy(alpha = 0.36f), Color.Black.copy(alpha = 0.42f)),
        radius = rMid + outerW / 2f, center = c, style = Stroke(outerW))
    //    Inner slope (faces inward, toward the centre): lit on the lower right instead.
    val innerW = rMid - rIn
    drawCircle(lightSweep(c, 180f, Color.White.copy(alpha = 0.20f), Color.Black.copy(alpha = 0.40f)),
        radius = rIn + innerW / 2f, center = c, style = Stroke(innerW))
    //    A thin glint along the outer slope where it faces the light squarely.
    val gr = rOut - s(6f)
    val gbox = Size(2 * gr, 2 * gr); val gtl = Offset(cx - gr, cy - gr)
    for ((sweep, a) in listOf(70f to 0.14f, 44f to 0.22f, 20f to 0.40f)) {
        drawArc(Color.White.copy(alpha = a), FaceLight.LIGHT_DEG - sweep / 2f, sweep, false, gtl, gbox, style = Stroke(s(1.6f)))
    }

    // 2. Recessed dial: the rim shades the dial just inside its edge, most on the
    //    upper left where it stands between the dial and the light. Stacked rings
    //    fading inward stand in for a blur.
    //    Kept to about 10 design px so the battery dots just inside the rim stay readable.
    val rings = 5
    for (i in 0 until rings) {
        val t = i / rings.toFloat()
        val a = 0.45f * (1f - t) * (1f - t)
        drawCircle(lightSweep(c, 180f, Color.Transparent, Color.Black.copy(alpha = a), ambient = 0.30f),
            radius = rIn - s(1.0f) - i * s(2.0f), center = c, style = Stroke(s(2.2f)))
    }

    // 3. Curved crystal. A faint bright edge all round (glass seen edge-on reflects more),
    val rGlass = rIn - s(2f)
    drawCircle(Color.White.copy(alpha = 0.06f), radius = rGlass - s(1.5f), center = c, style = Stroke(s(3f)))
    //    a window reflection: a crescent on the upper left, cut by a second circle
    //    shifted away from the light, fading toward the centre,
    val lx = cos(FaceLight.LIGHT_DEG * PI / 180.0).toFloat(); val ly = sin(FaceLight.LIGHT_DEG * PI / 180.0).toFloat()
    val glass = Path().apply { addOval(Rect(cx - rGlass, cy - rGlass, cx + rGlass, cy + rGlass)) }
    val shift = rGlass * 0.16f
    val cutter = Path().apply { addOval(Rect(cx - rGlass - lx * shift, cy - rGlass - ly * shift, cx + rGlass - lx * shift, cy + rGlass - ly * shift)) }
    val crescent = Path.combine(PathOperation.Difference, glass, cutter)
    drawPath(crescent, Brush.linearGradient(
        listOf(Color.White.copy(alpha = 0.16f), Color.White.copy(alpha = 0.05f), Color.Transparent),
        start = Offset(cx + lx * rGlass, cy + ly * rGlass), end = c))
    //    a small hot spot where the light source itself reflects,
    val hs = Offset(cx + lx * rGlass * 0.66f, cy + ly * rGlass * 0.66f)
    drawCircle(Brush.radialGradient(listOf(Color.White.copy(alpha = 0.30f), Color.Transparent), center = hs, radius = s(16f)),
        radius = s(16f), center = hs)
    //    and a weak second reflection opposite, as on a real domed crystal.
    val box = Size(2 * rGlass, 2 * rGlass); val tl = Offset(cx - rGlass, cy - rGlass)
    drawArc(Color(0x12BFD8FF), FaceLight.LIGHT_DEG - 180f - 25f, 50f, false, tl, box, style = Stroke(s(2f)))
}

internal fun DrawScope.drawFaceCastShadow(cx: Float, cy: Float, d: Float) {
    fun s(v: Float) = v * d / 454f
    val r = d / 2f
    // The face is a thick puck resting on the console, seen from a little above and to the
    // left, so its edge shows on the right and lower right (Michael, 2026-10-31: it looked
    // like it was floating; make it look thicker, sitting on top of the data screen).
    // [dir] points from the top face toward the base; the base disc is the puck's bottom face.
    val dir = Offset(0.94f, 0.34f)
    val t = s(FaceLight.THICKNESS)
    val base = Offset(cx + dir.x * t, cy + dir.y * t)

    // 1. A short, soft shadow just past the base: resting, not hovering.
    val sc = Offset(base.x + dir.x * s(4f), base.y + dir.y * s(4f))
    val sr = r + s(10f)
    drawCircle(Brush.radialGradient(
        0.0f to Color.Black.copy(alpha = 0.55f),
        (r / sr) to Color.Black.copy(alpha = 0.50f),
        ((r + s(4f)) / sr) to Color.Black.copy(alpha = 0.22f),
        1.0f to Color.Transparent,
        center = sc, radius = sr), radius = sr, center = sc)
    // 2. Contact shadow: a tight dark line where the edge meets the console.
    drawCircle(Color.Black.copy(alpha = 0.75f), radius = r + s(1.2f), center = base, style = Stroke(s(2.4f)))

    // 3. The side wall: every disc from the base up to the top face, so the band between
    //    the top face's edge and the base's edge is filled. The top face, drawn over it,
    //    leaves only the visible edge.
    val steps = maxOf(2, t.toInt())
    for (i in steps downTo 0) {
        val f = i / steps.toFloat()
        drawCircle(WallSteel, radius = r, center = Offset(cx + dir.x * t * f, cy + dir.y * t * f))
    }
    //    Shaded by the same light as the top: the wall's surface faces outward, so it is
    //    lit where it turns toward the upper left and in shade toward the lower right.
    val mid = Offset(cx + dir.x * t / 2f, cy + dir.y * t / 2f)
    drawCircle(lightSweep(mid, 0f, Color.White.copy(alpha = 0.22f), Color.Black.copy(alpha = 0.58f), ambient = 0.15f),
        radius = r + 0.5f, center = mid)
    //    Machining grooves along the wall: thin arcs at steps through its height.
    for (k in 1..3) {
        val f = k / 4f
        val gc = Offset(cx + dir.x * t * f, cy + dir.y * t * f)
        drawCircle(if (k % 2 == 0) Color.White.copy(alpha = 0.07f) else Color.Black.copy(alpha = 0.22f),
            radius = r, center = gc, style = Stroke(s(0.8f)))
    }
    //    Bottom edge: dark where the wall turns under onto the console.
    drawCircle(Color.Black.copy(alpha = 0.55f), radius = r - s(0.6f), center = base, style = Stroke(s(1.4f)))
    //    Top edge: a bright line where the wall meets the top face, strongest toward the
    //    light. It sits just outside the face's circle, so the face does not cover it.
    drawCircle(lightSweep(Offset(cx, cy), 0f, Color.White.copy(alpha = 0.55f), Color.White.copy(alpha = 0.10f), ambient = 1f),
        radius = r + s(0.4f), center = Offset(cx, cy), style = Stroke(s(1.2f)))
}

/** The side wall's metal: the rim's steel, a shade darker, as a machined edge would be. */
private val WallSteel = Color(0xFF4E545B)
