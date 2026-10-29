package com.projectrevivesolutions.layertime.controller.link

/**
 * Decoded LayerTime Link 0.1 values. Pure Kotlin, no Android dependencies, so
 * the codec that produces them can be compiled and tested on a host JVM
 * against contracts/vectors/link_frames.json.
 */

/** The 18-byte Status snapshot (contracts/link.md "Status snapshot"). */
data class StatusSnapshot(
    val linkVersion: Int,
    val sessionId: Int,
    val changeSeq: Long,
    val flags: Int,
    val selected: Int,
    val active: Int,
    val eventCount: Int,
    val lastAlertEventId: Long,
    val heartbeat: Int,
    val schedule: Int,
    val nextReportS: Int,
) {
    val monitoring get() = flags and LinkConstants.StatusFlag.MONITORING != 0
    val alertPending get() = flags and LinkConstants.StatusFlag.ALERT_PENDING != 0
    val sleepMode get() = flags and LinkConstants.StatusFlag.SLEEP_MODE != 0
    val noSdLog get() = flags and LinkConstants.StatusFlag.NO_SD_LOG != 0
    val linkMajor get() = (linkVersion shr 4) and 0x0F
    val linkMinor get() = linkVersion and 0x0F
}

/** A decoded Data-characteristic reply frame. */
sealed class Frame {
    abstract val reqId: Int
    abstract val linkStatus: Int

    data class HelloAck(
        override val reqId: Int,
        override val linkStatus: Int,
        val serverMajor: Int,
        val serverMinor: Int,
        val sessionId: Int,
        val capabilities: Int,
        val maxFrame: Int,
    ) : Frame()

    data class Ack(
        override val reqId: Int,
        override val linkStatus: Int,
        val token: Long,
        val heartbeat: Int,
    ) : Frame()

    data class Result(
        override val reqId: Int,
        override val linkStatus: Int,
        val commandType: Int,
        val commandResult: Int,
    ) : Frame()

    /** EVENT_SUMMARY: one RF detection event (contracts/link.md "EVENT_SUMMARY"). */
    data class EventSummary(
        override val reqId: Int,
        override val linkStatus: Int,
        val eventId: Long,
        val detector: Int,
        val confidence: Int,
        val sourceKind: Int,
        val band: Int,
        val channel: Int,
        val rssi: Int,      // signed i8
        val count: Int,     // saturating u16
        val ageSeconds: Int,// saturating u16
        val flags: Int,
    ) : Frame() {
        val hasSourceId get() = flags and LinkConstants.summaryFlagSourceId != 0
        val hasDetail get() = flags and LinkConstants.summaryFlagDetail != 0
    }

    data class End(
        override val reqId: Int,
        override val linkStatus: Int,
        val count: Int,
        val gap: Int,
        val changeSeq: Long,
    ) : Frame()

    data class Text(
        override val reqId: Int,
        override val linkStatus: Int,
        val field: Int,
        val index: Int,
        val total: Int,
        val bytes: ByteArray,
    ) : Frame() {
        override fun equals(other: Any?): Boolean {
            if (this === other) return true
            if (other !is Text) return false
            return reqId == other.reqId && linkStatus == other.linkStatus &&
                field == other.field && index == other.index && total == other.total &&
                bytes.contentEquals(other.bytes)
        }
        override fun hashCode(): Int {
            var r = reqId; r = 31 * r + linkStatus; r = 31 * r + field
            r = 31 * r + index; r = 31 * r + total; r = 31 * r + bytes.contentHashCode(); return r
        }
    }

    data class Error(
        override val reqId: Int,
        override val linkStatus: Int,
    ) : Frame()

    /** A frame this binding does not recognise (first byte is not a known type). */
    data class Unknown(
        override val reqId: Int,
        override val linkStatus: Int,
        val type: Int,
        val raw: ByteArray,
    ) : Frame() {
        override fun equals(other: Any?): Boolean {
            if (this === other) return true
            if (other !is Unknown) return false
            return type == other.type && raw.contentEquals(other.raw)
        }
        override fun hashCode(): Int = 31 * type + raw.contentHashCode()
    }
}
