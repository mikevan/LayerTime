package com.projectrevivesolutions.layertime.controller.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.selection.selectable
import androidx.compose.foundation.selection.selectableGroup
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.CropSquare
import androidx.compose.material.icons.outlined.Delete
import androidx.compose.material.icons.outlined.GpsFixed
import androidx.compose.material.icons.outlined.PlayArrow
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

private val DetectionInk=Color(0xFFE5EEF2)
private val DetectionCyan=Color(0xFF72EDFF)
private val DetectionMuted=Color(0xFF8198A4)
private val DetectionGold=Color(0xFFFFD431)
private val DetectionType=TextStyle(fontFamily=FontFamily.SansSerif,fontSize=11.sp,lineHeight=13.sp)
private val DetectionGreen=Color(0xFF8CF04A)
private val DetectionRed=Color(0xFFFF5A4E)

/** What QUICK START's badge says (Michael, 2026-11-04). HUNTING wins, then NOT_SCANNING. */
enum class ScanMode { NOT_SCANNING, QUICKSTART, ON_MISSION, HUNTING }

/** The badge text and color, pure so it is testable. */
internal fun scanModeLabel(mode:ScanMode):Pair<String,Color> = when(mode) {
    ScanMode.NOT_SCANNING -> "Not Scanning" to DetectionRed
    ScanMode.QUICKSTART -> "Quickstart on" to DetectionGreen
    ScanMode.ON_MISSION -> "On Mission" to DetectionGreen
    ScanMode.HUNTING -> "Hunting" to DetectionGold
}

/** Three reference-aligned rows; all behavior comes from existing callbacks. Phone/tablet only.
 *  SELECT SENSORS is a shortcut into MISSION: neither key is lit for a hand-edited list. */
@Composable
fun OpsDetectionPanel(
    hunters:Int,connected:Boolean,running:Boolean,mode:ScanMode,allSelected:Boolean,deauthSelected:Boolean,
    onAll:()->Unit,onDeauth:()->Unit,onStart:()->Unit,onStop:()->Unit,onClear:()->Unit,onHunt:()->Unit,
) {
    OpsConnectionHousing(Modifier.fillMaxWidth()) {
        // Keep controls inside the smoked glass, clear of bevels and the lower light channel.
        BoxWithConstraints(Modifier.fillMaxWidth().padding(start=18.dp,end=18.dp,top=20.dp,bottom=24.dp)) {
            val labelWidth=(maxWidth*.24f).coerceAtMost(98.dp)
            Column(verticalArrangement=Arrangement.spacedBy(3.dp)) {
                Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically,
                    horizontalArrangement=Arrangement.spacedBy(6.dp)) {
                    // The crosshairs carry the scan light: green in the center while the Hunter scans.
                    Box(Modifier.size(16.dp),contentAlignment=Alignment.Center) {
                        Icon(Icons.Outlined.GpsFixed,null,tint=DetectionInk,modifier=Modifier.size(16.dp))
                        if(running) Canvas(Modifier.size(16.dp)) {
                            drawCircle(DetectionGreen.copy(alpha=.35f),radius=3.6.dp.toPx(),center=Offset(size.width/2,size.height/2))
                            drawCircle(DetectionGreen,radius=2.dp.toPx(),center=Offset(size.width/2,size.height/2))
                        }
                    }
                    Text("QUICK START",color=DetectionInk,style=DetectionType.copy(fontSize=12.sp,lineHeight=14.sp,letterSpacing=.6.sp),
                        modifier=Modifier.weight(1f))
                    Box(Modifier.width(.5.dp).height(15.dp).background(DetectionMuted.copy(alpha=.5f)))
                    val badge=RoundedCornerShape(5.dp)
                    // The scan state, to the left of the Hunter count. HUNTING is boxed like the count.
                    val (modeText,modeColor)=scanModeLabel(mode)
                    if(mode==ScanMode.HUNTING) Text(modeText,color=modeColor,
                        style=DetectionType.copy(fontSize=10.sp,lineHeight=12.sp,fontWeight=FontWeight.Medium),
                        modifier=Modifier.background(Color(0x553F3204),badge)
                            .border(.6.dp,modeColor.copy(alpha=.8f),badge).padding(horizontal=6.dp,vertical=2.dp))
                    else Text(modeText,color=modeColor,style=DetectionType.copy(fontSize=10.sp,lineHeight=12.sp,fontWeight=FontWeight.Medium))
                    Text("$hunters ${if(hunters==1)"Hunter" else "Hunters"}",color=DetectionGold,
                        style=DetectionType.copy(fontSize=10.sp,lineHeight=12.sp),
                        modifier=Modifier.background(Color(0x553F3204),badge)
                            .border(.6.dp,DetectionGold.copy(alpha=.8f),badge).padding(horizontal=6.dp,vertical=2.dp))
                }
                DetectionRule()
                Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically) {
                    Text("Select Sensors",color=DetectionInk,style=DetectionType,modifier=Modifier.width(labelWidth))
                    val shape=RoundedCornerShape(5.dp)
                    Row(Modifier.weight(1f).selectableGroup()
                        .background(Brush.verticalGradient(listOf(Color(0xFF1B3440),Color(0xFF0B1D28))),shape)
                        .border(.6.dp,Color(0xFF66838E),shape)) {
                        DetectionKey("All Sensors",Modifier.weight(1f),emphasis=allSelected,selected=allSelected,onClick=onAll)
                        DetectionKey("Only Deauth",Modifier.weight(1f),emphasis=deauthSelected,selected=deauthSelected,onClick=onDeauth)
                    }
                }
                DetectionRule()
                Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically) {
                    Text("Control",color=DetectionInk,style=DetectionType,modifier=Modifier.width(labelWidth))
                    Row(Modifier.weight(1f),horizontalArrangement=Arrangement.spacedBy(6.dp)) {
                        // The lit key shows the Hunter's state: START while it is stopped, STOP while it scans.
                        DetectionKey("Start",Modifier.weight(1f),Icons.Outlined.PlayArrow,connected,emphasis=connected && !running,onClick=onStart)
                        DetectionKey("Stop",Modifier.weight(1f),Icons.Outlined.CropSquare,connected,emphasis=connected && running,onClick=onStop)
                        DetectionKey("Clear",Modifier.weight(1f),Icons.Outlined.Delete,connected,onClick=onClear)
                    }
                }
                DetectionRule()
                DetectionKey("Hunt Vixen",Modifier.fillMaxWidth(),Icons.Outlined.GpsFixed,connected,outlined=true,onClick=onHunt)
            }
        }
    }
}

