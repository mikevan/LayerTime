package com.projectrevivesolutions.layertime.controller.ui.theme

import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.clipPath
import androidx.compose.ui.unit.dp

/** Scoped to the phone/tablet shell; the hardware watch retains its original materials. */
val LocalSpacecraft = staticCompositionLocalOf { false }

fun Modifier.spacecraftPanel(edge: Color = LtColors.ConsoleEdge): Modifier = drawBehind {
    val cut = 14.dp.toPx().coerceAtMost(size.minDimension / 4)
    fun outline(inset: Float): Path = Path().apply {
        val l = inset; val t = inset; val r = size.width - inset; val b = size.height - inset
        moveTo(l + cut, t); lineTo(r - cut, t); lineTo(r, t + cut)
        lineTo(r, b - cut); lineTo(r - cut, b); lineTo(l + cut, b)
        lineTo(l, b - cut); lineTo(l, t + cut); close()
    }
    val hull = outline(1.dp.toPx())
    drawPath(hull, Brush.verticalGradient(listOf(Color(0xFF223B61), Color(0xFF0B142B), Color(0xFF162C49))))
    drawPath(hull, Color(0xFF365578), style = Stroke(1.dp.toPx()))
    val glass = outline(7.dp.toPx())
    drawPath(glass, Brush.verticalGradient(listOf(Color(0xFF091D2B), Color(0xFF030B16))))
    clipPath(glass) {
        val step = 20.dp.toPx()
        var x = step
        while (x < size.width) {
            drawLine(edge.copy(alpha = .035f), Offset(x, 0f), Offset(x, size.height)); x += step
        }
        var y = step
        while (y < size.height) {
            drawLine(edge.copy(alpha = .035f), Offset(0f, y), Offset(size.width, y)); y += step
        }
        drawPath(glass, Brush.linearGradient(listOf(Color.White.copy(alpha = .035f), Color.Transparent)), style = Stroke(12.dp.toPx()))
    }
    drawPath(glass, edge.copy(alpha = .28f), style = Stroke(.7.dp.toPx()))
    // Light channels are inset in the housing, leaving the dark facets visible.
    val inset = 5.dp.toPx()
    for (bottom in listOf(false, true)) {
        val y = if (bottom) size.height - inset else inset
        val start = if (bottom) size.width * .65f else size.width * .12f
        val end = start + size.width * .18f
        drawLine(edge.copy(alpha = .08f), Offset(start, y), Offset(end, y), 8.dp.toPx())
        drawLine(edge.copy(alpha = .25f), Offset(start, y), Offset(end, y), 4.dp.toPx())
        drawLine(edge, Offset(start, y), Offset(end, y), 1.4.dp.toPx())
    }
    for (x in listOf(12.dp.toPx(), size.width - 12.dp.toPx())) {
        drawCircle(Color(0xFF030916), 2.dp.toPx(), Offset(x, size.height / 2))
        drawLine(Color(0xFF304666), Offset(x - 1.dp.toPx(), size.height / 2), Offset(x + 1.dp.toPx(), size.height / 2), .6.dp.toPx())
    }
}
