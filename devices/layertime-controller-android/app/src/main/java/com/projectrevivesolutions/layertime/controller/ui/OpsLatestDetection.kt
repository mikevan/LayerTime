package com.projectrevivesolutions.layertime.controller.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.WifiTethering
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

/** A passive summary of the actual session mirror, including its empty state. */
@Composable
fun OpsLatestDetection(detector:String?,count:Int) {
    val gold=Color(0xFFFFD431)
    val ink=Color(0xFFE9ECEF)
    val style=TextStyle(fontFamily=FontFamily.SansSerif,fontSize=11.sp,lineHeight=14.sp)
    OpsConnectionHousing(Modifier.fillMaxWidth(),compact=true) {
        Canvas(Modifier.matchParentSize()) {
            val inset=9.dp.toPx()
            val cut=6.dp.toPx().coerceAtMost((size.height-2*inset).coerceAtLeast(0f)/2)
            val right=size.width-inset
            val bottom=size.height-inset
            val window=Path().apply {
                moveTo(inset+cut,inset);lineTo(right-cut,inset)
                lineTo(right,inset+cut);lineTo(right,bottom-cut)
                lineTo(right-cut,bottom);lineTo(inset+cut,bottom)
                lineTo(inset,bottom-cut);lineTo(inset,inset+cut);close()
            }
            drawPath(window,Brush.horizontalGradient(listOf(Color(0x77483908),Color(0x66261E08),Color(0x443B310D))))
            drawPath(window,gold.copy(alpha=.12f),style=Stroke(2.dp.toPx()))
            drawPath(window,gold.copy(alpha=.65f),style=Stroke(.5.dp.toPx()))
            // Short amber relief marks echo the instrument headers without competing with data.
            repeat(3) { i ->
                val x=size.width-21.dp.toPx()-i*3.dp.toPx()
                drawLine(gold.copy(alpha=.3f),Offset(x,bottom-5.dp.toPx()),
                    Offset(x+2.dp.toPx(),bottom-8.dp.toPx()),.8.dp.toPx())
            }
        }
        Row(Modifier.fillMaxWidth().padding(horizontal=18.dp,vertical=15.dp),
            verticalAlignment=Alignment.CenterVertically,horizontalArrangement=Arrangement.spacedBy(9.dp)) {
            Icon(Icons.Outlined.WifiTethering,null,tint=gold,modifier=Modifier.size(17.dp))
            Text("Latest detection",color=ink,style=style,modifier=Modifier.weight(1f))
            Text("|",color=ink.copy(alpha=.7f),style=style)
            Text(detector?.let { "$it / $count detections" } ?: "No detections yet",
                color=if(detector!=null)gold else Color(0xFFB8AD8B),style=style,
                modifier=Modifier.weight(1.35f))
        }
    }
}