@Composable
private fun DetectionKey(
    label:String,modifier:Modifier,icon:ImageVector?=null,enabled:Boolean=true,
    emphasis:Boolean=false,outlined:Boolean=false,selected:Boolean?=null,onClick:()->Unit,
) {
    val flash=rememberPressFlash()
    val shape=RoundedCornerShape(5.dp)
    val bright=enabled && (emphasis || outlined)
    val border=when {bright -> DetectionCyan; !enabled -> Color(0xFF415A68); selected!=null -> Color.Transparent; else -> Color(0xFF7095A2)}
    val colors=when {
        !enabled -> listOf(Color(0xFF172A35),Color(0xFF0D1B25))
        emphasis -> listOf(Color(0xFF087791),Color(0xFF064457),Color(0xFF0582A0))
        outlined -> listOf(Color(0xFF0B3442),Color(0xFF041720),Color(0xFF0C3F4B))
        selected!=null -> listOf(Color.Transparent,Color.Transparent)
        else -> listOf(Color(0xFF294957),Color(0xFF112A36),Color(0xFF1B3D4A))
    }
    val action={flash.fire();onClick()}
    val interaction=if(selected!=null) Modifier.selectable(selected=selected,enabled=enabled,role=Role.RadioButton,onClick=action)
        else Modifier.clickable(enabled=enabled,role=Role.Button,onClick=action)
    Row(modifier.heightIn(min=24.dp)
        .shadow(if(bright)3.dp else 0.dp,shape,clip=false,ambientColor=DetectionCyan.copy(alpha=.4f),spotColor=DetectionCyan)
        .background(Brush.verticalGradient(colors),shape)
        .border(if(bright) .9.dp else .6.dp,border,shape)
        .clip(shape).then(interaction).pressFlash(flash,false,DetectionCyan)
        .padding(horizontal=4.dp,vertical=3.dp),
        horizontalArrangement=Arrangement.spacedBy(4.dp,Alignment.CenterHorizontally),verticalAlignment=Alignment.CenterVertically) {
        icon?.let { Icon(it,null,tint=if(enabled)DetectionInk else DetectionMuted,modifier=Modifier.size(13.dp)) }
        Text(label,color=if(enabled)DetectionInk else DetectionMuted,style=DetectionType,
            fontWeight=if(emphasis)FontWeight.Medium else FontWeight.Normal)
    }
}

@Composable
private fun DetectionRule() {
    Box(Modifier.fillMaxWidth().height(.5.dp).background(Brush.horizontalGradient(
        listOf(DetectionMuted.copy(alpha=.15f),DetectionMuted.copy(alpha=.5f),DetectionMuted.copy(alpha=.15f)))))
}
