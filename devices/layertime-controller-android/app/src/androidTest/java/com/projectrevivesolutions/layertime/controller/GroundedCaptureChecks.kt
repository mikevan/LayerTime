package com.projectrevivesolutions.layertime.controller

import android.content.Context
import android.os.Bundle
import com.projectrevivesolutions.layertime.controller.ai.*
import com.projectrevivesolutions.layertime.controller.ai.runtime.BundledModelFactory
import com.projectrevivesolutions.layertime.controller.flow.*
import com.projectrevivesolutions.layertime.controller.wireshark.*
import kotlinx.coroutines.*
import java.io.File
import java.security.MessageDigest

/** Explicit debug checks only. No unqualified model is enabled in product reporting. */
internal fun runGroundedCaptureChecks(context:Context,testContext:Context,result:Bundle,includeAi:Boolean)=runBlocking {
    check(BuildConfig.DEBUG)
    check(WiresharkDecoder.ensureLoaded()) {"Native decoder is unavailable"}
    check(WiresharkDecoder.initialize()?.ok==true) {"Native decoder initialization failed"}
    val expected=testContext.assets.open("grounded-review/expected.json").bufferedReader().use {FlowJson.parse(it.readText())} as Map<*,*>
    val provider=if(includeAi)BundledModelFactory.development(context) else null
    try {
        if(provider!=null) {
            val availability=provider.availability()
            check(availability.state==ModelState.READY && availability.manifest?.blueTeamTrained==false)
            check(AiTask.REVIEW_GROUNDED_CAPTURE in availability.manifest!!.tasks)
        }
        val rows=(expected["cases"] as List<*>).map {value->
            val item=value as Map<*,*>;val name=item["file"] as String
            check(name.matches(Regex("TRAINING-[a-z-]+\\.pcap(ng)?")))
            val bytes=testContext.assets.open("grounded-review/$name").use {it.readBytes()}
            val hash=MessageDigest.getInstance("SHA-256").digest(bytes).joinToString(""){"%02x".format(it)}
            check(hash==item["sha256"]) {"Fixture integrity mismatch"}
            val file=File(context.cacheDir,"grounded-review/$name").apply {parentFile?.mkdirs();writeBytes(bytes)}
            val start=System.nanoTime()
            val reader=WiresharkDecoder.openPcap(file.absolutePath) ?: error("Cannot open test capture")
            val decoded=reader.use {CaptureDecodeLoop.run(it,500,{false},System::nanoTime,onBeat={})}
            check(decoded.readError==null && decoded.totalFrames.toLong()==item["frames"])
            val frames=decoded.observations
            check(frames.size==decoded.totalFrames)
            val evidence=CaptureEvidence.index(name,frames,EvidenceCoverage(name,frames.size,frames.size,
                untimedFrames=frames.count {!it.timestampValid},training=true))
            val page=evidence.pages(128).single()
            val facts=GroundedReviewTasks.facts(evidence,page)
            check(facts.map {it.claims}==item["claims"]) {"Native decoded facts differ for $name"}
            val row=linkedMapOf<String,Any?>("file" to name,"sha256" to hash,"frames" to frames.size,
                "untimedFrames" to frames.count {!it.timestampValid},"nativeFactsAgree" to true,
                "decodeAndAnalysisMs" to (System.nanoTime()-start)/1_000_000)
            if(provider!=null) {
                val request=GroundedReviewTasks.request("Review this offline training recording.",evidence,page)
                check(AiCoordinator(provider).execute(request){it} is AiOutcome.Unavailable) {"Unqualified model entered product reporting"}
                val loadStart=System.nanoTime()
                val tokens=withTimeout(120_000){provider.countInputTokens(request)}
                check(tokens+request.maxOutputTokens<=provider.availability().manifest!!.contextTokens)
                row["loadAndTokenizeMs"]=(System.nanoTime()-loadStart)/1_000_000
                val modelStart=System.nanoTime()
                val response=withTimeout(120_000){provider.generate(request)}
                check(response.toolCalls.isEmpty())
                val manifest=provider.availability().manifest!!
                check(response.modelId==manifest.id && response.modelVersion==manifest.version)
                GroundedReviewTasks.decode(response.json,evidence,page)
                row["modelGenerationMs"]=(System.nanoTime()-modelStart)/1_000_000
                row["exactModelAgreement"]=true
            }
            row
        }
        val memory=android.os.Debug.MemoryInfo().also {android.os.Debug.getMemoryInfo(it)}
        val report=mapOf("cases" to rows,"modelTested" to includeAi,"releaseEligible" to false,
            "processPssKiB" to memory.totalPss,"modelId" to provider?.availability()?.manifest?.id)
        val reportFile=File(context.filesDir,"diagnostics/grounded-capture-checks.json").apply {
            parentFile?.mkdirs();writeText(FlowJson.write(report))
        }
        result.putString("grounded_capture_checks","PASS: four native captures; model tested: $includeAi")
        result.putString("grounded_capture_report",reportFile.absolutePath)
    } finally {(provider as? AutoCloseable)?.close()}
}
