package com.projectrevivesolutions.layertime.controller.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.border
import androidx.compose.foundation.selection.toggleable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.CalendarMonth
import androidx.compose.material.icons.outlined.Layers
import androidx.compose.material.icons.outlined.Laptop
import androidx.compose.material.icons.outlined.Link
import androidx.compose.material.icons.outlined.LinkOff
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.projectrevivesolutions.layertime.controller.link.ConnNotice

private val OpsInk=Color(0xFFE5EDF1)
private val OpsMuted=Color(0xFF9FB0BB)
private val OpsCyan=Color(0xFF77E8FA)
private val OpsGreen=Color(0xFF8EE421)
private val OpsAmber=Color(0xFFFFCD43)
private val OpsType=TextStyle(fontFamily=FontFamily.SansSerif,fontSize=11.sp,lineHeight=14.sp)

@Composable
fun OpsLinkStrip(online:Boolean,connecting:Boolean,stale:Boolean,events:Int) {
    OpsConnectionHousing(Modifier.fillMaxWidth(), compact = true) {
        Row(Modifier.fillMaxWidth().padding(horizontal=18.dp,vertical=12.dp),
            verticalAlignment=Alignment.CenterVertically,horizontalArrangement=Arrangement.spacedBy(7.dp)) {
            Icon(Icons.Outlined.Layers,null,tint=OpsInk,modifier=Modifier.size(17.dp))
            Text("LAYERSCOPE",color=OpsInk,style=OpsType.copy(fontSize=10.sp,letterSpacing=.4.sp))
            Spacer(Modifier.weight(1f))
            val color=when { online && stale -> OpsAmber; online -> OpsGreen; connecting -> OpsCyan; else -> OpsMuted }
            Box(Modifier.size(5.dp).background(color,CircleShape))
            Text(when {online && stale -> "Link stale"; online -> "Link online"; connecting -> "Linking…"; else -> "Link offline"},
                color=color,style=OpsType.copy(fontSize=10.sp))
            Spacer(Modifier.weight(1f))
            Icon(Icons.Outlined.CalendarMonth,null,tint=OpsMuted,modifier=Modifier.size(15.dp))
            Text("$events events",color=OpsInk,style=OpsType.copy(fontSize=10.sp))
        }
    }
}

