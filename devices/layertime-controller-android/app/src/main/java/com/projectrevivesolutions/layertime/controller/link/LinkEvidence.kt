package com.projectrevivesolutions.layertime.controller.link

import com.projectrevivesolutions.layertime.controller.flow.FrameFabricator
import java.io.ByteArrayOutputStream

/**
 * Link 0.2 evidence, the phone's side (slice 2, 2026-11-04). The wire format is
 * src/core/link/LinkEvidence.h on the Hunter; the byte vectors in
 * LinkEvidenceTest are the ones its host test sends.
 *
 * GET_FRAMES lists a detection's window (FRAME_WINDOW, one FRAME_INDEX per kept
 * frame, END). GET_FRAME_BYTES streams the window's bytes from (index, offset)
 * to its end (FRAME_BYTES..., END), at the connection's notification size.
 * The assembler checks every frame arrived whole, then writes the window as a
 * pcap (radiotap, link type 127) so the existing Wireshark import, the triage,
 * and the frame list all run on the Hunter's own evidence unchanged.
 */
object LinkEvidence {
    const val OP_GET_FRAMES = 0x06
    const val OP_GET_FRAME_BYTES = 0x07
    const val TYPE_FRAME_WINDOW = 0x87
    const val TYPE_FRAME_INDEX = 0x88
    const val TYPE_FRAME_BYTES = 0x89
    const val CAP_RAW_CAPTURE = 0x0020
    const val FLAG_TRUNCATED = 0x01
    const val FLAG_TRIGGER = 0x04

    fun isEvidenceOp(op: Int) = op == OP_GET_FRAMES || op == OP_GET_FRAME_BYTES
    fun isEvidenceFrame(b: ByteArray) = b.isNotEmpty() && (b[0].toInt() and 0xFF) in TYPE_FRAME_WINDOW..TYPE_FRAME_BYTES

    private fun put32(b: ByteArray, o: Int, v: Long) { for (i in 0..3) b[o + i] = (v shr (8 * i)).toByte() }
    private fun u8(b: ByteArray, o: Int) = b[o].toInt() and 0xFF
    private fun u16(b: ByteArray, o: Int) = u8(b, o) or (u8(b, o + 1) shl 8)
    private fun u32(b: ByteArray, o: Int) = u8(b, o).toLong() or (u8(b, o + 1).toLong() shl 8) or (u8(b, o + 2).toLong() shl 16) or (u8(b, o + 3).toLong() shl 24)

    fun getFrames(reqId: Int, eventId: Long): ByteArray =
        ByteArray(6).also { it[0] = OP_GET_FRAMES.toByte(); it[1] = reqId.toByte(); put32(it, 2, eventId) }

    fun getFrameBytes(reqId: Int, eventId: Long, index: Int, offset: Int): ByteArray =
        ByteArray(9).also {
            it[0] = OP_GET_FRAME_BYTES.toByte(); it[1] = reqId.toByte(); put32(it, 2, eventId)
            it[6] = index.toByte(); it[7] = offset.toByte(); it[8] = (offset shr 8).toByte()
        }

    data class Window(val reqId: Int, val eventId: Long, val seen: Int, val retained: Int, val dropped: Int,
                      val truncated: Int, val sourceKind: Int, val collecting: Boolean)
    data class Index(val reqId: Int, val index: Int, val channel: Int, val rssi: Int, val onAir: Int, val captured: Int,
                     val flags: Int, val offsetMs: Int) {
        val trigger get() = flags and FLAG_TRIGGER != 0
        val truncatedFlag get() = flags and FLAG_TRUNCATED != 0
    }
    data class Bytes(val reqId: Int, val index: Int, val offset: Int, val data: ByteArray)

    fun decodeWindow(b: ByteArray): Window? = if (b.size != 14 || u8(b, 0) != TYPE_FRAME_WINDOW) null else
        Window(u8(b, 1), u32(b, 2), u16(b, 6), u8(b, 8), u16(b, 9), u8(b, 11), u8(b, 12), (u8(b, 13) and 1) != 0)

