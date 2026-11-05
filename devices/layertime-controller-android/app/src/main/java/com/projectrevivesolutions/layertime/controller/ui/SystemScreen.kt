package com.projectrevivesolutions.layertime.controller.ui

import android.content.Intent
import android.provider.Settings
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.selection.selectable
import androidx.compose.foundation.selection.selectableGroup
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Close
import androidx.compose.material.icons.outlined.GpsFixed
import androidx.compose.material.icons.outlined.Keyboard
import androidx.compose.material.icons.outlined.Settings
import androidx.compose.material3.Icon
import androidx.compose.material3.ProvideTextStyle
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.projectrevivesolutions.layertime.controller.service.LayerTimeKeyService
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

val LocalSystemScreen=staticCompositionLocalOf { false }
private val SysInk=Color(0xFFE8EFF3)
private val SysMuted=Color(0xFFA0B3BF)
private val SysCyan=Color(0xFF71E8FA)
private val SysGreen=Color(0xFF91E433)
private val SysAmber=Color(0xFFFFD044)
private val SysType=TextStyle(fontFamily=FontFamily.SansSerif,fontSize=11.sp,lineHeight=14.sp)

@Composable
fun SystemModule(title:String,icon:ImageVector=Icons.Outlined.Settings,status:Color?=null,content:@Composable ColumnScope.()->Unit) {
    OpsConnectionHousing(Modifier.fillMaxWidth()) {
        ProvideTextStyle(SysType) {
            Column(Modifier.fillMaxWidth().padding(start=18.dp,end=18.dp,top=18.dp,bottom=22.dp),
                verticalArrangement=Arrangement.spacedBy(7.dp)) {
                Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically,horizontalArrangement=Arrangement.spacedBy(7.dp)) {
                    if(status!=null) Box(Modifier.size(6.dp).background(status,CircleShape))
                    else Icon(icon,null,tint=SysCyan,modifier=Modifier.size(15.dp))
                    Text(title,color=SysInk,style=SysType.copy(fontSize=12.sp,lineHeight=15.sp,letterSpacing=.7.sp),modifier=Modifier.weight(1f))
                    Text("///",color=SysCyan.copy(alpha=.6f),style=SysType.copy(letterSpacing=1.sp))
                }
                Box(Modifier.fillMaxWidth().height(.5.dp).background(SysCyan.copy(alpha=.25f)))
                content()
            }
        }
    }
}

@Composable
fun SystemPositionPanels(
    gpsTime:Long?,manual:RingData.Manual?,useManual:Boolean,posLat:Double?,posLon:Double?,
    onSet:(Double,Double)->Unit,onClear:()->Unit,
) {
    var entry by rememberSaveable { mutableStateOf("") }
    var format by rememberSaveable { mutableStateOf("MGRS") }
    var letters by rememberSaveable { mutableStateOf(false) }
    var message by rememberSaveable { mutableStateOf<String?>(null) }
    var error by rememberSaveable { mutableStateOf(false) }
    val source=when {posLat==null || posLon==null -> "NO FIX"; useManual -> "MANUAL"; else -> "GPS"}
    val sourceColor=when(source) {"GPS"->SysGreen;"MANUAL"->SysAmber;else->SysMuted}
    val grid=if(posLat!=null && posLon!=null) GridPosition.toMgrs(posLat,posLon) else "Awaiting position"
    Column(verticalArrangement=Arrangement.spacedBy(0.dp)) {
        SystemModule("POSITION",Icons.Outlined.GpsFixed,sourceColor) {
            Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically,horizontalArrangement=Arrangement.spacedBy(7.dp)) {
                Text("SOURCE",color=SysMuted,style=SysType.copy(fontSize=9.sp,lineHeight=12.sp))
                val shape=RoundedCornerShape(4.dp)
                Text(source,color=sourceColor,style=SysType.copy(fontSize=10.sp,lineHeight=12.sp,fontWeight=FontWeight.Medium),
                    modifier=Modifier.background(sourceColor.copy(alpha=.08f),shape).border(.6.dp,sourceColor.copy(alpha=.6f),shape)
                        .padding(horizontal=6.dp,vertical=2.dp))
                Spacer(Modifier.weight(1f))
                Text("MGRS",color=SysMuted,style=SysType.copy(fontSize=9.sp,lineHeight=12.sp))
                Text(grid,color=SysInk,style=SysType.copy(fontFamily=FontFamily.Monospace,fontSize=10.sp,lineHeight=13.sp),modifier=Modifier.weight(3f,fill=false))
            }
            val time=if(useManual)manual?.time else gpsTime
            time?.let { Text("${if(useManual)"Entered" else "GPS fix"} ${SimpleDateFormat("MMM dd HH:mm",Locale.US).format(Date(it))}",
                color=SysMuted,style=SysType.copy(fontSize=9.sp,lineHeight=12.sp)) }
        }
        SystemModule("MANUAL POSITION",Icons.Outlined.Keyboard) {
            Row(Modifier.fillMaxWidth().selectableGroup(),horizontalArrangement=Arrangement.spacedBy(6.dp)) {
                for(f in listOf("MGRS","UTM","LAT-LON")) {
                    SystemKey(f,Modifier.weight(1f),selected=format==f) { format=f;message=null }
                }
            }
            val hint=when(format) {"MGRS"->"Enter MGRS grid";"UTM"->"Enter zone, easting, northing";else->"Enter latitude, longitude"}
            val well=RoundedCornerShape(5.dp)
            Row(Modifier.fillMaxWidth().background(Color(0xFF030D15),well).border(.7.dp,SysCyan.copy(alpha=.55f),well)
                .padding(horizontal=9.dp,vertical=7.dp),verticalAlignment=Alignment.CenterVertically) {
                Text(if(entry.isEmpty())hint else entry+"_",color=if(entry.isEmpty())SysMuted else SysInk,
                    style=SysType.copy(fontFamily=FontFamily.Monospace,fontSize=12.sp,lineHeight=15.sp),modifier=Modifier.weight(1f))
                if(entry.isNotEmpty()) Icon(Icons.Outlined.Close,contentDescription="Clear entry",tint=SysMuted,
                    modifier=Modifier.size(22.dp).clickable(role=Role.Button) {entry="";message=null})
            }
            fun key(k:String) {
                message=null
                entry=when(k) {"DEL"->entry.dropLast(1);"SPACE"->if(entry.isEmpty() || entry.endsWith(" "))entry else "$entry "
                    else->if(entry.length<32)entry+k else entry}
            }
            val rows=if(letters) "ABCDEFGHJKLMNPQRSTUVWXYZ".map {it.toString()}.chunked(6)
                else listOf(listOf("1","2","3"),listOf("4","5","6"),listOf("7","8","9"))
            for(row in rows) Row(Modifier.fillMaxWidth(),horizontalArrangement=Arrangement.spacedBy(5.dp)) {
                row.forEach {k->SystemKey(k,Modifier.weight(1f)) {key(k)}}
                if(letters) repeat(6-row.size) {Spacer(Modifier.weight(1f))}
            }
            Row(Modifier.fillMaxWidth(),horizontalArrangement=Arrangement.spacedBy(5.dp)) {
                SystemKey(if(letters)"123" else "ABC",Modifier.weight(1f),accent=SysAmber) {letters=!letters}
                SystemKey(if(letters)"SPACE" else "0",Modifier.weight(1f)) {key(if(letters)"SPACE" else "0")}
                SystemKey("DEL",Modifier.weight(1f),accent=SysAmber) {key("DEL")}
            }
            Row(Modifier.fillMaxWidth(),horizontalArrangement=Arrangement.spacedBy(5.dp)) {
                SystemKey("SPACE",Modifier.weight(1f)) {key("SPACE")}
                SystemKey("-",Modifier.weight(1f)) {key("-")}
                SystemKey(".",Modifier.weight(1f)) {key(".")}
            }
            Row(Modifier.fillMaxWidth(),horizontalArrangement=Arrangement.spacedBy(6.dp)) {
                SystemKey("SET POSITION",Modifier.weight(1f),enabled=entry.isNotBlank(),primary=true) {
                    when(val result=GridPosition.parse(entry)) {
                        is GridPosition.Parsed.Ok -> {
                            onSet(result.position.lat,result.position.lon)
                            message="Position set from ${result.format}: ${GridPosition.toMgrs(result.position.lat,result.position.lon)}."
                            error=false;entry=""
                        }
                        is GridPosition.Parsed.Error -> {message=result.message;error=true}
                    }
                }
                SystemKey("CLEAR MANUAL",Modifier.weight(1f),enabled=manual!=null) {onClear();message="Manual position cleared.";error=false}
            }
            Text("GPS resumes on its next fix. Formats are detected automatically.",color=SysMuted,style=SysType.copy(fontSize=9.sp,lineHeight=12.sp))
            message?.let {Text(it,color=if(error)Color(0xFFFFA25A) else SysGreen,style=SysType)}
        }
    }
}

