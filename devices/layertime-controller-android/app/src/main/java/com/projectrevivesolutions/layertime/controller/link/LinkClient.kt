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
import com.projectrevivesolutions.layertime.controller.mission.MissionPrefs
import java.util.ArrayDeque
import java.util.UUID

/**
 * Android LayerTime Link 0.1 central (the "controller"). Discovers a Hunter
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
    private val gattQueue = GattOperationQueue()

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

    // HUNTER ACTIONS (2026-11-06): the Hunter chosen on its row, by Bluetooth address.
    // While set, the scan connects only to that Hunter, and so do reconnects, so a
    // dropped link comes back to the same Hunter and never to another one in range.
    // Null: the first Hunter heard (the old Scan & connect).
    private var targetAddress: String? = null
    private var targetName: String? = null
    // The Hunter this client is connecting or connected to.
    @Volatile var currentAddress: String? = null
        private set
    @Volatile var currentName: String? = null
        private set
    /** The connected Hunter's address once the handshake is done, else null. */
    val connectedAddress: String? get() = if (handshakeDone) currentAddress else null
    private var cardClearSupported = false

    // Reconnection: we keep trying to restore the link until stop() is called.
    private var wantConnected = false
    private var reconnectAttempts = 0
    private var reconnectPending: Runnable? = null

    // Per-request timeout: the Node answers every request; silence means a lost
    // link or a stuck request, so we free the gate rather than hang forever.
    private var requestTimeout: Runnable? = null

    companion object {
        /** A Clear Card waiting for its Hunter to connect (HUNTER ACTIONS, 2026-11-06). */
        @Volatile var pendingClearAddress: String? = null
        const val SCAN_TIMEOUT_MS = 20_000L
        /** How long one step of bringing the link up may take (connect, discovery, each notification setup, HELLO). */
        const val STEP_TIMEOUT_MS = 4_000L
        const val CONNECT_TIMEOUT_MS = 10_000L
        // Service discovery on a first, cold connection can be slow or hang (the 2026-11-06 capture:
        // it answered only on a later connection). So after a short settle we give each discovery try
        // its own window and, on a miss, clear the service cache and try again on the SAME connection,
        // rather than tearing the link down and rescanning (which also resets the Hunter's advertising).
        const val DISCOVERY_SETTLE_MS = 600L
        const val DISCOVERY_TRY_MS = 5_000L
        const val DISCOVERY_RETRY_GAP_MS = 400L
        const val MAX_DISCOVERY_TRIES = 4
        // A first, cold GATT connection sometimes never reports CONNECTED to the app even though the
        // radio link is up (the 2026-11-06 capture: LE connected in 62 ms, no GATT callback after).
        // The old scan-time probe used to make these throwaway connections and warm the stack. With it
        // gone, the connect makes its own: if CONNECTED does not arrive in time, close and reopen, a few times.
        const val CONNECT_AFTER_SCAN_MS = 400L       // let the LE scan fully stop before connecting (avoids no-callback)
        const val CONNECT_TRY_MS = 6_000L            // a connect that is going to bind does so well under this
        const val CONNECT_RETRY_GAP_MS = 800L        // let the Hunter resume advertising before the next attempt
        const val MAX_CONNECT_TRIES = 4              // priming: the first cold connect often will not bind
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

    /** Begin scanning for a Hunter. [autoStart] optionally issues ReconStart after HELLO. */
    fun start(autoStart: Int? = null, address: String? = null, name: String? = null) = handler.post {
        if (address != null) {
            // Already on this Hunter: nothing to do.
            if (address == currentAddress && (handshakeDone || gatt != null)) return@post
            // Another Hunter is held: let it go first (one at a time, 2026-11-06).
            if (gatt != null) {
                gatt?.let { g -> runCatching { g.disconnect() }; runCatching { g.close() } }
                gatt = null; control = null; statusCh = null; dataCh = null
                stopHealthCheck()
                handshakeDone = false; linkBusy = false; gattQueue.reset(); dropEvidence(); dropCard(); dropClear()
                linkQueue.clear(); textTargets.clear(); summaries.clear(); textFrags.clear()
                requestTimeout?.let { handler.removeCallbacks(it) }; requestTimeout = null
                repo.reset()
            }
            targetAddress = address; targetName = name
        }
        // One tap is one clean attempt (build 69). Before this, a tap during a reconnect wait
        // left the scheduled retry in place; when it fired it closed the connection the tap had
        // just started, so the first tap looked dead and a second was needed. A tap could also
        // start a second scan on top of a running one, which Android rejects.
        reconnectPending?.let { handler.removeCallbacks(it) }; reconnectPending = null
        stopScan()
        if (!handshakeDone) {
            gatt?.let { g -> runCatching { g.disconnect() }; runCatching { g.close() } }
            gatt = null; control = null; statusCh = null; dataCh = null
            linkBusy = false; gattQueue.reset()
            linkQueue.clear(); textTargets.clear()
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
            if (gatt == null) { stopScan(); repo.setConn(ConnState.FAILED, targetName?.let { "$it was not found in 20 s" } ?: "no Hunter found in 20s") }
        }.also { handler.postDelayed(it, 20_000) }
    }

    fun stop() = handler.post {
        stepTimeout?.let { handler.removeCallbacks(it) }; stepTimeout = null; stepName = null
        stopHealthCheck()
        repo.setReconnecting(false)
        wantConnected = false
        reconnectPending?.let { handler.removeCallbacks(it) }; reconnectPending = null
        requestTimeout?.let { handler.removeCallbacks(it) }; requestTimeout = null
        huntWanted = false; huntPollOutstanding = false; handler.removeCallbacks(huntPoll)
        stopScan()
        gatt?.let { g -> g.disconnect(); g.close() }
        gatt = null; control = null; statusCh = null; dataCh = null
        handshakeDone = false; linkBusy = false; gattQueue.reset(); dropEvidence(); dropCard(); dropClear()
        linkQueue.clear(); textTargets.clear(); summaries.clear(); textFrags.clear()
        currentAddress = null; currentName = null; targetAddress = null; targetName = null
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

    // ---- connection step deadlines (2026-11-06) ----
    // The 2026-11-06 capture showed Android Bluetooth requests that never complete: service
    // discovery hung until the connection closed, and one notification setup never finished, so
    // the HELLO queued behind it was never sent while the Status kept arriving and the health
    // check saw nothing wrong. Every step of bringing the link up now has a deadline.
    private var stepTimeout: Runnable? = null
    private var stepName: String? = null
    private var discoveryTries = 0
    private var connectTries = 0
    private var connectAddress: String? = null

    private fun armStep(name: String, ms: Long = STEP_TIMEOUT_MS) {
        stepTimeout?.let { handler.removeCallbacks(it) }
        stepName = name
        repo.log(LogEntry.Category.LINK, "step: $name")
        stepTimeout = Runnable { stepStalled(name, ms) }.also { handler.postDelayed(it, ms) }
    }

    private fun clearStep(done: String) {
        stepTimeout?.let { handler.removeCallbacks(it) }; stepTimeout = null
        stepName?.let { repo.log(LogEntry.Category.LINK, "step done: $it ($done)") }
        stepName = null
    }

    /** A step missed its deadline (or Android refused it): drop the connection and reconnect, naming the step. */
    private fun stepStalled(name: String, ms: Long, detail: String? = null) {
        stepTimeout?.let { handler.removeCallbacks(it) }; stepTimeout = null; stepName = null
        val why = detail ?: "no answer in ${ms / 1000} s"
        repo.log(LogEntry.Category.ERROR, "connection step stalled: $name ($why); dropping and reconnecting")
        stopHealthCheck()
        gatt?.let { g -> runCatching { g.disconnect() }; runCatching { g.close() } }
        gatt = null; control = null; statusCh = null; dataCh = null
        handshakeDone = false; linkBusy = false; gattQueue.reset(); dropEvidence(); dropCard(); dropClear()
        linkQueue.clear(); textTargets.clear()
        requestTimeout?.let { handler.removeCallbacks(it) }; requestTimeout = null
        repo.noteBackgroundDisconnect()
        repo.setConn(ConnState.DISCONNECTED, "$name stalled")
        if (wantConnected) { repo.setReconnecting(true); scheduleReconnect("$name stalled") }
    }

    /** No Status within the allowance: drop the connection ourselves and reconnect. */
    private fun linkLost(silenceMs: Long) {
        stepTimeout?.let { handler.removeCallbacks(it) }; stepTimeout = null; stepName = null
        healthCheck = null
        repo.setLinkStale(false)
        repo.log(LogEntry.Category.ERROR, "link lost: no Status for ${silenceMs / 1000} s; dropping and reconnecting")
        gatt?.let { g -> runCatching { g.disconnect() }; runCatching { g.close() } }
        gatt = null; control = null; statusCh = null; dataCh = null
        handshakeDone = false; linkBusy = false; gattQueue.reset(); dropEvidence(); dropCard(); dropClear()
        linkQueue.clear(); textTargets.clear()
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
            handshakeDone = false; linkBusy = false; gattQueue.reset(); dropEvidence(); dropCard(); dropClear()
            linkQueue.clear(); textTargets.clear()
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
            targetAddress?.let { if (device.address != it) return } // only the Hunter chosen on its row
            stopScan()
            val name = runCatching { device.name }.getOrNull() ?: result.scanRecord?.deviceName
            currentAddress = device.address; currentName = name ?: targetName ?: "Hunter"
            repo.setDeviceLabel((name ?: "Hunter") + " / " + device.address)
            repo.setConn(ConnState.CONNECTING, "${name ?: "?"} ${device.address} rssi=${result.rssi}")
            handler.postDelayed({ if (gatt == null) connect(device) }, CONNECT_AFTER_SCAN_MS)
        }

        override fun onScanFailed(errorCode: Int) {
            // A scan is already running for this callback: not a failure, it keeps scanning.
            if (errorCode == SCAN_FAILED_ALREADY_STARTED) { repo.log(LogEntry.Category.CONNECTION, "scan already running; continuing"); return }
            repo.setConn(ConnState.FAILED, "scan failed code=$errorCode")
        }
    }

    /**
     * Connect, priming the way the old card probe did (Michael, 2026-11-06). On this phone the FIRST GATT
     * connection to a freshly advertising Hunter often never binds (Android sits in W4_CONN_ST); the probe
     * used to absorb that with its throwaway connects. So we make a direct connect and, if it does not
     * report CONNECTED in time, FULLY close and try again, up to [MAX_CONNECT_TRIES]. Every attempt is a
     * direct connect (autoConnect = false) with a matching close(), so no background GATT client is ever
     * left registered. The autoConnect in builds 104-106 was what leaked those and wedged the stack; a
     * direct connect's client is released on close, so this primes without leaking.
     */
    private fun connect(device: BluetoothDevice) {
        connectAddress = device.address
        connectTries = 0
        attemptConnect()
    }

    private fun attemptConnect() {
        val address = connectAddress ?: return
        val dev = adapter?.getRemoteDevice(address)
        if (dev == null) { stepStalled("connect", CONNECT_TRY_MS, "Bluetooth adapter is gone"); return }
        connectTries++
        stepTimeout?.let { handler.removeCallbacks(it) }
        stepName = "connect"
        repo.log(LogEntry.Category.LINK, "step: connect (try $connectTries of $MAX_CONNECT_TRIES)")
        stepTimeout = Runnable { connectRetryOrStall("no GATT connection in ${CONNECT_TRY_MS / 1000} s") }
            .also { handler.postDelayed(it, CONNECT_TRY_MS) }
        gatt?.let { g -> runCatching { g.disconnect() }; runCatching { g.close() } }
        gatt = dev.connectGatt(appContext, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
        if (gatt == null) connectRetryOrStall("Android refused to start the connection")
    }

    private fun connectRetryOrStall(why: String) {
        stepTimeout?.let { handler.removeCallbacks(it) }; stepTimeout = null; stepName = null
        val address = connectAddress ?: return
        if (connectTries >= MAX_CONNECT_TRIES) { stepStalled("connect", CONNECT_TRY_MS, "$why, after $connectTries tries"); return }
        repo.log(LogEntry.Category.LINK, "connect: $why; fully closing and priming again")
        gatt?.let { g -> runCatching { g.disconnect() }; runCatching { g.close() } }
        gatt = null
        handler.postDelayed({ if (connectAddress == address) attemptConnect() }, CONNECT_RETRY_GAP_MS)
    }

    /** Asks for the service list, with its own deadline; a miss retries on the same connection. */
    private fun beginDiscovery(g: BluetoothGatt) {
        if (g !== gatt) return
        discoveryTries++
        stepTimeout?.let { handler.removeCallbacks(it) }
        stepName = "service discovery"
        repo.log(LogEntry.Category.LINK, "step: service discovery (try $discoveryTries of $MAX_DISCOVERY_TRIES)")
        stepTimeout = Runnable { discoveryRetryOrStall(g, "no answer in ${DISCOVERY_TRY_MS / 1000} s") }
            .also { handler.postDelayed(it, DISCOVERY_TRY_MS) }
        if (!g.discoverServices()) discoveryRetryOrStall(g, "Android refused discoverServices")
    }

    /** Discovery missed or refused: clear the service cache and try again, or give up after the last try. */
    private fun discoveryRetryOrStall(g: BluetoothGatt, why: String) {
        stepTimeout?.let { handler.removeCallbacks(it) }; stepTimeout = null; stepName = null
        if (g !== gatt) return
        if (discoveryTries >= MAX_DISCOVERY_TRIES) { stepStalled("service discovery", DISCOVERY_TRY_MS, "$why, after $discoveryTries tries"); return }
        repo.log(LogEntry.Category.ERROR, "service discovery: $why; clearing the service cache and trying again on the same connection")
        refreshGatt(g)
        handler.postDelayed({ if (g === gatt) beginDiscovery(g) }, DISCOVERY_RETRY_GAP_MS)
    }

    /** Clears Android's cached GATT service list for this connection (hidden BluetoothGatt.refresh()). */
    private fun refreshGatt(g: BluetoothGatt) {
        runCatching { g.javaClass.getMethod("refresh").invoke(g) }
    }

    // ---- GATT callback ----

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            handler.post {
                // A connection LayerScope already dropped (a stalled step) can still report late; ignore it.
                if (g !== gatt) { repo.log(LogEntry.Category.LINK, "ignored a late state change ($newState, status $status) from a dropped connection"); return@post }
                when (newState) {
                    BluetoothProfile.STATE_CONNECTED -> {
                        repo.setConn(ConnState.DISCOVERING, "gattStatus=$status")
                        clearStep("connected, status $status")
                        discoveryTries = 0
                        // A short settle before discovery: calling discoverServices() straight from this
                        // callback is the classic cause of onServicesDiscovered never firing on a cold
                        // connection. The health check starts after the handshake (bring-up has its own
                        // deadlines), so a slow discovery cannot trip a false link-lost.
                        handler.postDelayed({ if (g === gatt) beginDiscovery(g) }, DISCOVERY_SETTLE_MS)
                    }
                    BluetoothProfile.STATE_DISCONNECTED -> {
                        stepTimeout?.let { handler.removeCallbacks(it) }; stepTimeout = null; stepName = null
                        stopHealthCheck()
                        // A measurement in progress ends with the connection, and the MTU resets.
                        mtuRequested = false
                        if (pingBurstLeft > 0) { pingBurstLeft = 0; LinkMeasure.addLine("PING burst stopped: the link dropped.") }
                        bulkRun?.let { reportBulk(it, null, "the link dropped.") }
                        LinkMeasure.setMtu(23)
                        handshakeDone = false; linkBusy = false; gattQueue.reset(); dropEvidence(); dropCard(); dropClear()
                        huntPollOutstanding = false
                        if (huntWanted) HuntSession.setProblem("The link to the Hunter dropped. Reconnecting.")
                        linkQueue.clear(); textTargets.clear()
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
                if (g !== gatt) { repo.log(LogEntry.Category.LINK, "ignored a late service list from a dropped connection"); return@post }
                if (status != BluetoothGatt.GATT_SUCCESS) {
                    discoveryRetryOrStall(g, "Android reported status $status"); return@post
                }
                clearStep("${g.services.size} services")
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
                enqueueGatt {
                    armStep("handshake (HELLO)")
                    sendControl(nextReqId().let { id ->
                        pending = Pending(id, LinkConstants.Op.HELLO)
                        LinkCodec.hello(id)
                    })
                }
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
            handler.post {
                if (g !== gatt) return@post
                val which = shortUuid(d.characteristic?.uuid)
                if (status != BluetoothGatt.GATT_SUCCESS) { stepStalled("notification setup $which", STEP_TIMEOUT_MS, "Android reported status $status"); return@post }
                clearStep("status $status")
                gattOpComplete()
            }
        }

        // Only the LINK MEASURE panel asks for a larger MTU (measureRequestMtu).
        override fun onMtuChanged(g: BluetoothGatt, mtu: Int, status: Int) {
            handler.post {
                LinkMeasure.setMtu(mtu)
                LinkMeasure.addLine(if (status == BluetoothGatt.GATT_SUCCESS) "MTU is now $mtu (asked for ${LinkMeasure.MTU_REQUEST})."
                    else "MTU request failed, status $status. MTU stays $mtu.")
                repo.log(LogEntry.Category.LINK, "MTU changed to $mtu status=$status")
                // Only complete the GATT op we started; a change the Hunter
                // started must not release another op still in flight.
                if (mtuRequested) { mtuRequested = false; gattOpComplete() }
            }
        }

        override fun onCharacteristicWrite(g: BluetoothGatt, ch: BluetoothGattCharacteristic, status: Int) {
            handler.post {
                if (g !== gatt) return@post
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

    private fun enqueueGatt(op: () -> Unit) = gattQueue.enqueue(op)
    private fun gattOpComplete() = gattQueue.complete()

    private fun enableNotify(ch: BluetoothGattCharacteristic) = enqueueGatt {
        val g = gatt ?: return@enqueueGatt gattOpComplete()
        val which = shortUuid(ch.uuid)
        armStep("notification setup $which")
        g.setCharacteristicNotification(ch, true)
        val cccd = ch.getDescriptor(UUID.fromString(LinkConstants.CCCD_UUID))
        if (cccd == null) { repo.log(LogEntry.Category.ERROR, "no CCCD on ${ch.uuid}"); clearStep("no CCCD"); return@enqueueGatt gattOpComplete() }
        // Android's answer to the request itself: anything but success means no callback will come.
        val accepted = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            val code = g.writeDescriptor(cccd, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)
            repo.log(LogEntry.Category.LINK, "writeDescriptor $which returned $code")
            code == android.bluetooth.BluetoothStatusCodes.SUCCESS
        } else {
            @Suppress("DEPRECATION") cccd.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
            @Suppress("DEPRECATION") val ok = g.writeDescriptor(cccd)
            repo.log(LogEntry.Category.LINK, "writeDescriptor $which returned $ok")
            ok
        }
        if (!accepted) stepStalled("notification setup $which", STEP_TIMEOUT_MS, "Android refused the request")
    }

    private fun shortUuid(u: UUID?): String = u?.toString()?.substring(0, 8) ?: "?"

    private fun sendControl(frame: ByteArray) {
        val g = gatt; val c = control
        if (g == null || c == null) {
            stepStalled("Control write", REQUEST_TIMEOUT_MS, "Control characteristic unavailable")
            return
        }
        val result = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            g.writeCharacteristic(c, frame, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)
        } else {
            @Suppress("DEPRECATION") c.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
            @Suppress("DEPRECATION") c.value = frame
            @Suppress("DEPRECATION") val accepted = g.writeCharacteristic(c)
            if (accepted) 0 else -1
        }
        if (result != 0) {
            // A local refusal has no completion callback. Reset the session through
            // the existing recovery path; never replay a possibly destructive command.
            stepStalled("Control write", REQUEST_TIMEOUT_MS,
                "request #${pending?.reqId} op=${pending?.kind} rejected locally, status=$result")
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
        armRequestTimeout(reqId, kind)
        enqueueGatt { sendControl(frame) }
    }

    private fun armRequestTimeout(reqId: Int, kind: Int) {
        requestTimeout?.let { handler.removeCallbacks(it) }
        requestTimeout = Runnable {
            if (linkBusy && pending?.reqId == reqId) {
                repo.log(LogEntry.Category.ERROR,
                    "request #$reqId op=0x%02X timed out after ${REQUEST_TIMEOUT_MS}ms; freeing link gate".format(kind))
                if (gattQueue.busy) {
                    stepStalled("Control write callback", REQUEST_TIMEOUT_MS, "request #$reqId timed out while GATT was busy")
                    return@Runnable
                }
                finishLink()
                if (kind == LinkConstants.Op.PING && pingBurstLeft > 0) onBurstPing(null)
                if (kind == LinkMeasure.OP_BULK) bulkRun?.let { reportBulk(it, null, "timed out") }
                if (kind == LinkHunt.OP_GET_HUNT) huntPollOutstanding = false
                if (LinkEvidence.isEvidenceOp(kind) && evidence != null) failEvidence("The Hunter stopped sending frames (no reply for ${REQUEST_TIMEOUT_MS / 1000} s).")
                if (LinkCard.isCardOp(kind) && card != null) onCardStall()
                if (kind == LinkCard.OP_CARD_CLEAR && clearing) { clearing = false; CardClear.set(CardClear.State.Failed(CardClear.TIMED_OUT)) }
                if (kind == LinkCard.OP_CARD_LIST && clearCheck != null) { clearCheck = null; CardClear.set(CardClear.State.Failed(CardClear.NO_LIST)) }
            }
        }.also { handler.postDelayed(it, if (kind == LinkCard.OP_CARD_CLEAR) CardClear.CLEAR_TIMEOUT_MS else REQUEST_TIMEOUT_MS) }
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

    // ---- SetEvidenceHold: MISSION's evidence hold (2026-11-04) ----

    private var evidenceHoldSupported = false
    private var sentEvidenceHold: Int? = null

    /** Sends the saved hold. Only a Hunter with Capability.EVIDENCE_HOLD takes it. */
    private fun sendEvidenceHold() {
        val sec = MissionPrefs.readEvidenceHold(appContext)
        sentEvidenceHold = sec
        val id = nextReqId(); submit(LinkConstants.Op.COMMAND, LinkCodec.setEvidenceHold(id, sec))
        repo.log(LogEntry.Category.COMMAND, "SetEvidenceHold ${sec}s #$id")
    }

    /** MISSION's hold changed; the new value is already saved. Sent now when the Hunter is up and takes it. */
    fun evidenceHoldChanged() = handler.post {
        repo.setEvidenceHoldApplied(null)
        if (handshakeDone && evidenceHoldSupported) sendEvidenceHold()
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
        lastNoSdLog = s.noSdLog
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
        if (LinkCard.isCardFrame(value)) { onCardFrame(value); return }
        when (val f = LinkCodec.decodeFrame(value)) {
            is Frame.HelloAck -> {
                clearStep("HELLO_ACK, server ${f.serverMajor}.${f.serverMinor}")
                repo.onServerInfo(f)
                if (f.linkStatus == LinkConstants.LinkStatus.VERSION_MISMATCH) {
                    repo.setConn(ConnState.FAILED,
                        "version mismatch: server ${f.serverMajor}.${f.serverMinor}, client ${LinkConstants.CLIENT_MAJOR}.${LinkConstants.CLIENT_MINOR}")
                } else {
                    handshakeDone = true
                    startHealthCheck()
                    repo.setReconnecting(false)
                    reconnectAttempts = 0 // link restored; reset backoff
                    repo.setConn(ConnState.CONNECTED, "link up, server ${f.serverMajor}.${f.serverMinor}")
                    // The MISSION sensor list goes first, so a start below never runs
                    // a sensor the operator switched off, not even for one frame.
                    sensorSetSupported = f.capabilities and LinkConstants.Capability.SENSOR_SET != 0
                    if (sensorSetSupported) sendSensors()
                    // The evidence hold goes before any start too, so the first deauth holds as set.
                    evidenceHoldSupported = f.capabilities and LinkConstants.Capability.EVIDENCE_HOLD != 0
                    if (evidenceHoldSupported) sendEvidenceHold()
                    // Evidence (slice 2): a Hunter that keeps frames streams them at the
                    // connection's packet size, so ask for the large one (Phase 0 M5:
                    // 56 KB/s at MTU 247 against 15 KB/s at the default 23).
                    rawCapture = f.capabilities and LinkEvidence.CAP_RAW_CAPTURE != 0
                    cardSupported = f.capabilities and LinkCard.CAP_CARD_READ != 0
                    lastNoSdLog = null // this Hunter's own Status says whether it has a card
                    cardClearSupported = f.capabilities and LinkCard.CAP_CARD_CLEAR != 0
                    if (pendingClearAddress != null && pendingClearAddress == currentAddress) {
                        pendingClearAddress = null
                        handler.post { sendCardClear() }
                    }
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
                } else if (f.commandType == LinkCard.OP_CARD_CLEAR) {
                    repo.log(LogEntry.Category.ERROR, "RESULT CARD_CLEAR => ${LinkConstants.CommandResult.name(f.commandResult)}; listing the card to see what is left")
                    // The trailing finishLink() below frees the gate; the list request is posted, so it goes after.
                    if (clearing) { clearing = false; verifyClear(0, complete = false) }
                } else if (f.commandType == LinkCard.OP_CARD_LIST && clearCheck != null) {
                    clearCheck = null
                    CardClear.set(CardClear.State.Failed(if (f.commandResult == LinkConstants.CommandResult.NOT_READY)
                        "${currentName ?: "The Hunter"} has no SD card open, so there is nothing to clear." else CardClear.NO_LIST))
                } else if (LinkCard.isCardOp(f.commandType) && card != null) {
                    failCard(when (f.commandResult) {
                        LinkConstants.CommandResult.NOT_READY -> "The Hunter has no SD card log open, or it is already sending a file. Try again in a moment."
                        LinkConstants.CommandResult.INVALID_ARGUMENT -> "The Hunter no longer has that file."
                        else -> "The Hunter answered ${LinkConstants.CommandResult.name(f.commandResult)}."
                    })
                } else if (LinkEvidence.isEvidenceOp(f.commandType) && evidence != null) {
                    failEvidence(when (f.commandResult) {
                        LinkConstants.CommandResult.NOT_READY -> "The Hunter kept no frames for this detection. Its window was reused by newer detections, or the detection was not on Wi-Fi."
                        LinkConstants.CommandResult.INVALID_ARGUMENT -> "The Hunter no longer holds this detection."
                        else -> "The Hunter answered ${LinkConstants.CommandResult.name(f.commandResult)}."
                    })
                } else if (f.commandType == LinkConstants.CommandType.SET_EVIDENCE_HOLD) {
                    val ok = f.commandResult == LinkConstants.CommandResult.OK
                    repo.setEvidenceHoldApplied(if (ok) sentEvidenceHold else null)
                    repo.log(if (ok) LogEntry.Category.COMMAND else LogEntry.Category.ERROR,
                        "RESULT SetEvidenceHold ${sentEvidenceHold ?: 0}s => ${LinkConstants.CommandResult.name(f.commandResult)}")
                } else if (f.commandType == LinkConstants.CommandType.RECON_CLEAR_EVENTS) {
                    // Clear means the same on both screens (2026-11-06): only after the
                    // Hunter says it cleared, so a refused Clear leaves the list as it is.
                    val ok = f.commandResult == LinkConstants.CommandResult.OK
                    if (ok) repo.clearEventList()
                    repo.log(if (ok) LogEntry.Category.COMMAND else LogEntry.Category.ERROR,
                        "RESULT ReconClearEvents => ${LinkConstants.CommandResult.name(f.commandResult)}")
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
            is Frame.End -> if (pending?.kind == LinkCard.OP_CARD_CLEAR) {
                finishLink()
                clearing = false
                repo.log(LogEntry.Category.COMMAND, "CARD_CLEAR done: ${f.count} files erased; listing the card to check")
                verifyClear(f.count, complete = true)
            } else if (pending?.kind == LinkCard.OP_CARD_LIST && clearCheck != null) {
                finishLink()
                val listed = clearCheck!!.map { it.id.name }.sorted()
                clearCheck = null
                repo.log(LogEntry.Category.COMMAND, "CARD_LIST after clear: ${listed.size} files${if (listed.isNotEmpty()) " (" + listed.joinToString() + ")" else ""}")
                CardClear.set(CardClear.State.Done(currentName ?: "the Hunter", clearErased, listed, clearComplete))
            } else if (pending?.kind?.let { LinkEvidence.isEvidenceOp(it) } == true) {
                val kind = pending!!.kind
                finishLink()
                onEvidenceEnd(kind)
            } else if (pending?.kind?.let { LinkCard.isCardOp(it) } == true) {
                val kind = pending!!.kind
                finishLink()
                onCardEnd(kind, f.count)
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
                if (pending?.kind == LinkCard.OP_CARD_CLEAR && clearing) {
                    clearing = false
                    CardClear.set(CardClear.State.Failed(if (f.linkStatus == LinkConstants.LinkStatus.UNKNOWN_OP) CardClear.OLD_FIRMWARE
                        else "The Hunter refused Clear Card (${LinkConstants.LinkStatus.name(f.linkStatus)})."))
                }
                if (pending?.kind == LinkCard.OP_CARD_LIST && clearCheck != null) { clearCheck = null; CardClear.set(CardClear.State.Failed(CardClear.NO_LIST)) }
                if (pending?.kind?.let { LinkCard.isCardOp(it) } == true && card != null) failCard(
                    if (f.linkStatus == LinkConstants.LinkStatus.UNKNOWN_OP) "This Hunter's firmware cannot send its card. Flash firmware build 93 or later."
                    else "The Hunter refused the card request (${LinkConstants.LinkStatus.name(f.linkStatus)}).")
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

    // ---- Link 0.2 card ops: the Hunter's SD card files, in the field (2026-11-04) ----

    private var cardSupported = false
    /** Whether the connected Hunter serves its card (HELLO_ACK capability bit 9). */
    val servesCard: Boolean get() = handshakeDone && cardSupported
    /** The latest Status's no-SD-log flag (2026-11-05); null until a Status arrives. */
    @Volatile var lastNoSdLog: Boolean? = null
        private set

    private class CardRun(val dest: java.io.File) {
        val listed = ArrayList<LinkCard.Entry>()
        var queue: List<LinkCard.Entry> = emptyList()
        var index = 0
        var file: LinkCard.Assembler? = null
        var resumes = 0
        var upToDate = 0
        var pulled = 0
        val failed = ArrayList<String>()
        val startedMs = android.os.SystemClock.elapsedRealtime()
    }
    private var card: CardRun? = null

    /**
     * Pulls every card file that is new or has changed size into [dest] (kept as
     * hunter_NNNN.log and ev_NNNN/event_EEEEE.pcap), checking each against the
     * Hunter's CRC-32. CardPull reports progress. Read only on the Hunter.
     */
    fun pullCard(dest: java.io.File) = handler.post {
        if (card != null) return@post
        if (!handshakeDone) { CardPull.set(CardPull.State.Failed("The Hunter is not connected.")); return@post }
        if (!cardSupported) { CardPull.set(CardPull.State.Failed("This Hunter's firmware cannot send its card. Flash firmware build 93 or later.")); return@post }
        if (evidence != null) { CardPull.set(CardPull.State.Failed("A frame pull is running. Try again when it finishes.")); return@post }
        card = CardRun(dest.apply { mkdirs() })
        val renamed = com.projectrevivesolutions.layertime.controller.card.HunterCardLog.renameLegacyLogs(dest)
        if (renamed > 0) repo.log(LogEntry.Category.CONNECTION, "renamed $renamed pulled layerwand_NNNN.log files to hunter_NNNN.log")
        CardPull.set(CardPull.State.Listing())
        if (rawCapture && LinkMeasure.state.value.mtu < LinkMeasure.MTU_REQUEST) requestLargeMtu()
        val id = nextReqId(); submit(LinkCard.OP_CARD_LIST, LinkCard.cardList(id))
        repo.log(LogEntry.Category.COMMAND, "CARD_LIST #$id")
    }

    private fun onCardFrame(value: ByteArray) {
        clearCheck?.let { list ->
            if ((value[0].toInt() and 0xFF) == LinkCard.TYPE_CARD_ENTRY) LinkCard.decodeEntry(value)?.takeIf { it.id.valid }?.let { list += it }
            pending?.let { armRequestTimeout(it.reqId, it.kind) }
            return
        }
        val r = card ?: return
        when (value[0].toInt() and 0xFF) {
            LinkCard.TYPE_CARD_ENTRY -> LinkCard.decodeEntry(value)?.takeIf { it.id.valid }?.let { r.listed += it }
            LinkCard.TYPE_CARD_BYTES -> LinkCard.decodeBytes(value)?.let { (offset, bytes) -> r.file?.onBytes(offset, bytes) }
            LinkCard.TYPE_CARD_DONE -> LinkCard.decodeDone(value)?.let { r.file?.onDone(it) }
        }
        pending?.let { armRequestTimeout(it.reqId, it.kind) }
        r.file?.let { f -> CardPull.set(CardPull.State.Pulling(r.index + 1, r.queue.size, f.id.name, f.received, f.expectedSize)) }
    }

    private fun localFile(r: CardRun, e: LinkCard.Entry) = java.io.File(r.dest, e.id.name)

    private fun onCardEnd(kind: Int, count: Int) {
        val r = card ?: return
        if (kind == LinkCard.OP_CARD_LIST) {
            if (count != r.listed.size) repo.log(LogEntry.Category.LINK, "CARD_LIST: END says $count, received ${r.listed.size}")
            r.queue = r.listed.filter { e -> localFile(r, e).let { !it.exists() || it.length() != e.size } }
            r.upToDate = r.listed.size - r.queue.size
            return nextCardFile()
        }
        val f = r.file ?: return nextCardFile()
        val problem = f.problem()
        if (problem == null) {
            val target = java.io.File(r.dest, f.id.name)
            target.parentFile?.mkdirs()
            val part = java.io.File(target.path + ".part")
            part.writeBytes(f.bytes())
            if (!part.renameTo(target)) { target.delete(); part.renameTo(target) }
            r.pulled++
            r.index++
            return nextCardFile()
        }
        if (f.done == null && f.received < f.expectedSize && r.resumes < 2) {
            r.resumes++
            repo.log(LogEntry.Category.LINK, "CARD_READ ${f.id.name} incomplete at ${f.received} bytes; resuming")
            val id = nextReqId(); submit(LinkCard.OP_CARD_READ, LinkCard.cardRead(id, f.id, f.received))
            return
        }
        r.failed += problem
        r.index++
        nextCardFile()
    }

    private fun nextCardFile() {
        val r = card ?: return
        if (r.index >= r.queue.size) {
            card = null
            val ms = android.os.SystemClock.elapsedRealtime() - r.startedMs
            repo.log(LogEntry.Category.LINK, "card pull: ${r.pulled} pulled, ${r.upToDate} up to date, ${r.failed.size} failed in $ms ms")
            CardPull.set(CardPull.State.Done(r.pulled, r.upToDate, r.failed.toList(), ms))
            return
        }
        val e = r.queue[r.index]
        r.file = LinkCard.Assembler(e.id, e.size)
        r.resumes = 0
        CardPull.set(CardPull.State.Pulling(r.index + 1, r.queue.size, e.id.name, 0, e.size))
        val id = nextReqId(); submit(LinkCard.OP_CARD_READ, LinkCard.cardRead(id, e.id, 0))
        repo.log(LogEntry.Category.COMMAND, "CARD_READ ${e.id.name} #$id")
    }

    /** No frame for REQUEST_TIMEOUT_MS: resume the file if it was moving, else fail it. */
    private fun onCardStall() {
        val r = card ?: return
        val f = r.file
        if (f != null && f.received < f.expectedSize && r.resumes < 2) {
            r.resumes++
            val id = nextReqId(); submit(LinkCard.OP_CARD_READ, LinkCard.cardRead(id, f.id, f.received))
            return
        }
        failCard("The Hunter stopped sending (no reply for ${REQUEST_TIMEOUT_MS / 1000} s).")
    }

    // ---- Clear Card (2026-11-06) ----

    private var clearing = false
    // After a clear: the card list being collected, and what the clear reported.
    private var clearCheck: ArrayList<LinkCard.Entry>? = null
    private var clearErased = 0
    private var clearComplete = true

    /** Lists the card again after a clear, so the result shown comes from the card itself. */
    private fun verifyClear(erased: Int, complete: Boolean) {
        clearErased = erased; clearComplete = complete
        clearCheck = ArrayList()
        CardClear.set(CardClear.State.Verifying(currentName ?: "the Hunter"))
        val id = nextReqId(); submit(LinkCard.OP_CARD_LIST, LinkCard.cardList(id))
    }

    /**
     * Clears the card of the Hunter at [address], after the operator confirmed. If this
     * client is connected to it, the request goes now; otherwise the caller connects to it
     * (LinkForegroundService.start with the address) and the request goes after HELLO.
     */
    fun clearCard(address: String) {
        handler.post {
            if (connectedAddress == address) sendCardClear()
            else pendingClearAddress = address
        }
    }

    private fun sendCardClear() {
        val name = currentName ?: "the Hunter"
        when {
            !handshakeDone -> { CardClear.set(CardClear.State.Failed("$name is not connected.")); return }
            !cardClearSupported -> { CardClear.set(CardClear.State.Failed(CardClear.OLD_FIRMWARE)); return }
            card != null -> { CardClear.set(CardClear.State.Failed(CardClear.PULL_RUNNING)); return }
        }
        clearing = true
        CardClear.set(CardClear.State.Clearing(name))
        val id = nextReqId(); submit(LinkCard.OP_CARD_CLEAR, LinkCard.cardClear(id))
        repo.log(LogEntry.Category.COMMAND, "CARD_CLEAR #$id to $name")
    }

    private fun dropClear() {
        if (clearing || clearCheck != null) { clearing = false; clearCheck = null; CardClear.set(CardClear.State.Failed(CardClear.DROPPED)) }
    }

    private fun dropCard() { if (card != null) failCard("The connection dropped during the card pull. Pull again to continue; finished files are kept.") }

    private fun failCard(message: String) {
        val r = card ?: return
        card = null
        repo.log(LogEntry.Category.ERROR, "card pull: $message")
        CardPull.set(CardPull.State.Failed(message + if (r.pulled > 0) " ${r.pulled} files were saved before it stopped." else ""))
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

    /** Ask the Hunter for a larger ATT MTU. Affects this connection until it drops. */
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

    /** BULK: the Hunter sends count frames of size bytes (capped at MTU minus 3), then BULK_DONE. */
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
