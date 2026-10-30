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
import com.projectrevivesolutions.layertime.controller.model.ConnState
import com.projectrevivesolutions.layertime.controller.model.LogEntry
import com.projectrevivesolutions.layertime.controller.state.ControllerRepository
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
        const val RECONNECT_BASE_MS = 1_500L
        const val RECONNECT_MAX_MS = 15_000L

        /** Shown when BLE scanning is blocked by the system Location switch (Android 11 and older). */
        const val LOCATION_OFF_MESSAGE = "Turn on Location in Settings; this Android version needs it to scan for the wand."

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
        wantConnected = true
        reconnectAttempts = 0
        autoStartTarget = autoStart
        val a = adapter
        if (a == null || !a.isEnabled) {
            repo.setConn(ConnState.FAILED, "Bluetooth is off"); return@post
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
            if (gatt == null) { stopScan(); repo.setConn(ConnState.FAILED, "no LayerWand found in 20s") }
        }.also { handler.postDelayed(it, 20_000) }
    }

    fun stop() = handler.post {
        stopHealthCheck()
        repo.setReconnecting(false)
        wantConnected = false
        reconnectPending?.let { handler.removeCallbacks(it) }; reconnectPending = null
        requestTimeout?.let { handler.removeCallbacks(it) }; requestTimeout = null
        stopScan()
        gatt?.let { g -> g.disconnect(); g.close() }
        gatt = null; control = null; statusCh = null; dataCh = null
        handshakeDone = false; linkBusy = false; gattBusy = false
        linkQueue.clear(); gattOps.clear(); summaries.clear(); textFrags.clear()
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
        handshakeDone = false; linkBusy = false; gattBusy = false
        linkQueue.clear(); gattOps.clear()
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
            handshakeDone = false; linkBusy = false; gattBusy = false
            linkQueue.clear(); gattOps.clear()
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
            repo.setDeviceLabel((name ?: "LayerWand") + " / " + device.address)
            repo.setConn(ConnState.CONNECTING, "${name ?: "?"} ${device.address} rssi=${result.rssi}")
            connect(device)
        }

        override fun onScanFailed(errorCode: Int) {
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
                        handshakeDone = false; linkBusy = false; gattBusy = false
                        linkQueue.clear(); gattOps.clear()
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
        if (kind == LinkConstants.Op.GET_CHANGED) summaries.clear()
        if (kind == LinkConstants.Op.GET_TEXT) textFrags.clear()
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

    fun clearEvents() {
        val id = nextReqId(); submit(LinkConstants.Op.COMMAND, LinkCodec.reconClearEvents(id))
        repo.log(LogEntry.Category.COMMAND, "ReconClearEvents #$id")
    }

    private fun getChanged() {
        val id = nextReqId(); submit(LinkConstants.Op.GET_CHANGED, LinkCodec.getChanged(id, lastSeenChangeSeq))
    }

    private fun getText(eventId: Long, field: Int) {
        val id = nextReqId()
        textEventId = eventId; textField = field
        submit(LinkConstants.Op.GET_TEXT, LinkCodec.getText(id, eventId, field))
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
                    autoStartTarget?.let { reconStart(it) }
                    getChanged() // pull whatever the Node already holds
                }
                finishLink()
            }
            is Frame.Ack -> { repo.log(LogEntry.Category.COMMAND, "ACK token=0x%08X hb=${f.heartbeat}".format(f.token)); finishLink() }
            is Frame.Result -> {
                repo.log(LogEntry.Category.COMMAND,
                    "RESULT cmd=${f.commandType} => ${LinkConstants.CommandResult.name(f.commandResult)}")
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
            is Frame.End -> {
                if (pending?.kind == LinkConstants.Op.GET_TEXT) {
                    val ordered = textFrags.sortedBy { it.index }
                    val bytes = ordered.fold(ByteArray(0)) { acc, t -> acc + t.bytes }
                    if (bytes.isNotEmpty()) repo.onEventText(textEventId, textField, String(bytes, Charsets.UTF_8))
                } else { // GET_CHANGED run
                    lastSeenChangeSeq = f.changeSeq
                    if (f.gap == 1) repo.log(LogEntry.Category.LINK, "GET_CHANGED gap=1: a change was dropped before we saw it")
                }
                finishLink()
            }
            is Frame.Error -> {
                repo.log(LogEntry.Category.ERROR, "ERROR linkStatus=${LinkConstants.LinkStatus.name(f.linkStatus)} reqId=${f.reqId}")
                finishLink()
            }
            is Frame.Unknown -> repo.log(LogEntry.Category.ERROR, "unknown frame type=0x%02X: ${LinkCodec.toHex(f.raw)}".format(f.type))
        }
    }

    // Deferred GET_TEXT so it does not reenter while a GET_CHANGED run is open.
    private val textWants = ArrayDeque<Pair<Long, Int>>()
    private fun getTextQueued(eventId: Long, field: Int) {
        textWants.add(eventId to field)
        // Drain after the current run finishes (pumpLink picks them up).
        handler.post { drainTextWants() }
    }
    private fun drainTextWants() {
        if (linkBusy) { handler.postDelayed({ drainTextWants() }, 150); return }
        val next = textWants.poll() ?: return
        getText(next.first, next.second)
    }
}
