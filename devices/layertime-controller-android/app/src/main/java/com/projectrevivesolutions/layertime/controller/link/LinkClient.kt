package com.projectrevivesolutions.layertime.controller.link

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.BluetoothLeScanner
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.content.pm.PackageManager
import android.location.LocationManager
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.ParcelUuid
import androidx.core.content.ContextCompat
import com.projectrevivesolutions.layertime.controller.hunt.HuntSession
import com.projectrevivesolutions.layertime.controller.model.ConnState
import com.projectrevivesolutions.layertime.controller.model.LogEntry
import com.projectrevivesolutions.layertime.controller.mission.SensorSelection
import com.projectrevivesolutions.layertime.controller.state.ControllerRepository
import com.projectrevivesolutions.layertime.controller.ui.MissionPrefs
import java.util.ArrayDeque
import java.util.UUID

/**
 * Android LayerTime Link 0.1 central (the "controller"). Discovers a LayerWand
 * by service UUID, connects, enables notifications, completes HELLO, and then
 * pulls detection events with GET_CHANGED whenever the Node's changeSeq moves.
 *
 * It honours the contract's rules: one outstanding Control request at a time,
 * ≤20-byte frames, pull-not-push. All wire encode/decode goes through
 * [LinkCodec], which is verified against contracts/vectors/link_frames.json.
 *
 * All queue manipulation and GATT calls are marshalled onto a single Handler so
 * the callback (binder) threads never race the caller.
 */
@SuppressLint("MissingPermission") // callers verify BLUETOOTH_* / location before start()
class LinkClient(private val appContext: Context) {

    private val repo = ControllerRepository
    private val handler = Handler(Looper.getMainLooper())

    private val adapter by lazy {
        (appContext.getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager)?.adapter
    }
    private var scanner: BluetoothLeScanner? = null
    private var gatt: BluetoothGatt? = null

    private var control: BluetoothGattCharacteristic? = null
    private var statusCh: BluetoothGattCharacteristic? = null
    private var dataCh: BluetoothGattCharacteristic? = null

    private var reqIdCounter = 0
    private fun nextReqId(): Int { reqIdCounter = (reqIdCounter % 254) + 1; return reqIdCounter }

    // GATT-level operation serialisation (Android allows one op at a time).
    private val gattOps = ArrayDeque<() -> Unit>()
    private var gattBusy = false

    // Link-level one-outstanding-request gate.
    private data class Pending(val reqId: Int, val kind: Int) // kind = op byte
    private val linkQueue = ArrayDeque<Pair<Int, ByteArray>>() // kind to frame
    private val textTargets = HashMap<Int, Pair<Long, Int>>() // reqId -> (eventId, field) for a queued GET_TEXT
    private var linkBusy = false
    private var pending: Pending? = null

    // Reassembly buffers.
    private val summaries = ArrayList<Frame.EventSummary>()
    private val textFrags = ArrayList<Frame.Text>()
    private var textEventId: Long = -1
    private var textField: Int = -1

    private var lastSeenChangeSeq: Long = 0
    // Link health: when the last Status arrived and what schedule it reported.
    private var lastStatusAt = 0L
    private var lastSchedule = 0
    private var lastNextReportS = 0
    private var healthCheck: Runnable? = null
    private var handshakeDone = false
    private var scanTimeout: Runnable? = null
    private var autoStartTarget: Int? = null

    // Reconnection: we keep trying to restore the link until stop() is called.
    private var wantConnected = false
    private var reconnectAttempts = 0
    private var reconnectPending: Runnable? = null

    // Per-request timeout: the Node answers every request; silence means a lost
    // link or a stuck request, so we free the gate rather than hang forever.
    private var requestTimeout: Runnable? = null

