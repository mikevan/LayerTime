package com.projectrevivesolutions.layertime.controller.ui

import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RadialGradient
import android.graphics.Shader
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asImageBitmap
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.exp
import kotlin.math.max
import kotlin.math.min
import kotlin.math.pow
import kotlin.math.sin
import kotlin.math.sqrt
import kotlin.random.Random

/**
 * The salvaged hull behind the watch layout (Michael, 2026-10-31): the console is a part
 * cut out of a crashed alien ship, and the watch face was bolted onto it by somebody
 * with a grinder and no plan. Mocked up and approved first (Claude outputs:
 * console_parked_detail.png, console_roll_mockup.png, cable_groove_zoom.png).
 *
 * Two images, each computed once per screen size off the main thread and cached:
 *  - [HullImages.hull], under everything: the plate behind the face, the console frame
 *    raised above it, ribbing down the frame's left side, invented glyphs engraved in the
 *    top band, a spine down the right band, heat scorch, the gear rack the face rolls on,
 *    and the scratches the face cut rolling over the high edges;
 *  - [HullImages.overlay], over the console's panels: the console's cover glass with its
 *    cracks, the glow tubes (one section dead, one dim), and the steel straps riveted
 *    through the glass.
 *
 * The relief is a height map lit by the same light as the face (FaceLight.LIGHT_DEG, the
 * upper left), so bevels, ribs, and engravings shade the way real metal would. Nothing
 * here touches the data: the panels scroll over the inner plate at full contrast, the
 * straps reach only over a panel's border, and every crack is clipped to the glass.
 *
 * The scratches are the paths a burr under the face really traces: the face slides left
 * by s and turns s / R radians (counter-clockwise), so a point at radius r and angle t0
 * travels (homeCx + s + r cos(t0 + s/R), R + r sin(t0 + s/R)). On the console those paths
 * climb to the left. They are cut only where the surface stands high.
 */
internal data class HullSpec(
    val w: Int, val h: Int, val dp: Float,
    /** The console's outer edge, in px. */
    val consoleL: Float, val consoleT: Float, val consoleR: Float, val consoleB: Float,
    /** Width of the console's raised frame band (its content padding), px. */
    val band: Float,
    /** The face: radius, home centre x (its centre y is faceR), and how far it slides, px. */
    val faceR: Float, val homeCx: Float, val travel: Float,
)

internal class HullImages(val hull: ImageBitmap, val overlay: ImageBitmap)

internal object HullRelief {
    fun render(s: HullSpec): HullImages {
        val t0 = System.nanoTime()
        val out = HullImages(renderHull(s).asImageBitmap(), renderOverlay(s).asImageBitmap())
        android.util.Log.i("LayerTimeHull", "hull rendered ${s.w}x${s.h} in ${(System.nanoTime() - t0) / 1_000_000} ms")
        return out
    }

    // Heights (0 low .. 1 high) of the hull's levels.
    private const val PLATE = 0.30f      // behind the face
    private const val INNER = 0.50f      // the console's inner plate, under the panels
    private const val BAND = 0.75f       // the console's raised frame
    private const val SCRATCH_FROM = 0.60f
    /** How tall the rack's teeth stand, dp (Michael, 2026-10-31: 20% taller than 11, so the
     *  rack reads as something the face could really roll on). */
    private const val TOOTH_TALL = 13.2f

    /** Height stored in a grey pixel of the height map. */
    private fun hgt0(pixel: Int) = (pixel and 0xFF) / 255f

    private fun gray(v: Float): Int { val g = (v.coerceIn(0f, 1f) * 255f).toInt(); return android.graphics.Color.rgb(g, g, g) }

    private fun chamfer(l: Float, t: Float, r: Float, b: Float, c: Float) = Path().apply {
        moveTo(l + c, t); lineTo(r - c, t); lineTo(r, t + c); lineTo(r, b - c)
        lineTo(r - c, b); lineTo(l + c, b); lineTo(l, b - c); lineTo(l, t + c); close()
    }

    /** Where the rack runs: under the face's whole path, ending just past its home contact point. */
    private fun rackEnd(s: HullSpec) = s.homeCx + 20f * s.dp
    private fun rackTop(s: HullSpec) = s.h - 6f * s.dp

