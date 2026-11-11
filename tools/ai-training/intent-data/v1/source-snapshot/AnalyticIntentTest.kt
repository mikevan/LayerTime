package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.ai.*
import com.projectrevivesolutions.layertime.controller.flow.*
import org.junit.Assert.*
import org.junit.Test
import java.io.File

class AnalyticIntentTest {
    private val ap="02:00:00:00:01:01"
    private val other="02:00:00:00:02:02"
    private val client="02:00:00:00:03:03"
    private val second="02:00:00:00:04:04"
    private fun frame(i:Int,ms:Long,eapol:Boolean=false,network:String=ap,target:String=client)=AnalyticFrame(
        i,1_700_000_000_000_000_000L+ms*1_000_000,if(eapol)AnalyticFrame.DATA else AnalyticFrame.DEAUTH,
        network,target,network,eapol=eapol)
    private fun fixtures()=listOf(
        listOf(frame(0,0),frame(1,500,true),frame(2,5000),frame(3,5500,true)),
        listOf(frame(0,0),frame(1,500,true),frame(2,5000,target=second),frame(3,5500,true,target=second)),
        listOf(frame(0,0),frame(1,500,true),frame(2,5000,network=other),frame(3,5500,true,network=other)),
        listOf(frame(0,0),frame(1,500,true).copy(timestampValid=false)),
        listOf(frame(0,0,true),frame(1,500)),
        listOf(frame(0,0,target=AnalyticFrame.BROADCAST),frame(1,500,true)))
    private fun json(scope:String="DEVICE_AP_PAIR",follow:Long=30000,gap:Long=3000,minimum:Long=2)=FlowJson.write(
        mapOf("schemaVersion" to 1L,"status" to "DRAFT","followOnMs" to follow,"episodeGapMs" to gap,
            "minLinkedEpisodes" to minimum,"scope" to scope))
    private fun checkExecution(answer:String,prompt:String="Create a reusable analytic.") {
        val result=AnalyticIntent.decode(answer,prompt,"test-model","1")
        if(result !is AnalyticIntent.Decision.Ready)return
        DeauthSteps.register()
        val plan=result.plan
        fixtures().forEachIndexed {i,frames->
            // Hand-derived oracle: each usable exchange is exactly 500 ms after
            // its request. Same-pair requests are 5000 ms apart; other fixtures
            // keep distinct clients/APs separate, or cannot establish ordering.
            val linked=if(plan.followOnMs<500 || i>=3)0L else when(i) {
                0->if(plan.episodeGapMs>=5000)1L else 2L
                else->if(plan.scope=="DEVICE_AP_PAIR")1L else 2L
            }
            val finding=Engine.run(result.draft.workflow,RunContext(DetectionEvent("DEAUTH",ap,null,null,frames.first().timeNanos),FrameSource {frames})).finding!!
            assertEquals("fixture $i: $answer",if(linked>=plan.minLinkedEpisodes)"DISCONNECT SEQUENCE FOUND" else "EPISODE THRESHOLD NOT MET",finding.assessmentLabel)
            assertTrue(result.draft.workflow.steps.all {it.settings["target_mac"].let {v->v==null || v==""}})
        }
    }
    @Test fun countsRecordingAndDeviceApPairSeparately() {
        checkExecution(json())
        checkExecution(json("RECORDING"))
        checkExecution(json(gap=5000))
        checkExecution(json(follow=499))
        checkExecution(json(follow=500))
    }
    @Test fun invalidAndInventedIntentFieldsCannotBecomeDrafts() {
        val base=FlowJson.parse(json()) as Map<*,*>
        listOf(mapOf("followOnMs" to 0L),mapOf("episodeGapMs" to 3600001L),mapOf("minLinkedEpisodes" to true),
            mapOf("followOnMs" to 1.5),mapOf("scope" to "INVENTED"),mapOf("code" to "ignored code"),mapOf("target_mac" to client)).forEach {patch->
            assertThrows(IllegalArgumentException::class.java) {AnalyticIntent.decode(FlowJson.write(base+patch),"test","model","1")}
        }
    }
    @Test fun unsupportedAndAmbiguousRequestsProduceNoWiring() {
        assertEquals(AnalyticIntent.Decision.Unsupported,AnalyticIntent.decode("""{"schemaVersion":1,"status":"UNSUPPORTED","reason":"NO_SUPPORTED_ATOM"}""","test","m","1"))
        assertTrue(AnalyticIntent.decode("""{"schemaVersion":1,"status":"CLARIFY","reason":"AMBIGUOUS_SETTINGS"}""","test","m","1") is AnalyticIntent.Decision.Clarify)
    }
    @Test fun exportNativeRequestTemplateOrAuditAuthoredExamples() {
        System.getenv("LAYERTIME_INTENT_TEMPLATE")?.let {destination->
            val file=File(destination);require(!file.exists());file.parentFile?.mkdirs()
            val request=AnalyticIntent.request("PLACEHOLDER")
            file.writeText(FlowJson.write(mapOf("task" to request.task.name,"outputSchema" to request.outputSchema,
                "messages" to request.messages.map {mapOf("role" to it.role.name.lowercase(),"content" to it.content)})))
        }
        System.getenv("LAYERTIME_INTENT_EXAMPLES")?.let {source->
            File(source).readLines().filter {it.isNotBlank()}.forEach {line->
                val row=FlowJson.parse(line) as Map<*,*>
                val messages=row["messages"] as List<*>
                val prompt=(messages[messages.size-2] as Map<*,*>)["content"] as String
                checkExecution((messages.last() as Map<*,*>)["content"] as String,prompt)
                val native=AnalyticIntent.request(prompt)
                assertEquals(native.outputSchema,row["outputSchema"])
                assertEquals(native.messages.map {mapOf("role" to it.role.name.lowercase(),"content" to it.content)},messages.dropLast(1))
            }
        }
    }
}
