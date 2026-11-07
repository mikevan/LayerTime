package com.projectrevivesolutions.layertime.controller.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Info
import androidx.compose.material.icons.outlined.Layers
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

private val ToolInk=Color(0xFFE7EEF3)
private val ToolMuted=Color(0xFFABBCC5)
private val ToolCyan=Color(0xFF70E9FA)
private val ToolGold=Color(0xFFFFD044)
private val ToolType=TextStyle(fontFamily=FontFamily.SansSerif,fontSize=11.sp,lineHeight=14.sp)

/** Existing capture import and analysis routes. Link measurements live in System. */
@Composable
fun ToolsScreen(onWireshark:()->Unit,onTimeline:()->Unit) {
    OpsTelemetryHousing(Modifier.fillMaxSize()) {
        LazyColumn(Modifier.fillMaxSize().padding(start=16.dp,end=16.dp,top=18.dp,bottom=24.dp),
            verticalArrangement=Arrangement.spacedBy(0.dp)) {
            item {ToolCard(LayerTool.WIRESHARK,onWireshark)}
            item {ToolCard(LayerTool.ATTACK_TIMELINE,onTimeline)}
            item {
                val shape=RoundedCornerShape(6.dp)
                Row(Modifier.fillMaxWidth().padding(horizontal=5.dp,vertical=8.dp)
                    .background(Brush.horizontalGradient(listOf(Color(0x77433508),Color(0x44231A05))),shape)
                    .border(.6.dp,ToolGold.copy(alpha=.7f),shape).padding(horizontal=11.dp,vertical=10.dp),
                    verticalAlignment=Alignment.CenterVertically,horizontalArrangement=Arrangement.spacedBy(10.dp)) {
                    Icon(Icons.Outlined.Info,null,tint=ToolGold,modifier=Modifier.size(20.dp))
                    Column(Modifier.weight(1f),verticalArrangement=Arrangement.spacedBy(3.dp)) {
                        Text("CAPTURE FILES AVAILABLE",color=ToolGold,style=ToolType.copy(fontSize=10.sp,lineHeight=13.sp,fontWeight=FontWeight.Medium,letterSpacing=.4.sp))
                        Text("Import a capture to analyze it. Live capture is not available.",color=ToolGold.copy(alpha=.85f),style=ToolType.copy(fontSize=10.sp,lineHeight=13.sp))
                    }
                }
            }

        }
    }
}

@Composable
private fun ToolCard(tool:LayerTool,onOpen:()->Unit) {
    val title=tool.title; val description=tool.description
    OpsConnectionHousing(Modifier.fillMaxWidth()) {
        Row(Modifier.fillMaxWidth().padding(start=20.dp,end=18.dp,top=20.dp,bottom=23.dp),
            verticalAlignment=Alignment.CenterVertically,horizontalArrangement=Arrangement.spacedBy(12.dp)) {
            ToolIcon(tool,28.dp,ToolCyan)
            Column(Modifier.weight(1f),verticalArrangement=Arrangement.spacedBy(4.dp)) {
                Text(title,color=ToolInk,style=ToolType.copy(fontSize=12.sp,lineHeight=15.sp,fontWeight=FontWeight.Medium,letterSpacing=.5.sp))
                Text(description,color=ToolMuted,style=ToolType.copy(fontSize=10.sp,lineHeight=13.sp))
            }
            ToolOpenButton(onOpen)
        }
    }
}

@Composable
private fun ToolOpenButton(onClick:()->Unit) {
    val flash=rememberPressFlash()
    val shape=RoundedCornerShape(12.dp)
    Box(Modifier.width(58.dp).heightIn(min=27.dp)
        .background(Brush.verticalGradient(listOf(Color(0xFFC7FF77),Color(0xFF8DDD36),Color(0xFF4E961E))),shape)
        .border(.6.dp,Color(0xFFC3F78B),shape).clip(shape)
        .clickable(role=Role.Button) {flash.fire();onClick()}.pressFlash(flash,false,Color.White)
        .padding(horizontal=9.dp,vertical=5.dp),contentAlignment=Alignment.Center) {
        Text("OPEN",color=Color(0xFF0D260B),style=ToolType.copy(fontSize=10.sp,lineHeight=12.sp,fontWeight=FontWeight.Bold))
    }
}
