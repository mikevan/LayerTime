package com.projectrevivesolutions.layertime.controller.wireshark

/**
 * On-device Wireshark decoder (libwireshark/epan via JNI). Wireshark is the
 * decoder; nothing is substituted. The native library `ltws` (ws_core.c +
 * ws_jni.c) links the arm64 libwireshark set built by
 * wireshark-spike/build_wireshark_arm64.sh.
 *
 * Used by the diagnostic screen (fixture decode) and by capture import on the
 * event-detail screen (a capture file the user selects, read frame by frame).
 * It is NOT wired to live Link events; contract 0.1 carries no raw frames (see
 * FRAME_EVIDENCE_PROPOSAL.md). Decoding a frame never produces a detection or
 * "countered" claim.
 */
object WiresharkDecoder {

    /** Known 802.11 deauthentication fixture, byte-identical to the host spike
     * (wireshark-spike/deauth_fixture.pcap). Broadcast DA, SA/BSSID
     * 00:11:22:33:aa:bb, seq 7, reason 7. Raw 802.11, no radiotap. */
    val DEAUTH_FIXTURE: ByteArray = byteArrayOf(
        0xC0.toByte(), 0x00, 0x3A, 0x01,
        0xFF.toByte(), 0xFF.toByte(), 0xFF.toByte(), 0xFF.toByte(), 0xFF.toByte(), 0xFF.toByte(),
        0x00, 0x11, 0x22, 0x33, 0xAA.toByte(), 0xBB.toByte(),
        0x00, 0x11, 0x22, 0x33, 0xAA.toByte(), 0xBB.toByte(),
        0x70, 0x00, 0x07, 0x00
    )

    /** A classic pcap file (DLT 105, IEEE 802.11, little-endian) wrapping [frame].
     * Lets the on-device PCAP path be exercised without pushing a file. */
    fun pcapBytes(frame: ByteArray): ByteArray {
        fun le32(v: Int) = byteArrayOf(v.toByte(), (v ushr 8).toByte(), (v ushr 16).toByte(), (v ushr 24).toByte())
        fun le16(v: Int) = byteArrayOf(v.toByte(), (v ushr 8).toByte())
        return le32(0xa1b2c3d4.toInt()) + le16(2) + le16(4) + le32(0) + le32(0) +
            le32(65535) + le32(105) +                     // global header, linktype 105
            le32(1700000000) + le32(123456) + le32(frame.size) + le32(frame.size) + // record header
            frame
    }

    /** A classic pcap file (DLT 105) with one record per frame. Record i is
     * stamped 1700000000 + i seconds, 0 microseconds. Byte-identical to the
     * files written by wireshark-spike/make_test_captures.py. */
    fun pcapBytes(frames: List<ByteArray>): ByteArray {
        fun le32(v: Int) = byteArrayOf(v.toByte(), (v ushr 8).toByte(), (v ushr 16).toByte(), (v ushr 24).toByte())
        fun le16(v: Int) = byteArrayOf(v.toByte(), (v ushr 8).toByte())
        val out = java.io.ByteArrayOutputStream()
        out.write(le32(0xa1b2c3d4.toInt()) + le16(2) + le16(4) + le32(0) + le32(0) + le32(65535) + le32(105))
        frames.forEachIndexed { i, f ->
            out.write(le32(1700000000 + i) + le32(0) + le32(f.size) + le32(f.size)); out.write(f)
        }
        return out.toByteArray()
    }

    private fun hex(s: String): ByteArray =
        s.replace(" ", "").chunked(2).map { it.toInt(16).toByte() }.toByteArray()

