package com.projectrevivesolutions.layertime.controller

import android.content.Context
import android.os.Bundle
import com.projectrevivesolutions.layertime.controller.ai.*
import com.projectrevivesolutions.layertime.controller.ai.runtime.*
import com.projectrevivesolutions.layertime.controller.flow.FlowJson
import kotlinx.coroutines.*
import java.io.File

/** Real candidate generation on frozen examples, with no reference answer in model input. */
internal fun runDomainAiAcceptance(context:Context,result:Bundle,selection:String)=runBlocking {
    check(BuildConfig.DEBUG)
    val provider=BundledModelFactory.development(context)
    val manifest=provider.availability().manifest ?: error("Trained candidate not installed")
    check(manifest.id=="layertime-qwen3-4b-domain-development") {"Wrong model: ${manifest.id}"}
    val requested=selection.split(',').toSet()
    val examples=DomainAiExamples.load(context).filter {it.id in requested}
    check(examples.size==requested.size && examples.isNotEmpty())
    val rows=mutableListOf<Map<String,Any?>>()
    val file=File(context.filesDir,"diagnostics/domain-candidate-acceptance.json")
    try {
        for(example in examples) {
            check(AiCoordinator(provider).execute(example.request){it} is AiOutcome.Unavailable)
            val start=android.os.SystemClock.elapsedRealtime()
            val tokens=withTimeout(180_000){provider.countInputTokens(example.request)}
            check(tokens+example.request.maxOutputTokens<=manifest.contextTokens)
            val generation=android.os.SystemClock.elapsedRealtime()
            val response=withTimeout(180_000){provider.generate(example.request)}
            check(response.modelId==manifest.id && response.modelVersion==manifest.version)
            val finish=android.os.SystemClock.elapsedRealtime()
            rows.add(mapOf("id" to example.id,"inputTokens" to tokens,"loadAndTokenizeMs" to generation-start,
                "generationMs" to finish-generation,"output" to response.json,"modelId" to manifest.id,"modelVersion" to manifest.version))
            file.parentFile?.mkdirs()
            file.writeText(FlowJson.write(mapOf("cases" to rows,"semanticReviewRequired" to true,"releaseEligible" to false)))
        }
        val cancelStart=android.os.SystemClock.elapsedRealtime()
        val cancelled=withTimeoutOrNull(100){provider.generate(examples.first().request)}
        check(cancelled==null) {"Cancellation probe did not reach its deadline"}
        val cancelMs=android.os.SystemClock.elapsedRealtime()-cancelStart
        // The coroutine returns before the native worker necessarily unwinds.
        val drainStart=android.os.SystemClock.elapsedRealtime()
        val tokensAfterCancellation=withTimeout(30_000){provider.countInputTokens(examples.last().request)}
        check(tokensAfterCancellation>0)
        val drainMs=android.os.SystemClock.elapsedRealtime()-drainStart
        val memory=android.os.Debug.MemoryInfo().also {android.os.Debug.getMemoryInfo(it)}
        file.writeText(FlowJson.write(mapOf("cases" to rows,"cancellationReturnMs" to cancelMs,
            "nativeWorkerDrainAfterCancellationMs" to drainMs,"workerReusableAfterCancellation" to true,
            "processPssKiB" to memory.totalPss,"semanticReviewRequired" to true,"releaseEligible" to false)))
        result.putString("domain_candidate","GENERATED: ${rows.size} real trained-model responses; semantic review pending")
        result.putString("domain_candidate_report",file.absolutePath)
        result.putString("domain_candidate_model",manifest.id+" / "+manifest.version)
    } finally {(provider as? AutoCloseable)?.close()}
}
