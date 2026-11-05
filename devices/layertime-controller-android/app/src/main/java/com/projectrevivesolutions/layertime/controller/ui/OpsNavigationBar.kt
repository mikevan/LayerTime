package com.projectrevivesolutions.layertime.controller.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.selection.selectable
import androidx.compose.foundation.selection.selectableGroup
import androidx.compose.foundation.shape.CutCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Build
import androidx.compose.material.icons.outlined.AccountTree
import androidx.compose.material.icons.outlined.CalendarMonth
import androidx.compose.material.icons.outlined.GpsFixed
import androidx.compose.material.icons.outlined.Settings
import androidx.compose.material.icons.outlined.WifiTethering
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

internal val NavigationInstrumentHeight = 53.dp

/** One physical instrument housing, shared destinations. Hardware watch is separate. */
@Composable
fun OpsNavigationBar(current:Destination,onSelect:(Destination)->Unit) {
    OpsConnectionHousing(Modifier.fillMaxWidth().padding(horizontal=10.dp,vertical=5.dp)) {
        Row(Modifier.fillMaxWidth().selectableGroup().padding(horizontal=13.dp,vertical=12.dp),
            verticalAlignment=Alignment.CenterVertically) {
            Destination.values().forEachIndexed { index,destination ->
                if(index>0) Box(Modifier.width(.5.dp).height(25.dp)
                    .background(Brush.verticalGradient(listOf(Color.Transparent,Color(0xFF244557),Color.Transparent))))
                NavigationInstrument(destination,destination==current,Modifier.weight(1f)) { onSelect(destination) }
            }
        }
    }
}

@Composable
private fun NavigationInstrument(destination:Destination,selected:Boolean,modifier:Modifier,onClick:()->Unit) {
    val cyan=Color(0xFF73EFFF)
    val ink=Color(0xFFE7EDF0)
    val flash=rememberPressFlash()
    val shape=CutCornerShape(7.dp)
    val icon=when(destination) {
        Destination.OPS -> Icons.Outlined.WifiTethering
        Destination.EVENTS -> Icons.Outlined.CalendarMonth
        Destination.TOOLS -> Icons.Outlined.Build
        Destination.BUILDER -> Icons.Outlined.AccountTree
        Destination.MISSION -> Icons.Outlined.GpsFixed
        Destination.SYSTEM -> Icons.Outlined.Settings
    }
    val label=when(destination) {
        Destination.OPS -> "Ops"; Destination.EVENTS -> "Events"; Destination.TOOLS -> "Tools"
        Destination.MISSION -> "Mission"; Destination.SYSTEM -> "System"
        Destination.BUILDER -> "Builder"
    }
    Column(modifier.padding(horizontal=3.dp).heightIn(min=NavigationInstrumentHeight)
        .background(Brush.verticalGradient(if(selected)
            listOf(Color(0xFF12536A),Color(0xFF052231),Color(0xFF0C4C61))
            else listOf(Color.Transparent,Color.Transparent)),shape)
        .border(if(selected) .9.dp else 0.dp,if(selected)cyan else Color.Transparent,shape)
        .clip(shape)
        .drawBehind {
            if(selected) {
                val glass=Path().apply {
                    moveTo(0f,0f);lineTo(size.width*.8f,0f)
                    lineTo(size.width*.35f,size.height);lineTo(0f,size.height);close()
                }
                drawPath(glass,Brush.linearGradient(listOf(Color.White.copy(alpha=.08f),Color.Transparent)))
                drawLine(cyan.copy(alpha=.6f),Offset(size.width*.2f,1.dp.toPx()),Offset(size.width*.8f,1.dp.toPx()),.5.dp.toPx())
            }
        }
        .selectable(selected=selected,role=Role.Tab,onClick={flash.fire();onClick()})
        .semantics(mergeDescendants=true) {}
        .pressFlash(flash,false,cyan).padding(vertical=6.dp),
        horizontalAlignment=Alignment.CenterHorizontally,verticalArrangement=Arrangement.spacedBy(3.dp,Alignment.CenterVertically)) {
        Icon(icon,null,tint=if(selected)cyan else ink,modifier=Modifier.size(21.dp))
        Text(label,color=ink,style=TextStyle(fontFamily=FontFamily.SansSerif,fontSize=10.5.sp,lineHeight=13.sp,
            fontWeight=if(selected)FontWeight.Medium else FontWeight.Normal),maxLines=1,softWrap=false)
    }
}