    private fun renderHull(s: HullSpec): Bitmap {
        val w = s.w; val h = s.h; val dp = s.dp
        val rnd = Random(41)

        // ---- 1. height map, drawn as grey levels ----
        val hb = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888)
        val c = Canvas(hb)
        val p = Paint(Paint.ANTI_ALIAS_FLAG)
        c.drawColor(gray(PLATE))
        // engraved panel lines on the plate behind the face
        p.style = Paint.Style.STROKE; p.strokeWidth = 2f * dp; p.color = gray(PLATE - 0.12f)
        c.drawPath(Path().apply { moveTo(0f, h * 0.19f); lineTo(30f * dp, h * 0.16f); lineTo(30f * dp, h * 0.73f); lineTo(0f, h * 0.79f) }, p)
        c.drawLine(6f * dp, h * 0.05f, 40f * dp, h * 0.05f, p)
        c.drawLine(6f * dp, h * 0.91f, 40f * dp, h * 0.91f, p)
        c.drawLine(s.consoleL * 0.5f, h * 0.30f, s.consoleL * 0.5f, h * 0.62f, p)
        // the console: raised frame band, lower inner plate
        p.style = Paint.Style.FILL
        p.color = gray(BAND); c.drawPath(chamfer(s.consoleL, s.consoleT, s.consoleR, s.consoleB, 22f * dp), p)
        val iL = s.consoleL + s.band; val iT = s.consoleT + s.band; val iR = s.consoleR - s.band; val iB = s.consoleB - s.band
        p.color = gray(INNER); c.drawPath(chamfer(iL, iT, iR, iB, 14f * dp), p)
        // ribbing down the left band
        p.style = Paint.Style.STROKE; p.strokeWidth = 1.6f * dp; p.color = gray(0.88f)
        var y = s.consoleT + s.band * 2f
        while (y < s.consoleB - s.band * 2f) { c.drawLine(s.consoleL + 3f * dp, y, s.consoleL + s.band - 3f * dp, y, p); y += 5f * dp }
        // ribbing along the top band, slanted
        p.color = gray(0.84f); p.strokeWidth = 1.4f * dp
        var x = s.consoleL + s.band * 3f
        while (x < s.consoleR - s.band * 4f) { c.drawLine(x, s.consoleT + 3f * dp, x + 5f * dp, s.consoleT + s.band - 3f * dp, p); x += 6.5f * dp }
        // invented glyphs engraved across the ribbing
        p.color = gray(0.55f); p.strokeWidth = 1.8f * dp
        x = s.consoleL + s.band * 3.5f
        while (x < s.consoleR - s.band * 4.5f) {
            val gw = s.band * 0.55f; val gy = s.consoleT + s.band * 0.22f
            val pts = FloatArray(8) { i -> if (i % 2 == 0) x + rnd.nextFloat() * gw else gy + rnd.nextFloat() * gw }
            c.drawLines(floatArrayOf(pts[0], pts[1], pts[2], pts[3], pts[2], pts[3], pts[4], pts[5], pts[4], pts[5], pts[6], pts[7]), p)
            c.drawLine(x, gy + gw / 2f, x + gw, gy + gw / 2f + (rnd.nextFloat() - 0.5f) * 4f * dp, p)
            x += s.band * 1.35f
        }
        // a raised spine with notches down the right band
        p.style = Paint.Style.FILL; p.color = gray(0.97f)
        val sx = s.consoleR - s.band * 0.55f
        c.drawPath(Path().apply {
            moveTo(sx - 3f * dp, s.consoleT + s.band * 2.6f); lineTo(sx + 3f * dp, s.consoleT + s.band * 2.2f)
            lineTo(sx + 3f * dp, s.consoleB - s.band * 2.2f); lineTo(sx - 3f * dp, s.consoleB - s.band * 2.6f); close()
        }, p)
        p.style = Paint.Style.STROKE; p.strokeWidth = 1.2f * dp; p.color = gray(0.70f)
        y = s.consoleT + s.band * 3f
        while (y < s.consoleB - s.band * 3f) { c.drawLine(sx - 3f * dp, y, sx + 3f * dp, y - 2f * dp, p); y += 22f * dp }
        // the gear rack the face rolls on
        p.style = Paint.Style.FILL; p.color = gray(0.72f)
        val rt = rackTop(s); val re = rackEnd(s)
        c.drawRect(0f, rt, re, h.toFloat(), p)
        // Teeth (Michael, 2026-10-31: much bigger; the strip under them stays as it is): gear-tooth
        // profiles, wide at the root and narrow at the tip, standing well proud of the strip.
        p.color = gray(0.97f)
        val pitch = 14f * dp; val root = 8f * dp; val tip = 4f * dp; val tall = TOOTH_TALL * dp
        x = 0f
        while (x < re - root) {
            c.drawPath(Path().apply {
                moveTo(x, rt + 2f * dp); lineTo(x + (root - tip) / 2f, rt + 2f * dp - tall)
                lineTo(x + (root + tip) / 2f, rt + 2f * dp - tall); lineTo(x + root, rt + 2f * dp); close()
            }, p)
            x += pitch
        }
        p.color = gray(0.92f); c.drawRect(re - 4f * dp, rt - tall, re + 3f * dp, h.toFloat(), p)   // end stop

