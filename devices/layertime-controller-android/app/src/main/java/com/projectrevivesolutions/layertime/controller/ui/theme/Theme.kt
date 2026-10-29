package com.projectrevivesolutions.layertime.controller.ui.theme

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.draw.drawWithContent
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Shadow
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.clipPath
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.lerp
import com.projectrevivesolutions.layertime.controller.R
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp

/**
 * LayerTime design-language palette, taken from the Garmin face
 * (devices/garmin-tactix/source/HomeView.mc). One system across targets.
 */
object LtColors {
    val Amber = Color(0xFFFFA020)
    val Ivory = Color(0xFFF5E6C8)
    val Cyan = Color(0xFF30D0FF)
    val Green = Color(0xFF60FF50)
    val Red = Color(0xFFFF4040)
    val Grey = Color(0xFFB4B4B4)   // lightened for small dark screens (was 0xFF8C8C8C)
    val Dim = Color(0xFF2E2E2E)
    val Panel = Color(0xFF121212)
    val Bg = Color(0xFF000000)
    // Data console: flat, bright, glowing, set against the dark bronze bezel.
    val ConsoleBg = Color(0xFF02060F)       // build 37: colder, toward blue-black
    val ConsoleSurface = Color(0xFF0B1A2C)  // build 37: colder steel-blue
    val ConsoleEdge = Color(0xFF6FE6FF)     // build 37: icier cyan
    val ConsoleGlow = Color(0xFF30D0FF)
    // Brushed-steel gradient stops (light, mid, dark, mid, light) for face and frame rims.
    val Steel = listOf(
        Color(0xFFCAD2DA), Color(0xFF8B939C), Color(0xFF40454B), Color(0xFF6E767E),
        Color(0xFFB9C1C9), Color(0xFF40454B), Color(0xFF8B939C), Color(0xFFCAD2DA),
    )
}

private val Scheme = darkColorScheme(
    primary = LtColors.Green,
    secondary = LtColors.Cyan,
    tertiary = LtColors.Amber,
    background = LtColors.Bg,
    surface = LtColors.Panel,
    onBackground = LtColors.Ivory,
    onSurface = LtColors.Ivory,
    onPrimary = LtColors.Bg,
    onSecondary = LtColors.Bg,
    onTertiary = LtColors.Bg,
)

@Composable
fun LayerTimeControllerTheme(darkTheme: Boolean = true, content: @Composable () -> Unit) {
    MaterialTheme(colorScheme = Scheme, content = content)
}

/** Width of the tube-shaded border around the console and its panels and cards (3.6 dp to build 34, 7.2 in build 35, 14.4 from build 36). */
val FrameEdge = 14.4.dp

/** Corner radius shared by every console panel, card, and button. */
val ConsoleShape = RoundedCornerShape(16.dp)
val ConsoleButtonShape = RoundedCornerShape(8.dp)

/**
 * The console frame: a one-pixel bright edge, a soft halo outside it, a tinted
 * drop shadow underneath, and a flat surface. Draw-only, so it costs nothing in
 * layout; the halo is three thin strokes of falling alpha just outside the edge.
 */
fun Modifier.consoleFrame(
    edge: Color = LtColors.ConsoleEdge,
    surface: Color = LtColors.ConsoleSurface,
    radius: Dp = 10.dp,
    elevation: Dp = 6.dp,
    halo: Boolean = true,
): Modifier = this
    .shadow(elevation, RoundedCornerShape(radius), clip = false, ambientColor = edge.copy(alpha = 0.7f), spotColor = edge)
    .drawBehind { if (halo) drawHalo(edge, radius.toPx()) }
    .background(surface, RoundedCornerShape(radius))
    .drawBehind { drawTube(edge, radius.toPx(), FrameEdge.toPx()) }
    .drawBehind {
        // inner glow just inside the edge, so the edge reads as lit
        val r = radius.toPx()
        for ((i, a) in listOf(0.22f, 0.12f, 0.06f).withIndex()) {
            val w = (i + 1) * 2.dp.toPx(); val inset = FrameEdge.toPx() + w / 2f
            drawRoundRect(edge.copy(alpha = a), Offset(inset, inset),
                Size(size.width - 2 * inset, size.height - 2 * inset),
                CornerRadius((r - inset).coerceAtLeast(0f)), style = Stroke(w))
        }
    }

