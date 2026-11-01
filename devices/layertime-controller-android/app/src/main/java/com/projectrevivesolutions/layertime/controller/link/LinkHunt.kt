package com.projectrevivesolutions.layertime.controller.link

/**
 * HUNT v1, the watch side (claude/hunt_v1_layervixen_plan.md): the HUNT_START,
 * HUNT_STOP and GET_HUNT requests and the HUNT_READING frame. The byte layout
 * matches src/core/link/LinkHunt.h on the LayerWand, and LinkHuntTest holds it to
 * the same bytes as test/test_link_hunt.
 *
 * v1 hunts one target: any LayerVixen (target kind 0xF0).
 */
object LinkHunt {
    const val OP_HUNT_START = 0x08
    const val OP_HUNT_STOP = 0x09
    const val OP_GET_HUNT = 0x0A
    const val TYPE_HUNT_READING = 0x8A
    const val READING_SIZE = 14
    const val TARGET_ANY_VIXEN = 0xF0
    const val CAP_HUNT = 0x0040
    const val FLAG_HUNTING = 0x40
    const val NO_READING = -128
    const val READING_HUNTING = 0x01
    const val READING_LOST = 0x02
    const val READING_LOCKED = 0x04
    /** RESULT.commandResult for HUNT_START. */
    const val STARTED = 0
    const val UNSUPPORTED_TARGET = 1

    fun isHuntOp(op: Int) = op == OP_HUNT_START || op == OP_HUNT_STOP || op == OP_GET_HUNT

    fun huntStart(reqId: Int, targetKind: Int = TARGET_ANY_VIXEN): ByteArray =
        byteArrayOf(OP_HUNT_START.toByte(), reqId.toByte(), targetKind.toByte(), 0, 0, 0, 0, 0, 0, 0)

    fun huntStop(reqId: Int): ByteArray = byteArrayOf(OP_HUNT_STOP.toByte(), reqId.toByte())
    fun getHunt(reqId: Int): ByteArray = byteArrayOf(OP_GET_HUNT.toByte(), reqId.toByte())

    /** One GET_HUNT answer. Signal fields are dBm, or null when there was no sighting. */
    data class Reading(
        val reqId: Int,
        val linkStatus: Int,
        val readings: Int,
        val latest: Int?,
        val strongest: Int?,
        val weakest: Int?,
        val mean: Int?,
        /** Null when the LayerVixen was never heard in this hunt; saturates at 65535. */
        val msSinceHeard: Int?,
        val channel: Int,
        val flags: Int,
    ) {
        val hunting get() = flags and READING_HUNTING != 0
        val lost get() = flags and READING_LOST != 0
    }

    fun isReading(frame: ByteArray) = frame.isNotEmpty() && (frame[0].toInt() and 0xFF) == TYPE_HUNT_READING

    fun decodeReading(frame: ByteArray): Reading? {
        if (frame.size != READING_SIZE || !isReading(frame)) return null
        fun u8(i: Int) = frame[i].toInt() and 0xFF
        fun dbm(i: Int) = frame[i].toInt().let { if (it == NO_READING) null else it } // signed byte
        val since = u8(9) or (u8(10) shl 8)
        return Reading(
            reqId = u8(1), linkStatus = u8(2), readings = u8(3) or (u8(4) shl 8),
            latest = dbm(5), strongest = dbm(6), weakest = dbm(7), mean = dbm(8),
            msSinceHeard = if (since == 0xFFFF) null else since, channel = u8(11), flags = u8(12),
        )
    }
}
