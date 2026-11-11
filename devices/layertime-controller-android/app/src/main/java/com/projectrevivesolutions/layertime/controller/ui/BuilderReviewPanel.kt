package com.projectrevivesolutions.layertime.controller.ui

import androidx.compose.foundation.layout.*
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.projectrevivesolutions.layertime.controller.ai.*
import com.projectrevivesolutions.layertime.controller.flow.*
import com.projectrevivesolutions.layertime.controller.ui.theme.LtColors
import com.projectrevivesolutions.layertime.controller.wireshark.PcapImport
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.Job
import kotlinx.coroutines.withContext

/** Review is useful before model delivery and is explicitly labeled as existing rule-based checks. */
@Composable
internal fun BuilderReviewPanel(onDismiss: () -> Unit, onWireshark: () -> Unit, onDraft: (Draft) -> Unit,
                                model: LocalModelProvider? = null) {
    val context=LocalContext.current
    val provider=remember(model,context){model ?: com.projectrevivesolutions.layertime.controller.ai.runtime.BundledModelFactory.production(context)}
    val state by PcapImport.state.collectAsState()
    val recording = state as? PcapImport.State.Done
    val currentCaptureId by rememberUpdatedState(recording?.captureId)
    val scope = rememberCoroutineScope()
    val coordinator = remember(provider) { AiCoordinator(provider) }
    var activeJob by remember { mutableStateOf<Job?>(null) }
    var aiAssessment by remember { mutableStateOf<AiOutcome<List<AiIssue>>?>(null) }
    var prompt by remember { mutableStateOf("") }
    var review by remember { mutableStateOf<RecordingReview?>(null) }
    var proposed by remember { mutableStateOf<Draft?>(null) }
    var busy by remember { mutableStateOf(false) }
    var notice by remember { mutableStateOf<String?>(null) }
    var shown by remember { mutableIntStateOf(10) }
    var expanded by remember { mutableStateOf<Set<String>>(emptySet()) }
    var showLimits by remember { mutableStateOf(false) }
    LaunchedEffect(recording?.captureId) {activeJob?.cancel();busy=false;review=null;shown=10;aiAssessment=null}
    val available=coordinator.availability().state==ModelState.READY
    BuilderModal(onDismiss) {
        Text("REVIEW RECORDING",color=LtColors.Cyan,fontSize=18.sp)
        if(review==null)Text("Review this recording for disconnect patterns.",color=LtColors.Ivory,fontSize=15.sp)
        if(recording==null)Text("Load a recording in the Builder first.",color=LtColors.Amber,fontSize=14.sp)
        else if(review==null)Text("${recording.result.totalFrames} decoded frames",color=LtColors.Grey,fontSize=14.sp)
        if(review==null)SciFiOutline(if(busy)"REVIEWING..." else "REVIEW DISCONNECT PATTERNS",recording!=null && !busy) {
            val snapshot=recording ?: return@SciFiOutline
            busy=true;notice=null
            activeJob=scope.launch {
                try {
                    val result=withContext(Dispatchers.Default) {
                        val fs=snapshot.result.observations
                        val coverage=EvidenceCoverage(snapshot.captureId,snapshot.result.totalFrames,fs.size,
                            snapshot.result.cancelled,snapshot.result.readError,snapshot.result.malformedFrames,
                            fs.count { !it.timestampValid },snapshot.training)
                        com.projectrevivesolutions.layertime.controller.ai.koog.KoogRecordingWorkflow(provider).review(
                            CaptureEvidence.index(snapshot.captureId,fs,coverage),prompt.ifBlank {"Review this recording for significant disconnect behavior."}) {baseline->
                            withContext(Dispatchers.Main){if(currentCaptureId==snapshot.captureId)review=baseline}
                        }
                    }
                    if(currentCaptureId==snapshot.captureId){review=result.baseline;aiAssessment=result.assessment}
                } catch(ex:CancellationException) {throw ex}
                catch(ex:Exception) {notice="Could not review this recording. Reload it and try again."}
                finally {busy=false}
            }
        }
        review?.let { r ->
            if(r.coverage.training)Text("PRACTICE - not a live warning",color=LtColors.Amber,fontSize=15.sp)
            Text(r.headline,color=if(r.issues.isEmpty())LtColors.Ivory else LtColors.Amber,fontSize=18.sp)
            val assessmentStatus=when {
                aiAssessment is AiOutcome.Completed -> "; checked recording summaries below."
                busy && available -> "; AI review in progress."
                aiAssessment is AiOutcome.Invalid -> "; AI review could not be completed."
                else -> "; custom AI is not available for this review."
            }
            r.issues.take(shown).forEach { issue ->
                Text("Network: ${issue.networkNames.joinToString().ifEmpty {"name not captured"}}",color=LtColors.Ivory,fontSize=16.sp)
                Text("${issue.disconnectCount} disconnect ${if(issue.disconnectCount==1)"frame" else "frames"}; ${issue.eapolCount} later network access ${if(issue.eapolCount==1)"message" else "messages"}.",color=LtColors.Ivory,fontSize=15.sp)
                Text(issue.networkComparison.headline,color=LtColors.Cyan,fontSize=16.sp)
                Text(issue.networkComparison.order,color=LtColors.Ivory,fontSize=15.sp)
                Text(issue.target?.let {"Device $it"} ?: "Broadcast or unresolved target",color=LtColors.Grey,fontSize=14.sp)
                Text("Disconnect access point: ${issue.network ?: "not established"}",color=LtColors.Ivory,fontSize=14.sp)
                if(issue.networkComparison.eapolAccessPoints.isNotEmpty())Text("EAPOL access point: ${issue.networkComparison.eapolAccessPoints.joinToString()}",color=LtColors.Ivory,fontSize=14.sp)
                SciFiOutline("VIEW ${issue.frameIds.size} SUPPORTING FRAMES",true) {
                    TriageSession.focusFrames.value=issue.frameIds.toSet();onDismiss();onWireshark()
                }
                SciFiOutline(if(issue.id in expanded)"HIDE EXPLANATION" else "WHY THIS MATTERS",true) {
                    expanded=if(issue.id in expanded)expanded-issue.id else expanded+issue.id
                }
                if(issue.id in expanded)Text(issue.headline+" "+issue.explanation,color=LtColors.Ivory,fontSize=14.sp)
                if(issue.id in expanded)Text(issue.networkComparison.explanation,color=LtColors.Ivory,fontSize=14.sp)
                if(issue.networkComparison.evidence.toSet()!=issue.frameIds.toSet())SciFiOutline("COMPARE ACCESS POINTS",true) {
                    TriageSession.focusFrames.value=issue.networkComparison.evidence.toSet();onDismiss();onWireshark()
                }
            }
            if(shown<r.issues.size)SciFiOutline("SHOW MORE (${shown} OF ${r.issues.size})",true){shown+=10}
            SciFiOutline(if(showLimits)"HIDE RECORDING LIMITS" else "RECORDING LIMITS",true){showLimits=!showLimits}
            if(showLimits)Text("${r.reviewedFrames} frames reviewed. Rule-based checks"+assessmentStatus+" These checks cover disconnect patterns, not every possible issue. "+r.coverage.sentence,color=LtColors.Grey,fontSize=13.sp)
        }
        (aiAssessment as? AiOutcome.Completed)?.let {assessment->
            Text("RECORDED FACTS",color=LtColors.Cyan,fontSize=16.sp)
            Text("Checked against this recording. Requests and exchanges do not confirm a successful disconnect or reconnect.",color=LtColors.Grey,fontSize=13.sp)
            if(assessment.value.isEmpty())Text("No disconnect episodes could be established by these checks. This does not establish safety.",color=LtColors.Ivory,fontSize=14.sp)
            assessment.value.forEach {issue->Text(issue.headline,color=LtColors.Amber,fontSize=16.sp);Text(issue.explanation,color=LtColors.Ivory,fontSize=14.sp)
                SciFiOutline("VIEW SUPPORTING FRAMES",true){TriageSession.focusFrames.value=issue.frameIds.toSet();onDismiss();onWireshark()}}
        }
        Text("ASK THE BUILDER",color=LtColors.Cyan,fontSize=16.sp)
        OutlinedTextField(value=prompt,onValueChange={if(it.length<=AnalyticAuthoring.MAX_PROMPT_CHARS){prompt=it;proposed=null}},
            label={Text("What would you like to investigate?")},minLines=2,maxLines=5,modifier=Modifier.fillMaxWidth())
        if(!available)Text("The custom LayerTime AI model is being prepared. Prompt-based authoring will activate when it is bundled with the product.",color=LtColors.Grey,fontSize=14.sp)
        SciFiOutline("CREATE ANALYTIC DRAFT",available && prompt.isNotBlank() && !busy) {
            busy=true;notice=null
            val userPrompt=prompt
            val reviewedContext=review
            scope.launch {
                try {
                    val manifest=coordinator.availability().manifest ?: error("No model.")
                    when(val result=withContext(Dispatchers.Default) {
                        coordinator.execute(AnalyticAuthoring.request(userPrompt,reviewedContext),timeoutMs=30_000) {
                            AnalyticAuthoring.decode(it,userPrompt,manifest.id,manifest.version)
                        }
                    }) {
                        is AiOutcome.Completed -> {proposed=result.value;notice="Draft validated. Open it to review and test the blocks."}
                        is AiOutcome.Unavailable -> notice=result.reason
                        is AiOutcome.Invalid -> notice=result.reason
                        AiOutcome.TimedOut -> notice="The model ran out of time. Your current analytic is unchanged."
                    }
                } catch(ex:CancellationException) {throw ex}
                catch(ex:Exception) {notice="Could not create an analytic draft."}
                finally {busy=false}
            }
        }
        proposed?.let { draft ->
            Text(draft.workflow.name,color=LtColors.Ivory,fontSize=16.sp)
            Text("${draft.workflow.steps.size} blocks. Opens as a draft; it is not automatically selected for triage.",color=LtColors.Grey,fontSize=14.sp)
            SciFiOutline("OPEN DRAFT",true) {onDraft(draft);onDismiss()}
        }
        notice?.let {Text(it,color=LtColors.Amber,fontSize=14.sp)}
    }
}
