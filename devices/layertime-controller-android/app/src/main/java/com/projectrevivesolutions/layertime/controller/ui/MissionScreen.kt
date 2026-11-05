package com.projectrevivesolutions.layertime.controller.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.selection.toggleable
import androidx.compose.foundation.selection.triStateToggleable
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.state.ToggleableState
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.projectrevivesolutions.layertime.controller.link.LinkConstants
import com.projectrevivesolutions.layertime.controller.mission.SensorSelection

private val MissionInk=Color(0xFFE6EEF2)
private val MissionMuted=Color(0xFFA1B1BC)
private val MissionCyan=Color(0xFF70E9FA)
private val MissionGreen=Color(0xFF94E934)
private val MissionGold=Color(0xFFFFD044)
private val MissionType=TextStyle(fontFamily=FontFamily.SansSerif,fontSize=11.sp,lineHeight=14.sp)

/** What the Hunter is doing with this list, for the note at the top of MISSION. */
enum class MissionLink { OFFLINE, OLD_FIRMWARE, SENDING, APPLIED }

/** The note's title and text, shared by the phone page and the watch's hull page. */
internal fun missionNote(link: MissionLink, selected: Set<Int>): Pair<String, String> = when {
    selected.isEmpty() -> "EVERY SENSOR OFF" to when (link) {
        MissionLink.OLD_FIRMWARE -> "This Hunter's firmware cannot take a sensor list, so it still runs every sensor."
        else -> "The Hunter will report nothing until you check a sensor."
    }
    link == MissionLink.OFFLINE -> "SAVED ON THIS DEVICE" to "The Hunter gets this list when it connects."
    link == MissionLink.OLD_FIRMWARE -> "HUNTER FIRMWARE TOO OLD" to
        "This Hunter's firmware cannot take a sensor list, so it runs every sensor. Update the Hunter to use this list."
    link == MissionLink.SENDING -> "SENDING TO THE HUNTER" to "The Hunter has not confirmed this list yet."
    else -> "RUNNING ON THE HUNTER" to "The Hunter reports only the sensors checked here."
}

/**
 * MISSION: which sensors the Hunter may report (2026-11-04). The presets are computed
 * from their sensors (SensorSelection): a tap checks every sensor in the group, or
 * clears them when all are already on. The sensors are listed in the three groups.
 */
