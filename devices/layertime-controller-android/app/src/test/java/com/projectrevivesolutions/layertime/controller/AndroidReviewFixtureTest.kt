package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.ai.*
import com.projectrevivesolutions.layertime.controller.flow.*
import org.junit.Assert.*
import org.junit.Test
import java.io.File
import java.nio.ByteBuffer
import java.nio.ByteOrder

/** Offline captures for repeatable operator checks. Never opens a radio interface. */
class AndroidReviewFixtureTest {
    private val ap="02:00:00:00:01:01"
    private val other="02:00:00:00:02:02"
    private val client="02:00:00:00:03:03"
    private val t=1_700_000_000_000_000_000L
    private fun fixture(network:String=ap,requests:Boolean=true)=FrameFabricator().apply {
        beacon(t,ap,"TRAINING-Shared",1,6,-50)
        beacon(t+100_000_000,other,"TRAINING-Shared",2,6,-50)
        if(requests)for(i in 0..2)deauth(t+1_000_000_000+i*100_000_000,ap,client,ap,7,3+i,6,-50)
        eapol(t+1_500_000_000,network,client,network,6,6,-50)
    }
    private fun le(vararg values:Int)=ByteBuffer.allocate(values.size*4).order(ByteOrder.LITTLE_ENDIAN).apply {
        values.forEach {putInt(it)}
    }.array()
    private fun block(type:Int,body:ByteArray):ByteArray {
        val padded=body+ByteArray((4-body.size%4)%4)
        return le(type,padded.size+12)+padded+le(padded.size+12)
    }
    private fun withoutLastTimestamp(source:FrameFabricator):ByteArray {
        val pcap=ByteBuffer.wrap(source.pcap()).order(ByteOrder.LITTLE_ENDIAN)
        pcap.position(24)
        var result=block(0x0a0d0d0a,le(0x1a2b3c4d,1,-1,-1))+block(1,le(127,65535))
        var index=0
        while(pcap.hasRemaining()) {
            val seconds=pcap.int.toLong() and 0xffffffffL
            val micros=pcap.int.toLong() and 0xffffffffL
            val size=pcap.int;pcap.int
            val packet=ByteArray(size);pcap.get(packet)
            val padded=packet+ByteArray((4-size%4)%4)
            result+=if(index==source.built.lastIndex)block(3,le(size)+padded)
            else {
                val timestamp=seconds*1_000_000+micros
                block(6,le(0,(timestamp ushr 32).toInt(),timestamp.toInt(),size,size)+padded)
            }
            index++
        }
        return result
    }
    @Test fun exportFourRepeatableOfflineCapturesAndExpectedFacts() {
        val linked=fixture();val different=fixture(other);val ordinary=fixture(requests=false)
        val cases=listOf(
            Triple("same-pair.pcap",linked.built,linked.pcap()),
            Triple("different-ap-same-name.pcap",different.built,different.pcap()),
            Triple("exchange-only.pcap",ordinary.built,ordinary.pcap()),
            Triple("untimed-exchange.pcapng",linked.built.mapIndexed {i,f->if(i==linked.built.lastIndex)f.copy(timestampValid=false) else f},withoutLastTimestamp(linked)))
        val expected=cases.map { (name,frames,bytes)->
            val evidence=CaptureEvidence.index("TRAINING-$name",frames,
                EvidenceCoverage("TRAINING-$name",frames.size,frames.size,training=true))
            val facts=GroundedReviewTasks.facts(evidence,evidence.pages(128).single())
            when(name) {
                "same-pair.pcap"->{assertEquals(1,facts.size);assertEquals(3L,facts.single().claims["disconnectCount"]);assertEquals(1L,facts.single().claims["linkedEapolCount"])}
                "different-ap-same-name.pcap"->{assertEquals(0L,facts.single().claims["linkedEapolCount"]);assertEquals("DIFFERENT_ACCESS_POINTS",facts.single().claims["networkMatch"])}
                "exchange-only.pcap"->assertTrue(facts.isEmpty())
                else->{assertEquals(0L,facts.single().claims["linkedEapolCount"]);assertEquals(1L,facts.single().claims["untimedEapolContextCount"])}
            }
            mapOf("file" to "TRAINING-$name","frames" to frames.size,"sha256" to java.security.MessageDigest.getInstance("SHA-256").digest(bytes).joinToString(""){"%02x".format(it)},
                "claims" to facts.map {it.claims})
        }
        val directory=System.getenv("LAYERTIME_ANDROID_REVIEW_EXPORT") ?: return
        val folder=File(directory);require(!folder.exists()) {"Preserve earlier test packs."};check(folder.mkdirs())
        cases.forEach {(name,_,bytes)->File(folder,"TRAINING-$name").writeBytes(bytes)}
        File(folder,"expected.json").writeText(FlowJson.write(mapOf("purpose" to "Synthetic offline operator test captures; never transmitted",
            "nativeAndroidDecodeVerified" to false,"cases" to expected)))
    }
}
