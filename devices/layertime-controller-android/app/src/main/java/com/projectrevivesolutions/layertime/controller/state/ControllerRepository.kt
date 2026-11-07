package com.projectrevivesolutions.layertime.controller.state

import com.projectrevivesolutions.layertime.controller.link.Frame
import com.projectrevivesolutions.layertime.controller.link.LinkConstants
import com.projectrevivesolutions.layertime.controller.link.StatusSnapshot
import com.projectrevivesolutions.layertime.controller.model.BackgroundTest
import com.projectrevivesolutions.layertime.controller.model.ConnState
import com.projectrevivesolutions.layertime.controller.model.DeviceReport
import com.projectrevivesolutions.layertime.controller.model.EventRow
import com.projectrevivesolutions.layertime.controller.model.LogEntry
import com.projectrevivesolutions.layertime.controller.model.RssiSample
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update

/**
 * Single source of truth shared between the foreground service (which owns the
 * BLE link) and the Compose UI. All mutations append to the diagnostic log so
 * the exported bundle is a faithful, timestamped record.
 *
 * MutableStateFlow is safe to update from the GATT callback threads.
 */
object ControllerRepository {

    private val _conn = MutableStateFlow(ConnState.IDLE)
    val conn: StateFlow<ConnState> = _conn.asStateFlow()

    /** The reason passed with the latest state; the CONNECTION panel turns it into a sentence (ConnNotice). */
    private val _connDetail = MutableStateFlow<String?>(null)
    val connDetail: StateFlow<String?> = _connDetail.asStateFlow()

    private val _device = MutableStateFlow<String>("")
    val deviceLabel: StateFlow<String> = _device.asStateFlow()

    private val _status = MutableStateFlow<StatusSnapshot?>(null)
    val status: StateFlow<StatusSnapshot?> = _status.asStateFlow()

    private val _server = MutableStateFlow<Frame.HelloAck?>(null)
    val server: StateFlow<Frame.HelloAck?> = _server.asStateFlow()

    // The SetSensors mask the connected Hunter last answered Ok to; null until it has,
    // and again whenever the link is not up. MISSION compares it with its own list.
    private val _sensorsApplied = MutableStateFlow<Long?>(null)
    val sensorsApplied: StateFlow<Long?> = _sensorsApplied.asStateFlow()
    fun setSensorsApplied(mask: Long?) { _sensorsApplied.value = mask }
    // The evidence hold the Hunter last confirmed (SetEvidenceHold, 2026-11-04); null until it does.
    private val _evidenceHoldApplied = MutableStateFlow<Int?>(null)
    val evidenceHoldApplied: StateFlow<Int?> = _evidenceHoldApplied.asStateFlow()
    fun setEvidenceHoldApplied(sec: Int?) { _evidenceHoldApplied.value = sec }

    private val _events = MutableStateFlow<List<EventRow>>(emptyList())
    val events: StateFlow<List<EventRow>> = _events.asStateFlow()

    private val _log = MutableStateFlow<List<LogEntry>>(emptyList())
    val log: StateFlow<List<LogEntry>> = _log.asStateFlow()

    private val _background = MutableStateFlow<BackgroundTest?>(null)
    val background: StateFlow<BackgroundTest?> = _background.asStateFlow()

    @Volatile var deviceReport: DeviceReport? = null

    /** True after a link to a Hunter was lost, until it is back or the operator disconnects. */
    private val _reconnecting = MutableStateFlow(false)
    val reconnecting: StateFlow<Boolean> = _reconnecting.asStateFlow()
    fun setReconnecting(on: Boolean) { _reconnecting.value = on }

    /** True while connected but the Status heartbeat is more than 3 s late (LinkHealth). */
    private val _linkStale = MutableStateFlow(false)
    val linkStale: StateFlow<Boolean> = _linkStale.asStateFlow()
    fun setLinkStale(stale: Boolean) {
        if (_linkStale.value != stale) {
            _linkStale.value = stale
            log(LogEntry.Category.LINK, if (stale) "link stale: Status heartbeat late" else "link healthy")
        }
    }

