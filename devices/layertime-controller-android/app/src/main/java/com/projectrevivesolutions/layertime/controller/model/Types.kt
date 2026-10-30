package com.projectrevivesolutions.layertime.controller.model

/** High-level link connection state shown in the UI and recorded in the log. */
enum class ConnState { IDLE, SCANNING, CONNECTING, DISCOVERING, HANDSHAKING, CONNECTED, DISCONNECTED, FAILED }

/** A diagnostic log entry with a wall-clock timestamp (epoch millis). */
data class LogEntry(
    val epochMillis: Long,
    val category: Category,
    val message: String,
) {
    enum class Category { CONNECTION, EVENT, COMMAND, LINK, BACKGROUND, ERROR }
}

/**
 * One RF detection event, mirrored from EVENT_SUMMARY frames and keyed by
 * (sessionId, eventId) per the contract's session rule. Text fields are filled
 * in later by GET_TEXT when present.
 */
data class EventRow(
    val sessionId: Int,
    val eventId: Long,
    val detector: Int,
    val detectorName: String,
    val confidence: Int,
    val sourceKind: Int,
    val band: Int,
    val channel: Int,
    val rssi: Int,
    val count: Int,
    val ageSeconds: Int,
    val hasSourceId: Boolean,
    val hasDetail: Boolean,
    val sourceId: String? = null,
    val detail: String? = null,
    val firstSeenMillis: Long,
    val lastUpdatedMillis: Long,
    /** Signal readings from successive EVENT_SUMMARY updates, oldest first, at most [RssiSample.KEEP]. */
    val rssiHistory: List<RssiSample> = emptyList(),
)

/** One signal reading for an event, as the phone received it. */
data class RssiSample(val timeMillis: Long, val rssi: Int) {
    companion object { const val KEEP = 32 }
}

/** Result of a screen-off / background reception test window. */
data class BackgroundTest(
    val running: Boolean,
    val startedMillis: Long,
    val endedMillis: Long?,
    val eventsAtStart: Int,
    val eventsReceivedDuring: Int,
    val disconnectsDuring: Int,
    val note: String,
)

/** Static description of the phone and its BLE capability. */
data class DeviceReport(
    val manufacturer: String,
    val model: String,
    val device: String,
    val product: String,
    val androidRelease: String,
    val sdkInt: Int,
    val bleSupported: Boolean,
    val bluetoothOn: Boolean,
    val le2mPhy: Boolean,
    val leCodedPhy: Boolean,
    val leExtendedAdvertising: Boolean,
    val multipleAdvertisement: Boolean,
    val maxConnectedAudioNote: String,
    val appVersionName: String,
    val appVersionCode: Long,
)
