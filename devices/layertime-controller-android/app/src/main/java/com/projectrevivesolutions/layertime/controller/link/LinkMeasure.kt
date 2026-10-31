package com.projectrevivesolutions.layertime.controller.link

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

/**
 * Link 0.2 Phase 0 measurement, the watch side (claude/link_0_2_phase0_plan.md,
 * M5): a PING burst for the request round trip, which sets the fastest HUNT poll
 * rate, and the test-only BULK op for throughput at 20 bytes and at a larger
 * MTU. Only a LayerWand running the measurement firmware (tdongle_c5_measure)
 * answers BULK; any other Node answers ERROR UnknownOp, which the panel reports.
 *
 * The frame layout matches src/core/link/LinkMeasure.h on the Node. The pure
 * parts are unit-tested (LinkMeasureTest).
 */
object LinkMeasure {
    const val OP_BULK = 0xF2
    const val BULK_REQUEST_SIZE = 5
    const val BULK_HEADER_SIZE = 6
    const val BULK_DONE_SEQ = 0xFFFF
    const val BULK_DONE_SIZE = 17
    const val MTU_REQUEST = 247

    fun bulk(reqId: Int, count: Int, size: Int): ByteArray = byteArrayOf(
        OP_BULK.toByte(), reqId.toByte(), (count and 0xFF).toByte(), ((count shr 8) and 0xFF).toByte(), size.toByte())

    private fun u16(b: ByteArray, i: Int) = (b[i].toInt() and 0xFF) or ((b[i + 1].toInt() and 0xFF) shl 8)
    private fun u32(b: ByteArray, i: Int) = (u16(b, i).toLong()) or (u16(b, i + 2).toLong() shl 16)

    fun isBulk(frame: ByteArray) = frame.isNotEmpty() && (frame[0].toInt() and 0xFF) == OP_BULK

    /** A BULK data frame with its header and every pattern byte intact. */
    fun dataFrameIntact(frame: ByteArray): Boolean {
        if (frame.size < BULK_HEADER_SIZE || !isBulk(frame) || u16(frame, 2) == BULK_DONE_SEQ) return false
        for (i in BULK_HEADER_SIZE until frame.size) if ((frame[i].toInt() and 0xFF) != (i and 0xFF)) return false
        return true
    }

    fun dataSeq(frame: ByteArray): Int = u16(frame, 2)

    data class Done(val reqId: Int, val sent: Int, val size: Int, val mtu: Int, val refused: Long, val elapsedMs: Long)

    fun decodeDone(frame: ByteArray): Done? {
        if (frame.size != BULK_DONE_SIZE || !isBulk(frame) || u16(frame, 2) != BULK_DONE_SEQ) return null
        return Done(frame[1].toInt() and 0xFF, u16(frame, 4), frame[6].toInt() and 0xFF, u16(frame, 7),
            u32(frame, 9), u32(frame, 13))
    }

    data class Stats(val n: Int, val min: Double, val median: Double, val p95: Double, val max: Double)

    /** Nearest-rank statistics, or null for an empty list. */
    fun stats(values: List<Double>): Stats? {
        if (values.isEmpty()) return null
        val s = values.sorted()
        fun rank(p: Double) = s[(Math.ceil(p * s.size).toInt() - 1).coerceIn(0, s.size - 1)]
        return Stats(s.size, s.first(), rank(0.5), rank(0.95), s.last())
    }

    /** What the LINK MEASURE panel shows; one line per finished measurement. */
    data class State(val running: String? = null, val mtu: Int = 23, val lines: List<String> = emptyList())

    private val _state = MutableStateFlow(State())
    val state: StateFlow<State> = _state.asStateFlow()

    internal fun setRunning(what: String?) { _state.value = _state.value.copy(running = what) }
    internal fun setMtu(mtu: Int) { _state.value = _state.value.copy(mtu = mtu) }
    internal fun addLine(line: String) {
        _state.value = _state.value.copy(running = null, lines = (_state.value.lines + line).takeLast(12))
    }
    fun clear() { _state.value = State(mtu = _state.value.mtu) }
}
