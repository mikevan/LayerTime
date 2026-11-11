package com.projectrevivesolutions.layertime.controller.ai.runtime

import com.projectrevivesolutions.layertime.controller.ai.*
import kotlinx.coroutines.suspendCancellableCoroutine
import java.io.File
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicLong
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

/** One worker owns model loading and inference. Cancelled native runs retain their handle until they stop. */
class NativeLlamaProvider(
    private val manifest: LocalModelManifest,
    private val modelFile: () -> File,
    private val grammar: ByteArray,
    private val developmentOnly: Boolean = false,
    private val promptFormat: String = LocalPromptFormat.ID,
) : LocalModelProvider, AutoCloseable {
    private val worker=Executors.newSingleThreadExecutor {task->Thread(task,"LayerTime-local-model").apply {isDaemon=true}}
    private val requests=AtomicLong(1)
    @Volatile private var handle=0L
    @Volatile private var closed=false

    override fun availability(): ModelAvailability = when {
        closed -> ModelAvailability(ModelState.INCOMPATIBLE,reason="The local model has been closed.")
        !manifest.blueTeamTrained && !developmentOnly -> ModelAvailability(ModelState.INCOMPATIBLE,
            reason="This model package is a development candidate and is not enabled for product assessments.")
        else -> ModelAvailability(ModelState.READY,manifest)
    }

    private fun readyHandle(): Long {
        check(!closed) {"The local model has been closed."}
        check(NativeLlama.loaded) {"The embedded inference library is unavailable."}
        if(handle==0L)handle=NativeLlama.load(modelFile().absolutePath)
        if(closed){NativeLlama.cancel(handle,Long.MAX_VALUE);error("The local model has been closed.")}
        return handle.also {check(it!=0L)}
    }

    override suspend fun countInputTokens(request: LocalModelRequest): Int {
        val text=LocalPromptFormat.render(request,promptFormat).toByteArray(Charsets.UTF_8)
        return queued {_->NativeLlama.countTokens(readyHandle(),text)}
    }

    override suspend fun generate(request: LocalModelRequest): LocalModelResponse {
        require(request.maxOutputTokens in 1..4096)
        val input=LocalPromptFormat.render(request,promptFormat).toByteArray(Charsets.UTF_8)
        val id=requests.getAndIncrement()
        return queued(onCancel={val h=handle;if(h!=0L)NativeLlama.cancel(h,id)}) {active->
            val loaded=readyHandle()
            check(active()) {"Local inference was cancelled during loading."}
            val out=NativeLlama.generate(loaded,id,input,grammar,request.maxOutputTokens,
                manifest.contextTokens,Runtime.getRuntime().availableProcessors().coerceIn(1,6))
            LocalModelResponse(out.toString(Charsets.UTF_8),manifest.id,manifest.version)
        }
    }

    private suspend fun <T> queued(onCancel: () -> Unit = {}, operation: (() -> Boolean) -> T): T = suspendCancellableCoroutine { continuation ->
        continuation.invokeOnCancellation {onCancel()}
        try {
            worker.execute {
                if(!continuation.isActive)return@execute
                try {val result=operation {continuation.isActive};if(continuation.isActive)continuation.resume(result)}
                catch(ex:Exception){if(continuation.isActive)continuation.resumeWithException(ex)}
            }
        }catch(ex:Exception){if(continuation.isActive)continuation.resumeWithException(ex)}
    }

    override fun close() {
        if(closed)return
        closed=true
        val h=handle
        if(h!=0L)NativeLlama.cancel(h,Long.MAX_VALUE)
        worker.execute {if(handle!=0L){NativeLlama.close(handle);handle=0L}}
        worker.shutdown()
    }
}
