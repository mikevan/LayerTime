package com.projectrevivesolutions.layertime.controller.ai

/** Product-owned contracts. No Android, network, or orchestration-framework types cross this boundary. */
enum class AiTask { REVIEW_CAPTURE, REVIEW_GROUNDED_CAPTURE, EXPLAIN_EVIDENCE, CREATE_ANALYTIC, PLAN_ANALYTIC, PLAN_ANALYTIC_QUANTITIES, DEFINE_ATOM, DEFINE_AGENT }
enum class ModelCapability { STRUCTURED_OUTPUT, TOOL_CALLS }
enum class ModelState { NOT_INSTALLED, READY, INCOMPATIBLE }

data class LocalModelManifest(
    val id: String,
    val version: String,
    val sha256: String,
    val contractVersion: Int,
    val tasks: Set<AiTask>,
    val capabilities: Set<ModelCapability>,
    val contextTokens: Int,
    val blueTeamTrained: Boolean,
)

data class ModelAvailability(val state: ModelState, val manifest: LocalModelManifest? = null, val reason: String? = null)
data class PromptMessage(val role: Role, val content: String, val toolCallId: String? = null, val name: String? = null) {
    enum class Role { SYSTEM, USER, ASSISTANT, TOOL }
}
data class LocalToolDefinition(val name: String, val description: String, val argumentsSchema: String)
data class LocalToolCall(val id: String, val name: String, val argumentsJson: String)
data class LocalModelRequest(
    val task: AiTask,
    val messages: List<PromptMessage>,
    val outputSchema: String,
    val maxOutputTokens: Int = 512,
    val tools: List<LocalToolDefinition> = emptyList(),
)
data class LocalModelResponse(val json: String, val modelId: String, val modelVersion: String,
                              val toolCalls: List<LocalToolCall> = emptyList())

/** An embedded model adapter must implement cancellation in its native inference callback. */
interface LocalModelProvider {
    fun availability(): ModelAvailability
    /** Count with the shipped tokenizer and actual chat template; never estimate from characters. */
    suspend fun countInputTokens(request: LocalModelRequest): Int
    suspend fun generate(request: LocalModelRequest): LocalModelResponse
}

/** Honest production state until a tested, custom-trained model package is bundled. */
object UninstalledLocalModel : LocalModelProvider {
    override fun availability() = ModelAvailability(ModelState.NOT_INSTALLED,
        reason = "The LayerTime AI model is not installed in this development build. Recording review remains available.")
    override suspend fun countInputTokens(request: LocalModelRequest): Int = error("No tokenizer is installed.")
    override suspend fun generate(request: LocalModelRequest): LocalModelResponse =
        error("No bundled LayerTime model is installed.")
}

sealed interface AiOutcome<out T> {
    data class Completed<T>(val value: T, val modelId: String, val modelVersion: String) : AiOutcome<T>
    data class Unavailable(val reason: String) : AiOutcome<Nothing>
    data class Invalid(val reason: String) : AiOutcome<Nothing>
    data object TimedOut : AiOutcome<Nothing>
}