    // Event mirror for the current session: the Hunter's records, shown one row per MAC.
    private var currentSession: Int = -1
    private val mirror = EventGrouper()
    private val mirrorLock = Any()

    private fun now() = System.currentTimeMillis()

    fun log(category: LogEntry.Category, message: String) {
        _log.update { it + LogEntry(now(), category, message) }
        // Also to logcat (tag LayerScope), so a capture on the bench computer shows the app's own
        // view of the link next to Android's Bluetooth log (2026-11-06 diagnostics).
        runCatching { android.util.Log.i("LayerScope", "[$category] $message") }
    }

    fun setConn(state: ConnState, detail: String? = null) {
        if (state != ConnState.CONNECTED) { _sensorsApplied.value = null; _evidenceHoldApplied.value = null }
        _connDetail.value = detail
        _conn.value = state
        log(LogEntry.Category.CONNECTION, "state=$state" + (detail?.let { " ($it)" } ?: ""))
    }

    fun setDeviceLabel(label: String) { _device.value = label }

    fun onServerInfo(ack: Frame.HelloAck) {
        _server.value = ack
        // HELLO_ACK carries the session id, and the Hunter can send events before its
        // first STATUS. Take the session from here so no event is filed under session -1
        // (an unknown session broke detection-alert de-duplication across Hunter restarts).
        if (ack.sessionId != currentSession) {
            if (currentSession != -1) log(LogEntry.Category.LINK,
                "new session ${ack.sessionId} from HELLO_ACK (was $currentSession): clearing event mirror")
            currentSession = ack.sessionId
            synchronized(mirrorLock) { mirror.clear() }
            publishEvents()
        }
        val caps = LinkConstants.Capability.describe(ack.capabilities).joinToString(",")
        log(LogEntry.Category.LINK,
            "HELLO_ACK server ${ack.serverMajor}.${ack.serverMinor}, session=${ack.sessionId}, " +
                "caps=[$caps], maxFrame=${ack.maxFrame}, linkStatus=${LinkConstants.LinkStatus.name(ack.linkStatus)}")
    }

    fun onStatus(s: StatusSnapshot) {
        val prev = _status.value
        _status.value = s
        if (s.sessionId != currentSession) {
            // Session rule: a new session invalidates the whole mirror.
            if (currentSession != -1) log(LogEntry.Category.LINK,
                "new session ${s.sessionId} (was $currentSession): clearing event mirror")
            currentSession = s.sessionId
            synchronized(mirrorLock) { mirror.clear() }
            publishEvents()
        }
        // A record whose MAC never arrived is shown on its own after a few seconds; the
        // Status heartbeat is what lets that happen when no other frame comes.
        onMirrorChange(synchronized(mirrorLock) { mirror.flush(now()) })
        // Only log a status line when something meaningful changed (not heartbeat).
        if (prev == null || prev.changeSeq != s.changeSeq || prev.flags != s.flags ||
            prev.eventCount != s.eventCount || prev.active != s.active
        ) {
            log(LogEntry.Category.LINK,
                "STATUS seq=${s.changeSeq} flags=0x%02X events=${s.eventCount} active=${LinkConstants.ReconTarget.name(s.active)}"
                    .format(s.flags))
        }
    }

