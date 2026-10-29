package com.projectrevivesolutions.layertime.controller.link

/**
 * LayerTime Link 0.1 constants.
 *
 * This is a platform binding of the LayerTime contract, NOT a new source of
 * truth. Every value here is copied from the canonical contract in the
 * repository:
 *   - contracts/link.md            (frame layout, UUIDs, rules)
 *   - contracts/vectors/enums.json (enum numbers and display names)
 *   - contracts/vectors/link_frames.json (byte-exact frames)
 *
 * When a value here and a contract vector disagree, this binding is wrong.
 * The codec is checked against contracts/vectors/link_frames.json by the
 * host conformance test in tools/ci (see that test's output in the handoff).
 */
object LinkConstants {

    // One base UUID, allocated once for LayerTime. 16-bit field derived per
    // attribute. These are fixed (contracts/link.md "UUIDs").
    const val BASE_UUID = "8ac687f5-a08b-4851-b89f-e6b39081268e"
    const val SERVICE_UUID = "8ac60001-a08b-4851-b89f-e6b39081268e"
    const val CONTROL_UUID = "8ac60002-a08b-4851-b89f-e6b39081268e" // write w/ response, interface -> Node
    const val STATUS_UUID = "8ac60003-a08b-4851-b89f-e6b39081268e"  // read + notify, Node -> interface
    const val DATA_UUID = "8ac60004-a08b-4851-b89f-e6b39081268e"    // notify, Node -> interface (reply frames)
    const val PROBE_UUID = "8ac600f0-a08b-4851-b89f-e6b39081268e"   // notify, test builds only
    const val CCCD_UUID = "00002902-0000-1000-8000-00805f9b34fb"    // Client Characteristic Configuration (0x2902)

    // The Node advertises its complete local name as LT-C5-xxxx in the scan
    // response, but discovery is BY SERVICE UUID, not by name
    // (contracts/link.md "Advertising").
    const val NAME_PREFIX = "LT-C5-"

    const val MAX_FRAME = 20          // every frame is 20 bytes or less
    const val STATUS_SIZE = 18        // the Status snapshot is exactly 18 bytes
    const val LINK_VERSION_BYTE = 0x01 // major in high nibble, minor in low nibble -> 0.1
    const val CLIENT_MAJOR = 0
    const val CLIENT_MINOR = 1
    const val TEXT_MAX_CHUNK = 14

    // EVENT_SUMMARY flag bits (contracts/vectors/link_frames.json "summaryFlags").
    const val summaryFlagSourceId = 1
    const val summaryFlagDetail = 2

    // Control request op bytes.
    object Op {
        const val HELLO = 0x01
        const val PING = 0x02
        const val COMMAND = 0x03
        const val GET_CHANGED = 0x04
        const val GET_TEXT = 0x05
        const val RX_MARK = 0xF1 // test-only, unbound; never sent by this client
    }

    // Data frame type bytes.
    object FrameType {
        const val HELLO_ACK = 0x81
        const val ACK = 0x82
        const val RESULT = 0x83
        const val EVENT_SUMMARY = 0x84
        const val END = 0x85
        const val TEXT = 0x86
        const val ERROR = 0x8F
    }

    // Link status (about the link itself, not the application).
    object LinkStatus {
        const val OK = 0
        const val UNKNOWN_OP = 1
        const val BAD_LENGTH = 2
        const val VERSION_MISMATCH = 3
        const val BUSY = 4
        fun name(v: Int) = when (v) {
            OK -> "Ok"; UNKNOWN_OP -> "UnknownOp"; BAD_LENGTH -> "BadLength"
            VERSION_MISMATCH -> "VersionMismatch"; BUSY -> "Busy"; else -> "Unknown($v)"
        }
    }

    // Status snapshot flag bits (contracts/link.md "Status snapshot").
    object StatusFlag {
        const val MONITORING = 0x01
        const val EARLY_WARNING_ENABLED = 0x02
        const val EARLY_WARNING_RESTING = 0x04
        const val ALERT_PENDING = 0x08
        const val SLEEP_MODE = 0x10
        const val NO_SD_LOG = 0x20
    }

    // HELLO_ACK capability bits.
    object Capability {
        const val LOCAL_WIFI_MONITOR = 0x01
        const val LOCAL_BLE_MONITOR = 0x02
        const val DISPLAY = 0x04
        const val LED = 0x08
        const val BUTTON = 0x10
        fun describe(mask: Int): List<String> = buildList {
            if (mask and LOCAL_WIFI_MONITOR != 0) add("wifi")
            if (mask and LOCAL_BLE_MONITOR != 0) add("ble")
            if (mask and DISPLAY != 0) add("display")
            if (mask and LED != 0) add("led")
            if (mask and BUTTON != 0) add("button")
        }
    }

    // CommandType numbers (contracts/vectors/enums.json). The Node accepts
    // these with the exact lengths below; everything else is rejected or
    // reported Unsupported (contracts/link.md "COMMAND").
    object CommandType {
        const val RECON_START = 1          // arg: target u8 (1..16)
        const val RECON_STOP = 2           // no arg
        const val RECON_CLEAR_EVENTS = 3   // no arg
        const val RECON_ACK_ALERT = 4      // no arg
        const val SET_SLEEP_MODE = 11      // arg: enabled u8 (0/1)
        const val SET_EARLY_WARNING = 12   // arg: enabled u8 (0/1)
    }

    object CommandResult {
        const val OK = 0
        const val UNSUPPORTED = 1
        const val INVALID_ARGUMENT = 2
        const val NOT_READY = 3
        const val FAILED = 4
        fun name(v: Int) = when (v) {
            OK -> "Ok"; UNSUPPORTED -> "Unsupported"; INVALID_ARGUMENT -> "InvalidArgument"
            NOT_READY -> "NotReady"; FAILED -> "Failed"; else -> "Unknown($v)"
        }
    }

    // ReconTarget / detector numbers and display names
    // (contracts/vectors/enums.json). Deauth is a first-class detector (5).
    object ReconTarget {
        const val NONE = 0
        const val ALL = 1
        const val DEAUTH = 5
        private val display = mapOf(
            0 to "STOPPED", 1 to "ALL", 2 to "TRACKERS", 3 to "COUNTER-SURVEIL",
            4 to "COUNTER-INTRUSION", 5 to "DEAUTH", 6 to "PWNAGOTCHI", 7 to "MULTISSID",
            8 to "FLOCK", 9 to "PINEAPPLE", 10 to "AIRTAG", 11 to "FLIPPER", 12 to "META",
            13 to "AXON", 14 to "TILE", 15 to "SMARTTAG", 16 to "GOOGLE TAG", 17 to "EARLY WARNING"
        )
        fun name(v: Int) = display[v] ?: "TARGET($v)"
    }

    object SourceKind {
        fun name(v: Int) = when (v) {
            0 -> "Unknown"; 1 -> "Wi-Fi"; 2 -> "BLE"; 3 -> "802.15.4"; else -> "Unknown($v)"
        }
    }

    object Band {
        fun name(v: Int) = when (v) {
            0 -> "Unknown"; 1 -> "2.4 GHz"; 2 -> "5 GHz"; else -> "Unknown($v)"
        }
    }

    object Confidence {
        fun name(v: Int) = when (v) { 0 -> "LOW"; 1 -> "MED"; 2 -> "HIGH"; else -> "?($v)" }
    }

    object TextField {
        const val SOURCE_ID = 0
        const val DETAIL = 1
    }
}
