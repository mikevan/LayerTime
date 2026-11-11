package com.projectrevivesolutions.layertime.controller.ui

import androidx.compose.foundation.layout.*
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.projectrevivesolutions.layertime.controller.flow.*
import com.projectrevivesolutions.layertime.controller.wireshark.PcapImport
import com.projectrevivesolutions.layertime.controller.ui.theme.LtColors

@Composable
internal fun BuilderAnalysisChooser(flows:List<WorkflowStore.Entry>,onPreset:()->Unit,onSelect:(WorkflowStore.Entry)->Unit) {
    OpsConnectionHousing(Modifier.fillMaxWidth()) {
        Column(Modifier.fillMaxWidth().padding(18.dp),verticalArrangement=Arrangement.spacedBy(12.dp)) {
            Text("1. CHOOSE AN ANALYSIS",color=LtColors.Cyan,fontSize=18.sp)
            Text("Was a device told to disconnect?",color=LtColors.Ivory,fontSize=20.sp)
            Text("Find deauth requests and check for later connection-setup messages involving the same device.",color=LtColors.Grey,fontSize=15.sp)
            SciFiPrimary("USE THIS ANALYSIS",true,large=true,onClick=onPreset)
            val latest=flows.groupBy {it.name}.values.mapNotNull {versions->versions.maxByOrNull {it.version}}
            if(latest.isNotEmpty())Text("Saved and included analyses",color=LtColors.Cyan,fontSize=15.sp)
            for(entry in latest) {
                Text(entry.name,color=LtColors.Ivory,fontSize=17.sp)
                Text("Version ${entry.version}",color=LtColors.Grey,fontSize=13.sp)
                if(entry.accepted)SciFiOutline("USE ${entry.name}",true){onSelect(entry)}
                else Text("Needs repair in the Builder: ${entry.problems.firstOrNull() ?: "invalid analysis"}",color=LtColors.Amber,fontSize=13.sp)
            }
        }
    }
}

@Composable
internal fun BuilderUsePanel(draft:Draft,capture:PcapImport.State,running:Boolean,hasResult:Boolean,error:String?,practice:Boolean,
    onPick:()->Unit,onRun:()->Unit,onCancel:()->Unit,onChangeAnalysis:()->Unit,onEdit:()->Unit) {
    OpsConnectionHousing(Modifier.fillMaxWidth()) {
        Column(Modifier.fillMaxWidth().padding(18.dp),verticalArrangement=Arrangement.spacedBy(12.dp)) {
            Text(if(practice)"PRACTICE ANALYSIS" else "USE AN ANALYSIS",color=LtColors.Cyan,fontSize=18.sp)
            Text(draft.workflow.name,color=LtColors.Ivory,fontSize=19.sp)
            if(!practice)SciFiOutline("CHANGE ANALYSIS",!running,onClick=onChangeAnalysis)
            when(val state=capture) {
                PcapImport.State.Idle -> {
                    Text("2. CHOOSE A RECORDING",color=LtColors.Cyan,fontSize=16.sp)
                    Text("Choose a .pcap or .pcapng file containing the wireless messages to examine.",color=LtColors.Ivory,fontSize=15.sp)
                    SciFiPrimary("CHOOSE RECORDING",true,large=true,onClick=onPick)
                }
                is PcapImport.State.Failed -> {
                    Text("The recording could not be loaded.",color=LtColors.Amber,fontSize=17.sp)
                    Text(state.message,color=LtColors.Ivory,fontSize=14.sp)
                    SciFiPrimary("CHOOSE ANOTHER RECORDING",true,large=true,onClick=onPick)
                }
                is PcapImport.State.Running -> {
                    Text("Loading ${state.fileName}...",color=LtColors.Cyan,fontSize=17.sp)
                    Text("${state.frames} packets decoded",color=LtColors.Grey,fontSize=14.sp)
                    SciFiOutline("CANCEL LOADING",true,onClick=onCancel)
                }
                is PcapImport.State.Done -> {
                    Text(if(state.training)"Practice recording - generated data" else state.fileName,color=if(state.training)LtColors.Amber else LtColors.Ivory,fontSize=16.sp)
                    Text("${state.result.observations.size} packets available for analysis.",color=LtColors.Grey,fontSize=14.sp)
                    if(state.result.cancelled || state.result.readError!=null || state.result.observations.size<state.result.totalFrames)
                        Text("This recording is incomplete. Any report will describe only the available packets.",color=LtColors.Amber,fontSize=14.sp)
                    when {
                        running -> Text("Analyzing the recording...",color=LtColors.Cyan,fontSize=18.sp)
                        state.result.observations.isEmpty() -> {
                            Text("There are no usable packets to analyze.",color=LtColors.Amber,fontSize=16.sp)
                            SciFiPrimary("CHOOSE ANOTHER RECORDING",true,large=true,onClick=onPick)
                        }
                        hasResult -> {
                            Text("Report ready above.",color=LtColors.Cyan,fontSize=16.sp)
                            SciFiOutline("CHOOSE ANOTHER RECORDING",true,onClick=onPick)
                        }
                        else -> {
                            Text("3. RUN THE ANALYSIS",color=LtColors.Cyan,fontSize=16.sp)
                            SciFiPrimary("RUN ANALYSIS",WorkflowValidator.validate(draft.workflow).isEmpty(),large=true,onClick=onRun)
                            SciFiOutline("CHANGE RECORDING",true,onClick=onPick)
                        }
                    }
                }
            }
            error?.let {Text(it,color=LtColors.Amber,fontSize=15.sp)}
            SciFiOutline("EDIT A COPY IN BUILDER",!running,onClick=onEdit)
        }
    }
}