    /** Five raw 802.11 frames for the capture-import test file
     * (wireshark-spike/lt_test_5frames.pcap): the deauth fixture, a
     * disassociation (reason 8), a beacon with SSID "LT-TEST" on channel 6, a
     * broadcast probe request, and the deauth fixture cut to 20 bytes so
     * Wireshark must report it as malformed. Synthetic test data, not traffic. */
    val TEST_FRAMES: List<ByteArray> = listOf(
        DEAUTH_FIXTURE,
        hex("a0003a01 001122334455 001122 33aabb 00112233aabb 8000 0800"),
        hex("80000000 ffffffffffff 00112233aabb 00112233aabb 9000 0000000000000000 6400 0104 00074c542d54455354 010182 030106"),
        hex("40000000 ffffffffffff 020000000001 ffffffffffff a000 0000 010182"),
        DEAUTH_FIXTURE.copyOf(20)
    )

    /** Same-version reference: wireshark-spike/tshark_4.4.19_decode.txt. */
    const val REFERENCE_FILE = "wireshark-spike/tshark_4.4.19_decode.txt"
    val TSHARK_4_4_19_EXPECTED: List<Pair<String, String>> = listOf(
        "wlan.fc.type_subtype" to "0x000c",
        "wlan.da" to "ff:ff:ff:ff:ff:ff",
        "wlan.sa" to "00:11:22:33:aa:bb",
        "wlan.bssid" to "00:11:22:33:aa:bb",
        "wlan.seq" to "7",
        "wlan.fixed.reason_code" to "0x0007"
    )

    /** Fields of [decoded] that do NOT match the TShark 4.4.19 reference (empty = all match). */
    fun mismatchesAgainstReference(decoded: String): List<String> =
        TSHARK_4_4_19_EXPECTED.filterNot { (name, value) ->
            decoded.lineSequence().any { line ->
                line.substringBefore(':').trim() == name && line.substringAfter(':').trim() == value
            }
        }.map { (name, value) -> "$name expected $value" }

    /** One node of the Wireshark protocol tree. [start]/[length] are byte offsets into the frame.
     * [display] is the value as Wireshark shows it (or the label for nodes without a value);
     * [label] is Wireshark's full tree text for the node (empty from builds before 17). */
    data class Field(val depth: Int, val name: String, val display: String, val start: Int, val length: Int,
                     val label: String = "") {
        /** Text for a one-line view: Wireshark's own label text after "Name: ", else the value. */
        val shown: String get() = when {
            label.isBlank() -> display
            label == display -> label
            label.contains(": ") -> label.substringAfter(": ")
            else -> label
        }
    }

    /** Parse the tab-separated tree dump from nativeDecodeTree / the PCAP reader. Lines starting with '#' are headers. */
    fun parseTree(raw: String): List<Field> = raw.lineSequence().mapNotNull { line ->
        if (line.startsWith("#")) return@mapNotNull null
        val p = line.split('\t')
        if (p.size >= 5) Field(p[0].toIntOrNull() ?: 0, p[1], p[2], p[3].toIntOrNull() ?: -1, p[4].toIntOrNull() ?: -1,
            p.getOrElse(5) { "" })
        else null
    }.toList()

    /** One frame from a capture file, dissected by Wireshark. */
    data class PcapFrame(
        val number: Int, val encap: Int, val encapName: String,
        val capturedLength: Int, val originalLength: Int,
        val hasTimestamp: Boolean, val epochSeconds: Long, val nanos: Int,
        val fileOffset: Long, val hex: String, val fields: List<Field>
    ) {
        fun first(name: String): Field? = fields.firstOrNull { it.name == name }
        /** Wireshark reported a malformed or truncated frame. */
        val malformed: Boolean get() = fields.any { it.name == "_ws.malformed" || it.name.startsWith("_ws.expert") && it.label.contains("Malformed", ignoreCase = true) }
        /** Short title for a frame list: the 802.11 type/subtype as Wireshark names it, else the top protocol. */
        val title: String get() =
            first("wlan.fc.type_subtype")?.shown
                ?: fields.lastOrNull { it.depth == 0 && it.name != "frame" && !it.name.startsWith("_ws") }?.let { it.label.ifBlank { it.name } }
                ?: "Frame"
        val addresses: String get() {
            val sa = first("wlan.sa")?.display ?: first("wlan.ta")?.display
            val da = first("wlan.da")?.display ?: first("wlan.ra")?.display
            return if (sa == null && da == null) "" else "${sa ?: "?"} to ${da ?: "?"}"
        }
    }

