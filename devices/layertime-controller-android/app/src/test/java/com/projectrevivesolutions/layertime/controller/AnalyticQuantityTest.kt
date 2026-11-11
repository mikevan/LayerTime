package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.ai.*
import com.projectrevivesolutions.layertime.controller.flow.FlowJson
import org.junit.Assert.*
import org.junit.Test
import java.io.File

class AnalyticQuantityTest {
    private fun proposal(f:String?=null,g:String?=null,n:String?=null,scope:String="DEVICE_AP_PAIR")=FlowJson.write(mapOf(
        "schemaVersion" to 1L,"status" to "PROPOSE","followOnText" to f,"episodeGapText" to g,"minimumText" to n,"scope" to scope))
    private fun parsed(json:String)=FlowJson.parse(json) as Map<*,*>
    @Test fun exactFractionsAndDecimalsAreComputedByTheAtom() {
        listOf("a quarter-second" to 250L,"three quarters of a second" to 750L,"0.001 hours" to 3600L,
            "0.015 minutes" to 900L,"3000 microseconds" to 3L,"1/3 minute" to 20000L,"1/3 hour" to 1200000L).forEach {(text,expected)->
            assertEquals(text,expected,AnalyticQuantityAtom.durationMs(text))
        }
    }
    @Test fun precisionAndMissingUnitsAreNotRoundedOrGuessed() {
        listOf("7","0 ms","1.5 milliseconds","2500 microseconds","61 minutes","-1 second","1/3 second").forEach {text->
            assertNull(text,AnalyticQuantityAtom.durationMs(text))
            assertEquals("CLARIFY",parsed(AnalyticQuantityAtom.normalize(proposal(f=text),"Use $text for the EAPOL window.").json)["status"])
        }
    }
    @Test fun inventedUnitsAndPartialNumbersAreRejected() {
        assertThrows(IllegalArgumentException::class.java) {AnalyticQuantityAtom.normalize(proposal(f="7 seconds"),"Use 7 for the exchange window.")}
        assertThrows(IllegalArgumentException::class.java) {AnalyticQuantityAtom.normalize(proposal(f="5 seconds"),"Use 0.5 seconds for the exchange window.")}
        assertThrows(IllegalArgumentException::class.java) {AnalyticQuantityAtom.normalize(proposal(f="2 seconds"),"Use -2 seconds for the exchange window.")}
    }
    @Test fun unassignedTimesAndNumbersCannotSilentlyBecomeDefaults() {
        for(prompt in listOf("Use a quarter-second for EAPOL.","Set the episode gap to 7.","Set the gap to seven."))
            assertEquals("CLARIFY",parsed(AnalyticQuantityAtom.normalize(proposal(),prompt).json)["status"])
    }
    @Test fun countCannotBeBorrowedFromAQuotedTime() {
        assertThrows(IllegalArgumentException::class.java) {AnalyticQuantityAtom.normalize(proposal(f="2 seconds",n="2"),"Use 2 seconds for EAPOL, other defaults.")}
        assertEquals(2L,parsed(AnalyticQuantityAtom.normalize(proposal(f="2 seconds",n="2"),"Use 2 seconds for EAPOL; require 2 episodes.").json)["minLinkedEpisodes"])
    }
    @Test fun sharedQuantitiesAndDefaultRolesRemainExplicit() {
        val result=parsed(AnalyticQuantityAtom.normalize(proposal(f="half a second",g="half a second",n="3"),"Set both intervals to half a second and require 3 episodes.").json)
        assertEquals(500L,result["followOnMs"]);assertEquals(500L,result["episodeGapMs"])
        val defaults=parsed(AnalyticQuantityAtom.normalize(proposal(g="750 ms"),"Set the episode gap to 750 ms; keep the other defaults.").json)
        assertEquals(30000L,defaults["followOnMs"])
    }
    @Test fun protocolNamesAreNotMistakenForUnassignedSettings() {
        assertEquals("DRAFT",parsed(AnalyticQuantityAtom.normalize(proposal(),"Build the default Wi-Fi 802.11 WPA2 disconnect/EAPOL analytic.").json)["status"])
    }
    @Test fun exportTemplateAndAuditDataOrSavedModelProposals() {
        System.getenv("LAYERTIME_QUANTITY_TEMPLATE")?.let {path->
            val file=File(path);require(!file.exists());file.parentFile?.mkdirs()
            val request=AnalyticQuantityAtom.request("PLACEHOLDER")
            file.writeText(FlowJson.write(mapOf("task" to request.task.name,"outputSchema" to request.outputSchema,
                "messages" to request.messages.map {mapOf("role" to it.role.name.lowercase(),"content" to it.content)})))
        }
        val examples=System.getenv("LAYERTIME_QUANTITY_EXAMPLES")?.let {path->File(path).readLines().filter {it.isNotBlank()}.map {FlowJson.parse(it) as Map<*,*>}}
        examples?.forEach {row->
            val messages=row["messages"] as List<*>;val prompt=(messages[messages.size-2] as Map<*,*>)["content"] as String
            val normalized=AnalyticQuantityAtom.normalize((messages.last() as Map<*,*>)["content"] as String,prompt)
            assertEquals(row["expectedIntent"],FlowJson.parse(normalized.json))
            AnalyticIntentTest().checkExecution(normalized.json,prompt)
            val request=AnalyticQuantityAtom.request(prompt)
            assertEquals(request.outputSchema,row["outputSchema"])
            assertEquals(request.messages.map {mapOf("role" to it.role.name.lowercase(),"content" to it.content)},messages.dropLast(1))
        }
        System.getenv("LAYERTIME_QUANTITY_RESPONSES")?.let {path->
            val indexed=requireNotNull(examples).associateBy {it["id"]}
            val rows=FlowJson.parse(File(path).readText()) as List<*>
            rows.forEach {value->
                val row=value as Map<*,*>;val messages=indexed.getValue(row["id"])["messages"] as List<*>
                val prompt=(messages[messages.size-2] as Map<*,*>)["content"] as String
                val result=runCatching {AnalyticQuantityAtom.normalize(row["output"] as String,prompt)}
                if(row["quantityProposalValid"]==true) {
                    val normalized=result.getOrThrow();assertEquals(row["predictedIntent"],FlowJson.parse(normalized.json))
                    assertEquals(row["normalizationReason"],normalized.reason)
                    AnalyticIntentTest().checkExecution(normalized.json,prompt)
                } else assertTrue("Invalid proposal accepted by native atom",result.isFailure)
            }
            println("Native quantity audit checked ${rows.size} actual model proposals.")
        }
    }
}
