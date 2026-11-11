package com.projectrevivesolutions.layertime.controller.ai.koog

import ai.koog.agents.core.tools.ToolDescriptor
import ai.koog.prompt.Prompt
import ai.koog.prompt.dsl.ModerationResult
import ai.koog.prompt.executor.model.PromptExecutor
import ai.koog.prompt.llm.LLModel
import ai.koog.prompt.message.Message
import ai.koog.prompt.message.ResponseMetaInfo
import ai.koog.prompt.streaming.StreamFrame
import ai.koog.utils.time.KoogClock
import com.projectrevivesolutions.layertime.controller.ai.*
import com.projectrevivesolutions.layertime.controller.flow.FlowJson
import kotlinx.coroutines.flow.Flow

/** Koog is an orchestration host. This executor has no remote client or fallback. */
class KoogLocalPromptExecutor(
    provider:LocalModelProvider,
    private val task:AiTask,
    private val outputSchema:String,
    private val maxOutputTokens:Int=512,
) : PromptExecutor() {
    private val coordinator=AiCoordinator(provider)
    override suspend fun execute(prompt:Prompt,model:LLModel,tools:List<ToolDescriptor>):Message.Assistant {
        require(tools.isEmpty()) {"Tool-call execution needs a tool-capable product model."}
        val messages=prompt.messages.map {message->
            val role=when(message.role.name.lowercase()) {
                "system"->PromptMessage.Role.SYSTEM
                "user"->PromptMessage.Role.USER
                "assistant"->PromptMessage.Role.ASSISTANT
                else->error("Unsupported message role for this local task.")
            }
            PromptMessage(role,message.textContent())
        }
        val request=LocalModelRequest(task,messages,outputSchema,maxOutputTokens)
        return when(val result=coordinator.execute(request) {json->require(FlowJson.parse(json) is Map<*,*>);json}) {
            is AiOutcome.Completed -> Message.Assistant(result.value,ResponseMetaInfo.create(KoogClock.System,modelId=result.modelId))
            is AiOutcome.Unavailable -> error(result.reason)
            is AiOutcome.Invalid -> error(result.reason)
            AiOutcome.TimedOut -> error("The local model task exceeded its deadline.")
        }
    }
    override fun executeStreaming(prompt:Prompt,model:LLModel,tools:List<ToolDescriptor>):Flow<StreamFrame> =
        error("Streaming is not advertised by this local runtime revision.")
    override suspend fun moderate(prompt:Prompt,model:LLModel):ModerationResult =
        error("No moderation service is configured or contacted.")
    override fun close() { /* Model lifecycle belongs to the product, not to one workflow instance. */ }
}