/**
 * A thick edge shaded like a tube: dark at its inner and outer edges, the edge
 * colour through the body, and a bright core line, so the bar reads as rounded.
 */
fun androidx.compose.ui.graphics.drawscope.DrawScope.drawTube(edge: Color, r: Float, width: Float) {
    val dark = lerp(edge, Color.Black, 0.55f)
    val bright = lerp(edge, Color.White, 0.55f)
    fun ring(w: Float, c: Color) {
        val inset = width / 2f
        drawRoundRect(c, Offset(inset, inset), Size(size.width - 2 * inset, size.height - 2 * inset),
            CornerRadius((r - inset).coerceAtLeast(0f)), style = Stroke(w))
    }
    ring(width, dark)
    ring(width * 0.62f, edge)
    ring(width * 0.22f, bright)
}

/** A horizontal divider bar shaded top-to-bottom like a rounded rail. */
fun railBrush(edge: Color): Brush = Brush.verticalGradient(
    listOf(lerp(edge, Color.Black, 0.6f), lerp(edge, Color.White, 0.45f), edge, lerp(edge, Color.Black, 0.6f)))

/** Vertical shading for a domed button face. */
fun domeBrush(base: Color): Brush = Brush.verticalGradient(
    listOf(lerp(base, Color.White, 0.35f), base, lerp(base, Color.Black, 0.35f)))

/** Outer halo: overlapping bands of growing width and falling alpha, centred just outside the edge. */
private fun androidx.compose.ui.graphics.drawscope.DrawScope.drawHalo(edge: Color, r: Float) {
    for ((i, a) in listOf(0.55f, 0.32f, 0.18f, 0.10f, 0.05f).withIndex()) {
        val w = (i + 1) * 2.4.dp.toPx()
        val inset = -w / 2f
        drawRoundRect(edge.copy(alpha = a), Offset(inset, inset),
            Size(size.width - 2 * inset, size.height - 2 * inset),
            CornerRadius(r - inset), style = Stroke(w))
    }
}

/** Halo only, for buttons that draw their own surface and edge. */
fun Modifier.haloOnly(edge: Color, radius: Dp = 8.dp): Modifier =
    this.drawBehind { drawHalo(edge, radius.toPx()) }

/** Text style for console headings: the heading font with a glow behind the letters. */
fun glowText(color: Color = LtColors.ConsoleEdge): TextStyle =
    TextStyle(fontFamily = ConsoleFonts.heading, shadow = Shadow(color = color, offset = Offset(0f, 0f), blurRadius = 12f))

/**
 * Console font sets the operator cycles through with the FONT button. All bundled
 * fonts are SIL Open Font License (licences in assets/licenses). Set 0 is the
 * system font, the look before build 33.
 */
object ConsoleFonts {
    private val orbitron = FontFamily(Font(R.font.lt_orbitron_semibold), Font(R.font.lt_orbitron_heavy, androidx.compose.ui.text.font.FontWeight.Bold))
    private val rajdhani = FontFamily(Font(R.font.lt_rajdhani_semibold), Font(R.font.lt_rajdhani_bold, androidx.compose.ui.text.font.FontWeight.Bold))
    private val exo2 = FontFamily(Font(R.font.lt_exo2_semibold), Font(R.font.lt_exo2_heavy, androidx.compose.ui.text.font.FontWeight.Bold))
    private val techMono = FontFamily(Font(R.font.lt_sharetechmono))

