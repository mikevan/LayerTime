package com.projectrevivesolutions.layertime.controller.ai

import com.projectrevivesolutions.layertime.controller.flow.FlowJson
import java.math.BigDecimal

/** Model binds original text to settings. This atom owns exact arithmetic and bounds. */
object AnalyticQuantityAtom {
    data class Normalized(val json:String,val reason:String)
    private val words=mapOf("a" to "1","zero" to "0","one" to "1","two" to "2","three" to "3","four" to "4","five" to "5",
        "six" to "6","seven" to "7","eight" to "8","nine" to "9","ten" to "10","fifteen" to "15","thirty" to "30","sixty" to "60",
        "a quarter" to "0.25","quarter" to "0.25","a half" to "0.5","half" to "0.5","half a" to "0.5",
        "three quarters" to "0.75","one and a half" to "1.5","two and a half" to "2.5")
    private fun scaledInteger(text:String,multiplier:BigDecimal):Long?=runCatching {
        val value=words[text] ?: text
        if('/' in value) {
            val parts=value.split('/');require(parts.size==2)
            parts[0].toBigDecimal().multiply(multiplier).divide(parts[1].toBigDecimal()).longValueExact()
        } else value.toBigDecimal().multiply(multiplier).longValueExact()
    }.getOrNull()
    fun durationMs(original:String):Long? {
        val text=original.lowercase().trim().replace('µ','u').replace(Regex("(?<=[a-z0-9])-(?=[a-z])")," ").replace(" of a "," ")
        val match=Regex("^(.+?)\\s*(microseconds?|milliseconds?|seconds?|minutes?|hours?|ms|sec|min|us|s|h)$").matchEntire(text) ?: return null
        val multiplier=when(match.groupValues[2]) {
            "microsecond","microseconds","us"->BigDecimal("0.001")
            "millisecond","milliseconds","ms"->BigDecimal.ONE
            "second","seconds","s","sec"->BigDecimal(1000)
            "minute","minutes","min"->BigDecimal(60000)
            else->BigDecimal(3600000)
        }
        return scaledInteger(match.groupValues[1].trim(),multiplier)?.takeIf {it in 1..3600000}
    }
    private fun count(text:String)=scaledInteger(text.lowercase().trim(),BigDecimal.ONE)?.takeIf {it in 1..1000000}
    private fun literalPattern(text:String)=Regex("(?<![\\p{L}\\p{N}_./-])"+Regex.escape(text)+"(?![\\p{L}\\p{N}_/-]|\\.[0-9])",RegexOption.IGNORE_CASE)
    private val remainingDigits=Regex("(?<![\\p{L}\\p{N}_])[0-9]")
    private val remainingWordTimes=Regex("\\b(a quarter|quarter|a half|half a|half|three quarters(?: of a)?|one and a half|two and a half|one|two|three|four|five|ten|fifteen|thirty|sixty)[ -]+(microseconds?|milliseconds?|seconds?|minutes?|hours?)\\b",RegexOption.IGNORE_CASE)
    private val remainingNumberWords=Regex("\\b(two|three|four|five|six|seven|eight|nine|ten|fifteen|thirty|sixty|quarter|half)\\b",RegexOption.IGNORE_CASE)
    fun normalize(json:String,prompt:String):Normalized {
        require(prompt.isNotBlank() && prompt.length<=8000 && json.length<=32000)
        val root=FlowJson.parse(json) as? Map<*,*> ?: error("Expected quantity proposal.")
        require(root["schemaVersion"]==1L)
        fun result(value:Map<String,Any?>,reason:String)=Normalized(FlowJson.write(value),reason)
        fun clarify(reason:String)=result(mapOf("schemaVersion" to 1L,"status" to "CLARIFY","reason" to "AMBIGUOUS_SETTINGS"),reason)
        if(root["status"] in setOf("CLARIFY","UNSUPPORTED")) {
            require(root.keys==setOf("schemaVersion","status","reason"))
            if(root["status"]=="CLARIFY") {require(root["reason"]=="AMBIGUOUS_SETTINGS");return clarify("MODEL_CLARIFICATION")}
            require(root["reason"]=="NO_SUPPORTED_ATOM")
            return result(mapOf("schemaVersion" to 1L,"status" to "UNSUPPORTED","reason" to "NO_SUPPORTED_ATOM"),"MODEL_UNSUPPORTED")
        }
        require(root["status"]=="PROPOSE" && root.keys==setOf("schemaVersion","status","followOnText","episodeGapText","minimumText","scope"))
        require(root["scope"] in setOf("DEVICE_AP_PAIR","RECORDING"))
        fun text(key:String):String? {
            val value=root[key] ?: return null
            require(value is String && value.isNotBlank() && value.length<=80)
            val literal=value.trim()
            require(literalPattern(literal).containsMatchIn(prompt)) {"A proposed quantity is absent from the request."}
            return literal
        }
        val follow=text("followOnText");val gap=text("episodeGapText");val minimum=text("minimumText")
        if(minimum!=null) {
            val timeSpans=listOfNotNull(follow,gap).flatMap {literalPattern(it).findAll(prompt).map {match->match.range}.toList()}
            require(literalPattern(minimum).findAll(prompt).any {match->timeSpans.none {match.range.first>=it.first && match.range.last<=it.last}}) {"Episode count cannot be borrowed from a time quantity."}
        }
        var remaining=prompt
        listOfNotNull(follow,gap,minimum).distinct().sortedByDescending {it.length}.forEach {remaining=literalPattern(it).replace(remaining," ")}
        remaining=Regex("\\b802\\.11[a-z0-9]*\\b",RegexOption.IGNORE_CASE).replace(remaining," ")
        if(remainingDigits.containsMatchIn(remaining) || remainingWordTimes.containsMatchIn(remaining) || remainingNumberWords.containsMatchIn(remaining))
            return clarify("UNASSIGNED_QUANTITY")
        val followMs=if(follow==null)30000L else durationMs(follow) ?: return clarify("UNIT_OR_PRECISION")
        val gapMs=if(gap==null)3000L else durationMs(gap) ?: return clarify("UNIT_OR_PRECISION")
        val n=if(minimum==null)1L else count(minimum) ?: return clarify("EPISODE_COUNT")
        return result(mapOf("schemaVersion" to 1L,"status" to "DRAFT","followOnMs" to followMs,"episodeGapMs" to gapMs,
            "minLinkedEpisodes" to n,"scope" to root["scope"]),"EXACT_CONVERSION")
    }
    fun decode(json:String,prompt:String,modelId:String,modelVersion:String)=
        AnalyticIntent.decode(normalize(json,prompt).json,prompt,modelId,modelVersion)

