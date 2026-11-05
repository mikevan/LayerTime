package com.projectrevivesolutions.layertime.controller.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

private val FieldInk=Color(0xFFE7EEF2)
private val FieldMuted=Color(0xFFB1BCC6)
private val FieldCyan=Color(0xFF64E6FF)
private val FieldGold=Color(0xFFFFD431)
// Explicit line metrics: inherited Material body line height inflated the compact readouts.
private val FieldType=TextStyle(fontFamily=FontFamily.SansSerif,fontSize=9.sp,lineHeight=11.sp)

/** Reference composition: title, link light, ruled device/sensor rows, amber count, latest badge. */
@Composable
fun FieldStatusPanel(
    modifier: Modifier,
    minHeight: Dp,
    online: Boolean,
    stale: Boolean,
    connecting: Boolean,
    device: String,
    sensor: String,
    detections: Int,
    latest: String?,
) {
    val signal=when { online && stale -> FieldGold; online -> Color(0xFF8FE326); connecting -> FieldCyan; else -> FieldMuted }
    OpsTelemetryHousing(modifier) {
        Column(Modifier.fillMaxWidth().heightIn(min=minHeight)
            .padding(start=18.dp,end=16.dp,top=16.dp,bottom=14.dp),
            verticalArrangement=Arrangement.spacedBy(3.dp)) {
            Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically) {
                Text("FIELD STATUS",color=FieldInk,
                    style=FieldType.copy(fontSize=10.5.sp,lineHeight=12.sp,letterSpacing=.4.sp,fontWeight=FontWeight.Medium),
                    modifier=Modifier.weight(1f))
                Canvas(Modifier.size(13.dp,5.dp)) {
                    repeat(3) { i ->
                        val x=i*size.width/3
                        drawLine(FieldCyan.copy(alpha=.65f),Offset(x,size.height),Offset(x+size.width*.22f,0f),1.dp.toPx())
                    }
                }
            }
            Row(Modifier.heightIn(min=18.dp),verticalAlignment=Alignment.CenterVertically,
                horizontalArrangement=Arrangement.spacedBy(6.dp)) {
                Box(Modifier.size(6.dp).background(signal,CircleShape))
                Text(when { stale && online -> "Link stale"; online -> "Hunter online"; connecting -> "Connecting…"; else -> "Hunter offline" },
                    color=signal,style=FieldType.copy(fontSize=10.sp,lineHeight=13.sp))
            }
            FieldRule()
            FieldRow("Device",device.ifBlank { "—" },if(online)FieldInk else FieldMuted)
            FieldRule()
            Row(Modifier.fillMaxWidth().heightIn(min=20.dp),verticalAlignment=Alignment.CenterVertically,
                horizontalArrangement=Arrangement.spacedBy(5.dp)) {
                Text("Active sensor",color=FieldMuted,style=FieldType,modifier=Modifier.weight(1f))
                val shape=RoundedCornerShape(11.dp)
                Text(if(online)sensor else "—",color=if(online)FieldInk else FieldMuted,style=FieldType,
                    modifier=Modifier.weight(1.1f,fill=false)
                        .background(Brush.verticalGradient(listOf(Color(0xAA174356),Color(0x99112633))),shape)
                        .border(.6.dp,FieldCyan.copy(alpha=.7f),shape)
                        .padding(horizontal=8.dp,vertical=2.dp))
            }
            val shape=RoundedCornerShape(5.dp)
            Row(Modifier.fillMaxWidth()
                .background(Brush.horizontalGradient(listOf(Color(0x884B3B06),Color(0x66231F0B),Color(0x77493606))),shape)
                .border(.6.dp,FieldGold.copy(alpha=.75f),shape).padding(horizontal=7.dp,vertical=4.dp),
                verticalAlignment=Alignment.CenterVertically,horizontalArrangement=Arrangement.spacedBy(8.dp)) {
                FieldRadioGlyph()
                Box(Modifier.size(width=.5.dp,height=13.dp).background(FieldGold.copy(alpha=.55f)))
                Text("$detections detections",color=FieldGold,
                    style=FieldType.copy(fontSize=12.sp,lineHeight=15.sp),modifier=Modifier.weight(1f))
            }
            Row(Modifier.fillMaxWidth().heightIn(min=20.dp),verticalAlignment=Alignment.CenterVertically,
                horizontalArrangement=Arrangement.spacedBy(5.dp)) {
                Text("Last alert",color=FieldMuted,style=FieldType,modifier=Modifier.weight(1f))
                // The caller resolves the Hunter's actual lastAlertEventId, not the latest detection.
                val badge=RoundedCornerShape(12.dp)
                Text(latest ?: "—",color=if(latest!=null)FieldGold else FieldMuted,style=FieldType,
                    modifier=Modifier.weight(1.1f,fill=false)
                        .background(if(latest!=null)Color(0x663C310B) else Color(0x220B1822),badge)
                        .border(.5.dp,if(latest!=null)FieldGold.copy(alpha=.35f) else FieldMuted.copy(alpha=.15f),badge)
                        .padding(horizontal=7.dp,vertical=3.dp))
            }
        }
    }
}

@Composable
private fun FieldRule() {
    Box(Modifier.fillMaxWidth().height(.5.dp).background(Brush.horizontalGradient(
        listOf(FieldMuted.copy(alpha=.12f),FieldMuted.copy(alpha=.5f),FieldMuted.copy(alpha=.12f)))))
}

@Composable
private fun FieldRow(label:String,value:String,color:Color) {
    Row(Modifier.fillMaxWidth().heightIn(min=18.dp),verticalAlignment=Alignment.CenterVertically,
        horizontalArrangement=Arrangement.spacedBy(5.dp)) {
        Text(label,color=FieldMuted,style=FieldType,modifier=Modifier.weight(1f))
        Text(value,color=color,style=FieldType,modifier=Modifier.weight(1.4f))
    }
}

@Composable
private fun FieldRadioGlyph() {
    Canvas(Modifier.size(14.dp)) {
        val c=center
        drawCircle(FieldGold,1.2.dp.toPx(),c)
        drawLine(FieldGold,Offset(c.x,c.y),Offset(c.x,size.height*.88f),1.dp.toPx())
        for(f in listOf(.55f,.9f)) {
            val r=size.minDimension*.5f*f
            for(a in listOf(-52f,128f)) drawArc(FieldGold,a,104f,false,
                Offset(c.x-r,c.y-r),Size(2*r,2*r),style=Stroke(.8.dp.toPx()))
        }
    }
}