    data class Set(val name: String, val heading: FontFamily, val body: FontFamily, val mono: FontFamily)
    val sets = listOf(
        Set("SYSTEM", FontFamily.Default, FontFamily.Default, FontFamily.Monospace),
        Set("ORBITRON + RAJDHANI", orbitron, rajdhani, techMono),
        Set("ORBITRON", orbitron, orbitron, techMono),
        Set("EXO 2", exo2, exo2, techMono),
        Set("RAJDHANI", rajdhani, rajdhani, techMono),
    )
    var index by mutableIntStateOf(1)
    val current: Set get() = sets[index.coerceIn(0, sets.size - 1)]
    val heading: FontFamily get() = current.heading
    val body: FontFamily get() = current.body
    val mono: FontFamily get() = current.mono
    fun next() { index = (index + 1) % sets.size }
}

// ---- HUD console (build 34) ----

/** Pill shape for console buttons. */
val PillShape = RoundedCornerShape(percent = 50)

/**
 * A HUD panel: the console frame (tube edge, halo, shadow) over a surface that
 * falls from lit at the top to dark at the bottom with a faint targeting grid,
 * and bright L-shaped brackets glowing at the four corners.
 */
fun Modifier.hudPanel(
    edge: Color = LtColors.ConsoleEdge,
    radius: Dp = 18.dp,
    elevation: Dp = 6.dp,
    halo: Boolean = true,
): Modifier = this
    .shadow(elevation, RoundedCornerShape(radius), clip = false, ambientColor = edge.copy(alpha = 0.7f), spotColor = edge)
    .drawBehind {
        if (halo) drawHalo(edge, radius.toPx())
        val r = radius.toPx()
        // surface: lit at the top, dark at the bottom
        drawRoundRect(
            Brush.verticalGradient(listOf(lerp(LtColors.ConsoleSurface, edge, 0.16f), LtColors.ConsoleSurface,
                lerp(LtColors.ConsoleSurface, Color.Black, 0.5f))),
            cornerRadius = CornerRadius(r))
        // targeting grid
        val step = 16.dp.toPx()
        val gc = lerp(edge, Color(0xFFBFEFFF), 0.4f).copy(alpha = 0.10f)
        val clip = androidx.compose.ui.graphics.Path().apply {
            addRoundRect(androidx.compose.ui.geometry.RoundRect(0f, 0f, size.width, size.height, CornerRadius(r)))
        }
        clipPath(clip) {
            var x = step
            while (x < size.width) { drawLine(gc, Offset(x, 0f), Offset(x, size.height), 1f); x += step }
            var y = step
            while (y < size.height) { drawLine(gc, Offset(0f, y), Offset(size.width, y), 1f); y += step }
        }
        drawTube(edge, r, FrameEdge.toPx())
        for ((i, a) in listOf(0.22f, 0.12f, 0.06f).withIndex()) {
            val w = (i + 1) * 2.dp.toPx(); val inset = FrameEdge.toPx() + w / 2f
            drawRoundRect(edge.copy(alpha = a), Offset(inset, inset),
                Size(size.width - 2 * inset, size.height - 2 * inset),
                CornerRadius((r - inset).coerceAtLeast(0f)), style = Stroke(w))
        }
    }
    .drawWithContent { drawContent(); drawCornerBrackets(edge, radius.toPx()) }