@OptIn(ExperimentalLayoutApi::class)
@Composable
fun OpsConnectionPanel(
    online:Boolean, connecting:Boolean, canDisconnect:Boolean, device:String,
    metadata:String?, notice:ConnNotice.Notice?, swipeDisconnects:Boolean,
    onSwipeChange:(Boolean)->Unit,onConnect:()->Unit,onDisconnect:()->Unit,
) {
    OpsConnectionHousing(Modifier.fillMaxWidth()) {
        Column(Modifier.fillMaxWidth().padding(start=18.dp,end=18.dp,top=15.dp,bottom=if(metadata!=null)23.dp else 15.dp),verticalArrangement=Arrangement.spacedBy(5.dp)) {
            Row(verticalAlignment=Alignment.CenterVertically,horizontalArrangement=Arrangement.spacedBy(8.dp)) {
                Box(Modifier.size(6.dp).background(if(online)OpsGreen else OpsMuted,CircleShape))
                Text(if(online) "1 CONNECTION" else "NO CONNECTION",color=OpsInk,style=OpsType.copy(fontSize=13.sp,lineHeight=16.sp))
                Spacer(Modifier.weight(1f))
                Text("///",color=OpsCyan.copy(alpha=.65f),style=OpsType.copy(letterSpacing=1.sp))
            }
            OpsRule()
            Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.Bottom,horizontalArrangement=Arrangement.spacedBy(7.dp)) {
                Icon(Icons.Outlined.Laptop,null,tint=OpsInk,modifier=Modifier.size(18.dp))
                Text(device.substringBefore(" / ").ifBlank { "No Hunter selected" },color=if(online)OpsInk else OpsMuted,
                    style=OpsType.copy(fontSize=11.sp,lineHeight=14.sp),maxLines=1,
                    modifier=Modifier.weight(1f).alignByBaseline())
                device.substringAfter(" / ","").takeIf {it.isNotBlank()}?.let {
                    Text("|",color=OpsMuted,style=OpsType,modifier=Modifier.alignByBaseline())
                    Text("MAC $it",color=OpsMuted,style=OpsType.copy(fontFamily=FontFamily.Monospace,fontSize=9.sp,lineHeight=14.sp),
                        maxLines=1,modifier=Modifier.alignByBaseline())
                }
            }
            notice?.let { Text(it.text,color=if(it.tone==ConnNotice.Tone.PROBLEM)OpsAmber else OpsCyan,style=OpsType) }
            Row(Modifier.fillMaxWidth(),horizontalArrangement=Arrangement.spacedBy(8.dp)) {
                OpsGlassAction("Scan & connect",Icons.Outlined.Link,!online && !connecting,Modifier.weight(1f),onConnect)
                OpsGlassAction("Disconnect",Icons.Outlined.LinkOff,canDisconnect,Modifier.weight(1f),onDisconnect)
            }
            Row(Modifier.fillMaxWidth().heightIn(min=26.dp)
                .toggleable(value=swipeDisconnects,role=Role.Switch,onValueChange=onSwipeChange),verticalAlignment=Alignment.CenterVertically,
                horizontalArrangement=Arrangement.spacedBy(8.dp)) {
                Text("Swipe away disconnects",color=OpsInk,style=OpsType.copy(fontSize=12.sp,lineHeight=15.sp),modifier=Modifier.weight(1f))
                Text(if(swipeDisconnects)"ON" else "OFF",color=OpsMuted,style=OpsType.copy(fontSize=11.sp,lineHeight=14.sp))
                OpsCompactSwitch(swipeDisconnects)
            }
            metadata?.let {
                OpsRule()
                val sections=it.split('|').map { section -> section.trim() }
                Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically) {
                    sections.forEachIndexed { index, section ->
                        if(index>0) Text("|",color=OpsMuted,style=OpsType.copy(fontSize=11.sp,lineHeight=14.sp))
                        Box(Modifier.weight(1f).padding(horizontal=3.dp),contentAlignment=Alignment.Center) {
                            Text(section,color=OpsMuted,style=OpsType.copy(fontSize=11.sp,lineHeight=14.sp),
                                textAlign=TextAlign.Center)
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun OpsGlassAction(label:String,icon:ImageVector,enabled:Boolean,modifier:Modifier,onClick:()->Unit) {
    val shape=RoundedCornerShape(7.dp)
    // Keep a 48dp interaction slot while the visible glass key follows the slim reference.
    Box(modifier.heightIn(min=40.dp),contentAlignment=Alignment.Center) {
    OutlinedButton(onClick=onClick,enabled=enabled,shape=shape,
        modifier=Modifier.fillMaxWidth().height(32.dp)
            .drawBehind {
                // Material reserves extra touch space: shade only the visible 32dp key.
                val h=32.dp.toPx().coerceAtMost(size.height)
                val y=(size.height-h)/2
                drawRoundRect(Brush.verticalGradient(if(enabled)listOf(Color(0xFF174556),Color(0xFF071A26))
                    else listOf(Color(0xFF24333D),Color(0xFF12212C)),startY=y,endY=y+h),
                    Offset(0f,y),Size(size.width,h),CornerRadius(7.dp.toPx()))
            },
        border=androidx.compose.foundation.BorderStroke(if(enabled)1.dp else .7.dp,
            if(enabled)OpsCyan else Color(0xFF526773)),
        contentPadding=PaddingValues(horizontal=7.dp,vertical=4.dp),
        colors=ButtonDefaults.outlinedButtonColors(contentColor=OpsInk,disabledContentColor=Color(0xFF8C9DA7))) {
        Icon(icon,null,modifier=Modifier.size(17.dp))
        Spacer(Modifier.width(5.dp))
        Text(label,style=OpsType.copy(fontSize=11.sp,lineHeight=14.sp),fontWeight=FontWeight.Medium)
    }
    }
}

/** The visible track matches the 12sp label's capital height; the entire row toggles it. */
@Composable
private fun OpsCompactSwitch(checked:Boolean) {
    val letterHeight=with(LocalDensity.current) { 12.sp.toDp()*.73f }
    Canvas(Modifier.size(width=letterHeight*3.1f,height=letterHeight)) {
        val r=size.height/2
        drawRoundRect(Brush.verticalGradient(if(checked)listOf(Color(0xFF397889),Color(0xFF183D4C))
            else listOf(Color(0xFF49616C),Color(0xFF233D49))),cornerRadius=CornerRadius(r))
        drawRoundRect(if(checked)OpsCyan.copy(alpha=.8f) else Color(0xFF68838E),
            cornerRadius=CornerRadius(r),style=Stroke(.5.dp.toPx()))
        val cx=if(checked)size.width-r else r
        drawCircle(Brush.radialGradient(listOf(Color.White,Color(0xFFCDD6DA)),
            center=Offset(cx-r*.2f,r*.7f),radius=r*1.3f),r*.78f,Offset(cx,r))
    }
}

@Composable
private fun OpsRule() {
    Box(Modifier.fillMaxWidth().height(.5.dp).background(Brush.horizontalGradient(
        listOf(OpsMuted.copy(alpha=.1f),OpsMuted.copy(alpha=.45f),OpsMuted.copy(alpha=.1f)))))
}