@Composable
fun MissionScreen(selected:Set<Int>,link:MissionLink,onSelectionChange:(Set<Int>)->Unit) {
    fun toggle(id:Int) {onSelectionChange(if(id in selected)selected-id else selected+id)}
    OpsTelemetryHousing(Modifier.fillMaxSize()) {
        Column(Modifier.fillMaxSize().padding(start=18.dp,end=18.dp,top=20.dp,bottom=24.dp),
            verticalArrangement=Arrangement.spacedBy(8.dp)) {
            Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically) {
                Text("MISSION CONFIGURATION",color=MissionInk,style=MissionType.copy(fontSize=12.sp,lineHeight=15.sp,letterSpacing=.5.sp),modifier=Modifier.weight(1f))
                Text("///",color=MissionCyan.copy(alpha=.6f),style=MissionType.copy(letterSpacing=1.sp))
            }
            MissionRule()
            LazyColumn(Modifier.fillMaxWidth().weight(1f),verticalArrangement=Arrangement.spacedBy(6.dp)) {
                item {
                    val (title,text)=missionNote(link,selected)
                    val tone=if(link==MissionLink.APPLIED && selected.isNotEmpty())MissionGreen else MissionGold
                    val shape=RoundedCornerShape(5.dp)
                    Column(Modifier.fillMaxWidth().background(Brush.horizontalGradient(
                        if(tone==MissionGreen)listOf(Color(0x66244A10),Color(0x33122806)) else listOf(Color(0x77463808),Color(0x44221806))),shape)
                        .border(.6.dp,tone.copy(alpha=.7f),shape).padding(horizontal=10.dp,vertical=8.dp),
                        verticalArrangement=Arrangement.spacedBy(3.dp)) {
                        Text(title,color=tone,style=MissionType.copy(fontSize=10.sp,lineHeight=12.sp,fontWeight=FontWeight.Medium,letterSpacing=.5.sp))
                        Text(text,color=tone.copy(alpha=.85f),style=MissionType.copy(fontSize=10.sp,lineHeight=13.sp))
                    }
                }
                item {MissionSectionHeading("PRESETS")}
                item {
                    Row(Modifier.fillMaxWidth(),horizontalArrangement=Arrangement.spacedBy(6.dp)) {
                        for((id,label) in listOf(SensorSelection.TRACKERS to "TRACKERS",SensorSelection.COUNTER_SURVEIL to "COUNTER-\nSURVEIL",
                                SensorSelection.COUNTER_INTRUSION to "COUNTER-\nINTRUSION")) {
                            val state=SensorSelection.presetState(id,selected)
                            val lit=state!=SensorSelection.PresetState.NONE
                            val flash=rememberPressFlash()
                            val shape=RoundedCornerShape(5.dp)
                            Row(Modifier.weight(1f).heightIn(min=43.dp)
                                .background(Brush.verticalGradient(if(lit)listOf(Color(0xFF1D3E18),Color(0xFF0B1F16)) else listOf(Color(0xFF142D38),Color(0xFF081A25))),shape)
                                .border(.7.dp,if(state==SensorSelection.PresetState.ALL)MissionGreen else if(lit)MissionGreen.copy(alpha=.5f) else MissionCyan.copy(alpha=.45f),shape)
                                .clip(shape).triStateToggleable(state=state.toToggle(),role=Role.Checkbox,
                                    onClick={flash.fire();onSelectionChange(SensorSelection.togglePreset(id,selected))})
                                .semantics(mergeDescendants=true) {}.pressFlash(flash,false,MissionGreen)
                                .padding(horizontal=6.dp,vertical=7.dp),verticalAlignment=Alignment.CenterVertically,
                                horizontalArrangement=Arrangement.spacedBy(5.dp)) {
                                MissionCheck(state)
                                Text(label,color=if(lit)MissionGreen else MissionInk,
                                    style=MissionType.copy(fontSize=8.5.sp,lineHeight=11.sp,fontWeight=FontWeight.Medium),modifier=Modifier.weight(1f))
                            }
                        }
                    }
                }
                item {
                    Row(Modifier.fillMaxWidth().padding(top=7.dp),verticalAlignment=Alignment.CenterVertically,
                        horizontalArrangement=Arrangement.spacedBy(5.dp)) {
                        Text("SENSORS",color=MissionCyan,style=MissionType.copy(fontSize=10.sp,lineHeight=13.sp,letterSpacing=1.sp),modifier=Modifier.weight(1f))
                        MissionBatch("ALL") {onSelectionChange(SensorSelection.ALL_SENSORS.toSet())}
                        MissionBatch("NONE") {onSelectionChange(emptySet())}
                    }
                }
                item {MissionRule()}
                for(group in SensorSelection.GROUPS) {
                    item(key="g${group.preset}") {
                        Text(group.label,color=MissionMuted,style=MissionType.copy(fontSize=9.sp,lineHeight=12.sp,letterSpacing=1.sp),
                            modifier=Modifier.padding(top=6.dp,start=2.dp))
                    }
                    items(group.members,key={it}) {id ->
                        val checked=id in selected
                        val flash=rememberPressFlash()
                        val shape=RoundedCornerShape(5.dp)
                        Row(Modifier.fillMaxWidth().heightIn(min=29.dp)
                            .background(Brush.verticalGradient(if(checked)listOf(Color(0xFF123A38),Color(0xFF071F25)) else listOf(Color(0xFF102631),Color(0xFF06151E))),shape)
                            .border(.6.dp,if(checked)MissionGreen.copy(alpha=.75f) else MissionCyan.copy(alpha=.55f),shape)
                            .clip(shape).toggleable(value=checked,role=Role.Checkbox,onValueChange={flash.fire();toggle(id)})
                            .semantics(mergeDescendants=true) {}.pressFlash(flash,false,MissionCyan)
                            .padding(horizontal=9.dp,vertical=5.dp),verticalAlignment=Alignment.CenterVertically,
                            horizontalArrangement=Arrangement.spacedBy(10.dp)) {
                            MissionCheck(if(checked)SensorSelection.PresetState.ALL else SensorSelection.PresetState.NONE)
                            Text(LinkConstants.ReconTarget.name(id),color=if(checked)MissionGreen else MissionInk,
                                style=MissionType.copy(fontSize=10.sp,lineHeight=13.sp,letterSpacing=.5.sp))
                        }
                    }
                }
            }
        }
    }
}

