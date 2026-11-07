package com.projectrevivesolutions.layertime.controller.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.size
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Layers
import androidx.compose.material3.Icon
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.unit.Dp

/**
 * The tools on the Tools page, with the icon each one shows there (Michael, 2026-11-06:
 * the Builder uses the same icons). One definition, so the Tools page, the Builder's
 * Tools Palette, and a tool step on the workspace always match.
 */
enum class LayerTool(val id: String, val title: String, val description: String) {
    WIRESHARK("wireshark", "WIRESHARK", "Decode a capture file."),
    ATTACK_TIMELINE("attack_timeline", "ATTACK TIMELINE", "Inspect frames by transmitter.");

    /** The Material icon for a tool drawn as an icon, or null for the Wireshark fin, which is drawn. */
    val vector: ImageVector? get() = if (this == ATTACK_TIMELINE) Icons.Outlined.Layers else null

    companion object {
        fun of(id: String?): LayerTool? = values().firstOrNull { it.id == id }
    }
}

/** The Wireshark fin, as on the Tools page, in a square of [side] px at [topLeft]. */
fun DrawScope.drawWiresharkFin(topLeft: Offset, side: Float, tint: Color, strokePx: Float) {
    val fin = Path().apply {
        moveTo(topLeft.x + side * .08f, topLeft.y + side * .84f)
        cubicTo(topLeft.x + side * .12f, topLeft.y + side * .48f, topLeft.x + side * .42f, topLeft.y + side * .16f, topLeft.x + side * .8f, topLeft.y + side * .13f)
        cubicTo(topLeft.x + side * .59f, topLeft.y + side * .41f, topLeft.x + side * .60f, topLeft.y + side * .66f, topLeft.x + side * .87f, topLeft.y + side * .84f)
        close()
    }
    drawPath(fin, tint.copy(alpha = .09f))
    drawPath(fin, tint, style = Stroke(strokePx))
}

/** A tool's icon at [size], the same one the Tools page shows. */
@Composable
fun ToolIcon(tool: LayerTool, size: Dp, tint: Color) {
    val v = tool.vector
    if (v != null) Icon(v, null, tint = tint, modifier = Modifier.size(size))
    else Canvas(Modifier.size(size)) { drawWiresharkFin(Offset.Zero, this.size.minDimension, tint, (size.value / 28f * 1.2f) * density) }
}
