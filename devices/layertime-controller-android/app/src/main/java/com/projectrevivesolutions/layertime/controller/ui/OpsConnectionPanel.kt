package com.projectrevivesolutions.layertime.controller.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.CalendarMonth
import androidx.compose.material.icons.outlined.Layers
import androidx.compose.material.icons.outlined.Laptop
import androidx.compose.material.icons.outlined.Link
import androidx.compose.material.icons.outlined.LinkOff
import androidx.compose.material.icons.outlined.Search
import androidx.compose.material.icons.outlined.DeleteSweep
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
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.projectrevivesolutions.layertime.controller.link.ConnNotice
import com.projectrevivesolutions.layertime.controller.link.HunterRoster

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

/**
 * HUNTER ACTIONS (Michael, 2026-11-06), the phone OPS page. SCAN at the top finds every
 * Hunter in range and checks each for an SD card; each Hunter gets a row with its name,
 * Connect, Disconnect, and, only for a Hunter with an SD card, Clear Card. One Hunter is
 * connected at a time; Connect on another row lets the current one go first.
 */
@OptIn(ExperimentalLayoutApi::class)
@Composable
fun OpsHunterActionsPanel(
    hunters:List<HunterRoster.Hunter>, scanning:Boolean, scanSentence:String?,
    currentAddress:String?, online:Boolean, connecting:Boolean,
    metadata:String?, notice:ConnNotice.Notice?, clearSentence:String?, clearFailed:Boolean, clearBusy:Boolean,
    onScan:()->Unit, onConnect:(HunterRoster.Hunter)->Unit, onDisconnect:()->Unit, onClearCard:(HunterRoster.Hunter)->Unit,
) {
    OpsConnectionHousing(Modifier.fillMaxWidth()) {
        Column(Modifier.fillMaxWidth().padding(start=18.dp,end=18.dp,top=15.dp,bottom=if(metadata!=null)23.dp else 15.dp),verticalArrangement=Arrangement.spacedBy(6.dp)) {
            Row(verticalAlignment=Alignment.CenterVertically,horizontalArrangement=Arrangement.spacedBy(8.dp)) {
                Box(Modifier.size(6.dp).background(if(online)OpsGreen else OpsMuted,CircleShape))
                Text("HUNTER ACTIONS",color=OpsInk,style=OpsType.copy(fontSize=13.sp,lineHeight=16.sp))
                Spacer(Modifier.weight(1f))
                Text(if(hunters.size==1)"1 Hunter" else "${hunters.size} Hunters",color=OpsMuted,style=OpsType)
                Text("///",color=OpsCyan.copy(alpha=.65f),style=OpsType.copy(letterSpacing=1.sp))
            }
            OpsRule()
            OpsGlassAction(if(scanning)"Scanning…" else "Scan",Icons.Outlined.Search,!scanning,Modifier.fillMaxWidth(),onScan)
            scanSentence?.let { Text(it,color=OpsCyan,style=OpsType) }
            notice?.let { Text(it.text,color=if(it.tone==ConnNotice.Tone.PROBLEM)OpsAmber else OpsCyan,style=OpsType) }
            clearSentence?.let { Text(it,color=if(clearFailed)OpsAmber else OpsCyan,style=OpsType) }
            if(hunters.isEmpty() && !scanning)
                Text("Tap Scan to find the Hunters in range.",color=OpsMuted,style=OpsType.copy(fontSize=12.sp,lineHeight=15.sp))
            hunters.forEach { h ->
                OpsRule()
                val mine=h.address==currentAddress
                val linked=mine && online
                Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.Bottom,horizontalArrangement=Arrangement.spacedBy(7.dp)) {
                    Icon(Icons.Outlined.Laptop,null,tint=if(linked)OpsGreen else OpsInk,modifier=Modifier.size(18.dp))
                    Text(h.name,color=if(linked)OpsInk else OpsMuted,style=OpsType.copy(fontSize=12.sp,lineHeight=15.sp),maxLines=1,
                        modifier=Modifier.weight(1f).alignByBaseline())
                    Text(when { linked -> "Connected"; mine && connecting -> "Linking…"
                        h.card==HunterRoster.Card.YES -> "SD card"; h.card==HunterRoster.Card.NO -> "No SD card"; else -> "" },
                        color=if(linked)OpsGreen else OpsMuted,style=OpsType.copy(fontSize=10.sp),modifier=Modifier.alignByBaseline())
                }
                FlowRow(Modifier.fillMaxWidth(),horizontalArrangement=Arrangement.spacedBy(8.dp)) {
                    OpsGlassAction("Connect",Icons.Outlined.Link,HunterRoster.canConnect(h.address,currentAddress,online,connecting,scanning),Modifier.weight(1f),{ onConnect(h) })
                    OpsGlassAction("Disconnect",Icons.Outlined.LinkOff,mine && (online || connecting),Modifier.weight(1f),onDisconnect)
                    if(h.card==HunterRoster.Card.YES)
                        OpsGlassAction("Clear Card",Icons.Outlined.DeleteSweep,!scanning && !clearBusy,Modifier.weight(1f),{ onClearCard(h) })
                }
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

@Composable
private fun OpsRule() {
    Box(Modifier.fillMaxWidth().height(.5.dp).background(Brush.horizontalGradient(
        listOf(OpsMuted.copy(alpha=.1f),OpsMuted.copy(alpha=.45f),OpsMuted.copy(alpha=.1f)))))
}