private fun SensorSelection.PresetState.toToggle():ToggleableState = when(this) {
    SensorSelection.PresetState.ALL -> ToggleableState.On
    SensorSelection.PresetState.PARTIAL -> ToggleableState.Indeterminate
    SensorSelection.PresetState.NONE -> ToggleableState.Off
}

@Composable
private fun MissionCheck(state:SensorSelection.PresetState) {
    val checked=state==SensorSelection.PresetState.ALL
    val partial=state==SensorSelection.PresetState.PARTIAL
    Canvas(Modifier.size(12.dp)) {
        val inset=.8.dp.toPx()
        val square=Size(size.width-2*inset,size.height-2*inset)
        drawRoundRect(if(checked||partial)MissionGreen else MissionMuted,Offset(inset,inset),square,CornerRadius(1.dp.toPx()),
            style=if(checked||partial) androidx.compose.ui.graphics.drawscope.Fill else Stroke(.8.dp.toPx()))
        if(checked) {
            drawLine(Color(0xFF10251B),Offset(size.width*.22f,size.height*.50f),Offset(size.width*.43f,size.height*.70f),1.2.dp.toPx(),StrokeCap.Round)
            drawLine(Color(0xFF10251B),Offset(size.width*.43f,size.height*.70f),Offset(size.width*.79f,size.height*.29f),1.2.dp.toPx(),StrokeCap.Round)
        } else if(partial) {
            drawLine(Color(0xFF10251B),Offset(size.width*.26f,size.height*.5f),Offset(size.width*.74f,size.height*.5f),1.2.dp.toPx(),StrokeCap.Round)
        }
    }
}

@Composable
private fun MissionSectionHeading(label:String) {
    Column(Modifier.fillMaxWidth().padding(top=6.dp),verticalArrangement=Arrangement.spacedBy(4.dp)) {
        Text(label,color=MissionCyan,style=MissionType.copy(fontSize=10.sp,lineHeight=13.sp,letterSpacing=1.sp))
        MissionRule()
    }
}

@Composable
private fun MissionRule() {
    Box(Modifier.fillMaxWidth().height(.5.dp).background(Brush.horizontalGradient(listOf(MissionCyan.copy(alpha=.15f),MissionCyan.copy(alpha=.5f),MissionCyan.copy(alpha=.1f)))))
}

@Composable
private fun MissionBatch(label:String,onClick:()->Unit) {
    val flash=rememberPressFlash()
    val shape=RoundedCornerShape(12.dp)
    Text(label,color=MissionInk,style=MissionType.copy(fontSize=9.sp,lineHeight=11.sp),
        modifier=Modifier.background(Color(0xFF0B2430),shape).border(.6.dp,MissionCyan.copy(alpha=.6f),shape)
            .clip(shape).clickable(role=Role.Button) {flash.fire();onClick()}.pressFlash(flash,false,MissionCyan)
            .padding(horizontal=10.dp,vertical=3.dp))
}