    /**
     * Upsert a Hunter record from an EVENT_SUMMARY frame; returns true if the record is new
     * (so its text fields still have to be fetched). The list shows one row per MAC.
     *
     * Logging, build 74: a repeat sighting no longer writes a line. A line is written when
     * a row first appears and when a row's confidence goes up. The row's count and signal
     * history keep the rest.
     */
    fun onEventSummary(e: Frame.EventSummary): Boolean {
        val t = now()
        val (new, change) = synchronized(mirrorLock) {
            val existing = mirror.record(e.eventId)
            val row = EventRow(
                sessionId = currentSession,
                eventId = e.eventId,
                detector = e.detector,
                detectorName = LinkConstants.ReconTarget.name(e.detector),
                confidence = e.confidence,
                sourceKind = e.sourceKind,
                band = e.band,
                channel = e.channel,
                rssi = e.rssi,
                count = e.count,
                ageSeconds = e.ageSeconds,
                hasSourceId = e.hasSourceId,
                hasDetail = e.hasDetail,
                sourceId = existing?.sourceId,
                detail = existing?.detail,
                firstSeenMillis = existing?.firstSeenMillis ?: t,
                lastUpdatedMillis = t,
                rssiHistory = ((existing?.rssiHistory ?: emptyList()) + RssiSample(t, e.rssi)).takeLast(RssiSample.KEEP),
            )
            (existing == null) to mirror.onRecord(row, t)
        }
        onMirrorChange(change)
        // Count toward an active background test.
        _background.update { bt -> if (bt?.running == true) bt.copy(eventsReceivedDuring = bt.eventsReceivedDuring + 1) else bt }
        return new
    }

    fun onEventText(eventId: Long, field: Int, text: String) {
        onMirrorChange(synchronized(mirrorLock) { mirror.onText(eventId, field, text, now()) })
    }

    private fun onMirrorChange(change: EventGrouper.Change) {
        for (r in change.newRows) log(LogEntry.Category.EVENT, "NEW " + describeRow(r))
        for ((was, r) in change.raised) log(LogEntry.Category.EVENT,
            "RAISED ${LinkConstants.Confidence.name(was)} -> ${LinkConstants.Confidence.name(r.confidence)}: " + describeRow(r))
        if (change.touched) publishEvents()
    }

    private fun describeRow(r: EventRow): String =
        "#${r.eventId} ${r.detectorName} ${r.sourceId ?: "(no id)"} ${LinkConstants.SourceKind.name(r.sourceKind)} " +
            "${LinkConstants.Band.name(r.band)} ch${r.channel} ${r.rssi}dBm x${r.count} " +
            "conf=${LinkConstants.Confidence.name(r.confidence)}" + (r.detail?.let { " \"$it\"" } ?: "") +
            (if (r.mergedEventIds.size > 1) " (${r.mergedEventIds.size} Hunter records)" else "")

    private fun publishEvents() {
        _events.value = synchronized(mirrorLock) { mirror.rows() }
    }

    /** Rows on screen: one per MAC address. */
    fun eventCount(): Int = synchronized(mirrorLock) { mirror.rowCount() }

    // ---- background / screen-off reception test ----

    fun startBackgroundTest(note: String) {
        _background.value = BackgroundTest(
            running = true, startedMillis = now(), endedMillis = null,
            eventsAtStart = eventCount(), eventsReceivedDuring = 0, disconnectsDuring = 0, note = note
        )
        log(LogEntry.Category.BACKGROUND, "background reception test STARTED ($note)")
    }

    fun noteBackgroundDisconnect() {
        _background.update { bt -> if (bt?.running == true) bt.copy(disconnectsDuring = bt.disconnectsDuring + 1) else bt }
    }

    fun stopBackgroundTest() {
        _background.update { bt ->
            if (bt?.running == true) {
                val done = bt.copy(running = false, endedMillis = now())
                log(LogEntry.Category.BACKGROUND,
                    "background reception test ENDED: received ${done.eventsReceivedDuring} event(s), " +
                        "${done.disconnectsDuring} disconnect(s) over ${(done.endedMillis!! - done.startedMillis) / 1000}s")
                done
            } else bt
        }
    }

    /** The Hunter cleared its detections (ReconClearEvents answered OK): the same session, an empty list. */
    fun clearEventList() {
        synchronized(mirrorLock) { mirror.clear() }
        _events.value = emptyList()
    }

    fun reset() {
        synchronized(mirrorLock) { mirror.clear() }; currentSession = -1
        _events.value = emptyList(); _status.value = null; _server.value = null
        _background.value = null
    }
}
