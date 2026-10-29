package com.projectrevivesolutions.layertime.controller.wireshark

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

/** Capture import: record parsing, the decode loop, the test capture file, and the stability verdict. */
class WiresharkCaptureImportTest {

    // Record set in the layout ltws_pcap_next emits (header, hex, then 6-column tree records).
    private val record = listOf(
        "#frame\t1\t20\tIEEE 802.11 Wireless LAN\t26\t26\t1\t1700000000\t0\t24",
        "#hex\tc0003a01ffffffffffff00112233aabb00112233aabb70000700",
        "0\tframe\tFrame 1: 26 bytes on wire\t0\t26\tFrame 1: 26 bytes on wire (208 bits)",
        "0\twlan\tIEEE 802.11 Deauthentication, Flags: ........\t0\t24\tIEEE 802.11 Deauthentication, Flags: ........",
        "1\twlan.fc.type_subtype\t0x000c\t0\t1\tType/Subtype: Deauthentication (0x000c)",
        "1\twlan.sa\t00:11:22:33:aa:bb\t10\t6\tSource address: 00:11:22:33:aa:bb (00:11:22:33:aa:bb)",
        "1\twlan.da\tff:ff:ff:ff:ff:ff\t4\t6\tDestination address: Broadcast (ff:ff:ff:ff:ff:ff)",
        "2\twlan.fixed.reason_code\t0x0007\t24\t2\tReason code: Class 3 frame received from nonassociated STA (0x0007)",
    ).joinToString("\n")

    @Test
    fun parseFrameReadsHeaderHexAndTree() {
        val f = WiresharkDecoder.parseFrame(record)!!
        assertEquals(1, f.number)
        assertEquals(20, f.encap)
        assertEquals("IEEE 802.11 Wireless LAN", f.encapName)
        assertEquals(26, f.capturedLength)
        assertTrue(f.hasTimestamp)
        assertEquals(1700000000L, f.epochSeconds)
        assertEquals(24L, f.fileOffset)
        assertEquals(6, f.fields.size)
        val reason = f.first("wlan.fixed.reason_code")!!
        assertEquals("0x0007", reason.display)
        assertEquals(24, reason.start); assertEquals(2, reason.length)
        assertEquals("Class 3 frame received from nonassociated STA (0x0007)", reason.shown)
        assertEquals("Deauthentication (0x000c)", f.title)
        assertEquals("00:11:22:33:aa:bb to ff:ff:ff:ff:ff:ff", f.addresses)
        assertFalse(f.malformed)
    }

    @Test
    fun parseFrameRejectsMissingHeader() {
        assertNull(WiresharkDecoder.parseFrame("1\twlan.sa\tx\t0\t6\tSA: x"))
    }

    @Test
    fun malformedNodeIsDetected() {
        val f = WiresharkDecoder.parseFrame(record + "\n0\t_ws.malformed\t[Malformed Packet: 802.11]\t0\t0\t[Malformed Packet: 802.11]")!!
        assertTrue(f.malformed)
    }

    @Test
    fun shownFallsBackForProtocolNodesAndOldFiveColumnRecords() {
        val f = WiresharkDecoder.parseFrame(record)!!
        assertEquals("IEEE 802.11 Deauthentication, Flags: ........", f.first("wlan")!!.shown)
        val old = WiresharkDecoder.parseTree("1\twlan.seq\t7\t22\t2")
        assertEquals("7", old.single().shown)
    }

    @Test
    fun hexDumpHasOffsetsAndSixteenBytesPerLine() {
        val f = WiresharkDecoder.parseFrame(record)!!
        val lines = WiresharkDecoder.hexDump(f.hex)
        assertEquals(2, lines.size)
        assertEquals("0000  c0 00 3a 01 ff ff ff ff ff ff 00 11 22 33 aa bb", lines[0])
        assertEquals("0010  00 11 22 33 aa bb 70 00 07 00", lines[1])
    }

    @Test
    fun testCaptureFileMatchesTheFramesTheAppWrites() {
        // Gradle runs unit tests with the module directory (app/) as the working directory.
        val file = File("../wireshark-spike/lt_test_5frames.pcap")
        assertTrue("missing ${file.absolutePath}", file.exists())
        assertArrayEquals(file.readBytes(), WiresharkDecoder.pcapBytes(WiresharkDecoder.TEST_FRAMES))
        assertEquals(20, WiresharkDecoder.TEST_FRAMES.last().size)
    }

    // ---- decode loop ----

    private fun frame(n: Int, offset: Long, malformed: Boolean = false) = WiresharkDecoder.PcapFrame(
        n, 20, "IEEE 802.11 Wireless LAN", 26, 26, true, 1700000000L + n, 0, offset, "",
        if (malformed) listOf(WiresharkDecoder.Field(0, "_ws.malformed", "[Malformed]", 0, 0, "[Malformed]")) else emptyList())

    private class FakeSource(val frames: List<WiresharkDecoder.PcapFrame>, override val fileSize: Long,
                             val errorAtEnd: String? = null) : CaptureDecodeLoop.FrameSource {
        var i = 0
        var closed = false
        override val error: String? get() = if (i >= frames.size) errorAtEnd else null
        override val fileType = "pcap"
        override fun next() = frames.getOrNull(i)?.also { i++ }
        override fun close() { closed = true }
    }

    @Test
    fun loopCountsEveryFrameButKeepsOnlyTheLimit() {
        val src = FakeSource((1..20).map { frame(it, it * 10L, malformed = it % 7 == 0) }, 200)
        var t = 0L
        val r = CaptureDecodeLoop.run(src, keepLimit = 5, cancelled = { false }, nowNs = { t += 1_000_000; t }, onBeat = {})
        assertEquals(20, r.totalFrames)
        assertEquals(5, r.kept.size)
        assertEquals(2, r.malformedFrames)
        assertFalse(r.cancelled)
        assertNull(r.readError)
    }