    /** Parse one record set from the PCAP reader. Null if the header line is missing. Pure; unit-tested. */
    fun parseFrame(raw: String): PcapFrame? {
        val lines = raw.lines()
        val h = lines.firstOrNull { it.startsWith("#frame\t") }?.split('\t') ?: return null
        if (h.size < 10) return null
        val hexLine = lines.firstOrNull { it.startsWith("#hex\t") }?.substringAfter('\t') ?: ""
        return PcapFrame(
            number = h[1].toIntOrNull() ?: return null,
            encap = h[2].toIntOrNull() ?: -1, encapName = h[3],
            capturedLength = h[4].toIntOrNull() ?: -1, originalLength = h[5].toIntOrNull() ?: -1,
            hasTimestamp = h[6] == "1", epochSeconds = h[7].toLongOrNull() ?: 0, nanos = h[8].toIntOrNull() ?: 0,
            fileOffset = h[9].toLongOrNull() ?: -1, hex = hexLine, fields = parseTree(raw)
        )
    }

    /** Hex dump lines, 16 bytes each, prefixed with the byte offset. Pure; unit-tested. */
    fun hexDump(hex: String): List<String> = hex.chunked(32).mapIndexed { i, row ->
        "%04x  %s".format(i * 16, row.chunked(2).joinToString(" "))
    }

    /** Frame-by-frame capture reader. Not thread-safe; use from one thread and close it. */
    class PcapReader internal constructor(private var handle: Long) : CaptureDecodeLoop.FrameSource {
        /** Set when the file could not be opened, or after next() returned null because of a read error. */
        override val error: String? get() = if (handle == 0L) "Reader is closed." else nativePcapError(handle)
        override val fileType: String = if (handle != 0L) nativePcapFileType(handle) else "unknown"
        override val fileSize: Long = if (handle != 0L) nativePcapFileSize(handle) else -1
        /** Next frame, or null at end of file or on a read error (see [error]). */
        override fun next(): PcapFrame? {
            if (handle == 0L) return null
            val raw = nativePcapNext(handle) ?: return null
            return parseFrame(raw)
        }
        override fun close() { if (handle != 0L) { nativePcapClose(handle); handle = 0L } }
    }

    /** Open a capture file for frame-by-frame decoding. Null if the decoder is not loaded and initialized.
     * Check [PcapReader.error] before reading. */
    fun openPcap(path: String): PcapReader? {
        val init = initialize() ?: return null
        if (!init.ok) return null
        return PcapReader(nativePcapOpen(path))
    }

    /** The decoder's own JNI library. Everything else it loads is a dependency. */
    const val PRIMARY_LIBRARY = "libltws.so"

    enum class FailureKind {
        MISSING_PRIMARY_LIBRARY, MISSING_TRANSITIVE_DEPENDENCY, UNRESOLVED_SYMBOL, OTHER_LOADER_ERROR
    }

    /** A classified native load failure. [raw] is the loader's message, unmodified. */
    data class LoadFailure(
        val kind: FailureKind, val library: String?, val neededBy: String?, val symbol: String?, val raw: String
    ) {
        fun explanation(): String = when (kind) {
            FailureKind.MISSING_PRIMARY_LIBRARY ->
                "The decoder's own native library ($library) is not in this APK."
            FailureKind.MISSING_TRANSITIVE_DEPENDENCY ->
                "$neededBy is packaged and was being loaded, but it depends on $library, " +
                    "which is not in this APK. This is a missing transitive dependency, " +
                    "not a missing Wireshark build."
            FailureKind.UNRESOLVED_SYMBOL ->
                "All library files were found, but $neededBy references the symbol $symbol, " +
                    "which no loaded library exports."
            FailureKind.OTHER_LOADER_ERROR ->
                "The Android native loader rejected the library set. See the loader error."
        }
    }