/** Bright L brackets over each corner of the frame, with a soft glow under them. */
fun androidx.compose.ui.graphics.drawscope.DrawScope.drawCornerBrackets(edge: Color, r: Float) {
    val len = 32.dp.toPx().coerceAtMost(minOf(size.width, size.height) / 3f)
    val w = 12.dp.toPx()
    val bright = lerp(edge, Color.White, 0.65f)
    val o = w / 2f
    val corners = listOf(
        Triple(Offset(o, o), 1f, 1f), Triple(Offset(size.width - o, o), -1f, 1f),
        Triple(Offset(o, size.height - o), 1f, -1f), Triple(Offset(size.width - o, size.height - o), -1f, -1f),
    )
    for ((p, sx, sy) in corners) {
        val path = androidx.compose.ui.graphics.Path().apply {
            moveTo(p.x + sx * len, p.y)
            lineTo(p.x + sx * r * 0.45f, p.y)
            quadraticBezierTo(p.x, p.y, p.x, p.y + sy * r * 0.45f)
            lineTo(p.x, p.y + sy * len)
        }
        drawPath(path, edge.copy(alpha = 0.35f), style = Stroke(w * 3.2f, cap = androidx.compose.ui.graphics.StrokeCap.Round))
        drawPath(path, edge, style = Stroke(w, cap = androidx.compose.ui.graphics.StrokeCap.Round))
        drawPath(path, bright, style = Stroke(w * 0.4f, cap = androidx.compose.ui.graphics.StrokeCap.Round))
    }
}

/** A rail under a panel title: tube-shaded bar with tick marks every 8 dp and a bright end cap. */
fun Modifier.hudRail(edge: Color = LtColors.ConsoleEdge): Modifier = this.drawBehind {
    val h = size.height
    drawRoundRect(railBrush(edge), Offset.Zero, Size(size.width, h), CornerRadius(h / 2f))
    val step = 8.dp.toPx(); var x = step
    var i = 0
    while (x < size.width - step) {
        val tall = i % 4 == 0
        drawLine(lerp(edge, Color.White, 0.5f).copy(alpha = if (tall) 0.9f else 0.45f),
            Offset(x, if (tall) -h * 0.9f else 0f), Offset(x, h), 1.2.dp.toPx())
        x += step; i++
    }
    drawCircle(lerp(edge, Color.White, 0.7f), h * 0.9f, Offset(size.width - h, h / 2f))
}

/** Glass fill for an outlined pill button: lit at the top, edge-tinted, with a specular line. */
fun Modifier.glassPill(edge: Color, enabled: Boolean): Modifier = this.drawBehind {
    val r = size.height / 2f
    drawRoundRect(
        Brush.verticalGradient(
            if (enabled) listOf(edge.copy(alpha = 0.30f), edge.copy(alpha = 0.08f), Color.Black.copy(alpha = 0.35f))
            else listOf(Color(0xFF1A2733), Color(0xFF0E161E))),
        cornerRadius = CornerRadius(r))
    if (enabled) {
        val inset = size.height * 0.16f
        drawRoundRect(Color.White.copy(alpha = 0.22f), Offset(r * 0.6f, inset), Size(size.width - r * 1.2f, size.height * 0.10f),
            CornerRadius(size.height * 0.05f))
    }
}

/** Specular highlight across the top of a filled (primary) pill. */
fun Modifier.pillShine(): Modifier = this.drawWithContent {
    drawContent()
    val r = size.height / 2f
    drawRoundRect(Color.White.copy(alpha = 0.35f), Offset(r * 0.6f, size.height * 0.14f),
        Size(size.width - r * 1.2f, size.height * 0.12f), CornerRadius(size.height * 0.06f))
}

/** A recessed readout well: dark inset with an inner shadow along its top edge and a lit rim. */
fun Modifier.readoutWell(edge: Color = LtColors.ConsoleEdge): Modifier = this.drawBehind {
    val r = 6.dp.toPx()
    drawRoundRect(Color(0xFF02060A), cornerRadius = CornerRadius(r))
    drawRoundRect(Brush.verticalGradient(listOf(Color.Black.copy(alpha = 0.8f), Color.Transparent), endY = size.height * 0.35f),
        cornerRadius = CornerRadius(r))
    drawRoundRect(edge.copy(alpha = 0.55f), cornerRadius = CornerRadius(r), style = Stroke(1.2.dp.toPx()))
}
