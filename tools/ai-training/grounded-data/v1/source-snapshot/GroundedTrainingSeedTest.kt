package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.ai.*
import com.projectrevivesolutions.layertime.controller.flow.*
import org.junit.Assert.*
import org.junit.Test
import java.io.File

/** Original scenario families are split before variants/pages. No field recordings or attack labels. */
class GroundedTrainingSeedTest {
    private data class Scenario(val name:String,val split:String,val expected:List<Pair<Int,Int>>,
                                val frames:(Int)->List<AnalyticFrame>,val gap:Long=3000,val follow:Long=30000)
    @Test fun exportEngineCheckedGroundedConversations() {
        val ap="02:00:00:00:01:01";val b="02:00:00:00:02:02";val c="02:00:00:00:03:03";val d="02:00:00:00:04:04"
        fun f(i:Int,ms:Long,e:Boolean=false,net:String?=ap,src:String=ap,dst:String=c)=AnalyticFrame(i,
            1_700_000_000_000_000_000L+ms*1_000_000,if(e)AnalyticFrame.DATA else AnalyticFrame.DEAUTH,
            src,dst,net,eapol=e,reasonCode=if(e)null else 7)
        val cases=listOf(
            Scenario("one_request","train",listOf(1 to 0),{listOf(f(0,0))}),
            Scenario("same_pair","train",listOf(2 to 1),{listOf(f(0,0),f(1,100),f(2,300,true))}),
            Scenario("reverse_exchange","train",listOf(1 to 1),{listOf(f(0,0),f(1,500,true,src=c,dst=ap))}),
            Scenario("other_ap","train",listOf(1 to 0),{listOf(f(0,0),f(1,500,true,b,b,c))}),
            Scenario("other_client","train",listOf(1 to 0),{listOf(f(0,0),f(1,500,true,dst=d))}),
            Scenario("broadcast","train",listOf(1 to 0),{listOf(f(0,0,dst=AnalyticFrame.BROADCAST),f(1,500,true))}),
            Scenario("before","train",listOf(1 to 0),{listOf(f(0,0,true),f(1,500))}),
            Scenario("untimed_exchange","train",listOf(1 to 0),{listOf(f(0,0),f(1,500,true).copy(timestampValid=false))}),
            Scenario("malformed_request","train",emptyList(),{listOf(f(0,0).copy(malformed=true),f(1,500,true))}),
            Scenario("ordinary_exchange_only","train",emptyList(),{listOf(f(0,0,true),f(1,500,true))}),
            Scenario("separate_episodes","train",listOf(1 to 1,1 to 1),{listOf(f(0,0),f(1,500,true),f(2,10000),f(3,11000,true))}),
            Scenario("unresolved_exchange_ap","train",listOf(1 to 0),{listOf(f(0,0),f(1,500,true,net=null))}),
            Scenario("equal_time","validation",listOf(1 to 0),{listOf(f(0,0),f(1,0,true))}),
            Scenario("interleaved","validation",listOf(3 to 1),{listOf(f(0,0),f(1,100),f(2,150,true),f(3,200))}),
            Scenario("two_targets","validation",listOf(1 to 1,1 to 0),{listOf(f(0,0),f(1,0,dst=d),f(2,500,true))}),
            Scenario("unknown_request_ap","validation",listOf(1 to 0),{listOf(f(0,0,net=null),f(1,500,true))}),
            // Held out from this contract's training; related behaviors occurred in earlier diagnostics.
            Scenario("three_targets_overlapping","test",listOf(1 to 1,1 to 1,1 to 0),{listOf(f(0,0),f(1,0,dst=d),f(2,0,dst=b),f(3,250,true),f(4,400,true,dst=b))}),
            Scenario("follow_on_expired","test",listOf(1 to 0),{listOf(f(0,0),f(1,1001,true))},follow=1000),
            Scenario("boundary_follow_on","test",listOf(1 to 1),{listOf(f(0,0),f(1,1000,true))},follow=1000),
            Scenario("untimed_other_client","test",listOf(1 to 0),{listOf(f(0,0),f(1,100,true,dst=d).copy(timestampValid=false))}),
            Scenario("valid_plus_untimed","test",listOf(1 to 1),{listOf(f(0,0),f(1,100,true),f(2,200,true).copy(timestampValid=false))}),
            Scenario("all_request_clocks_invalid","test",emptyList(),{listOf(f(0,0).copy(timestampValid=false),f(1,100).copy(timestampValid=false))}),
            Scenario("merged_requests_reversed_exchange","test",listOf(2 to 2),{listOf(f(0,0),f(1,100,true,src=c,dst=ap),f(2,5000),f(3,5100,true,src=c,dst=ap))},gap=6000),
            Scenario("two_aps_shared_name","test",listOf(1 to 0),{listOf(
                f(0,0).copy(typeSubtype=AnalyticFrame.BEACON,ssid="CampNet",da=AnalyticFrame.BROADCAST),
                f(1,10,net=b,src=b).copy(typeSubtype=AnalyticFrame.BEACON,ssid="CampNet",da=AnalyticFrame.BROADCAST),
                f(2,100),f(3,500,true,b,b,c))})
        )
        val rows=arrayListOf<Map<String,Any?>>()
        for(case in cases)for(variant in 0..3) {
            // Relabel observations without changing their behavior; variants are not new scenarios.
            val fs=case.frames(variant).map {it.copy(index=it.index+variant*100,
                timeNanos=it.timeNanos+variant*1_000_000_000L,
                sa=it.sa?.replace("02:00:","02:0$variant:"),da=it.da?.replace("02:00:","02:0$variant:"),
                bssid=it.bssid?.replace("02:00:","02:0$variant:"))}
            val token=java.security.MessageDigest.getInstance("SHA-256").digest("${case.name}/$variant".toByteArray())
                .take(8).joinToString(""){"%02x".format(it.toInt() and 255)}
            val capture="g1-$token"
            val evidence=CaptureEvidence.index(capture,fs,followOnMs=case.follow,episodeGapMs=case.gap)
            assertEquals(case.name,case.expected,evidence.episodes.map {it.disconnectFrames.size to it.followOnFrames.size})
            for(page in evidence.pages(2)) {
                val request=GroundedReviewTasks.request("Check the supplied recording facts and preserve unresolved evidence.",evidence,page)
                val facts=GroundedReviewTasks.facts(evidence,page)
                if(case.name=="interleaved" && facts.isNotEmpty())assertEquals(50L,facts.single().claims["linkedDelayMinMs"])
                if(case.name=="untimed_other_client" && facts.isNotEmpty())assertEquals(0L,facts.single().claims["untimedEapolContextCount"])
                if(case.name=="valid_plus_untimed" && facts.isNotEmpty())assertEquals(1L,facts.single().claims["untimedEapolContextCount"])
                val target=FlowJson.write(mapOf("schemaVersion" to 2L,"captureId" to capture,"page" to page.number.toLong(),
                    "assessments" to facts.map {mapOf("factId" to it.id,"claims" to it.claims)}))
                assertEquals(facts.size,GroundedReviewTasks.decode(target,evidence,page).size)
                rows+=mapOf("id" to "grounded-v1-${case.name}-$variant-page-${page.number}","family" to case.name,"split" to case.split,
                    "sourceId" to "layertime-grounded-synthesis-v1","task" to "REVIEW_GROUNDED_CAPTURE",
                    "labelStatus" to "native-engine-checked-contract-target-not-attack-ground-truth",
                    "auditFollowOnMs" to case.follow,"auditEpisodeGapMs" to case.gap,
                    "auditPageIds" to page.frames.map {it.index.toLong()},
                    "auditObservations" to fs.map {f->mapOf("frameId" to f.index.toLong(),"timeNanos" to f.timeNanos,
                        "timestampValid" to f.timestampValid,"malformed" to f.malformed,"typeSubtype" to f.typeSubtype.toLong(),
                        "source" to f.sa,"destination" to f.da,"network" to f.bssid,"eapol" to f.eapol)},
                    "outputSchema" to request.outputSchema,
                    "messages" to request.messages.map {mapOf("role" to it.role.name.lowercase(),"content" to it.content)}+
                        listOf(mapOf("role" to "assistant","content" to target)))
            }
        }
        assertEquals(24,rows.map {it["family"]}.distinct().size)
        System.getenv("LAYERTIME_GROUNDED_EXPORT")?.let { destination->
            val file=File(destination);require(!file.exists()){"Use a fresh export path"};file.parentFile?.mkdirs()
            file.writeText(rows.joinToString("\n",postfix="\n"){FlowJson.write(it)})
        }
    }
}