    fun decodeIndex(b: ByteArray): Index? = if (b.size != 14 || u8(b, 0) != TYPE_FRAME_INDEX) null else
        Index(u8(b, 1), u8(b, 2), u8(b, 3), b[4].toInt(), u16(b, 5), u16(b, 7), u8(b, 9), u32(b, 10).toInt())

    fun decodeBytes(b: ByteArray): Bytes? = if (b.size < 6 || u8(b, 0) != TYPE_FRAME_BYTES) null else
        Bytes(u8(b, 1), u8(b, 2), u16(b, 3), b.copyOfRange(5, b.size))

    /** One frame of the window, whole. */
    data class Frame(val index: Index, val bytes: ByteArray)

    /**
     * Collects one pull's replies. complete() is true only when every listed frame's
     * bytes arrived in order with no gap; anything else names what is missing.
     */
    class Assembler(val eventId: Long) {
        var window: Window? = null; private set
        private val indexes = sortedMapOf<Int, Index>()
        private val parts = HashMap<Int, ByteArrayOutputStream>()
        var gaps = 0; private set
        val bytesReceived get() = parts.values.sumOf { it.size() }
        val bytesExpected get() = indexes.values.sumOf { it.captured }
        val listed get() = indexes.size

        fun onWindow(w: Window) { window = w }
        fun onIndex(i: Index) { indexes[i.index] = i }
        fun onBytes(b: Bytes) {
            val buf = parts.getOrPut(b.index) { ByteArrayOutputStream() }
            if (b.offset != buf.size()) { gaps++; return }   // out of order or repeated: refuse, don't splice
            buf.write(b.data)
        }
        /** Where a resumed GET_FRAME_BYTES should start: the first frame not yet whole, at its received length. */
        fun resumePoint(): Pair<Int, Int>? = indexes.values.firstOrNull { (parts[it.index]?.size() ?: 0) < it.captured }
            ?.let { it.index to (parts[it.index]?.size() ?: 0) }
        fun missing(): List<String> = indexes.values.mapNotNull { i ->
            val got = parts[i.index]?.size() ?: 0
            if (got == i.captured) null else "frame ${i.index}: $got of ${i.captured} bytes"
        }
        fun complete() = window != null && listed == window!!.retained && gaps == 0 && missing().isEmpty()
        fun frames(): List<Frame> = indexes.values.map { Frame(it, parts[it.index]?.toByteArray() ?: ByteArray(0)) }
    }

    /**
     * The window as a pcap, radiotap link type 127, one record per frame, with the
     * Hunter's channel and signal in the radiotap header. Times are the phone's
     * wall clock at `triggerWallMs` plus each frame's offset from the trigger.
     * A frame the Hunter cut at 1,024 bytes keeps its on-air length as orig_len.
     */
    fun toPcap(frames: List<Frame>, triggerWallMs: Long): ByteArray {
        val out = ByteArrayOutputStream()
        fun le16(v: Int) { out.write(v and 0xFF); out.write((v shr 8) and 0xFF) }
        fun le32(v: Long) { for (i in 0..3) out.write(((v shr (8 * i)) and 0xFF).toInt()) }
        le32(0xA1B2C3D4L); le16(2); le16(4); le32(0); le32(0); le32(65535); le32(127)
        for (f in frames) {
            val rt = FrameFabricator.radiotap(f.index.channel, f.index.rssi)
            val tMs = triggerWallMs + f.index.offsetMs
            le32(tMs / 1000); le32((tMs % 1000) * 1000)
            le32((rt.size + f.bytes.size).toLong()); le32((rt.size + maxOf(f.index.onAir, f.bytes.size)).toLong())
            out.write(rt); out.write(f.bytes)
        }
        return out.toByteArray()
    }
}
