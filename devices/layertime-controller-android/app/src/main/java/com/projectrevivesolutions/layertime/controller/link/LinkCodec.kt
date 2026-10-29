package com.projectrevivesolutions.layertime.controller.link

/**
 * LayerTime Link 0.1 wire codec. Pure Kotlin, no Android dependencies.
 *
 * Encodes the Control requests this interface (controller) sends, and decodes
 * the Status snapshot and the Data reply frames the Node sends. Integers are
 * little-endian and unsigned unless stated; rssi is signed i8. Every frame is
 * at most [LinkConstants.MAX_FRAME] bytes.
 *
 * Conformance: encode cases must match contracts/vectors/link_frames.json
 * byte-for-byte; decode cases must produce exactly the listed fields. The host
 * test tools/ci/link_conformance checks this binding against that file, the
 * same vectors the C++ and Monkey C bindings are checked against.
 */
object LinkCodec {

    // ---- little-endian helpers ----

    private fun u8(b: ByteArray, o: Int): Int = b[o].toInt() and 0xFF
    private fun i8(b: ByteArray, o: Int): Int = b[o].toInt() // Byte is already signed
    private fun u16(b: ByteArray, o: Int): Int = u8(b, o) or (u8(b, o + 1) shl 8)
    private fun u32(b: ByteArray, o: Int): Long {
        return (u8(b, o).toLong()) or
            (u8(b, o + 1).toLong() shl 8) or
            (u8(b, o + 2).toLong() shl 16) or
            (u8(b, o + 3).toLong() shl 24)
    }

    private fun putU16(b: ByteArray, o: Int, v: Int) {
        b[o] = (v and 0xFF).toByte()
        b[o + 1] = ((v shr 8) and 0xFF).toByte()
    }

    private fun putU32(b: ByteArray, o: Int, v: Long) {
        b[o] = (v and 0xFF).toByte()
        b[o + 1] = ((v shr 8) and 0xFF).toByte()
        b[o + 2] = ((v shr 16) and 0xFF).toByte()
        b[o + 3] = ((v shr 24) and 0xFF).toByte()
    }

    // ---- Control request encoders (interface -> Node) ----

    /** HELLO: op | reqId | clientMajor | clientMinor (4 bytes). */
    fun hello(
        reqId: Int,
        clientMajor: Int = LinkConstants.CLIENT_MAJOR,
        clientMinor: Int = LinkConstants.CLIENT_MINOR,
    ): ByteArray = byteArrayOf(
        LinkConstants.Op.HELLO.toByte(), reqId.toByte(),
        clientMajor.toByte(), clientMinor.toByte()
    )

    /** PING: op | reqId | token u32 (6 bytes). */
    fun ping(reqId: Int, token: Long): ByteArray {
        val b = ByteArray(6)
        b[0] = LinkConstants.Op.PING.toByte(); b[1] = reqId.toByte()
        putU32(b, 2, token)
        return b
    }

    /** COMMAND with a one-byte argument: op | reqId | commandType | arg (4 bytes). */
    private fun commandArg(reqId: Int, commandType: Int, arg: Int): ByteArray = byteArrayOf(
        LinkConstants.Op.COMMAND.toByte(), reqId.toByte(), commandType.toByte(), arg.toByte()
    )

    /** COMMAND with no argument: op | reqId | commandType (3 bytes). */
    private fun commandNoArg(reqId: Int, commandType: Int): ByteArray = byteArrayOf(
        LinkConstants.Op.COMMAND.toByte(), reqId.toByte(), commandType.toByte()
    )

    fun reconStart(reqId: Int, target: Int) =
        commandArg(reqId, LinkConstants.CommandType.RECON_START, target)

    fun reconStop(reqId: Int) =
        commandNoArg(reqId, LinkConstants.CommandType.RECON_STOP)

    fun reconClearEvents(reqId: Int) =
        commandNoArg(reqId, LinkConstants.CommandType.RECON_CLEAR_EVENTS)

    fun reconAcknowledgeAlert(reqId: Int) =
        commandNoArg(reqId, LinkConstants.CommandType.RECON_ACK_ALERT)

    fun setSleepMode(reqId: Int, enabled: Boolean) =
        commandArg(reqId, LinkConstants.CommandType.SET_SLEEP_MODE, if (enabled) 1 else 0)

    fun setEarlyWarning(reqId: Int, enabled: Boolean) =
        commandArg(reqId, LinkConstants.CommandType.SET_EARLY_WARNING, if (enabled) 1 else 0)

    /** GET_CHANGED: op | reqId | sinceChangeSeq u32 (6 bytes). */
    fun getChanged(reqId: Int, sinceChangeSeq: Long): ByteArray {
        val b = ByteArray(6)
        b[0] = LinkConstants.Op.GET_CHANGED.toByte(); b[1] = reqId.toByte()
        putU32(b, 2, sinceChangeSeq)
        return b
    }

