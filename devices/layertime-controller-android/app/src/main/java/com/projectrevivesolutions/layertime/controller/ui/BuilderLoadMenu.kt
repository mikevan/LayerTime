package com.projectrevivesolutions.layertime.controller.ui

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import com.projectrevivesolutions.layertime.controller.flow.WorkflowStore
import com.projectrevivesolutions.layertime.controller.ui.theme.LtColors

@Composable
internal fun BuilderModal(onDismiss:()->Unit,content:@Composable ()->Unit) {
    val height=(LocalConfiguration.current.screenHeightDp*.78f).coerceAtMost(650f).dp
    Dialog(onDismissRequest=onDismiss) {
        OpsTelemetryHousing(Modifier.fillMaxWidth().height(height),compact=true) {
            Column(Modifier.fillMaxSize().padding(start=14.dp,end=14.dp,top=38.dp,bottom=38.dp).verticalScroll(rememberScrollState()),
                verticalArrangement=Arrangement.spacedBy(10.dp)){content()}
            Box(Modifier.align(Alignment.TopStart)){FrameBackControl(onBack=onDismiss)}
            Box(Modifier.align(Alignment.BottomStart)){FrameBackControl(bottom=true,onBack=onDismiss)}
        }
    }
}

@Composable
internal fun BuilderLoadMenu(flows:List<WorkflowStore.Entry>,onDismiss:()->Unit,onLoad:(WorkflowStore.Entry)->Unit,onImport:()->Unit,onPractice:()->Unit) {
    BuilderModal(onDismiss) {
        Text("LOAD ANALYTIC",color=LtColors.Cyan,fontSize=18.sp)
        SciFiOutline("LOAD FROM FILE",true,onClick=onImport)
        SciFiOutline("LOAD PRACTICE ANALYTIC",true,onClick=onPractice)
        val latest=flows.groupBy {it.name to it.shipped}.values.mapNotNull {it.maxByOrNull {entry->entry.version}}
        for(shipped in listOf(false,true)) {
            Text(if(shipped)"INCLUDED ANALYTICS" else "YOUR SAVED ANALYTICS",color=LtColors.Cyan,fontSize=14.sp)
            val entries=latest.filter {it.shipped==shipped}.sortedBy {it.name}
            if(entries.isEmpty())Text("No saved analytics yet.",color=LtColors.Grey,fontSize=14.sp)
            for(entry in entries) {
                Text(entry.name,color=LtColors.Ivory,fontSize=17.sp)
                Text("Version ${entry.version}"+if(entry.accepted)"" else " - needs repair",color=if(entry.accepted)LtColors.Grey else LtColors.Amber,fontSize=13.sp)
                SciFiOutline("LOAD",true){onLoad(entry)}
            }
        }
    }
}