    private val MISSING = Regex("""library "([^"]+)" not found(?:: needed by (\S+))?""")
    private val NOT_FOUND_JAVA = Regex("""couldn't find "([^"]+)"""")
    private val SYMBOL = Regex("""cannot locate symbol "([^"]+)" referenced by "([^"]+)"""")

    /** Classify a native loader message. Pure function, unit-tested on the JVM. */
    fun classify(raw: String): LoadFailure {
        MISSING.find(raw)?.let { m ->
            val lib = m.groupValues[1]
            val by = m.groupValues[2].ifEmpty { null }?.substringAfterLast('/')
            val kind = if (by == null || lib == PRIMARY_LIBRARY)
                FailureKind.MISSING_PRIMARY_LIBRARY else FailureKind.MISSING_TRANSITIVE_DEPENDENCY
            return LoadFailure(kind, lib, by, null, raw)
        }
        NOT_FOUND_JAVA.find(raw)?.let { m ->
            return LoadFailure(FailureKind.MISSING_PRIMARY_LIBRARY, m.groupValues[1], null, null, raw)
        }
        SYMBOL.find(raw)?.let { m ->
            return LoadFailure(FailureKind.UNRESOLVED_SYMBOL, null,
                m.groupValues[2].substringAfterLast('/'), m.groupValues[1], raw)
        }
        return LoadFailure(FailureKind.OTHER_LOADER_ERROR, null, null, null, raw)
    }

    /** Result of native initialization, measured inside the native code. */
    data class InitResult(
        val performedNow: Boolean, val ok: Boolean,
        val epanInitMillis: Double, val totalInitMillis: Double, val callsInProcess: Long
    )

    @Volatile private var loaded = false
    @Volatile private var failure: LoadFailure? = null

    @Volatile var loadMillis: Double = -1.0
        private set
    @Volatile var loadedByFirstCaller = false
        private set

    @Synchronized
    fun ensureLoaded(): Boolean {
        if (loaded) return true
        if (failure != null) return false
        val t0 = System.nanoTime()
        return try {
            System.loadLibrary("ltws")
            loadMillis = (System.nanoTime() - t0) / 1e6
            loaded = true
            loadedByFirstCaller = true
            true
        } catch (t: Throwable) {
            failure = classify(t.message ?: t.toString())
            false
        }
    }

    fun failure(): LoadFailure? = failure

    fun initialize(): InitResult? {
        if (!ensureLoaded()) return null
        val a = nativeInit()
        return InitResult(a[0] == 1L, a[1] == 1L, a[2] / 1e6, a[3] / 1e6, a[4])
    }

    fun version(): String? = if (loaded) nativeVersion() else null

    /** Field dump, or null if the library is not loaded or initialization did not succeed. */
    fun decode(frame: ByteArray): String? = if (loaded) nativeDecode(frame) else null

    /** Control: the same JNI round trip and result-string allocation, no epan work. */
    fun control(frame: ByteArray): String? = if (loaded) nativeControl(frame) else null

    /** Full protocol tree as structured fields, or null. */
    fun decodeTree(frame: ByteArray): List<Field>? =
        if (loaded) nativeDecodeTree(frame)?.let { parseTree(it) } else null

    /** Decode frames from a pcap/pcapng file; returns the tab-separated summary (or an error string). */
    fun decodePcap(path: String, maxFrames: Int): String? = if (loaded) nativeDecodePcap(path, maxFrames) else null

    private external fun nativeInit(): LongArray
    private external fun nativeVersion(): String
    private external fun nativeDecode(frame: ByteArray): String?
    private external fun nativeControl(frame: ByteArray): String?
    private external fun nativeDecodeTree(frame: ByteArray): String?
    private external fun nativeDecodePcap(path: String, maxFrames: Int): String?
    private external fun nativePcapOpen(path: String): Long
    private external fun nativePcapNext(handle: Long): String?
    private external fun nativePcapError(handle: Long): String?
    private external fun nativePcapFileType(handle: Long): String
    private external fun nativePcapFileSize(handle: Long): Long
    private external fun nativePcapClose(handle: Long)
}
