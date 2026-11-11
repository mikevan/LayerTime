package com.projectrevivesolutions.layertime.controller.ai.runtime

import android.content.Context
import com.projectrevivesolutions.layertime.controller.ai.*
import com.projectrevivesolutions.layertime.controller.flow.FlowJson

data class DomainAiExample(val id:String,val title:String,val referenceHeadline:String,val request:LocalModelRequest)

/** Evaluation assets are not optimizer data or live recording assessments. */
object DomainAiExamples {
    fun load(context:Context):List<DomainAiExample> {
        val rubrics=context.assets.open("ai/development/acceptance-rubrics.jsonl").bufferedReader().useLines { lines->
            lines.filter {it.isNotBlank()}.map {FlowJson.parse(it) as Map<*,*>}.associateBy {it["id"] as String}
        }
        return context.assets.open("ai/development/acceptance.jsonl").bufferedReader().useLines { lines->
            lines.filter {it.isNotBlank()}.map {line->
                val data=FlowJson.parse(line) as Map<*,*>;val id=data["id"] as String;val rubric=rubrics.getValue(id)
                val turns=(data["messages"] as List<*>).dropLast(1).map {value->
                    val message=value as Map<*,*>
                    PromptMessage(PromptMessage.Role.valueOf((message["role"] as String).uppercase()),message["content"] as String)
                }
                DomainAiExample(id,(rubric["scenario"] as String).replace('-',' '),rubric["requiredHeadlineMeaning"] as String,
                    LocalModelRequest(AiTask.EXPLAIN_EVIDENCE,turns,data["outputSchema"] as String,384))
            }.toList()
        }
    }
}