    companion object {
        const val SCAN_TIMEOUT_MS = 20_000L
        const val REQUEST_TIMEOUT_MS = 5_000L
        /** HUNT v1: GET_HUNT four to five times a second (contracts/link_0_2.md). */
        const val HUNT_POLL_MS = 220L
        const val RECONNECT_BASE_MS = 1_500L
        const val RECONNECT_MAX_MS = 15_000L

        /** Shown when BLE scanning is blocked by the system Location switch (Android 11 and older). */
        const val LOCATION_OFF_MESSAGE = "Turn on Location in Settings; this Android version needs it to scan for the Hunter."

        /**
         * On Android 11 (API 30) and older, Android returns no BLE scan results unless
         * the system Location switch is on, whatever permissions the app holds. The
         * LOKMAT watch (Android 10) showed this as a silent empty scan. Android 12+
         * scans with BLUETOOTH_SCAN and does not need Location.
         */
        fun locationSwitchBlocksScanning(ctx: Context): Boolean {
            if (Build.VERSION.SDK_INT > Build.VERSION_CODES.R) return false
            val lm = ctx.getSystemService(Context.LOCATION_SERVICE) as? LocationManager ?: return false
            val enabled = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) lm.isLocationEnabled
                else lm.isProviderEnabled(LocationManager.GPS_PROVIDER) || lm.isProviderEnabled(LocationManager.NETWORK_PROVIDER)
            return !enabled
        }
    }

    fun hasRequiredPermissions(): Boolean {
        val needed = buildList {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                add(Manifest.permission.BLUETOOTH_SCAN); add(Manifest.permission.BLUETOOTH_CONNECT)
            } else {
                add(Manifest.permission.ACCESS_FINE_LOCATION)
            }
        }
        return needed.all { ContextCompat.checkSelfPermission(appContext, it) == PackageManager.PERMISSION_GRANTED }
    }

    /** Begin scanning for a LayerWand. [autoStart] optionally issues ReconStart after HELLO. */
    fun start(autoStart: Int? = null) = handler.post {
        // One tap is one clean attempt (build 69). Before this, a tap during a reconnect wait
        // left the scheduled retry in place; when it fired it closed the connection the tap had
        // just started, so the first tap looked dead and a second was needed. A tap could also
        // start a second scan on top of a running one, which Android rejects.
        reconnectPending?.let { handler.removeCallbacks(it) }; reconnectPending = null
        stopScan()
        if (!handshakeDone) {
            gatt?.let { g -> runCatching { g.disconnect() }; runCatching { g.close() } }
            gatt = null; control = null; statusCh = null; dataCh = null
            linkBusy = false; gattBusy = false
            linkQueue.clear(); gattOps.clear(); textTargets.clear()
        }
        wantConnected = true
        reconnectAttempts = 0
        autoStartTarget = autoStart
        val a = adapter
        if (a == null || !a.isEnabled) {
            repo.setConn(ConnState.FAILED, ConnNotice.BLUETOOTH_OFF); return@post
        }
        if (locationSwitchBlocksScanning(appContext)) {
            repo.log(LogEntry.Category.ERROR, LOCATION_OFF_MESSAGE)
            repo.setConn(ConnState.FAILED, LOCATION_OFF_MESSAGE); return@post
        }
        scanner = a.bluetoothLeScanner
        val filter = ScanFilter.Builder()
            .setServiceUuid(ParcelUuid(UUID.fromString(LinkConstants.SERVICE_UUID)))
            .build()
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()
        repo.setConn(ConnState.SCANNING, "by service UUID ${LinkConstants.SERVICE_UUID}")
        scanner?.startScan(listOf(filter), settings, scanCallback)
        scanTimeout = Runnable {
            if (gatt == null) { stopScan(); repo.setConn(ConnState.FAILED, "no Hunter found in 20s") }
        }.also { handler.postDelayed(it, 20_000) }
    }

    fun stop() = handler.post {
        stopHealthCheck()
        repo.setReconnecting(false)
        wantConnected = false
        reconnectPending?.let { handler.removeCallbacks(it) }; reconnectPending = null
        requestTimeout?.let { handler.removeCallbacks(it) }; requestTimeout = null
        huntWanted = false; huntPollOutstanding = false; handler.removeCallbacks(huntPoll)
        stopScan()
        gatt?.let { g -> g.disconnect(); g.close() }
        gatt = null; control = null; statusCh = null; dataCh = null
        handshakeDone = false; linkBusy = false; gattBusy = false; dropEvidence()
        linkQueue.clear(); gattOps.clear(); textTargets.clear(); summaries.clear(); textFrags.clear()
        repo.setConn(ConnState.IDLE)
    }

    // ---- link health (Status heartbeat watchdog) ----

    /** Checks once a second; the clock starts at connect so a link that never sends Status is caught too. */
    private fun startHealthCheck() {
        stopHealthCheck()
        lastStatusAt = android.os.SystemClock.elapsedRealtime()
        healthCheck = object : Runnable {
            override fun run() {
                val silence = android.os.SystemClock.elapsedRealtime() - lastStatusAt
                when (LinkHealth.state(silence, lastSchedule, lastNextReportS)) {
                    LinkHealth.State.OK -> repo.setLinkStale(false)
                    LinkHealth.State.STALE -> repo.setLinkStale(true)
                    LinkHealth.State.LOST -> { linkLost(silence); return }
                }
                handler.postDelayed(this, 1_000L)
            }
        }.also { handler.postDelayed(it, 1_000L) }
    }

    private fun stopHealthCheck() {
        healthCheck?.let { handler.removeCallbacks(it) }; healthCheck = null
        repo.setLinkStale(false)
    }

    /** No Status within the allowance: drop the connection ourselves and reconnect. */
    private fun linkLost(silenceMs: Long) {
        healthCheck = null
        repo.setLinkStale(false)
        repo.log(LogEntry.Category.ERROR, "link lost: no Status for ${silenceMs / 1000} s; dropping and reconnecting")
        gatt?.let { g -> runCatching { g.disconnect() }; runCatching { g.close() } }
        gatt = null; control = null; statusCh = null; dataCh = null
        handshakeDone = false; linkBusy = false; gattBusy = false; dropEvidence()
        linkQueue.clear(); gattOps.clear(); textTargets.clear()
        requestTimeout?.let { handler.removeCallbacks(it) }; requestTimeout = null
        repo.noteBackgroundDisconnect()
        repo.setConn(ConnState.DISCONNECTED, "no Status for ${silenceMs / 1000} s")
        if (wantConnected) repo.setReconnecting(true)
        scheduleReconnect("link timed out")
    }

    /** Schedule a reconnect with capped exponential backoff, unless stop() ran. */
    private fun scheduleReconnect(why: String) {
        if (!wantConnected) return
        reconnectPending?.let { handler.removeCallbacks(it) }
        val delay = (RECONNECT_BASE_MS * (1L shl reconnectAttempts.coerceAtMost(3)))
            .coerceAtMost(RECONNECT_MAX_MS)
        reconnectAttempts++
        repo.log(LogEntry.Category.CONNECTION, "reconnect #$reconnectAttempts in ${delay}ms ($why)")
        reconnectPending = Runnable {
            gatt?.let { g -> runCatching { g.close() } }
            gatt = null; control = null; statusCh = null; dataCh = null
            handshakeDone = false; linkBusy = false; gattBusy = false; dropEvidence()
            linkQueue.clear(); gattOps.clear(); textTargets.clear()
            start(autoStartTarget)
        }.also { handler.postDelayed(it, delay) }
    }

    private fun stopScan() {
        scanTimeout?.let { handler.removeCallbacks(it) }; scanTimeout = null
        runCatching { scanner?.stopScan(scanCallback) }
    }

    // ---- scanning ----

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val device = result.device ?: return
            if (gatt != null) return // already connecting/connected
            stopScan()
            val name = runCatching { device.name }.getOrNull() ?: result.scanRecord?.deviceName
            repo.setDeviceLabel((name ?: "Hunter") + " / " + device.address)
            repo.setConn(ConnState.CONNECTING, "${name ?: "?"} ${device.address} rssi=${result.rssi}")
            connect(device)
        }

        override fun onScanFailed(errorCode: Int) {
            // A scan is already running for this callback: not a failure, it keeps scanning.
            if (errorCode == SCAN_FAILED_ALREADY_STARTED) { repo.log(LogEntry.Category.CONNECTION, "scan already running; continuing"); return }
            repo.setConn(ConnState.FAILED, "scan failed code=$errorCode")
        }
    }

    private fun connect(device: BluetoothDevice) {
        gatt = device.connectGatt(appContext, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
    }

    // ---- GATT callback ----

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            handler.post {
                when (newState) {
                    BluetoothProfile.STATE_CONNECTED -> {
                        repo.setConn(ConnState.DISCOVERING, "gattStatus=$status")
                        g.discoverServices()
                        startHealthCheck()
                    }
                    BluetoothProfile.STATE_DISCONNECTED -> {
                        stopHealthCheck()
                        // A measurement in progress ends with the connection, and the MTU resets.
                        mtuRequested = false
                        if (pingBurstLeft > 0) { pingBurstLeft = 0; LinkMeasure.addLine("PING burst stopped: the link dropped.") }
                        bulkRun?.let { reportBulk(it, null, "the link dropped.") }
                        LinkMeasure.setMtu(23)
                        handshakeDone = false; linkBusy = false; gattBusy = false; dropEvidence()
                        huntPollOutstanding = false
                        if (huntWanted) HuntSession.setProblem("The link to the Hunter dropped. Reconnecting.")
                        linkQueue.clear(); gattOps.clear(); textTargets.clear()
                        requestTimeout?.let { handler.removeCallbacks(it) }; requestTimeout = null
                        repo.noteBackgroundDisconnect()
                        repo.setConn(ConnState.DISCONNECTED, "gattStatus=$status")
                        // The contract is pull-not-push: monitoring continues on
                        // the Node while we are away, so we simply reconnect and
                        // re-pull with GET_CHANGED from lastSeenChangeSeq.
                        if (wantConnected) { repo.setReconnecting(true); scheduleReconnect("gattStatus=$status") }
                    }
                }
            }
        }

        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            handler.post {
                if (status != BluetoothGatt.GATT_SUCCESS) {
                    repo.setConn(ConnState.FAILED, "service discovery status=$status"); return@post
                }
                val svc = g.getService(UUID.fromString(LinkConstants.SERVICE_UUID))
                if (svc == null) { repo.setConn(ConnState.FAILED, "LayerTime service not present"); return@post }
                control = svc.getCharacteristic(UUID.fromString(LinkConstants.CONTROL_UUID))
                statusCh = svc.getCharacteristic(UUID.fromString(LinkConstants.STATUS_UUID))
                dataCh = svc.getCharacteristic(UUID.fromString(LinkConstants.DATA_UUID))
                if (control == null || statusCh == null || dataCh == null) {
                    repo.setConn(ConnState.FAILED, "missing Control/Status/Data characteristic"); return@post
                }
                repo.setConn(ConnState.HANDSHAKING, "enabling notifications")
                // Enable notifications on Status then Data, then HELLO.
                enableNotify(statusCh!!)
                enableNotify(dataCh!!)
                enqueueGatt { sendControl(nextReqId().let { id ->
                    pending = Pending(id, LinkConstants.Op.HELLO)
                    LinkCodec.hello(id)
                }) }
            }
        }

        @Deprecated("Pre-API 33 notification callback")
        override fun onCharacteristicChanged(g: BluetoothGatt, ch: BluetoothGattCharacteristic) {
            @Suppress("DEPRECATION") val value = ch.value ?: ByteArray(0)
            handler.post { onNotify(ch.uuid, value) }
        }

        override fun onCharacteristicChanged(g: BluetoothGatt, ch: BluetoothGattCharacteristic, value: ByteArray) {
            handler.post { onNotify(ch.uuid, value) }
        }

        override fun onDescriptorWrite(g: BluetoothGatt, d: BluetoothGattDescriptor, status: Int) {
            handler.post { gattOpComplete() }
        }

        // Only the LINK MEASURE panel asks for a larger MTU (measureRequestMtu).
        override fun onMtuChanged(g: BluetoothGatt, mtu: Int, status: Int) {
            handler.post {
                LinkMeasure.setMtu(mtu)
                LinkMeasure.addLine(if (status == BluetoothGatt.GATT_SUCCESS) "MTU is now $mtu (asked for ${LinkMeasure.MTU_REQUEST})."
                    else "MTU request failed, status $status. MTU stays $mtu.")
                repo.log(LogEntry.Category.LINK, "MTU changed to $mtu status=$status")
                // Only complete the GATT op we started; a change the LayerWand
                // started must not release another op still in flight.
                if (mtuRequested) { mtuRequested = false; gattOpComplete() }
            }
        }

        override fun onCharacteristicWrite(g: BluetoothGatt, ch: BluetoothGattCharacteristic, status: Int) {
            handler.post {
                if (status != BluetoothGatt.GATT_SUCCESS) {
                    repo.log(LogEntry.Category.ERROR, "Control write failed status=$status; freeing link gate")
                    linkBusy = false; pending = null
                }
                gattOpComplete()
                pumpLink()
            }
        }
    }

    // ---- GATT op queue ----

    private fun enqueueGatt(op: () -> Unit) { gattOps.add(op); drainGatt() }
    private fun drainGatt() {
        if (gattBusy) return
        val op = gattOps.poll() ?: return
        gattBusy = true
        op()
    }
    private fun gattOpComplete() { gattBusy = false; drainGatt() }

    private fun enableNotify(ch: BluetoothGattCharacteristic) = enqueueGatt {
        val g = gatt ?: return@enqueueGatt gattOpComplete()
        g.setCharacteristicNotification(ch, true)
        val cccd = ch.getDescriptor(UUID.fromString(LinkConstants.CCCD_UUID))
        if (cccd == null) { repo.log(LogEntry.Category.ERROR, "no CCCD on ${ch.uuid}"); return@enqueueGatt gattOpComplete() }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            g.writeDescriptor(cccd, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)
        } else {
            @Suppress("DEPRECATION") cccd.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
            @Suppress("DEPRECATION") g.writeDescriptor(cccd)
        }
    }

    private fun sendControl(frame: ByteArray) {
        val g = gatt; val c = control
        if (g == null || c == null) { gattOpComplete(); return }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            g.writeCharacteristic(c, frame, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)
        } else {
            @Suppress("DEPRECATION") c.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
            @Suppress("DEPRECATION") c.value = frame
            @Suppress("DEPRECATION") g.writeCharacteristic(c)
        }
    }

    // ---- Link request gate (one outstanding) ----

    private fun submit(kind: Int, frame: ByteArray) = handler.post {
        linkQueue.add(kind to frame); pumpLink()
    }

    private fun pumpLink() {
        if (linkBusy || !handshakeDone && pending?.kind != LinkConstants.Op.HELLO) {
            // During handshake only the HELLO (already pending) may proceed.
        }
        if (linkBusy) return
        val (kind, frame) = linkQueue.poll() ?: return
        val reqId = frame[1].toInt() and 0xFF
        pending = Pending(reqId, kind)
        linkBusy = true
        if (kind == LinkConstants.Op.PING && pingBurstLeft > 0) pingSentNs = android.os.SystemClock.elapsedRealtimeNanos()
        if (kind == LinkMeasure.OP_BULK) bulkRun?.requestedNs = android.os.SystemClock.elapsedRealtimeNanos()
        if (kind == LinkConstants.Op.GET_CHANGED) summaries.clear()
        if (kind == LinkConstants.Op.GET_TEXT) {
            textFrags.clear()
            // Bind the reply target now, at send time. Several GET_TEXTs can sit in the
            // FIFO at once; binding at queue time would let a later one overwrite it.
            textTargets.remove(reqId)?.let { textEventId = it.first; textField = it.second }
        }
        enqueueGatt { sendControl(frame) }
        armRequestTimeout(reqId, kind)
    }

    private fun armRequestTimeout(reqId: Int, kind: Int) {
        requestTimeout?.let { handler.removeCallbacks(it) }
        requestTimeout = Runnable {
            if (linkBusy && pending?.reqId == reqId) {
                repo.log(LogEntry.Category.ERROR,
                    "request #$reqId op=0x%02X timed out after ${REQUEST_TIMEOUT_MS}ms; freeing link gate".format(kind))
                finishLink()
                if (kind == LinkConstants.Op.PING && pingBurstLeft > 0) onBurstPing(null)
                if (kind == LinkMeasure.OP_BULK) bulkRun?.let { reportBulk(it, null, "timed out") }
                if (kind == LinkHunt.OP_GET_HUNT) huntPollOutstanding = false
                if (LinkEvidence.isEvidenceOp(kind) && evidence != null) failEvidence("The Hunter stopped sending frames (no reply for ${REQUEST_TIMEOUT_MS / 1000} s).")
            }
        }.also { handler.postDelayed(it, REQUEST_TIMEOUT_MS) }
    }

    private fun finishLink() {
        requestTimeout?.let { handler.removeCallbacks(it) }; requestTimeout = null
        linkBusy = false; pending = null; pumpLink()
    }

    // ---- public commands ----

    fun ping() {
        val id = nextReqId(); submit(LinkConstants.Op.PING, LinkCodec.ping(id, System.nanoTime() and 0xFFFFFFFFL))
        repo.log(LogEntry.Category.COMMAND, "PING #$id")
    }

    fun reconStart(target: Int) {
        val id = nextReqId(); submit(LinkConstants.Op.COMMAND, LinkCodec.reconStart(id, target))
        repo.log(LogEntry.Category.COMMAND, "ReconStart(${LinkConstants.ReconTarget.name(target)}) #$id")
    }

    fun reconStop() {
        val id = nextReqId(); submit(LinkConstants.Op.COMMAND, LinkCodec.reconStop(id))
        repo.log(LogEntry.Category.COMMAND, "ReconStop #$id")
    }

    // ---- SetSensors: the MISSION allow-list (2026-11-04) ----

    private var sensorSetSupported = false
    private var sentSensorMask: Long? = null

    /** Sends the saved MISSION list. Only a Hunter with Capability.SENSOR_SET takes it. */
    private fun sendSensors() {
        val mask = SensorSelection.mask(MissionPrefs.readSensors(appContext))
        sentSensorMask = mask
        val id = nextReqId(); submit(LinkConstants.Op.COMMAND, LinkCodec.setSensors(id, mask))
        repo.log(LogEntry.Category.COMMAND, "SetSensors 0x%05X #$id".format(mask))
    }

    /** MISSION changed; the new list is already saved. Sent now when the Hunter is up and takes it. */
    fun sensorsChanged() = handler.post {
        repo.setSensorsApplied(null)
        if (handshakeDone && sensorSetSupported) sendSensors()
    }

    fun clearEvents() {
        val id = nextReqId(); submit(LinkConstants.Op.COMMAND, LinkCodec.reconClearEvents(id))
        repo.log(LogEntry.Category.COMMAND, "ReconClearEvents #$id")
    }

    private fun getChanged() {
        val id = nextReqId(); submit(LinkConstants.Op.GET_CHANGED, LinkCodec.getChanged(id, lastSeenChangeSeq))
    }

    private fun getText(eventId: Long, field: Int) {
        val id = nextReqId()
        val frame = LinkCodec.getText(id, eventId, field)
        handler.post { textTargets[id] = eventId to field; linkQueue.add(LinkConstants.Op.GET_TEXT to frame); pumpLink() }
    }

    // ---- notification handling ----

    private fun onNotify(uuid: UUID, value: ByteArray) {
        when (uuid.toString().lowercase()) {
            LinkConstants.STATUS_UUID -> onStatusNotify(value)
            LinkConstants.DATA_UUID -> onDataFrame(value)
            else -> repo.log(LogEntry.Category.LINK, "notify on ${uuid}: ${LinkCodec.toHex(value)}")
        }
    }

    private fun onStatusNotify(value: ByteArray) {
        val s = LinkCodec.decodeStatus(value) ?: run {
            repo.log(LogEntry.Category.ERROR, "bad Status length ${value.size}"); return
        }
        repo.onStatus(s)
        lastStatusAt = android.os.SystemClock.elapsedRealtime()
        lastSchedule = s.schedule; lastNextReportS = s.nextReportS
        repo.setLinkStale(false)
        // Pull changed events whenever the Node's changeSeq has advanced.
        if (handshakeDone && s.changeSeq > lastSeenChangeSeq) getChanged()
    }

    private fun onDataFrame(value: ByteArray) {
        if (LinkMeasure.isBulk(value) && bulkRun != null) { onBulkFrame(value); return }
        if (LinkHunt.isReading(value)) { onHuntReading(value); return }
        if (LinkEvidence.isEvidenceFrame(value)) { onEvidenceFrame(value); return }
        when (val f = LinkCodec.decodeFrame(value)) {
            is Frame.HelloAck -> {
                repo.onServerInfo(f)
                if (f.linkStatus == LinkConstants.LinkStatus.VERSION_MISMATCH) {
                    repo.setConn(ConnState.FAILED,
                        "version mismatch: server ${f.serverMajor}.${f.serverMinor}, client ${LinkConstants.CLIENT_MAJOR}.${LinkConstants.CLIENT_MINOR}")
                } else {
                    handshakeDone = true
                    repo.setReconnecting(false)
                    reconnectAttempts = 0 // link restored; reset backoff
                    repo.setConn(ConnState.CONNECTED, "link up, server ${f.serverMajor}.${f.serverMinor}")
                    // The MISSION sensor list goes first, so a start below never runs
                    // a sensor the operator switched off, not even for one frame.
                    sensorSetSupported = f.capabilities and LinkConstants.Capability.SENSOR_SET != 0
                    if (sensorSetSupported) sendSensors()
                    // Evidence (slice 2): a Hunter that keeps frames streams them at the
                    // connection's packet size, so ask for the large one (Phase 0 M5:
                    // 56 KB/s at MTU 247 against 15 KB/s at the default 23).
                    rawCapture = f.capabilities and LinkEvidence.CAP_RAW_CAPTURE != 0
                    if (rawCapture && LinkMeasure.state.value.mtu < LinkMeasure.MTU_REQUEST) requestLargeMtu()
                    autoStartTarget?.let { reconStart(it) }
                    // A hunt that was running when the link dropped picks up again.
                    if (huntWanted) sendHuntStart()
                    getChanged() // pull whatever the Node already holds
                }
                finishLink()
            }
            is Frame.Ack -> {
                if (pingBurstLeft > 0) {
                    val rttMs = (android.os.SystemClock.elapsedRealtimeNanos() - pingSentNs) / 1e6
                    finishLink(); onBurstPing(rttMs)
                } else {
                    repo.log(LogEntry.Category.COMMAND, "ACK token=0x%08X hb=${f.heartbeat}".format(f.token)); finishLink()
                }
            }
            is Frame.Result -> {
                if (f.commandType == LinkHunt.OP_HUNT_START || f.commandType == LinkHunt.OP_HUNT_STOP) {
                    onHuntResult(f.commandType, f.commandResult)
                } else if (LinkEvidence.isEvidenceOp(f.commandType) && evidence != null) {
                    failEvidence(when (f.commandResult) {
                        LinkConstants.CommandResult.NOT_READY -> "The Hunter kept no frames for this detection. Its window was reused by newer detections, or the detection was not on Wi-Fi."
                        LinkConstants.CommandResult.INVALID_ARGUMENT -> "The Hunter no longer holds this detection."
                        else -> "The Hunter answered ${LinkConstants.CommandResult.name(f.commandResult)}."
                    })
                } else if (f.commandType == LinkConstants.CommandType.SET_SENSORS) {
                    val ok = f.commandResult == LinkConstants.CommandResult.OK
                    repo.setSensorsApplied(if (ok) sentSensorMask else null)
                    repo.log(if (ok) LogEntry.Category.COMMAND else LogEntry.Category.ERROR,
                        "RESULT SetSensors 0x%05X => %s".format(sentSensorMask ?: 0L, LinkConstants.CommandResult.name(f.commandResult)))
                } else {
                    repo.log(LogEntry.Category.COMMAND,
                        "RESULT cmd=${f.commandType} => ${LinkConstants.CommandResult.name(f.commandResult)}")
                }
                finishLink()
            }
            is Frame.EventSummary -> {
                summaries.add(f)
                val newOne = repo.onEventSummary(f)
                // Fetch human-readable identifiers the first time we see them.
                if (newOne && f.hasSourceId) getTextQueued(f.eventId, LinkConstants.TextField.SOURCE_ID)
                if (newOne && f.hasDetail) getTextQueued(f.eventId, LinkConstants.TextField.DETAIL)
            }
            is Frame.Text -> textFrags.add(f)
            is Frame.End -> if (pending?.kind?.let { LinkEvidence.isEvidenceOp(it) } == true) {
                val kind = pending!!.kind
                finishLink()
                onEvidenceEnd(kind)
            } else {
                if (pending?.kind == LinkConstants.Op.GET_TEXT) {
                    val ordered = textFrags.sortedBy { it.index }
                    val bytes = ordered.fold(ByteArray(0)) { acc, t -> acc + t.bytes }
                    if (bytes.isNotEmpty()) repo.onEventText(textEventId, textField, String(bytes, Charsets.UTF_8))
                    else repo.log(LogEntry.Category.LINK, "GET_TEXT event #$textEventId field=$textField came back empty (record likely evicted on the Hunter)")
                } else { // GET_CHANGED run
                    lastSeenChangeSeq = f.changeSeq
                    if (f.gap == 1) repo.log(LogEntry.Category.LINK, "GET_CHANGED gap=1: a change was dropped before we saw it")
                }
                finishLink()
            }
            is Frame.Error -> {
                repo.log(LogEntry.Category.ERROR, "ERROR linkStatus=${LinkConstants.LinkStatus.name(f.linkStatus)} reqId=${f.reqId}")
                val wasBulk = pending?.kind == LinkMeasure.OP_BULK
                if (pending?.kind?.let { LinkEvidence.isEvidenceOp(it) } == true && evidence != null) failEvidence(
                    if (f.linkStatus == LinkConstants.LinkStatus.UNKNOWN_OP) "This Hunter's firmware does not keep frames. Flash the evidence firmware."
                    else "The Hunter refused the request (${LinkConstants.LinkStatus.name(f.linkStatus)}).")
                val huntOp = pending?.kind?.takeIf { LinkHunt.isHuntOp(it) }
                if (huntOp == LinkHunt.OP_GET_HUNT) huntPollOutstanding = false
                if (huntOp != null && huntWanted) HuntSession.setProblem(
                    if (f.linkStatus == LinkConstants.LinkStatus.UNKNOWN_OP)
                        "This Hunter cannot hunt yet. Flash it with the hunt firmware."
                    else "The Hunter refused the hunt request (${LinkConstants.LinkStatus.name(f.linkStatus)}).")
                finishLink()
                if (wasBulk) bulkRun?.let {
                    reportBulk(it, null, "the Hunter answered ${LinkConstants.LinkStatus.name(f.linkStatus)}. " +
                        "BULK needs the measurement firmware (tdongle_c5_measure).")
                }
            }
            is Frame.Unknown -> repo.log(LogEntry.Category.ERROR, "unknown frame type=0x%02X: ${LinkCodec.toHex(f.raw)}".format(f.type))
        }
    }

    // ---- Link 0.2 evidence: the frames behind a detection (slice 2) ----

    private var rawCapture = false
    private var evidence: LinkEvidence.Assembler? = null
    private var evidenceStartedMs = 0L
    private var evidenceResumes = 0

    /** Whether the connected Hunter keeps frames (HELLO_ACK capability bit 5). */
    val keepsFrames: Boolean get() = rawCapture

    /** Pulls one detection's window; EvidencePull reports progress and the pcap. */
    fun pullEvidence(eventId: Long) = handler.post {
        if (evidence != null) return@post
        if (!handshakeDone) { EvidencePull.set(EvidencePull.State.Failed(eventId, "The Hunter is not connected.")); return@post }
        if (!rawCapture) { EvidencePull.set(EvidencePull.State.Failed(eventId, "This Hunter's firmware does not keep frames. Flash the evidence firmware.")); return@post }
        evidence = LinkEvidence.Assembler(eventId)
        evidenceStartedMs = android.os.SystemClock.elapsedRealtime()
        evidenceResumes = 0
        EvidencePull.set(EvidencePull.State.Pulling(eventId, "Listing the frames.", 0, 0, 0))
        val id = nextReqId(); submit(LinkEvidence.OP_GET_FRAMES, LinkEvidence.getFrames(id, eventId))
        repo.log(LogEntry.Category.COMMAND, "GET_FRAMES event #$eventId #$id")
    }

    private fun requestLargeMtu() = enqueueGatt {
        val g = gatt
        mtuRequested = g != null && g.requestMtu(LinkMeasure.MTU_REQUEST)
        if (!mtuRequested) gattOpComplete()
    }

    private fun onEvidenceFrame(value: ByteArray) {
        val a = evidence ?: return
        when ((value[0].toInt() and 0xFF)) {
            LinkEvidence.TYPE_FRAME_WINDOW -> LinkEvidence.decodeWindow(value)?.let { a.onWindow(it) }
            LinkEvidence.TYPE_FRAME_INDEX -> LinkEvidence.decodeIndex(value)?.let { a.onIndex(it) }
            LinkEvidence.TYPE_FRAME_BYTES -> LinkEvidence.decodeBytes(value)?.let { a.onBytes(it) }
        }
        // A long stream is one request: keep its timeout alive while frames arrive.
        pending?.let { armRequestTimeout(it.reqId, it.kind) }
        val phase = if (a.bytesReceived == 0) "Listing the frames." else "Pulling the frames."
        EvidencePull.set(EvidencePull.State.Pulling(a.eventId, phase, a.listed, a.bytesReceived, a.bytesExpected))
    }

    private fun onEvidenceEnd(kind: Int) {
        val a = evidence ?: return
        if (kind == LinkEvidence.OP_GET_FRAMES) {
            val w = a.window ?: return failEvidence("The Hunter sent no window header.")
            if (a.listed == 0) return failEvidence("The Hunter's window for this detection is empty.")
            if (a.listed != w.retained) repo.log(LogEntry.Category.LINK, "GET_FRAMES listed ${a.listed} of ${w.retained} (window reused while listing)")
            val id = nextReqId(); submit(LinkEvidence.OP_GET_FRAME_BYTES, LinkEvidence.getFrameBytes(id, a.eventId, 0, 0))
            return
        }
        if (a.complete()) return finishEvidence(a)
        val resume = a.resumePoint()
        if (resume != null && a.gaps == 0 && evidenceResumes < 2) {
            evidenceResumes++
            repo.log(LogEntry.Category.LINK, "GET_FRAME_BYTES incomplete; resuming at frame ${resume.first} byte ${resume.second}")
            val id = nextReqId(); submit(LinkEvidence.OP_GET_FRAME_BYTES, LinkEvidence.getFrameBytes(id, a.eventId, resume.first, resume.second))
            return
        }
        failEvidence("Some frames arrived incomplete: " + a.missing().take(3).joinToString("; ") + (if (a.gaps > 0) "; ${a.gaps} out-of-order chunks refused." else "."))
    }

    private fun finishEvidence(a: LinkEvidence.Assembler) {
        evidence = null
        val frames = a.frames()
        val dir = java.io.File(appContext.cacheDir, "evidence").apply { mkdirs() }
        val file = java.io.File(dir, "hunter_event_${a.eventId}.pcap")
        file.writeBytes(LinkEvidence.toPcap(frames, System.currentTimeMillis()))
        val ms = android.os.SystemClock.elapsedRealtime() - evidenceStartedMs
        repo.log(LogEntry.Category.LINK, "evidence event #${a.eventId}: ${frames.size} frames, ${a.bytesReceived} bytes in $ms ms")
        EvidencePull.set(EvidencePull.State.Done(a.eventId, a.window!!, frames.size, file, ms))
    }

    private fun dropEvidence() { if (evidence != null) failEvidence("The connection dropped during the pull.") }

    private fun failEvidence(message: String) {
        val a = evidence ?: return
        evidence = null
        repo.log(LogEntry.Category.ERROR, "evidence event #${a.eventId}: $message")
        EvidencePull.set(EvidencePull.State.Failed(a.eventId, message))
    }

    // ---- Link 0.2 Phase 0 measurement (the LINK MEASURE panel) ----

    private var mtuRequested = false
    private var pingBurstLeft = 0
    private var pingBurstTotal = 0
    private var pingLost = 0
    private var pingSentNs = 0L
    private val pingRtts = ArrayList<Double>()

    private class BulkRun(val count: Int, val size: Int) {
        var requestedNs = 0L
        var firstNs = 0L
        var lastNs = 0L
        var received = 0
        var bytes = 0L
        var broken = 0
        var outOfOrder = 0
        var lastSeq = -1
    }
    private var bulkRun: BulkRun? = null

    /** Ask the LayerWand for a larger ATT MTU. Affects this connection until it drops. */
    fun measureRequestMtu() = handler.post {
        if (!handshakeDone) { LinkMeasure.addLine("MTU request: not connected."); return@post }
        LinkMeasure.setRunning("Requesting MTU ${LinkMeasure.MTU_REQUEST}.")
        enqueueGatt {
            val g = gatt
            mtuRequested = g != null && g.requestMtu(LinkMeasure.MTU_REQUEST)
            if (!mtuRequested) { LinkMeasure.addLine("MTU request could not be sent."); gattOpComplete() }
        }
    }

    /** One request at a time, each sent when the last reply arrives: the round trip that limits HUNT polling. */
    fun measurePingBurst(count: Int = 100) = handler.post {
        if (!handshakeDone) { LinkMeasure.addLine("PING burst: not connected."); return@post }
        if (pingBurstLeft > 0 || bulkRun != null) return@post
        pingRtts.clear(); pingLost = 0; pingBurstTotal = count; pingBurstLeft = count
        LinkMeasure.setRunning("PING burst, $count requests.")
        submit(LinkConstants.Op.PING, LinkCodec.ping(nextReqId(), 0))
    }

    private fun onBurstPing(rttMs: Double?) {
        if (rttMs != null) pingRtts.add(rttMs) else pingLost++
        pingBurstLeft--
        if (pingBurstLeft > 0) {
            submit(LinkConstants.Op.PING, LinkCodec.ping(nextReqId(), 0)); return
        }
        val s = LinkMeasure.stats(pingRtts)
        LinkMeasure.addLine(if (s == null) "PING burst: no replies of $pingBurstTotal." else
            "PING x$pingBurstTotal: lost $pingLost, round trip ms min %.0f, median %.0f, p95 %.0f, max %.0f. Fastest steady poll about %.1f per second."
                .format(s.min, s.median, s.p95, s.max, 1000.0 / s.p95))
        repo.log(LogEntry.Category.LINK, "MEASURE ping burst done: ${LinkMeasure.state.value.lines.lastOrNull()}")
    }

    /** BULK: the LayerWand sends count frames of size bytes (capped at MTU minus 3), then BULK_DONE. */
    fun measureBulk(count: Int, size: Int) = handler.post {
        if (!handshakeDone) { LinkMeasure.addLine("BULK: not connected."); return@post }
        if (pingBurstLeft > 0 || bulkRun != null) return@post
        bulkRun = BulkRun(count, size)
        LinkMeasure.setRunning("BULK, $count frames of up to $size bytes.")
        submit(LinkMeasure.OP_BULK, LinkMeasure.bulk(nextReqId(), count, size))
    }

    private fun onBulkFrame(value: ByteArray) {
        val run = bulkRun ?: return
        val now = android.os.SystemClock.elapsedRealtimeNanos()
        val done = LinkMeasure.decodeDone(value)
        if (done != null) {
            finishLink()
            reportBulk(run, done, null)
            return
        }
        if (run.firstNs == 0L) run.firstNs = now
        run.lastNs = now
        run.received++
        run.bytes += value.size
        if (!LinkMeasure.dataFrameIntact(value)) run.broken++
        val seq = LinkMeasure.dataSeq(value)
        if (seq != run.lastSeq + 1) run.outOfOrder++
        run.lastSeq = seq
        // A long run must not trip the 5 s request timeout while frames keep arriving.
        pending?.let { armRequestTimeout(it.reqId, it.kind) }
    }

    private fun reportBulk(run: BulkRun, done: LinkMeasure.Done?, failure: String?) {
        bulkRun = null
        val spanMs = if (run.lastNs > run.firstNs) (run.lastNs - run.firstNs) / 1e6 else 0.0
        val totalMs = if (run.requestedNs > 0) (android.os.SystemClock.elapsedRealtimeNanos() - run.requestedNs) / 1e6 else 0.0
        val rate = if (totalMs > 0) run.bytes * 1000.0 / totalMs else 0.0
        val line = buildString {
            append("BULK ${run.count} x ${run.size}: ")
            if (failure != null) append("$failure ")
            append("received ${run.received} frames, ${run.bytes} bytes, broken ${run.broken}, out of order ${run.outOfOrder}, ")
            append("%.0f ms request to done (%.0f ms first to last frame), %.0f bytes/s at the watch.".format(totalMs, spanMs, rate))
            if (done != null) append(" Hunter: sent ${done.sent} x ${done.size} bytes in ${done.elapsedMs} ms at MTU ${done.mtu}, notify refused ${done.refused}.")
        }
        LinkMeasure.addLine(line)
        repo.log(LogEntry.Category.LINK, "MEASURE $line")
    }

    // ---- HUNT v1 (claude/hunt_v1_layervixen_plan.md) ----

    private var huntWanted = false
    private var huntPollOutstanding = false
    private var huntReadings = 0L

    /** GET_HUNT every HUNT_POLL_MS, one request at a time; skipped while one is outstanding. */
    private val huntPoll = object : Runnable {
        override fun run() {
            if (!huntWanted) return
            if (handshakeDone && !huntPollOutstanding) {
                huntPollOutstanding = true
                submit(LinkHunt.OP_GET_HUNT, LinkHunt.getHunt(nextReqId()))
            }
            handler.postDelayed(this, HUNT_POLL_MS)
        }
    }

    private fun sendHuntStart() {
        val id = nextReqId()
        submit(LinkHunt.OP_HUNT_START, LinkHunt.huntStart(id))
        repo.log(LogEntry.Category.COMMAND, "HUNT_START (any Vixen) #$id")
    }

    /** Starts (or restarts) a hunt for any LayerVixen and keeps polling until huntStop(). */
    fun huntStart() = handler.post {
        huntWanted = true
        huntReadings = 0
        if (handshakeDone) sendHuntStart()
        else HuntSession.setProblem("The Hunter is not connected. Reconnecting.")
        handler.removeCallbacks(huntPoll)
        handler.post(huntPoll)
    }

    fun huntStop() = handler.post {
        if (!huntWanted) return@post
        huntWanted = false
        handler.removeCallbacks(huntPoll)
        if (handshakeDone) {
            val id = nextReqId()
            submit(LinkHunt.OP_HUNT_STOP, LinkHunt.huntStop(id))
            repo.log(LogEntry.Category.COMMAND, "HUNT_STOP #$id after $huntReadings readings")
        }
    }

    private fun onHuntResult(op: Int, result: Int) {
        if (op == LinkHunt.OP_HUNT_START) {
            repo.log(LogEntry.Category.COMMAND, "RESULT HUNT_START => " +
                if (result == LinkHunt.STARTED) "started" else "target not supported ($result)")
            if (result != LinkHunt.STARTED && huntWanted) HuntSession.setProblem("This Hunter cannot hunt a Vixen.")
        } else {
            repo.log(LogEntry.Category.COMMAND, "RESULT HUNT_STOP => stopped; Recon resumes on the Hunter")
        }
    }

    private fun onHuntReading(value: ByteArray) {
        if (pending?.kind == LinkHunt.OP_GET_HUNT) finishLink()
        huntPollOutstanding = false
        val r = LinkHunt.decodeReading(value) ?: run {
            repo.log(LogEntry.Category.ERROR, "bad HUNT_READING: ${LinkCodec.toHex(value)}"); return
        }
        huntReadings++
        HuntSession.onReading(r.readings, r.mean, r.latest, r.strongest, r.hunting, r.lost,
            android.os.SystemClock.elapsedRealtime())
        // One log line every 5 s or so, not one per poll.
        if (huntReadings % 23 == 1L) repo.log(LogEntry.Category.LINK,
            "HUNT reading: ${r.readings} sightings, mean ${r.mean ?: "-"} dBm, strongest ${r.strongest ?: "-"}, " +
                "heard ${r.msSinceHeard?.let { "$it ms ago" } ?: "never"}, flags 0x%02X".format(r.flags))
    }

    // GET_TEXT goes straight into the link FIFO (linkQueue). One request is outstanding at a
    // time, so it cannot interleave with the GET_CHANGED run that produced it; it is sent in
    // order once that run ends. The earlier design parked these in a separate deque that only
    // submitted when it sampled the link idle, every 150 ms. Under an event flood the link is
    // almost never idle at those instants, so MAC fetches starved (field test 2026-11-02:
    // 2 of 31 rows carried a source MAC). By the time a fetch did go out the Hunter had often
    // evicted the weakest (LOW) record, so it came back empty. Prompt FIFO fetching closes that.
    private fun getTextQueued(eventId: Long, field: Int) = getText(eventId, field)
}
