package com.projectrevivesolutions.layertime.controller.ai

import com.projectrevivesolutions.layertime.controller.flow.*

/** Model interprets the request; native code owns wiring, rules, and report language. */
object AnalyticIntent {
    data class Plan(val followOnMs:Long,val episodeGapMs:Long,val minLinkedEpisodes:Long,val scope:String)
    sealed interface Decision {
        data class Ready(val plan:Plan,val draft:Draft):Decision
        data class Clarify(val reason:String):Decision
        data object Unsupported:Decision
    }
    fun request(prompt:String):LocalModelRequest {
        require(prompt.isNotBlank() && prompt.length<=8000)
        return LocalModelRequest(AiTask.PLAN_ANALYTIC,listOf(
            PromptMessage(PromptMessage.Role.SYSTEM,
                "Interpret the user's request for a reusable, passive Wi-Fi analytic. Return only JSON. " +
                "The only supported analytic groups disconnect requests into episodes by device/access-point pair and links later EAPOL for that same pair. " +
                "For a supported request return schemaVersion 1, status DRAFT, followOnMs, episodeGapMs, minLinkedEpisodes, scope. " +
                "scope is DEVICE_AP_PAIR unless the user explicitly asks to count episodes across the whole recording, then RECORDING. " +
                "Defaults: followOnMs 30000, episodeGapMs 3000, minLinkedEpisodes 1. Convert explicit seconds/minutes to whole milliseconds. " +
                "Both time settings must be 1..3600000 ms and the episode threshold 1..1000000. " +
                "Never silently change explicit values or units to fit those limits. " +
                "If required units are missing, values conflict or are out of range, return only schemaVersion 1, status CLARIFY, reason AMBIGUOUS_SETTINGS. " +
                "For other tools, fixed-address filters, confirmed reconnect/password/attack conclusions or predictions, return only schemaVersion 1, status UNSUPPORTED, reason NO_SUPPORTED_ATOM. " +
                "Do not generate workflow blocks, code, observed facts, addresses or prose."),
            PromptMessage(PromptMessage.Role.USER,prompt.trim())),
            """{"type":"object","required":["schemaVersion","status"],"properties":{"schemaVersion":{"const":1},"status":{"enum":["DRAFT","CLARIFY","UNSUPPORTED"]},"followOnMs":{"type":"integer"},"episodeGapMs":{"type":"integer"},"minLinkedEpisodes":{"type":"integer"},"scope":{"enum":["DEVICE_AP_PAIR","RECORDING"]},"reason":{"enum":["AMBIGUOUS_SETTINGS","NO_SUPPORTED_ATOM"]}},"additionalProperties":false}""",128)
    }
    fun decode(json:String,prompt:String,modelId:String,modelVersion:String):Decision {
        require(json.length<=32000 && prompt.isNotBlank() && prompt.length<=8000)
        val root=FlowJson.parse(json) as? Map<*,*> ?: error("Expected analytic intent.")
        require(root["schemaVersion"]==1L)
        return when(root["status"]) {
            "CLARIFY","UNSUPPORTED"->{
                require(root.keys==setOf("schemaVersion","status","reason"))
                if(root["status"]=="CLARIFY") {
                    require(root["reason"]=="AMBIGUOUS_SETTINGS");Decision.Clarify("AMBIGUOUS_SETTINGS")
                } else {require(root["reason"]=="NO_SUPPORTED_ATOM");Decision.Unsupported}
            }
            "DRAFT"->{
                require(root.keys==setOf("schemaVersion","status","followOnMs","episodeGapMs","minLinkedEpisodes","scope"))
                fun number(key:String,max:Long)=requireNotNull(root[key] as? Long) {"Invalid setting."}.also {require(it in 1..max)}
                val plan=Plan(number("followOnMs",3600000),number("episodeGapMs",3600000),number("minLinkedEpisodes",1000000),root["scope"] as? String ?: error("Missing scope."))
                require(plan.scope in setOf("DEVICE_AP_PAIR","RECORDING"))
                Decision.Ready(plan,compile(plan,prompt,modelId,modelVersion))
            }
            else->error("Unsupported analytic decision.")
        }
    }
    private fun compile(plan:Plan,prompt:String,modelId:String,modelVersion:String):Draft {
        val metric=if(plan.scope=="DEVICE_AP_PAIR")"max_linked_episodes_per_pair" else "linked_episodes"
        val summary=if(plan.scope=="DEVICE_AP_PAIR")
            "One device/access-point pair had {timeline.max_linked_episodes_per_pair} episodes with later security messages."
        else "{timeline.linked_episodes} episodes across this recording had later security messages."
        val rules=VerdictRuleSet(emptyList(),listOf(
            VerdictRule(Cond.MetricAtLeast("timeline",metric,plan.minLinkedEpisodes),Verdict.WATCH,
                "$summary This does not confirm a reconnect, password loss, or an attack.","DISCONNECT SEQUENCE FOUND"),
            VerdictRule(Cond.Always,Verdict.WATCH,"The selected episode threshold was not met. Missing observations do not establish safety.","EPISODE THRESHOLD NOT MET")))
        val workflow=Workflow("Disconnects followed by security messages",1,listOf(
            Step("capture","capture_source"),Step("timeline","target_behavior",mapOf("follow_on_ms" to plan.followOnMs,
                "episode_gap_ms" to plan.episodeGapMs,"target_mac" to "","network_mac" to "")),
            Step("report","deauth_verdict",mapOf("rules" to rules.toJson()))),listOf(
            Edge("capture","timeline","frames",DataType.FRAMES),Edge("timeline","report","features",DataType.FEATURE)))
        val proposal=FlowJson.write(mapOf("schemaVersion" to 1L,"workflow" to FlowJson.parse(WorkflowLoader.write(workflow))))
        return AnalyticAuthoring.decode(proposal,prompt,modelId,modelVersion)
    }
}
