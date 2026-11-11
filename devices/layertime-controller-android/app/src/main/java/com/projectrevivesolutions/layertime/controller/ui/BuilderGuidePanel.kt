package com.projectrevivesolutions.layertime.controller.ui

import androidx.compose.foundation.layout.*
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.projectrevivesolutions.layertime.controller.flow.*
import com.projectrevivesolutions.layertime.controller.wireshark.PcapImport
import com.projectrevivesolutions.layertime.controller.ui.theme.LtColors

@Composable
internal fun BuilderGuidePanel(draft:Draft?,capture:PcapImport.State,running:Boolean,showWiring:Boolean,
    onNew:()->Unit,onCopy:()->Unit,onSave:()->Unit,onRun:()->Unit,onPractice:()->Unit,onWiring:()->Unit,
    onRename:(String)->Unit,onSelect:(String)->Unit,onAdd:(String)->Unit,onPick:()->Unit) {
    OpsConnectionHousing(Modifier.fillMaxWidth()) {
        Column(Modifier.fillMaxWidth().padding(18.dp),verticalArrangement=Arrangement.spacedBy(10.dp)) {
            Text("BUILD AN ANALYSIS",color=LtColors.Cyan,fontSize=18.sp)
            Text("Choose evidence, add tools, then read the result.",color=LtColors.Ivory,fontSize=15.sp)
            Row(horizontalArrangement=Arrangement.spacedBy(8.dp)) {
                SciFiOutline("NEW",true,onClick=onNew)
                SciFiOutline(if(draft?.shipped==true)"MAKE MY COPY" else "SAVE",draft!=null) {if(draft?.shipped==true)onCopy() else onSave()}
                SciFiOutline("PRACTICE",true,onClick=onPractice)
            }
            if(draft!=null)OutlinedTextField(value=draft.workflow.name,onValueChange=onRename,enabled=draft.editable,
                label={Text("Analysis name")},modifier=Modifier.fillMaxWidth(),textStyle=TextStyle(color=LtColors.Ivory,fontSize=15.sp))
            Text("1. EVIDENCE",color=LtColors.Cyan,fontSize=14.sp)
            Text(when(capture) {
                is PcapImport.State.Done->"${if(capture.training)"Practice recording" else capture.fileName}: ${capture.result.totalFrames} packets loaded."
                is PcapImport.State.Running->"Loading recording..."
                else->"No recording loaded. Choose a recording here when you are ready to test your analysis."
            },color=LtColors.Ivory,fontSize=15.sp)
            SciFiOutline(if(capture is PcapImport.State.Done) "CHANGE RECORDING" else "CHOOSE RECORDING",capture !is PcapImport.State.Running,onClick=onPick)
            Text("2. TOOLS",color=LtColors.Cyan,fontSize=14.sp)
            Row(horizontalArrangement=Arrangement.spacedBy(8.dp)) {
                SciFiOutline("+ WIRESHARK",true){onAdd(StepRegistry.WIRESHARK_PLACEHOLDER)}
                SciFiOutline("+ TIMELINE",true){onAdd("target_behavior")}
            }
            Text("Tap to add. With one evidence source and one report, connections are made for you.",color=LtColors.Grey,fontSize=13.sp)
            for(step in draft?.workflow?.steps.orEmpty().filter {StepRegistry.spec(it.type)?.output!=DataType.FRAMES})
                SciFiOutline((if(step.type=="deauth_verdict")"EDIT REPORT" else "EDIT TOOL")+": ${StepRegistry.spec(step.type)?.title ?: step.type}",true){onSelect(step.id)}
            Text("3. RUN AND READ",color=LtColors.Cyan,fontSize=14.sp)
            val valid=draft!=null && WorkflowValidator.validate(draft.workflow).isEmpty()
            SciFiPrimary(if(running)"ANALYZING..." else "RUN ANALYSIS",!running && valid && capture is PcapImport.State.Done,large=true,onClick=onRun)
            if(!valid)Text("Add a tool and connect it to the report before running. Any remaining problems are listed below.",color=LtColors.Amber,fontSize=13.sp)
            SciFiOutline(if(showWiring)"HIDE WIRING CANVAS" else "SHOW WIRING CANVAS / ADVANCED ACTIONS",true,onClick=onWiring)
        }
    }
}
