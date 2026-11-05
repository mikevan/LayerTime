package com.projectrevivesolutions.layertime.controller.link

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

/**
 * Link 0.2 card ops (2026-11-04): pulling the Hunter's SD card files over the Link,
 * so field analysis needs no computer. Mirrors src/core/link/LinkCard.h; the bytes
 * are pinned by LinkCardTest against the C++ tests. Pure, host-tested.
 *
 *   CARD_LIST 0x0B op reqId -> CARD_ENTRY 0x8B per file, END
 *   CARD_READ 0x0C op reqId kind serial(u16) eventId(u32) offset(u32)
 *     -> CARD_BYTES 0x8C (type reqId offset(u32) bytes) to the end, CARD_DONE 0x8D, END
 *   CARD_ENTRY: type reqId kind serial(u16) eventId(u32) size(u32)
 *   CARD_DONE:  type reqId size(u32) crc32(u32), the CRC of the whole file
 */
object LinkCard {
    const val OP_CARD_LIST = 0x0B
    const val OP_CARD_READ = 0x0C
    const val TYPE_CARD_ENTRY = 0x8B
    const val TYPE_CARD_BYTES = 0x8C
    const val TYPE_CARD_DONE = 0x8D
    const val CAP_CARD_READ = 0x0200
    const val KIND_LOG = 0
    const val KIND_PCAP = 1

    fun isCardOp(op: Int) = op == OP_CARD_LIST || op == OP_CARD_READ
    fun isCardFrame(b: ByteArray) = b.isNotEmpty() && (b[0].toInt() and 0xFF) in TYPE_CARD_ENTRY..TYPE_CARD_DONE

    data class FileId(val kind: Int, val serial: Int, val eventId: Long) {
        val valid: Boolean get() = serial in 1..9999 && when (kind) {
            KIND_LOG -> eventId == 0L
            KIND_PCAP -> eventId in 1..0xFFFFFFFFL
            else -> false
        }
        /** "layerwand_0007.log" or "ev_0007/event_00042.pcap". */
        val name: String get() = if (kind == KIND_LOG) "layerwand_%04d.log".format(serial)
            else "ev_%04d/event_%05d.pcap".format(serial, eventId)
    }

    data class Entry(val reqId: Int, val id: FileId, val size: Long)
    data class Done(val reqId: Int, val size: Long, val crc32: Long)

    private fun u16(b: ByteArray, o: Int) = (b[o].toInt() and 0xFF) or ((b[o + 1].toInt() and 0xFF) shl 8)
    private fun u32(b: ByteArray, o: Int): Long =
        (0..3).fold(0L) { acc, i -> acc or ((b[o + i].toLong() and 0xFF) shl (8 * i)) }
    private fun put32(b: ByteArray, o: Int, v: Long) { for (i in 0..3) b[o + i] = ((v shr (8 * i)) and 0xFF).toByte() }

    fun cardList(reqId: Int): ByteArray = byteArrayOf(OP_CARD_LIST.toByte(), reqId.toByte())

    fun cardRead(reqId: Int, id: FileId, offset: Long): ByteArray {
        val b = ByteArray(13)
        b[0] = OP_CARD_READ.toByte(); b[1] = reqId.toByte(); b[2] = id.kind.toByte()
        b[3] = (id.serial and 0xFF).toByte(); b[4] = (id.serial shr 8).toByte()
        put32(b, 5, id.eventId); put32(b, 9, offset)
        return b
    }

    fun decodeEntry(b: ByteArray): Entry? = if (b.size == 13 && (b[0].toInt() and 0xFF) == TYPE_CARD_ENTRY)
        Entry(b[1].toInt() and 0xFF, FileId(b[2].toInt() and 0xFF, u16(b, 3), u32(b, 5)), u32(b, 9)) else null

    /** CARD_BYTES: the offset and the bytes. */
    fun decodeBytes(b: ByteArray): Pair<Long, ByteArray>? = if (b.size >= 6 && (b[0].toInt() and 0xFF) == TYPE_CARD_BYTES)
        u32(b, 2) to b.copyOfRange(6, b.size) else null

    fun decodeDone(b: ByteArray): Done? = if (b.size == 10 && (b[0].toInt() and 0xFF) == TYPE_CARD_DONE)
        Done(b[1].toInt() and 0xFF, u32(b, 2), u32(b, 6)) else null

    private val CRC_TABLE = LongArray(256) { n ->
        var c = n.toLong()
        repeat(8) { c = if (c and 1L != 0L) 0xEDB88320L xor (c ushr 1) else c ushr 1 }
        c
    }
    /** CRC-32 (IEEE), the Hunter's. */
    fun crc32(data: ByteArray, start: Long = 0L): Long {
        var c = start.inv() and 0xFFFFFFFFL
        for (x in data) c = CRC_TABLE[((c xor (x.toLong() and 0xFF)) and 0xFF).toInt()] xor (c ushr 8)
        return c.inv() and 0xFFFFFFFFL
    }

    /**
     * One file arriving. Chunks must arrive in order (offset = bytes held); an
     * out-of-order chunk is refused and counted, so the file is never silently
     * wrong. A resumed read starts at [received].
     */
    class Assembler(val id: FileId, val expectedSize: Long, startWith: ByteArray = ByteArray(0)) {
        private val out = java.io.ByteArrayOutputStream().apply { write(startWith) }
        var refused = 0; private set
        var done: Done? = null; private set
        val received: Long get() = out.size().toLong()
        fun onBytes(offset: Long, bytes: ByteArray) {
            if (offset != received) { refused++; return }
            out.write(bytes)
        }
        fun onDone(d: Done) { done = d }
        fun bytes(): ByteArray = out.toByteArray()
        /** Null when the file is whole and its size and CRC match CARD_DONE, else the reason. */
        fun problem(): String? {
            val d = done ?: return "The Hunter did not finish sending ${id.name}."
            val b = bytes()
            if (b.size.toLong() != d.size) return "${id.name} arrived with ${b.size} of ${d.size} bytes."
            if (crc32(b) != d.crc32) return "${id.name} does not match the Hunter's checksum."
            return null
        }
    }
}

/** Where a card pull stands (2026-11-04). LinkClient drives it; HUNTER CARD watches it. */
object CardPull {
    sealed interface State {
        data object Idle : State
        data class Listing(val note: String = "Asking the Hunter for its card list.") : State
        data class Pulling(val fileIndex: Int, val files: Int, val name: String, val bytes: Long, val size: Long) : State
        data class Done(val pulled: Int, val upToDate: Int, val failed: List<String>, val elapsedMs: Long) : State
        data class Failed(val message: String) : State
    }
    private val _state = MutableStateFlow<State>(State.Idle)
    val state: StateFlow<State> = _state.asStateFlow()
    internal fun set(s: State) { _state.value = s }
    fun reset() { _state.value = State.Idle }
}
