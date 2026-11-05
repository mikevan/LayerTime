package com.projectrevivesolutions.layertime.controller.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.selection.selectable
import androidx.compose.foundation.selection.selectableGroup
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.ChevronRight
import androidx.compose.material.icons.outlined.FileDownload
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.projectrevivesolutions.layertime.controller.link.LinkConstants
import com.projectrevivesolutions.layertime.controller.model.EventRow

private val EventsInk=Color(0xFFE5EEF2)
private val EventsMuted=Color(0xFFA2B3BF)
private val EventsCyan=Color(0xFF71E9FC)
private val EventsGold=Color(0xFFFFCF3D)
private val EventsGreen=Color(0xFF91E833)
private val EventsType=TextStyle(fontFamily=FontFamily.SansSerif,fontSize=11.sp,lineHeight=14.sp)

@Composable
fun EventsScreen(events:List<EventRow>,emptyMessage:String,onImport:()->Unit,onOpen:(EventRow)->Unit,onImportCard:(()->Unit)?=null) {
    var filter by rememberSaveable {mutableStateOf("ALL")}
    val shown=remember(events,filter) {events.filter {filter=="ALL" || it.sourceKind==if(filter=="BLE")2 else 1}}
    OpsTelemetryHousing(Modifier.fillMaxSize()) {
        Column(Modifier.fillMaxSize().padding(start=18.dp,end=18.dp,top=20.dp,bottom=24.dp),
            verticalArrangement=Arrangement.spacedBy(8.dp)) {
            Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically) {
                Text("DETECTIONS  /",color=EventsInk,style=EventsType.copy(fontSize=12.sp,lineHeight=15.sp,letterSpacing=.6.sp))
                Text("  ${events.size}",color=EventsGold,style=EventsType.copy(fontSize=12.sp,lineHeight=15.sp,fontWeight=FontWeight.Medium))
                Spacer(Modifier.weight(1f))
                Text("///",color=EventsCyan.copy(alpha=.6f),style=EventsType.copy(letterSpacing=1.sp))
            }
            EventsRule()
            val pill=RoundedCornerShape(12.dp)
            Row(horizontalArrangement=Arrangement.spacedBy(8.dp)) {
                // IMPORT HUNTER CARD (2026-11-04): the Hunter's SD card log and its saved frames.
                for((label,action) in listOfNotNull("IMPORT CAPTURE" to onImport,onImportCard?.let {"IMPORT HUNTER CARD" to it})) {
                    val flash=rememberPressFlash()
                    Row(Modifier.background(Brush.verticalGradient(listOf(Color(0xFF123A48),Color(0xFF061B28))),pill)
                        .border(.7.dp,EventsCyan.copy(alpha=.7f),pill).clip(pill)
                        .clickable(role=Role.Button) {flash.fire();action()}.semantics(mergeDescendants=true) {}
                        .pressFlash(flash,false,EventsCyan).padding(horizontal=11.dp,vertical=6.dp),
                        verticalAlignment=Alignment.CenterVertically,horizontalArrangement=Arrangement.spacedBy(6.dp)) {
                        Icon(Icons.Outlined.FileDownload,null,tint=EventsInk,modifier=Modifier.size(15.dp))
                        Text(label,color=EventsInk,style=EventsType.copy(fontSize=10.sp,lineHeight=12.sp,fontWeight=FontWeight.Medium))
                    }
                }
            }
            EventsRule()
            Row(Modifier.fillMaxWidth().selectableGroup(),horizontalArrangement=Arrangement.spacedBy(7.dp)) {
                for(name in listOf("ALL","BLE","WI-FI")) {
                    val selected=name==filter
                    val f=rememberPressFlash()
                    Text(name,color=if(selected)Color(0xFF122709) else EventsInk,
                        style=EventsType.copy(fontSize=10.sp,lineHeight=12.sp,fontWeight=FontWeight.Medium),textAlign=TextAlign.Center,
                        modifier=Modifier.weight(1f)
                            .background(Brush.verticalGradient(if(selected)listOf(Color(0xFFB5F965),EventsGreen,Color(0xFF579C21)) else listOf(Color(0xFF0E2B37),Color(0xFF061723))),pill)
                            .border(.7.dp,if(selected)EventsGreen else EventsCyan.copy(alpha=.6f),pill).clip(pill)
                            .selectable(selected=selected,role=Role.Tab,onClick={f.fire();filter=name}).pressFlash(f,false,EventsCyan)
                            .padding(horizontal=8.dp,vertical=5.dp))
                }
            }
            LazyColumn(Modifier.fillMaxWidth().weight(1f),verticalArrangement=Arrangement.spacedBy(6.dp)) {
                if(shown.isEmpty()) item {
                    Text(if(events.isEmpty())emptyMessage else "No ${if(filter=="BLE")"BLE" else "Wi-Fi"} detections in this session.",
                        color=EventsMuted,style=EventsType,modifier=Modifier.padding(vertical=9.dp))
                }
                items(shown,key={"${it.sessionId}:${it.eventId}"}) {event->CompactDetectionRow(event) {onOpen(event)}}
            }
        }
    }
}