        val px = IntArray(w * h); hb.getPixels(px, 0, w, 0, 0, w, h); hb.recycle()
        val hgt = FloatArray(w * h) { i -> (px[i] and 0xFF) / 255f + (rnd.nextFloat() - 0.5f) * 0.016f }
        val rackTopAll = (rt - TOOTH_TALL * dp + 2f * dp).toInt()
        val rackMask = BooleanArray(w * h) { i -> i / w >= rackTopAll && i % w < (re + 3f * dp).toInt() && (i / w >= rt.toInt() || hgt0(px[i]) > 0.9f) }
        boxBlur(hgt, w, h, max(1, (1.6f * dp).toInt())); boxBlur(hgt, w, h, max(1, (1.6f * dp).toInt()))

        // ---- 2. scratches: the face's real paths, cut only into the high surfaces ----
        val sb = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888)
        val sc = Canvas(sb)
        val lit = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE; color = android.graphics.Color.RED }
        val dark = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE; color = android.graphics.Color.GREEN }
        val sr = Random(23)
        repeat(110) {
            val r = s.faceR * (0.62f + 0.37f * sr.nextFloat()); val t0 = sr.nextFloat() * 2f * PI.toFloat()
            val s0 = -sr.nextFloat() * s.travel * 0.85f
            val s1 = max(-s.travel, s0 - s.faceR * (0.13f + 0.42f * sr.nextFloat()))
            val path = Path(); val shadow = Path()
            for (k in 0..40) {
                val sl = s0 + (s1 - s0) * k / 40f
                val a = t0 + sl / s.faceR
                val qx = s.homeCx + sl + r * cos(a); val qy = s.faceR + r * sin(a)
                if (k == 0) { path.moveTo(qx, qy); shadow.moveTo(qx + 1.2f * dp, qy + 1.2f * dp) }
                else { path.lineTo(qx, qy); shadow.lineTo(qx + 1.2f * dp, qy + 1.2f * dp) }
            }
            val wd = (1f + sr.nextFloat()) * dp
            dark.strokeWidth = wd; lit.strokeWidth = wd
            sc.drawPath(shadow, dark); sc.drawPath(path, lit)
        }
        val spx = IntArray(w * h); sb.getPixels(spx, 0, w, 0, 0, w, h); sb.recycle()

        // ---- 3. light it: one light from the upper left, as on the face ----
        val lx = -0.55f; val ly = -0.62f; val lz = 0.56f
        val ln = sqrt(lx * lx + ly * ly + lz * lz); val Lx = lx / ln; val Ly = ly / ln; val Lz = lz / ln
        var hx = Lx; var hy = Ly; var hz = Lz + 1f; val hn = sqrt(hx * hx + hy * hy + hz * hz); hx /= hn; hy /= hn; hz /= hn
        val k = 30f
        val tone = smoothNoise(w, h, (12f * dp).toInt().coerceAtLeast(4), Random(5))
        val out = IntArray(w * h)
        val scorchX = s.consoleR - 20f * dp; val scorchY = s.consoleT + 8f * dp
        val scW = 110f * dp; val scH = 80f * dp
        for (yy in 0 until h) {
            for (xx in 0 until w) {
                val i = yy * w + xx
                val gx = (hgt[yy * w + min(w - 1, xx + 1)] - hgt[yy * w + max(0, xx - 1)]) * 0.5f
                val gy = (hgt[min(h - 1, yy + 1) * w + xx] - hgt[max(0, yy - 1) * w + xx]) * 0.5f
                var nx = -gx * k; var ny = -gy * k; var nz = 1f
                val nn = sqrt(nx * nx + ny * ny + nz * nz); nx /= nn; ny /= nn; nz /= nn
                val lam = max(0f, nx * Lx + ny * Ly + nz * Lz)
                val spec = max(0f, nx * hx + ny * hy + nz * hz).pow(28)
                val tn = tone[i]
                var ar: Float; var ag: Float; var ab: Float
                if (rackMask[i]) { ar = 100f + 8f * tn; ag = 106f + 8f * tn; ab = 112f + 8f * tn }
                else { ar = 62f + 10f * tn; ag = 56f + 9f * tn; ab = 40f + 6f * tn }
                val shade = 0.28f + 1.05f * lam
                var r = ar * shade + spec * 150f; var g = ag * shade + spec * 138f; var b = ab * shade + spec * 112f
                val dxs = (xx - scorchX) / scW; val dys = (yy - scorchY) / scH
                val scorch = exp(-(dxs * dxs + dys * dys)) * 0.82f
                r *= 1f - scorch; g *= 1f - scorch; b *= 1f - scorch
                val high = ((hgt[i] - SCRATCH_FROM) / 0.15f).coerceIn(0f, 1f)
                if (high > 0f) {
                    val cut = ((spx[i] shr 16) and 0xFF) / 255f; val sh = ((spx[i] shr 8) and 0xFF) / 255f
                    val m = 1f - 0.7f * sh * high
                    r = r * m + cut * high * 185f; g = g * m + cut * high * 172f; b = b * m + cut * high * 145f
                }
                out[i] = android.graphics.Color.rgb(r.toInt().coerceIn(0, 255), g.toInt().coerceIn(0, 255), b.toInt().coerceIn(0, 255))
            }
        }
        return Bitmap.createBitmap(out, w, h, Bitmap.Config.ARGB_8888)
    }

    private fun renderOverlay(s: HullSpec): Bitmap {
        val w = s.w; val h = s.h; val dp = s.dp
        val bm = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888)
        val c = Canvas(bm)
        val iL = s.consoleL + s.band; val iT = s.consoleT + s.band; val iR = s.consoleR - s.band; val iB = s.consoleB - s.band
        val rnd = Random(11)

        // The straps: three, not quite parallel, reaching from the plate over the frame onto
        // the console's cover glass just far enough to cover a panel's border.
        class Fastener(val x: Float, val y: Float, val kind: Char)   // r rivet, b bolt, s screw, h hole
        class Strap(val x0: Float, val y0: Float, val x1: Float, val y1: Float, val wd: Float, val kinks: String)
        val ch = iB - iT
        val straps = listOf(
            Strap(s.consoleL - 22f * dp, iT + ch * 0.30f, iL + 16f * dp, iT + ch * 0.30f + 3f * dp, 13f * dp, "rbr"),
            Strap(s.consoleL - 24f * dp, iT + ch * 0.60f + 2f * dp, iL + 15f * dp, iT + ch * 0.60f - 2f * dp, 12f * dp, "srh"),
            Strap(s.consoleL - 20f * dp, iT + ch * 0.90f, iL + 14f * dp, iT + ch * 0.90f - 1f * dp, 10f * dp, "rrb"),
        )
        val fasteners = ArrayList<Fastener>()
        for (st in straps) {
            // fasteners: one on the plate, one on the frame band, one through the glass
            val xs = floatArrayOf(st.x0 + 7f * dp, s.consoleL + s.band * 0.5f, iL + 8f * dp)
            for ((j, fx) in xs.withIndex()) {
                val t = (fx - st.x0) / (st.x1 - st.x0)
                fasteners.add(Fastener(fx, st.y0 + (st.y1 - st.y0) * t, st.kinks[j]))
            }
        }

        // ---- cracks in the cover glass: clipped to the glass, so none reach the metal ----
        val crack = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE; strokeWidth = 1f; color = android.graphics.Color.argb(150, 150, 225, 245) }
        val ring = Paint(crack).apply { color = android.graphics.Color.argb(60, 150, 225, 245) }
        fun spider(cx: Float, cy: Float, n: Int, lmin: Float, lmax: Float, rings: FloatArray) {
            val off = rnd.nextFloat()
            for (i in 0 until n) {
                val a = off + i * 2f * PI.toFloat() / n
                val len = lmin + (lmax - lmin) * rnd.nextFloat()
                val path = Path(); path.moveTo(cx + cos(a) * 3f * dp, cy + sin(a) * 3f * dp)
                for (st in 1..4) {
                    val aa = a + (rnd.nextFloat() - 0.5f) * 0.44f
                    path.lineTo(cx + cos(aa) * len * st / 4f, cy + sin(aa) * len * st / 4f)
                }
                c.drawPath(path, crack)
            }
            for (rr in rings) c.drawCircle(cx, cy, rr, ring)
        }
        c.save()
        c.clipRect(iL, iT, iR, iB)
        spider(iR - 12f * dp, iT + 12f * dp, 12, 25f * dp, 65f * dp, floatArrayOf(11f * dp, 23f * dp))  // the corner impact
        for (f in fasteners) if (f.x > iL) spider(f.x, f.y, if (f.kind == 'h') 7 else 5, 8f * dp, 23f * dp, floatArrayOf(7.5f * dp))
        c.restore()

        // ---- glow tubes along the inside of the frame: one dead section, one dim ----
        fun tube(x0: Float, y0: Float, x1: Float, y1: Float, a: Float) {
            for ((wd, al) in listOf(6f to 0.16f, 3.5f to 0.32f, 1.5f to 0.85f)) {
                val tp = Paint(Paint.ANTI_ALIAS_FLAG).apply {
                    style = Paint.Style.STROKE; strokeWidth = wd * dp; strokeCap = Paint.Cap.ROUND
                    color = android.graphics.Color.argb((255 * al * a).toInt(), 48, 208, 255)
                }
                c.drawLine(x0, y0, x1, y1, tp)
            }
        }
        val ty = iT - 3f * dp
        tube(iL + 10f * dp, ty, iL + (iR - iL) * 0.40f, ty, 1f)                  // ..then dead to 0.52
        tube(iL + (iR - iL) * 0.52f, ty, iR - 10f * dp, ty, 0.45f)                // and dim after it
        tube(iL + 10f * dp, iB + 3f * dp, iR - 10f * dp, iB + 3f * dp, 1f)
        val lx = iL - 3f * dp
        var y0 = iT + 10f * dp
        for (st in straps.sortedBy { it.y0 }) {                                  // the left tube runs under the straps
            val sy = min(st.y0, st.y1) - st.wd
            if (sy > y0) tube(lx, y0, lx, sy, 1f)
            y0 = max(st.y0, st.y1) + st.wd
        }
        if (y0 < iB - 10f * dp) tube(lx, y0, lx, iB - 10f * dp, 1f)

        // ---- the straps ----
        for (st in straps) {
            val ang = kotlin.math.atan2(st.y1 - st.y0, st.x1 - st.x0)
            val nx = -sin(ang) * st.wd / 2f; val ny = cos(ang) * st.wd / 2f
            val poly = Path().apply {
                moveTo(st.x0 + nx, st.y0 + ny); lineTo(st.x1 + nx, st.y1 + ny)
                lineTo(st.x1 - nx, st.y1 - ny); lineTo(st.x0 - nx, st.y0 - ny); close()
            }
            val shadow = Path(poly); shadow.offset(1.2f * dp, 2f * dp)
            c.drawPath(shadow, Paint(Paint.ANTI_ALIAS_FLAG).apply { color = android.graphics.Color.argb(115, 0, 0, 0) })
            // steel, lit along its top edge and in shade along its bottom edge
            val steel = Paint(Paint.ANTI_ALIAS_FLAG).apply {
                shader = LinearGradient(0f, (st.y0 + st.y1) / 2f - st.wd / 2f, 0f, (st.y0 + st.y1) / 2f + st.wd / 2f,
                    intArrayOf(android.graphics.Color.rgb(196, 200, 206), android.graphics.Color.rgb(138, 142, 148), android.graphics.Color.rgb(84, 88, 94)),
                    floatArrayOf(0f, 0.45f, 1f), Shader.TileMode.CLAMP)
            }
            c.drawPath(poly, steel)
            // where it climbs onto the frame it faces the light; where it drops onto the glass it does not
            fun across(x: Float, color: Int, wd: Float) {
                val t = (x - st.x0) / (st.x1 - st.x0); val cy = st.y0 + (st.y1 - st.y0) * t
                c.drawLine(x, cy - st.wd / 2f, x, cy + st.wd / 2f, Paint(Paint.ANTI_ALIAS_FLAG).apply { this.color = color; strokeWidth = wd })
            }
            across(s.consoleL + 1.5f * dp, android.graphics.Color.argb(170, 240, 244, 248), 2.2f * dp)
            across(iL + 1.2f * dp, android.graphics.Color.argb(170, 20, 22, 26), 2.4f * dp)
            // brushing along its length
            val brush = Paint(Paint.ANTI_ALIAS_FLAG).apply { strokeWidth = 0.7f; color = android.graphics.Color.argb(40, 255, 255, 255) }
            for (k in 0 until 5) {
                val o = (k - 2) * st.wd / 6f
                c.drawLine(st.x0 + 3f * dp, st.y0 + o, st.x1 - 3f * dp, st.y1 + o, brush)
            }
            c.drawPath(poly, Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE; strokeWidth = 0.8f * dp; color = android.graphics.Color.rgb(48, 52, 56) })
        }
        // ---- fasteners, none of them matching ----
        for (f in fasteners) {
            val r = (if (f.kind == 'b') 4.6f else 3.6f) * dp
            if (f.kind == 'h') {
                c.drawCircle(f.x, f.y, 2.6f * dp, Paint(Paint.ANTI_ALIAS_FLAG).apply { color = android.graphics.Color.rgb(4, 8, 10) })
                c.drawCircle(f.x, f.y, 2.6f * dp, Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE; strokeWidth = 1f; color = android.graphics.Color.argb(160, 170, 230, 250) })
                continue
            }
            c.drawCircle(f.x + r * 0.30f, f.y + r * 0.38f, r * 1.05f, Paint(Paint.ANTI_ALIAS_FLAG).apply { color = android.graphics.Color.argb(115, 0, 0, 0) })
            c.drawCircle(f.x, f.y, r, Paint(Paint.ANTI_ALIAS_FLAG).apply {
                shader = RadialGradient(f.x - r * 0.32f, f.y - r * 0.32f, r * 1.1f,
                    intArrayOf(android.graphics.Color.rgb(238, 242, 245), android.graphics.Color.rgb(154, 163, 172), android.graphics.Color.rgb(58, 63, 69)),
                    floatArrayOf(0f, 0.5f, 1f), Shader.TileMode.CLAMP)
            })
            val line = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE; strokeWidth = 1.2f * dp; color = android.graphics.Color.argb(200, 40, 40, 40) }
            if (f.kind == 'b') {
                val hex = Path()
                for (i in 0..6) { val a = i * PI.toFloat() / 3f; val qx = f.x + cos(a) * r; val qy = f.y + sin(a) * r; if (i == 0) hex.moveTo(qx, qy) else hex.lineTo(qx, qy) }
                c.drawPath(hex, line)
            }
            if (f.kind == 's') { c.drawLine(f.x - r * 0.6f, f.y, f.x + r * 0.6f, f.y, line); c.drawLine(f.x, f.y - r * 0.6f, f.x, f.y + r * 0.6f, line) }
        }
        return bm
    }

    /** Two passes of this approximate a Gaussian. In place. */
    private fun boxBlur(a: FloatArray, w: Int, h: Int, r: Int) {
        val tmp = FloatArray(a.size)
        val n = (2 * r + 1).toFloat()
        for (y in 0 until h) {
            var acc = 0f
            for (x in -r..r) acc += a[y * w + x.coerceIn(0, w - 1)]
            for (x in 0 until w) {
                tmp[y * w + x] = acc / n
                acc += a[y * w + (x + r + 1).coerceAtMost(w - 1)] - a[y * w + (x - r).coerceAtLeast(0)]
            }
        }
        for (x in 0 until w) {
            var acc = 0f
            for (y in -r..r) acc += tmp[y.coerceIn(0, h - 1) * w + x]
            for (y in 0 until h) {
                a[y * w + x] = acc / n
                acc += tmp[(y + r + 1).coerceAtMost(h - 1) * w + x] - tmp[(y - r).coerceAtLeast(0) * w + x]
            }
        }
    }

    /** Smooth tone variation in -1..1: a coarse random grid, bilinearly filled in. */
    private fun smoothNoise(w: Int, h: Int, cell: Int, rnd: Random): FloatArray {
        val gw = w / cell + 2; val gh = h / cell + 2
        val g = FloatArray(gw * gh) { rnd.nextFloat() * 2f - 1f }
        return FloatArray(w * h) { i ->
            val x = (i % w).toFloat() / cell; val y = (i / w).toFloat() / cell
            val x0 = x.toInt(); val y0 = y.toInt(); val fx = x - x0; val fy = y - y0
            val a = g[y0 * gw + x0]; val b = g[y0 * gw + x0 + 1]; val c = g[(y0 + 1) * gw + x0]; val d = g[(y0 + 1) * gw + x0 + 1]
            (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy
        }
    }
}
