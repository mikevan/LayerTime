package com.projectrevivesolutions.layertime.controller.ui

import androidx.compose.foundation.layout.*
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.projectrevivesolutions.layertime.controller.ai.runtime.*
import com.projectrevivesolutions.layertime.controller.flow.FlowJson
import com.projectrevivesolutions.layertime.controller.ui.theme.LtColors
import kotlinx.coroutines.*
import java.io.File

@Composable internal fun DomainAiPracticePanel() {
    val context=LocalContext.current
    val provider=remember {BundledModelFactory.development(context)}
    val examples=remember {runCatching {DomainAiExamples.load(context)}.getOrDefault(emptyList())}
    val scope=rememberCoroutineScope()
    var selected by remember {mutableIntStateOf(0)}
    var running by remember {mutableStateOf(false)}
    var job by remember {mutableStateOf<Job?>(null)}
    var status by remember {mutableStateOf("")}
    var output by remember {mutableStateOf<Map<*,*>?>(null)}
    var showEvidence by remember {mutableStateOf(false)}
    DisposableEffect(provider) {onDispose {(provider as? AutoCloseable)?.close()}}
    val example=examples.getOrNull(selected)
    val candidate=provider.availability().manifest?.id=="layertime-qwen3-4b-domain-development"
    OpsConnectionHousing(Modifier.fillMaxWidth()) {
        Column(Modifier.fillMaxWidth().padding(18.dp),verticalArrangement=Arrangement.spacedBy(8.dp)) {
            Text("TRAINED AI · EXPERIMENTAL",color=LtColors.Cyan,fontSize=16.sp)
            Text("Saved test examples only. This candidate is not enabled for live assessments.",color=LtColors.Amber,fontSize=13.sp)
            if(!candidate)Text("The trained domain candidate is not installed in this build.",color=LtColors.Amber,fontSize=14.sp)
            if(example!=null) {
                Text("Example ${selected+1} of ${examples.size}: ${example.title}",color=LtColors.Ivory,fontSize=15.sp)
                Row(horizontalArrangement=Arrangement.spacedBy(8.dp)) {
                    SciFiOutline("PREVIOUS",!running && selected>0){selected--;output=null;status=""}
                    SciFiOutline("NEXT",!running && selected<examples.lastIndex){selected++;output=null;status=""}
                }
                SciFiOutline(if(showEvidence)"HIDE EVIDENCE" else "SHOW EVIDENCE",true){showEvidence=!showEvidence}
                if(showEvidence)Text(example.request.messages.last().content,color=LtColors.Grey,fontSize=12.sp)
                SciFiPrimary(if(running)"READING EXAMPLE…" else "RUN TRAINED AI",candidate && !running,large=true) {
                    running=true;output=null;status="Loading and reading this example…"
                    job=scope.launch {
                        val start=android.os.SystemClock.elapsedRealtime()
                        try {
                            val response=withTimeout(180_000) {provider.generate(example.request)}
                            val elapsed=android.os.SystemClock.elapsedRealtime()-start
                            withContext(Dispatchers.IO) {
                                File(context.filesDir,"diagnostics/domain-ai-practice.jsonl").apply {
                                    parentFile?.mkdirs()
                                    appendText(FlowJson.write(mapOf("caseId" to example.id,"modelId" to response.modelId,
                                        "modelVersion" to response.modelVersion,"milliseconds" to elapsed,"output" to response.json))+"\n")
                                }
                            }
                            output=FlowJson.parse(response.json) as? Map<*,*> ?: error("The model did not return a report object.")
                            status="Completed in ${"%.1f".format(elapsed/1000.0)} seconds. Compare the finding below."
                        } catch(cancel:CancellationException) {status="Stopped.";throw cancel}
                          catch(error:Exception) {status=error.message ?: "The test could not finish."}
                        finally {running=false}
                    }
                }
                if(running)SciFiOutline("STOP",true){job?.cancel()}
                if(status.isNotBlank())Text(status,color=LtColors.Cyan,fontSize=13.sp)
                output?.let {answer->
                    Text(answer["headline"] as? String ?: "No headline returned",color=LtColors.Ivory,fontSize=18.sp)
                    for(key in listOf("significance","limitations","passiveNextStep"))
                        (answer[key] as? String)?.let {Text(it,color=LtColors.Ivory,fontSize=14.sp)}
                    Text("REFERENCE FINDING",color=LtColors.Amber,fontSize=13.sp)
                    Text(example.referenceHeadline,color=LtColors.Ivory,fontSize=14.sp)
                    Text("The reference is for your review. It was not sent to the model.",color=LtColors.Grey,fontSize=12.sp)
                }
            }
        }
    }
}
