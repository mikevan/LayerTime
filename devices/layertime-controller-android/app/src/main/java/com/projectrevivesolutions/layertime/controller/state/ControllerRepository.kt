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
import java.util.concurrent.ConcurrentHashMap

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

    private val _device = MutableStateFlow<String>("")
    val deviceLabel: StateFlow<String> = _device.asStateFlow()

    private val _status = MutableStateFlow<StatusSnapshot?>(null)
    val status: StateFlow<StatusSnapshot?> = _status.asStateFlow()

    private val _server = MutableStateFlow<Frame.HelloAck?>(null)
    val server: StateFlow<Frame.HelloAck?> = _server.asStateFlow()

    private val _events = MutableStateFlow<List<EventRow>>(emptyList())
    val events: StateFlow<List<EventRow>> = _events.asStateFlow()

    private val _log = MutableStateFlow<List<LogEntry>>(emptyList())
    val log: StateFlow<List<LogEntry>> = _log.asStateFlow()

    private val _background = MutableStateFlow<BackgroundTest?>(null)
    val background: StateFlow<BackgroundTest?> = _background.asStateFlow()

    @Volatile var deviceReport: DeviceReport? = null

    /** True after a link to a LayerWand was lost, until it is back or the operator disconnects. */
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

    // Event mirror keyed by eventId for the current session.
    private var currentSession: Int = -1
    private val mirror = ConcurrentHashMap<Long, EventRow>()

    private fun now() = System.currentTimeMillis()

    fun log(category: LogEntry.Category, message: String) {
        _log.update { it + LogEntry(now(), category, message) }
    }

    fun setConn(state: ConnState, detail: String? = null) {
        _conn.value = state
        log(LogEntry.Category.CONNECTION, "state=$state" + (detail?.let { " ($it)" } ?: ""))
    }

    fun setDeviceLabel(label: String) { _device.value = label }

    fun onServerInfo(ack: Frame.HelloAck) {
        _server.value = ack
        // HELLO_ACK carries the session id, and the LayerWand can send events before its
        // first STATUS. Take the session from here so no event is filed under session -1
        // (an unknown session broke detection-alert de-duplication across LayerWand restarts).
        if (ack.sessionId != currentSession) {
            if (currentSession != -1) log(LogEntry.Category.LINK,
                "new session ${ack.sessionId} from HELLO_ACK (was $currentSession): clearing event mirror")
            currentSession = ack.sessionId
            mirror.clear()
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
            mirror.clear()
            publishEvents()
        }
        // Only log a status line when something meaningful changed (not heartbeat).
        if (prev == null || prev.changeSeq != s.changeSeq || prev.flags != s.flags ||
            prev.eventCount != s.eventCount || prev.active != s.active
        ) {
            log(LogEntry.Category.LINK,
                "STATUS seq=${s.changeSeq} flags=0x%02X events=${s.eventCount} active=${LinkConstants.ReconTarget.name(s.active)}"
                    .format(s.flags))
        }
    }

    /** Upsert an event from an EVENT_SUMMARY frame; returns true if newly created. */
    fun onEventSummary(e: Frame.EventSummary): Boolean {
        val t = now()
        val existing = mirror[e.eventId]
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
        mirror[e.eventId] = row
        publishEvents()
        val new = existing == null
        log(LogEntry.Category.EVENT,
            (if (new) "NEW " else "upd ") +
                "#${e.eventId} ${row.detectorName} ${LinkConstants.SourceKind.name(e.sourceKind)} " +
                "${LinkConstants.Band.name(e.band)} ch${e.channel} ${e.rssi}dBm x${e.count} " +
                "age${e.ageSeconds}s conf=${LinkConstants.Confidence.name(e.confidence)}")
        // Count toward an active background test.
        _background.update { bt -> if (bt?.running == true) bt.copy(eventsReceivedDuring = bt.eventsReceivedDuring + 1) else bt }
        return new
    }

    fun onEventText(eventId: Long, field: Int, text: String) {
        val row = mirror[eventId] ?: return
        mirror[eventId] = when (field) {
            LinkConstants.TextField.SOURCE_ID -> row.copy(sourceId = text, lastUpdatedMillis = now())
            LinkConstants.TextField.DETAIL -> row.copy(detail = text, lastUpdatedMillis = now())
            else -> row
        }
        publishEvents()
        log(LogEntry.Category.EVENT, "#$eventId text field=$field => \"$text\"")
    }

    private fun publishEvents() {
        _events.value = mirror.values.sortedByDescending { it.lastUpdatedMillis }
    }

    fun eventCount(): Int = mirror.size

    // ---- background / screen-off reception test ----

    fun startBackgroundTest(note: String) {
        _background.value = BackgroundTest(
            running = true, startedMillis = now(), endedMillis = null,
            eventsAtStart = mirror.size, eventsReceivedDuring = 0, disconnectsDuring = 0, note = note
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

    fun reset() {
        mirror.clear(); currentSession = -1
        _events.value = emptyList(); _status.value = null; _server.value = null
        _background.value = null
    }
}