@Composable
fun SystemSideButtonPanel(nowMs:Long) {
    val context=LocalContext.current
    val on=remember(nowMs/3000L) {LayerTimeKeyService.isEnabled(context)}
    SystemModule("SIDE BUTTON",status=if(on)SysGreen else SysAmber) {
        Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically) {
            Text(if(on)"Accessibility shortcut active" else "Enable in Accessibility settings to open LayerScope.",
                color=SysMuted,style=SysType,modifier=Modifier.weight(1f))
            Text(if(on)"ON" else "OFF",color=if(on)SysGreen else SysAmber,style=SysType.copy(fontSize=10.sp,lineHeight=12.sp),
                modifier=Modifier.padding(start=8.dp))
        }
        if(on) Text("A short press opens LayerScope; hold for Back in other apps.",color=SysMuted,style=SysType)
        SystemKey("OPEN SETTINGS",Modifier.fillMaxWidth()) {
            runCatching {context.startActivity(Intent(Settings.ACTION_ACCESSIBILITY_SETTINGS).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))}
        }
    }
}

@Composable
private fun SystemKey(label:String,modifier:Modifier,accent:Color=SysCyan,enabled:Boolean=true,primary:Boolean=false,selected:Boolean?=null,onClick:()->Unit) {
    val flash=rememberPressFlash()
    val lit=primary || selected==true
    val shape=RoundedCornerShape(5.dp)
    val action={flash.fire();onClick()}
    val interaction=if(selected!=null)Modifier.selectable(selected=selected,enabled=enabled,role=Role.Tab,onClick=action)
        else Modifier.clickable(enabled=enabled,role=Role.Button,onClick=action)
    Box(modifier.heightIn(min=30.dp)
        .background(Brush.verticalGradient(if(lit && enabled)listOf(Color(0xFF11627B),Color(0xFF082F42)) else listOf(Color(0xFF15313F),Color(0xFF06151F))),shape)
        .border(if(lit).9.dp else .6.dp,accent.copy(alpha=if(!enabled).18f else if(lit).95f else .5f),shape)
        .clip(shape).then(interaction).pressFlash(flash,false,accent).padding(horizontal=4.dp,vertical=5.dp),
        contentAlignment=Alignment.Center) {
        Text(label,color=if(enabled)SysInk else SysMuted.copy(alpha=.55f),style=SysType.copy(fontSize=11.sp,lineHeight=13.sp),maxLines=1)
    }
}