@Composable
private fun CompactDetectionRow(event:EventRow,onClick:()->Unit) {
    val shape=RoundedCornerShape(5.dp)
    val flash=rememberPressFlash()
    Row(Modifier.fillMaxWidth().heightIn(min=43.dp)
        .background(Brush.verticalGradient(listOf(Color(0xFF112F3B),Color(0xFF05131D))),shape)
        .border(.6.dp,EventsCyan.copy(alpha=.6f),shape).clip(shape)
        .drawBehind {drawRect(EventsGold,Offset(0f,0f),Size(2.dp.toPx(),size.height))}
        .clickable(role=Role.Button) {flash.fire();onClick()}.semantics(mergeDescendants=true) {}
        .pressFlash(flash,false,EventsCyan).padding(start=10.dp,end=6.dp,top=6.dp,bottom=6.dp),
        verticalAlignment=Alignment.CenterVertically,horizontalArrangement=Arrangement.spacedBy(6.dp)) {
        Column(Modifier.weight(1f),verticalArrangement=Arrangement.spacedBy(3.dp)) {
            Row(horizontalArrangement=Arrangement.spacedBy(7.dp)) {
                Text("#${event.eventId}",color=EventsInk,style=EventsType.copy(fontSize=10.sp,lineHeight=12.sp))
                Text(event.detectorName,color=EventsGold,style=EventsType.copy(fontSize=10.sp,lineHeight=12.sp,fontWeight=FontWeight.Medium),modifier=Modifier.weight(1f))
            }
            Text("${LinkConstants.SourceKind.name(event.sourceKind)} / ${LinkConstants.Band.name(event.band)}",
                color=EventsMuted,style=EventsType.copy(fontSize=10.sp,lineHeight=12.sp))
        }
        Text("${event.rssi} dBm",color=EventsInk,style=EventsType.copy(fontSize=10.sp,lineHeight=12.sp),
            textAlign=TextAlign.End,modifier=Modifier.widthIn(min=48.dp))
        Text("x${event.count}",color=EventsInk,style=EventsType.copy(fontSize=10.sp,lineHeight=12.sp),
            textAlign=TextAlign.Center,modifier=Modifier.widthIn(min=26.dp))
        Icon(Icons.Outlined.ChevronRight,null,tint=EventsInk,modifier=Modifier.size(17.dp))
    }
}

@Composable
private fun EventsRule() {
    Box(Modifier.fillMaxWidth().height(.5.dp).background(Brush.horizontalGradient(listOf(EventsCyan.copy(alpha=.1f),EventsCyan.copy(alpha=.45f),EventsCyan.copy(alpha=.1f)))))
}