    @Test
    fun loopStopsOnCancelAndReportsNoError() {
        val src = FakeSource((1..100).map { frame(it, it.toLong()) }, 100, errorAtEnd = "should not be reported")
        var t = 0L
        val r = CaptureDecodeLoop.run(src, 500, cancelled = { src.i >= 10 }, nowNs = { t += 1; t }, onBeat = {})
        assertTrue(r.cancelled)
        assertEquals(10, r.totalFrames)
        assertNull(r.readError)
    }

    @Test
    fun loopReportsReadError() {
        val src = FakeSource((1..3).map { frame(it, it.toLong()) }, 3, errorAtEnd = "Read error after frame 3: truncated")
        var t = 0L
        val r = CaptureDecodeLoop.run(src, 500, cancelled = { false }, nowNs = { t += 1; t }, onBeat = {})
        assertEquals(3, r.totalFrames)
        assertEquals("Read error after frame 3: truncated", r.readError)
    }

    @Test
    fun heartbeatsArriveEachIntervalAndFlagSlowing() {
        // 1 frame per ms for 6,000 frames, then 1 frame per 10 ms: rate falls to a tenth.
        val frames = (1..6600).map { frame(it, it.toLong()) }
        val src = FakeSource(frames, 6600)
        var t = 0L
        val beats = ArrayList<CaptureDecodeLoop.Beat>()
        CaptureDecodeLoop.run(src, 10, cancelled = { false },
            nowNs = { t += if (src.i <= 6000) 1_000_000L else 10_000_000L; t },
            beatIntervalNs = 5_000_000_000L, onBeat = { beats.add(it) })
        assertTrue("expected periodic beats, got ${beats.size}", beats.size >= 3)
        assertFalse(beats.first().slowing)
        assertTrue(beats.any { it.slowing && !it.last })
        assertTrue(beats.last().last)
        assertEquals(6600, beats.last().frames)
        assertEquals(100, beats.last().percentOfFile)
        assertTrue(beats.first().line().startsWith("HEARTBEAT pcap "))
        assertTrue(beats.first().line().contains("% of file)"))
    }

    @Test
    fun percentIsClampedAndUnknownWithoutSize() {
        assertEquals(50, CaptureDecodeLoop.percentOf(50, 100))
        assertEquals(100, CaptureDecodeLoop.percentOf(150, 100))
        assertNull(CaptureDecodeLoop.percentOf(10, -1))
    }

    // ---- stability verdict (diagnostic threshold) ----

    @Test
    fun noiseLevelNativeChangeIsWithinThreshold() {
        // Build 16 on the TCL: +7 KB over 20,000 decodes.
        assertFalse(StabilityVerdict.nativeGrewAboveThreshold(7, 20_000))
        val v = StabilityVerdict.verdict(false, 0, 7, 20_000, 0)
        assertTrue(v, v.startsWith("native heap change +7 KB (+0.4 bytes per iteration) is within the diagnostic threshold"))
    }

    @Test
    fun realGrowthIsReported() {
        assertTrue(StabilityVerdict.nativeGrewAboveThreshold(200, 20_000))          // 10 bytes per iteration
        assertFalse(StabilityVerdict.nativeGrewAboveThreshold(100, 1_000_000))      // 0.1 bytes per iteration
        assertFalse(StabilityVerdict.nativeGrewAboveThreshold(60, 1_000))           // under 64 KB
        assertTrue(StabilityVerdict.verdict(false, 0, 200, 20_000, 0).startsWith("native heap grew +200 KB"))
    }

    @Test
    fun oneOffStepIsNotSteadyGrowth() {
        // LOKMAT watch, build 17: a +300 KB step between 6,000 and 8,000, flat afterwards.
        val samples = listOf(2000 to -121L, 4000 to -101L, 6000 to -135L, 8000 to 301L, 10000 to 164L,
            12000 to 271L, 14000 to 254L, 16000 to 113L, 18000 to 114L, 20000 to 115L)
        val t = StabilityVerdict.secondHalfTrend(samples)
        assertEquals(5, t.samples)
        assertFalse(t.steadyGrowth)
        val v = StabilityVerdict.verdict(false, 0, 115, 20_000, 0, t)
        assertTrue(v, v.contains("a one-off step, not steady growth"))
    }

    @Test
    fun steadyClimbIsReportedEvenWhenSmall() {
        // 20 bytes per iteration throughout: start-to-end is +400 KB, trend agrees.
        val samples = (1..10).map { it * 2000 to (it * 2000L * 20 / 1024) }
        val t = StabilityVerdict.secondHalfTrend(samples)
        assertTrue(t.steadyGrowth)
        assertTrue(StabilityVerdict.verdict(false, 0, 390, 20_000, 0, t).startsWith("native heap grew"))
    }

    @Test
    fun fallIsWordedAsRelease() {
        val v = StabilityVerdict.verdict(false, 0, -6163, 4_000, 0)
        assertTrue(v, v.startsWith("native heap fell 6,163 KB"))
    }

    @Test
    fun mismatchesWinAndRssDropsAreMentioned() {
        assertTrue(StabilityVerdict.verdict(false, 3, 0, 100, 0).startsWith("OUTPUT MISMATCHES (3 of 100)"))
        assertTrue(StabilityVerdict.verdict(false, 0, 1, 100, 1).contains("RSS fell by more than"))
        val line = StabilityVerdict.rssDropLine(6000, 8000, -80532, -12020, -12720, -54024, 79459, -497)
        assertTrue(line, line.contains("RSS fell 80,532 KB between iterations 6000 and 8000"))
        assertTrue(line.contains("The cause was not verified"))
    }
}
