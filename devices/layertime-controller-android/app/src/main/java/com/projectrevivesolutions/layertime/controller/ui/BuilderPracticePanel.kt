package com.projectrevivesolutions.layertime.controller.ui

import androidx.compose.foundation.layout.*
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.projectrevivesolutions.layertime.controller.flow.BuilderPractice
import com.projectrevivesolutions.layertime.controller.flow.Workflow
import com.projectrevivesolutions.layertime.controller.wireshark.PcapImport
import com.projectrevivesolutions.layertime.controller.ui.theme.LtColors

@Composable
internal fun BuilderPracticePanel(onOpen:(Workflow)->Unit,onWireshark:()->Unit) {
    val context=LocalContext.current
    val capture by PcapImport.state.collectAsState()
    var error by remember {mutableStateOf<String?>(null)}
    OpsConnectionHousing(Modifier.fillMaxWidth()) {
        Column(Modifier.fillMaxWidth().padding(18.dp),verticalArrangement=Arrangement.spacedBy(8.dp)) {
            Text("PRACTICE: VERIFY TARGET CORRELATION",color=LtColors.Cyan,fontSize=15.sp)
            Text("Generated offline data. Nothing is transmitted. Loading it replaces the current imported capture, not saved files or workflows.",color=LtColors.Amber,fontSize=13.sp)
            SciFiOutline("LOAD PRACTICE CAPTURE",capture !is PcapImport.State.Running) {
                runCatching {
                    check(!PcapImport.running){"Another decode is running. Try again when it finishes."}
                    val file=java.io.File(context.cacheDir,BuilderPractice.FILE_NAME)
                    file.writeBytes(BuilderPractice.fixture().pcap())
                    PcapImport.start(context,android.net.Uri.fromFile(file),training=true)
                    error=null
                }.onFailure {error=it.message}
            }
            when(val current=capture) {
                is PcapImport.State.Running->Text("Loading ${current.fileName}: ${current.frames} frames decoded.",color=LtColors.Cyan,fontSize=13.sp)
                is PcapImport.State.Done->if(current.training) Text("Practice data loaded: ${current.result.totalFrames} frames, ${current.result.malformedFrames} malformed. Expected: 6 frames, 0 malformed.",color=LtColors.Amber,fontSize=13.sp)
                is PcapImport.State.Failed->Text(current.message,color=LtColors.Red,fontSize=13.sp)
                else->{}
            }
            error?.let {Text(it,color=LtColors.Red,fontSize=13.sp)}
            Text("Expected packets (1-based positions)",color=LtColors.Cyan,fontSize=13.sp)
            Text("1 at 1 s: disconnect to client 02:00:00:00:00:02\n2 at 2 s: another disconnect to that client\n3 at 3 s: EAPOL to that client\n4 at 4 s: EAPOL to OTHER client 02:00:00:00:00:03\n5 at 10 s: disconnect to the original client\n6 at 11 s: EAPOL from the original client",color=LtColors.Ivory,fontSize=13.sp)
            Text("Expected result: 2 disconnect episodes, 2 linked episodes, 2 related EAPOL frames. Frame 4 must not strengthen the original client's report. Episode evidence: [1, 2, 3] and [5, 6].",color=LtColors.Ivory,fontSize=13.sp)
            SciFiOutline("OPEN REFERENCE WORKFLOW",true){onOpen(BuilderPractice.workflow())}
            Text("Quick check: open the reference, Copy, Run capture, then tap Verdict. Expect TWO LINKED EPISODES. In its first rule, change At least from 2 to 3 and run again: expect THRESHOLD NOT MET. Change it back to 2: the original heading must return.",color=LtColors.Ivory,fontSize=13.sp)
            Text("Rebuild it yourself: New, add Capture, Wireshark (Rate and shape), Behavior Timeline, and Verdict. Wire Capture to both tools; wire both tools to Verdict's features. In Verdict, Start my own rules, then Add a rule. Choose Timeline count / Linked episodes / At least 2, set your heading and sentence. Keep Always last. No rules text is required.",color=LtColors.Ivory,fontSize=13.sp)
            SciFiOutline("INSPECT PRACTICE PACKETS",capture is PcapImport.State.Done){onWireshark()}
        }
    }
}
