package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.ai.*
import com.projectrevivesolutions.layertime.controller.ai.runtime.LocalPromptFormat
import com.projectrevivesolutions.layertime.controller.flow.FlowJson
import org.junit.Test
import org.junit.Assert.*

class DomainPromptParityTest {
    @Test fun compactPromptExactlyMatchesTrainingTokenizerTemplate() {
        val raw=javaClass.getResourceAsStream("/ai/domain-prompt-parity.json")!!.bufferedReader().use {it.readText()}
        val data=FlowJson.parse(raw) as Map<*,*>
        val messages=(data["messages"] as List<*>).map { value->
            val m=value as Map<*,*>
            PromptMessage(PromptMessage.Role.valueOf((m["role"] as String).uppercase()),m["content"] as String)
        }
        val request=LocalModelRequest(AiTask.EXPLAIN_EVIDENCE,messages,data["outputSchema"] as String,384)
        assertEquals(data["expectedPrompt"],LocalPromptFormat.render(request,LocalPromptFormat.COMPACT_2507))
    }
    @Test fun missingExplicitSchemaIsRejected() {
        val request=LocalModelRequest(AiTask.EXPLAIN_EVIDENCE,listOf(PromptMessage(PromptMessage.Role.USER,"Hello")),"{}")
        assertThrows(IllegalArgumentException::class.java) {LocalPromptFormat.render(request,LocalPromptFormat.COMPACT_2507)}
    }
}