    /** GET_TEXT: op | reqId | eventId u32 | field u8 (7 bytes). */
    fun getText(reqId: Int, eventId: Long, field: Int): ByteArray {
        val b = ByteArray(7)
        b[0] = LinkConstants.Op.GET_TEXT.toByte(); b[1] = reqId.toByte()
        putU32(b, 2, eventId); b[6] = field.toByte()
        return b
    }

    // ---- decoders (Node -> interface) ----

    /** Decode the 18-byte Status snapshot. Returns null if the length is wrong. */
    fun decodeStatus(b: ByteArray): StatusSnapshot? {
        if (b.size < LinkConstants.STATUS_SIZE) return null
        return StatusSnapshot(
            linkVersion = u8(b, 0),
            sessionId = u16(b, 1),
            changeSeq = u32(b, 3),
            flags = u8(b, 7),
            selected = u8(b, 8),
            active = u8(b, 9),
            eventCount = u8(b, 10),
            lastAlertEventId = u32(b, 11),
            heartbeat = u8(b, 15),
            schedule = u8(b, 16),
            nextReportS = u8(b, 17),
        )
    }

    /**
     * Decode one Data-characteristic reply frame by its type byte. A frame
     * whose length does not fit its type, or whose type is unknown, is returned
     * as [Frame.Unknown] rather than throwing: bytes observed over the air are
     * untrusted, and the caller decides what to do with an unrecognised frame.
     */
    fun decodeFrame(b: ByteArray): Frame {
        if (b.isEmpty()) return Frame.Unknown(0, 0, -1, b)
        val type = u8(b, 0)
        val reqId = if (b.size >= 2) u8(b, 1) else 0
        val status = if (b.size >= 3) u8(b, 2) else 0
        return when (type) {
            LinkConstants.FrameType.HELLO_ACK ->
                if (b.size >= 10) Frame.HelloAck(
                    reqId, status,
                    serverMajor = u8(b, 3), serverMinor = u8(b, 4),
                    sessionId = u16(b, 5), capabilities = u16(b, 7), maxFrame = u8(b, 9)
                ) else unknown(type, reqId, status, b)

            LinkConstants.FrameType.ACK ->
                if (b.size >= 8) Frame.Ack(reqId, status, token = u32(b, 3), heartbeat = u8(b, 7))
                else unknown(type, reqId, status, b)

            LinkConstants.FrameType.RESULT ->
                if (b.size >= 5) Frame.Result(reqId, status, commandType = u8(b, 3), commandResult = u8(b, 4))
                else unknown(type, reqId, status, b)

            LinkConstants.FrameType.EVENT_SUMMARY ->
                if (b.size >= 18) Frame.EventSummary(
                    reqId, status,
                    eventId = u32(b, 3), detector = u8(b, 7), confidence = u8(b, 8),
                    sourceKind = u8(b, 9), band = u8(b, 10), channel = u8(b, 11),
                    rssi = i8(b, 12), count = u16(b, 13), ageSeconds = u16(b, 15), flags = u8(b, 17)
                ) else unknown(type, reqId, status, b)

            LinkConstants.FrameType.END ->
                if (b.size >= 9) Frame.End(reqId, status, count = u8(b, 3), gap = u8(b, 4), changeSeq = u32(b, 5))
                else unknown(type, reqId, status, b)

            LinkConstants.FrameType.TEXT ->
                if (b.size >= 6) Frame.Text(
                    reqId, status, field = u8(b, 3), index = u8(b, 4), total = u8(b, 5),
                    bytes = b.copyOfRange(6, b.size)
                ) else unknown(type, reqId, status, b)

            LinkConstants.FrameType.ERROR ->
                if (b.size >= 3) Frame.Error(reqId, status) else unknown(type, reqId, status, b)

            else -> unknown(type, reqId, status, b)
        }
    }

    private fun unknown(type: Int, reqId: Int, status: Int, b: ByteArray) =
        Frame.Unknown(reqId, status, type, b)

    // ---- hex helpers (used by diagnostics and the host conformance test) ----

    fun toHex(b: ByteArray): String =
        b.joinToString("") { "%02X".format(it.toInt() and 0xFF) }

    fun fromHex(s: String): ByteArray {
        val clean = s.trim()
        require(clean.length % 2 == 0) { "hex length must be even: '$clean'" }
        return ByteArray(clean.length / 2) {
            ((Character.digit(clean[it * 2], 16) shl 4) or
                Character.digit(clean[it * 2 + 1], 16)).toByte()
        }
    }
}
