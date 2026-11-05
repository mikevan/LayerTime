package com.projectrevivesolutions.layertime.controller.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.Image
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.imageResource
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.IntSize
import androidx.compose.ui.unit.dp
import com.projectrevivesolutions.layertime.controller.R

/** Photographic case over a separate circular live dial. Decorative art never handles input. */
@Composable
fun OpsWatchAssembly(modifier: Modifier, face: @Composable (Modifier) -> Unit) {
    val art = ImageBitmap.imageResource(R.drawable.ops_watch_case_v2)
    BoxWithConstraints(modifier) {
        val side = maxWidth.coerceAtMost(maxHeight)
        // Measured source alpha aperture: (619,552), rx=401, ry=410 in 1254px.
        // Correct generated vertical distortion; the live dial always stays square.
        val artHeight = side * .978f
        val dial = side * .653f
        val cx = side * .494f
        val cy = side * .441f
        Canvas(Modifier.fillMaxSize()) {
            val r = size.minDimension * .407f
            for (i in 10 downTo 1) drawCircle(Color.Black.copy(alpha = .025f), r + i * 1.5.dp.toPx(),
                Offset(size.width*.51f, size.height*.46f))
            // Brass bracket and jumper sit above the title plate, with a local contact shadow.
            val u=size.minDimension
            for(i in 8 downTo 1) {
                val spread=i*.003f*u
                drawOval(Color.Black.copy(alpha=.045f),
                    Offset(u*.265f-spread,u*.916f-spread*.5f),
                    Size(u*.22f+spread*2,u*.045f+spread))
                drawOval(Color.Black.copy(alpha=.025f),
                    Offset(u*.77f-spread,u*.91f-spread*.5f),
                    Size(u*.18f+spread*2,u*.04f+spread))
            }
        }
        Box(Modifier.offset(x = cx-dial/2, y = cy-dial/2).size(dial).clip(CircleShape)) {
            face(Modifier.fillMaxSize())
        }
        Image(art, contentDescription = null, contentScale = ContentScale.FillBounds,
            modifier = Modifier.offset(y = side*.011f).size(side,artHeight))
        Canvas(Modifier.fillMaxSize()) {
            drawCircle(Color(0xFF080B0D),size.minDimension*.320f,
                Offset(size.width*.494f,size.height*.441f),style=Stroke(size.minDimension*.003f))
        }
    }
}

/** Nine-slice housing: corners retain proportions as live telemetry changes panel height. */
@Composable
fun OpsTelemetryHousing(modifier: Modifier, compact: Boolean = false, content: @Composable BoxScope.() -> Unit) {
    val image = ImageBitmap.imageResource(R.drawable.ops_telemetry_housing_v2)
    Box(modifier) {
        // A decorative background follows the content's measured size; it must not
        // participate in measurement and consume all remaining screen height.
        Canvas(Modifier.matchParentSize()) {
            val sx = intArrayOf(0,(image.width*.20f).toInt(),(image.width*.80f).toInt(),image.width)
            val sy = intArrayOf(0,(image.height*.20f).toInt(),(image.height*.80f).toInt(),image.height)
            val corner = (if(compact) 14.dp else 30.dp).toPx().coerceAtMost(size.minDimension/3)
            val dx = intArrayOf(0,corner.toInt(),(size.width-corner).toInt(),size.width.toInt())
            val dy = intArrayOf(0,corner.toInt(),(size.height-corner).toInt(),size.height.toInt())
            for (y in 0..2) for(x in 0..2) drawImage(image,
                srcOffset=IntOffset(sx[x],sy[y]),srcSize=IntSize(sx[x+1]-sx[x],sy[y+1]-sy[y]),
                dstOffset=IntOffset(dx[x],dy[y]),dstSize=IntSize(dx[x+1]-dx[x],dy[y+1]-dy[y]))
        }
        content()
    }
}

/** Landscape material with constant-size corners and proportional illuminated rail sections. */
@Composable
fun OpsConnectionHousing(modifier: Modifier, compact: Boolean = false, content: @Composable BoxScope.() -> Unit) {
    val image=ImageBitmap.imageResource(R.drawable.ops_connection_housing_v1)
    Box(modifier) {
        Canvas(Modifier.matchParentSize()) {
            val sx=intArrayOf(0,(image.width*.1f).toInt(),(image.width*.9f).toInt(),image.width)
            val sy=intArrayOf(0,(image.height*.25f).toInt(),(image.height*.75f).toInt(),image.height)
            val corner=(if(compact)14.dp else 30.dp).toPx().coerceAtMost(size.minDimension/3)
            val dx=intArrayOf(0,corner.toInt(),(size.width-corner).toInt(),size.width.toInt())
            val dy=intArrayOf(0,corner.toInt(),(size.height-corner).toInt(),size.height.toInt())
            for(y in 0..2) for(x in 0..2) drawImage(image,
                srcOffset=IntOffset(sx[x],sy[y]),srcSize=IntSize(sx[x+1]-sx[x],sy[y+1]-sy[y]),
                dstOffset=IntOffset(dx[x],dy[y]),dstSize=IntSize(dx[x+1]-dx[x],dy[y+1]-dy[y]))
        }
        content()
    }
}