    fun request(prompt:String):LocalModelRequest {
        require(prompt.isNotBlank() && prompt.length<=8000)
        return LocalModelRequest(AiTask.PLAN_ANALYTIC_QUANTITIES,listOf(
            PromptMessage(PromptMessage.Role.SYSTEM,
                "Interpret a request for a reusable passive disconnect/EAPOL analytic. Extract original quantity text; do not calculate or convert units. " +
                "The analytic links later EAPOL for the same device/AP pair and counts linked episodes. " +
                "For a supported goal return schemaVersion 1, status PROPOSE, followOnText, episodeGapText, minimumText, scope. " +
                "Every non-null quantity must be copied verbatim from the user request, including its original unit if present. Never invent units. " +
                "Use null only for an unspecified setting: native defaults are 30 seconds follow-on, 3 seconds episode gap, and one episode. " +
                "Keep exchange-window and episode-gap text in their own fields even if mentioned in reverse order. If both intervals explicitly share one quantity, copy that same text into both fields. " +
                "Copy quantities even when units are missing, precision is fractional milliseconds, or values are out of range; the conversion atom will decide whether clarification is required. " +
                "scope is DEVICE_AP_PAIR by default; use RECORDING when the threshold combines episodes from different pairs. " +
                "For conflicting settings or roles you cannot resolve, return only schemaVersion 1, status CLARIFY, reason AMBIGUOUS_SETTINGS. " +
                "For goals outside this planner (other tools, fixed-address filters, or proof of completed reconnect/password loss/attacks), return only schemaVersion 1, status UNSUPPORTED, reason NO_SUPPORTED_ATOM. " +
                "No workflow code, arithmetic results, addresses, or prose."),
            PromptMessage(PromptMessage.Role.USER,prompt.trim())),
            """{"type":"object","required":["schemaVersion","status"],"properties":{"schemaVersion":{"const":1},"status":{"enum":["PROPOSE","CLARIFY","UNSUPPORTED"]},"followOnText":{"type":["string","null"]},"episodeGapText":{"type":["string","null"]},"minimumText":{"type":["string","null"]},"scope":{"enum":["DEVICE_AP_PAIR","RECORDING"]},"reason":{"enum":["AMBIGUOUS_SETTINGS","NO_SUPPORTED_ATOM"]}},"additionalProperties":false}""",128)
    }
}
