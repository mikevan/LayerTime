package com.projectrevivesolutions.layertime.controller

import android.content.Context
import android.os.Bundle
import com.projectrevivesolutions.layertime.controller.flow.*
import com.projectrevivesolutions.layertime.controller.wireshark.*
import java.io.File

/** Native decoder plus actual imported analytics; no language model or radio required. */
internal fun runIntentPracticeChecks(context:Context,testContext:Context,result:Bundle) {
    check(BuildConfig.DEBUG && WiresharkDecoder.ensureLoaded())
    check(WiresharkDecoder.initialize()?.ok==true)
    DeauthSteps.register()
    val rows=mutableListOf<Map<String,Any?>>()
    listOf("TRAINING-two-devices-one-episode-each.pcap","TRAINING-one-device-two-episodes.pcap").forEachIndexed {case,name->
        val file=File(context.cacheDir,"intent-practice/$name").apply {
            parentFile?.mkdirs();testContext.assets.open("intent-practice/$name").use {input->outputStream().use {input.copyTo(it)}}
        }
        val reader=WiresharkDecoder.openPcap(file.absolutePath) ?: error("Cannot open practice capture")
        val decoded=reader.use {CaptureDecodeLoop.run(it,500,{false},System::nanoTime,onBeat={})}
        check(decoded.readError==null && decoded.totalFrames==4 && decoded.observations.size==4)
        listOf("DEVICE_AP_PAIR","RECORDING").forEach {scope->
            val draft=testContext.assets.open("intent-practice/TRAINING-$scope.json").use {WorkflowExchange.read(it)}
            check(WorkflowValidator.validate(draft.workflow).isEmpty())
            val frames=decoded.observations
            val run=Engine.run(draft.workflow,RunContext(DetectionEvent("DEAUTH",null,null,null,frames.first().timeNanos),FrameSource {frames},
                EvidenceCoverage(name,4,4,training=true)))
            val expected=if(case==1 || scope=="RECORDING")"DISCONNECT SEQUENCE FOUND" else "EPISODE THRESHOLD NOT MET"
            check(run.finding?.assessmentLabel==expected) {"Unexpected result for $name / $scope"}
            val metrics=run.finding!!.features.single {it.checkType=="target_behavior"}.metrics
            check(metrics["linked_episodes"]==2L)
            check(metrics["max_linked_episodes_per_pair"]==if(case==0)1L else 2L)
            rows+=mapOf("capture" to name,"scope" to scope,"label" to expected,"metrics" to metrics)
        }
    }
    val report=File(context.filesDir,"diagnostics/intent-practice-checks.json").apply {
        parentFile?.mkdirs();writeText(FlowJson.write(mapOf("generatedTrainingData" to true,"modelUsed" to false,"cases" to rows)))
    }
    result.putString("intent_practice","PASS: two captures, two counting scopes, four native checks")
    result.putString("intent_practice_report",report.absolutePath)
}
