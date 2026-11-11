package com.projectrevivesolutions.layertime.controller

import android.app.Activity
import android.app.Instrumentation
import android.os.Bundle
import com.projectrevivesolutions.layertime.controller.flow.*
import com.projectrevivesolutions.layertime.controller.wireshark.CaptureDecodeLoop
import com.projectrevivesolutions.layertime.controller.wireshark.WiresharkDecoder

/** No test framework dependency: exercise the actual Android regex and native decoder. */
class BuilderSmokeInstrumentation:Instrumentation() {
    private var fieldCapture:String?=null
    private var aiRuntime=false
    private var groundedPack=false
    private var groundedAi=false
    private var intentPractice=false
    private var domainAiCase:String?=null
    override fun onCreate(arguments:Bundle?) {fieldCapture=arguments?.getString("fieldCapture");aiRuntime=arguments?.getString("aiRuntime")=="true";
        groundedPack=arguments?.getString("groundedPack")=="true";groundedAi=arguments?.getString("groundedAi")=="true";
        intentPractice=arguments?.getString("intentPractice")=="true";domainAiCase=arguments?.getString("domainAiCase");super.onCreate(arguments);start()}
    override fun onStart() {
        val result=Bundle()
        try {
            domainAiCase?.let {runDomainAiAcceptance(targetContext,result,it);finish(Activity.RESULT_OK,result);return}
            if(intentPractice){runIntentPracticeChecks(targetContext,context,result);finish(Activity.RESULT_OK,result);return}
            if(groundedPack || groundedAi){runGroundedCaptureChecks(targetContext,context,result,groundedAi);finish(Activity.RESULT_OK,result);return}
            if(aiRuntime){runLocalAiBenchmark(targetContext,result);finish(Activity.RESULT_OK,result);return}
            check(Summary.fill("{count} related frames.",mapOf("count" to "2"))=="2 related frames.")
            check(Summary.blanksIn("{one} {one} {two}")==listOf("one","two"))
            result.putString("templates","PASS")
            check(WiresharkDecoder.ensureLoaded()) {"Native decoder not loaded: ${WiresharkDecoder.failure()}"}
            check(WiresharkDecoder.initialize()?.ok==true) {"Native decoder initialization failed"}
            fieldCapture?.let {relative->
                val input=java.io.File(targetContext.cacheDir,relative).canonicalFile
                check(input.toPath().startsWith(targetContext.cacheDir.canonicalFile.toPath())) {"Capture must be in this app's cache."}
                val reader=WiresharkDecoder.openPcap(input.absolutePath) ?: error("Cannot open field capture")
                val decoded=reader.use {CaptureDecodeLoop.run(it,500,{false},System::nanoTime,onBeat={})}
                val frames=decoded.observations
                val reports=TargetBehavior.analyze(frames)
                val json=FlowJson.write(linkedMapOf("capture" to relative,"decodedFrames" to decoded.totalFrames.toLong(),
                    "availableFrames" to frames.size.toLong(),"readError" to decoded.readError,
                    "malformedFrames" to decoded.malformedFrames.toLong(),
                    "firstTimeNanos" to frames.filter {it.timestampValid}.minOfOrNull {it.timeNanos},
                    "lastTimeNanos" to frames.filter {it.timestampValid}.maxOfOrNull {it.timeNanos},
                    "deauthCount" to frames.count {it.isDeauth}.toLong(),"eapolCount" to frames.count {it.eapol}.toLong(),
                    "networkNames" to frames.filter {it.ssid!=null}.map {mapOf("bssid" to it.bssid,"ssid" to it.ssid)}.distinct(),
                    "eapol" to frames.filter {it.eapol}.map {mapOf("frame" to it.index.toLong(),"timeNanos" to it.timeNanos,
                        "source" to it.sa,"destination" to it.da,"bssid" to it.bssid,"message" to it.eapolMsg?.toLong())},
                    "reports" to reports.map {r->val comparison=NetworkComparisons.read(r);mapOf(
                        "target" to r.target,"bssid" to r.network,"match" to comparison.match.name,
                        "headline" to comparison.headline,"explanation" to comparison.explanation,"order" to comparison.order,
                        "deauthFrames" to r.deauthenticationFrames.map(Int::toLong),
                        "linkedEapolFrames" to r.followOnFrames.map(Int::toLong),
                        "eapolAccessPoints" to comparison.eapolAccessPoints,
                        "comparisonFrames" to comparison.evidence.map(Int::toLong))}))
                val out=java.io.File(targetContext.filesDir,"diagnostics/field-network-review.json").apply {parentFile?.mkdirs();writeText(json)}
                result.putString("field_review_file",out.absolutePath)
                result.putString("field_review","PASS: ${decoded.totalFrames} decoded frames; ${reports.size} disconnect episodes")
                finish(Activity.RESULT_OK,result);return
            }
            val file=java.io.File(targetContext.cacheDir,"instrumentation-training.pcap")
            file.writeBytes(BuilderPractice.fixture().pcap())
            val reader=WiresharkDecoder.openPcap(file.absolutePath) ?: error("Cannot open practice PCAP")
            val decoded=reader.use {CaptureDecodeLoop.run(it,500,{false},System::nanoTime,onBeat={})}
            check(decoded.readError==null) {"Read error: ${decoded.readError}"}
            check(decoded.totalFrames==6) {"Expected 6 packets, got ${decoded.totalFrames}"}
            check(decoded.malformedFrames==0) {"Malformed packets: ${decoded.malformedFrames}"}
            check(decoded.observations.count {it.eapol}==3) {"Expected 3 decoded EAPOL packets, got ${decoded.observations.count {it.eapol}}"}
            result.putString("native_capture","PASS: 6 packets, 3 EAPOL, 0 malformed")
            val review=com.projectrevivesolutions.layertime.controller.ai.CaptureReviewer.review(
                com.projectrevivesolutions.layertime.controller.ai.CaptureEvidence.index("native-practice",decoded.observations))
            check(review.reviewedFrames==6 && review.issues.size==2)
            check(review.issues.map {it.frameIds}==listOf(listOf(0,1,2),listOf(4,5))) {"Review evidence: ${review.issues.map {it.frameIds}}"}
            result.putString("capture_review","PASS: all 6 frames reviewed, 2 patterns, zero-based references preserved")
            val koog=kotlinx.coroutines.runBlocking {
                com.projectrevivesolutions.layertime.controller.ai.koog.KoogRecordingWorkflow(
                    com.projectrevivesolutions.layertime.controller.ai.UninstalledLocalModel).review(
                    com.projectrevivesolutions.layertime.controller.ai.CaptureEvidence.index("native-koog",decoded.observations),"Review this recording.")
            }
            check(koog.baseline.issues.size==2 && koog.assessment is com.projectrevivesolutions.layertime.controller.ai.AiOutcome.Unavailable)
            result.putString("koog_android","PASS: embedded graph, baseline retained, no model service")
            DeauthSteps.register()
            val frames=decoded.observations
            fun run(threshold:Long):Finding {
                val base=BuilderPractice.workflow()
                val flow=base.copy(steps=base.steps.map {if(it.type=="deauth_verdict")it.copy(settings=mapOf("rules" to BuilderPractice.expectedRules(threshold).toJson())) else it})
                val run=Engine.run(flow,RunContext(DetectionEvent("deauth",BuilderPractice.AP,6,-50,frames.first().timeNanos),FrameSource {frames}))
                check(run.steps.all {it.error==null}) {"Step errors: ${run.steps.filter {it.error!=null}}"}
                return run.finding ?: error("No finding")
            }
            val first=run(2)
            check(first.assessmentLabel==BuilderPractice.HEADING) {"Heading: ${first.assessmentLabel}; ${first.summary}"}
            check(first.summary=="2 disconnect episodes, 2 linked episodes, 2 related EAPOL frames.") {first.summary}
            val reports=first.features.single {it.checkType=="target_behavior"}.targetReports
            check(reports.map {it.evidence}==listOf(listOf(0,1,2),listOf(4,5))) {"Evidence: ${reports.map {it.evidence}}"}
            check(run(3).assessmentLabel==BuilderPractice.FALLBACK)
            check(run(2).assessmentLabel==BuilderPractice.HEADING)
            result.putString("practice_run","PASS: threshold 2 / 3 / 2; unrelated packet excluded")
            result.putString("result","PASS")
            finish(Activity.RESULT_OK,result)
        } catch(t:Throwable) {
            result.putString("result","FAIL")
            result.putString("error",android.util.Log.getStackTraceString(t))
            finish(Activity.RESULT_CANCELED,result)
        }
    }
}
