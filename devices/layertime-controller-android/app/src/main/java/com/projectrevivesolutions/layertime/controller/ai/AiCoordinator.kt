package com.projectrevivesolutions.layertime.controller.ai

import com.projectrevivesolutions.layertime.controller.flow.FlowJson
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.TimeoutCancellationException
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withTimeout

/** Model lifecycle boundary, not an agent loop. A workflow framework calls this API. */
class AiCoordinator(private val provider: LocalModelProvider) {
    private val execution = Mutex()
    fun availability(): ModelAvailability = provider.availability()

    suspend fun <T> execute(request: LocalModelRequest, timeoutMs: Long = 10_000,
                           decode: (String) -> T): AiOutcome<T> = executeResponse(request, timeoutMs) { response ->
        require(response.toolCalls.isEmpty()) { "A structured task cannot dispatch tools." }
        decode(response.json)
    }

    /** Koog or another host can request a tool turn. This API validates calls but never executes them. */
    suspend fun turn(request: LocalModelRequest, timeoutMs: Long = 10_000): AiOutcome<LocalModelResponse> =
        executeResponse(request, timeoutMs) { it }

    private suspend fun <T> executeResponse(request: LocalModelRequest, timeoutMs: Long,
                                          decode: (LocalModelResponse) -> T): AiOutcome<T> {
        require(timeoutMs in 1..120_000)
        if (request.messages.isEmpty() || request.messages.sumOf { it.content.length.toLong() } > 1_000_000 ||
            request.maxOutputTokens !in 1..4096) return AiOutcome.Invalid("The AI request exceeds its supported limits.")
        val available = provider.availability()
        val manifest = available.manifest
        if (available.state != ModelState.READY || manifest == null)
            return AiOutcome.Unavailable(available.reason ?: "The bundled model is unavailable.")
        if (manifest.contractVersion != 1 || !manifest.blueTeamTrained || manifest.id.isBlank() || manifest.version.isBlank() ||
            !manifest.sha256.matches(Regex("[a-fA-F0-9]{64}")))
            return AiOutcome.Unavailable("The bundled model is not compatible with this product release.")
        if (request.task !in manifest.tasks || ModelCapability.STRUCTURED_OUTPUT !in manifest.capabilities)
            return AiOutcome.Unavailable("This model does not support this task.")
        if(request.tools.isNotEmpty() && ModelCapability.TOOL_CALLS !in manifest.capabilities)
            return AiOutcome.Unavailable("This model does not support tool-based workflows.")
        if(request.tools.size>32 || request.tools.map {it.name}.distinct().size!=request.tools.size ||
            request.tools.any { !it.name.matches(Regex("[a-z][a-z0-9_]{0,63}")) || it.argumentsSchema.length>32000 })
            return AiOutcome.Invalid("The tool catalog is not valid.")
        return try {
            withTimeout(timeoutMs) {
                execution.withLock {
                    val inputTokens = provider.countInputTokens(request)
                    require(inputTokens >= 0 && inputTokens.toLong() + request.maxOutputTokens <= manifest.contextTokens) {
                        "The supplied evidence exceeds the model context window."
                    }
                    val response = provider.generate(request)
                    require(response.modelId == manifest.id && response.modelVersion == manifest.version) {
                        "The returned model identity does not match the selected package."
                    }
                    require(response.json.length <= 1_000_000) { "The model result is too large." }
                    require(response.toolCalls.size<=8 && response.toolCalls.map {it.id}.distinct().size==response.toolCalls.size)
                    for(call in response.toolCalls) {
                        require(call.id.isNotBlank() && call.id.length<=120 && request.tools.any {it.name==call.name})
                        require(call.argumentsJson.length<=32000 && FlowJson.parse(call.argumentsJson) is Map<*,*>)
                    }
                    // The host framework must also validate each tool's argument schema before dispatch.
                    AiOutcome.Completed(decode(response), response.modelId, response.modelVersion)
                }
            }
        } catch (ex: TimeoutCancellationException) { AiOutcome.TimedOut
        } catch (ex: CancellationException) { throw ex
        } catch (ex: Exception) {
            // Do not propagate model output or recorded identifiers through error messages.
            AiOutcome.Invalid("The AI result could not be validated. The original observations remain available.")
        }
    }
}
