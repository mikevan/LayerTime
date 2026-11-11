package com.projectrevivesolutions.layertime.controller

import android.content.Context
import android.os.Bundle
import com.projectrevivesolutions.layertime.controller.ai.*
import com.projectrevivesolutions.layertime.controller.ai.runtime.BundledModelFactory
import com.projectrevivesolutions.layertime.controller.flow.FlowJson
import kotlinx.coroutines.*

/** Explicit debug instrumentation only: unqualified weights never enter product assessments. */
internal fun runLocalAiBenchmark(context:Context,result:Bundle)=runBlocking {
    val provider=BundledModelFactory.development(context)
    try {
        check(provider.availability().state==ModelState.READY) {"Development model missing"}
        check(provider.availability().manifest?.blueTeamTrained==false)
        val request=LocalModelRequest(AiTask.EXPLAIN_EVIDENCE,
            listOf(PromptMessage(PromptMessage.Role.USER,"Return exactly the JSON object {\"ok\":true}.")),
            "{\"type\":\"object\",\"properties\":{\"ok\":{\"type\":\"boolean\"}},\"required\":[\"ok\"],\"additionalProperties\":false}",32)
        check(AiCoordinator(provider).execute(request){it} is AiOutcome.Unavailable) {"Unqualified model entered product coordinator"}
        val start=System.nanoTime()
        val tokens=withTimeout(120_000){provider.countInputTokens(request)}
        val loadMs=(System.nanoTime()-start)/1_000_000
        suspend fun generate():Pair<String,Long> {
            val before=System.nanoTime()
            val response=withTimeout(120_000){provider.generate(request)}
            check((FlowJson.parse(response.json) as? Map<*,*>)?.get("ok")==true) {"Unexpected smoke response: ${response.json}"}
            return response.json to (System.nanoTime()-before)/1_000_000
        }
        val cold=generate()
        val cancelStart=System.nanoTime()
        val cancelled=withTimeoutOrNull(100) {provider.generate(request.copy(maxOutputTokens=512,
            messages=listOf(PromptMessage(PromptMessage.Role.USER,"Return a JSON object with a very long array of numbers."))))}
        check(cancelled==null) {"Cancellation probe finished before its deadline; repeat with longer input"}
        val cancelMs=(System.nanoTime()-cancelStart)/1_000_000
        val warm=generate()
        val memory=android.os.Debug.MemoryInfo().also {android.os.Debug.getMemoryInfo(it)}
        val report=linkedMapOf("modelId" to provider.availability().manifest!!.id,"releaseEligible" to false,
            "inputTokens" to tokens.toLong(),"loadAndTokenizeMs" to loadMs,"firstGenerationMs" to cold.second,
            "warmGenerationMs" to warm.second,"cancellationReturnMs" to cancelMs,"reusedAfterCancellation" to true,
            "response" to warm.first,"processPssKiB" to memory.totalPss.toLong())
        val file=java.io.File(context.filesDir,"diagnostics/local-ai-benchmark.json").apply {parentFile?.mkdirs();writeText(FlowJson.write(report))}
        result.putString("local_ai","PASS: packaged model, JSON generation, cancellation, reuse, production gate")
        result.putString("local_ai_report",file.absolutePath)
        result.putString("local_ai_timings","load ${loadMs}ms; first ${cold.second}ms; warm ${warm.second}ms; cancel ${cancelMs}ms")
    } finally {(provider as? AutoCloseable)?.close()}
}
