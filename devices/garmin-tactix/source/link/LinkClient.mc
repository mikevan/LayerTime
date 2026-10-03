// LayerTime - passive early-warning system. Connect IQ Device App for the
// Garmin tactix 8 AMOLED.
//
// Copyright (C) 2026 Michael Van Geertruy
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

import Toybox.BluetoothLowEnergy;
import Toybox.Lang;
import Toybox.System;
import Toybox.Timer;
import Toybox.WatchUi;

// The LayerTime Link client: discovers a Node by service UUID, pairs,
// registers the profile, enables notifications, says HELLO, watches the
// Status heartbeat, answers the wearer's PINGs with a round-trip time, and
// reconnects when the link drops. Detects a new Node session (a LayerWand
// power cycle) through sessionId.
//
// Recon integration (Increment 2B): keeps the ReconMirror in step with the
// Node through GET_CHANGED whenever Status's changeSeq moves, fetches event
// text with GET_TEXT, and sends the Recon Controls as COMMANDs. Replies to
// GET_CHANGED and GET_TEXT are runs of frames ending with END; each frame
// restarts the request's accounting timeout.
//
// Protocol bytes come from Link (LinkCodec.mc); ordering of GATT operations
// from RequestQueue. This class is the only place that touches
// Toybox.BluetoothLowEnergy.
class LinkClient extends BluetoothLowEnergy.BleDelegate {

    enum State {
        STATE_IDLE,
        STATE_REGISTERING,
        STATE_SCANNING,
        STATE_PAIRING,
        STATE_SETUP,
        STATE_READY,
        STATE_LOST
    }

    private const REQUEST_TIMEOUT_MS = 3000;   // accounting only (F2)
    private const SYNC_MIN_INTERVAL_MS = 1000; // at most one GET_CHANGED a second
    private const STALE_MS = 3000;             // Status older than this is stale on the face
    private const HEARTBEAT_LOST_MS = 5000;
    private const RESCAN_AFTER_LOST_MS = 5000;
    private const SCAN_WATCHDOG_MS = 15000;    // correction B

    private var _state as State = STATE_IDLE;
    private var _device as Device? = null;
    private var _control as Characteristic? = null;
    private var _status as Characteristic? = null;
    private var _data as Characteristic? = null;
    private var _probe as Characteristic? = null;
    private var _queue as RequestQueue = new RequestQueue();
    private var _timer as Timer.Timer = new Timer.Timer();
    private var _nextReqId as Number = 1;
    private var _profileOk as Boolean = false;
    // F2: the item most recently handed to the stack, and one the queue has
    // released but that waits for the stack to finish the previous request.
    private var _issued as Dictionary? = null;
    private var _deferred as Dictionary? = null;
    private var _scanStartedMs as Number = 0;

    // What the view shows.
    public var nodeName as String = "";
    public var sessionId as Number = 0;
    public var previousSessionId as Number = 0;
    public var newSession as Boolean = false;
    public var heartbeat as Number = -1;
    public var lastStatusMs as Number = 0;
    public var serverVersion as String = "";
    public var capabilities as Number = 0;
    public var pingsSent as Number = 0;
    public var acksReceived as Number = 0;
    public var lastRttMs as Number = -1;
    public var lastError as String = "";
    public var probeReceived as Number = -1;
    public var probeExpected as Number = -1;
    public var disconnects as Number = 0;
    private var _lostAtMs as Number = 0;

    // The rest of the Status snapshot (contracts/link.md), kept from the
    // latest decode for the home screen: Recon flags, selection, event
    // count, the alert event id, and the change sequence. The two u32
    // fields are narrowed to Number (Link.narrow) so == compares values.
    public var flags as Number = 0;
    public var selected as Number = 0;
    public var active as Number = 0;
    public var eventCount as Number = 0;
    public var lastAlertEventId as Number = 0;
    public var changeSeq as Number = 0;

    // The Recon event list, kept by GET_CHANGED.
    public var mirror as ReconMirror = new ReconMirror();
    // Called once per RESULT: the command type and its CommandResult, or
    // -1 when the command got no RESULT (timeout, write failure, ERROR).
    public var resultObserver as Method(commandType as Number, result as Number) as Void? = null;
    // Called when a GET_TEXT completes, so a detail page can redraw.
    public var textObserver as Method(eventId as Number) as Void? = null;
    // Called once each time a GET_CHANGED reply reports a gap: detections
    // were dropped on the Node before the watch fetched them.
    public var gapObserver as Method() as Void? = null;
    // Called once when Status first reports FLAG_NO_SD_LOG on a connection,
    // and again if the flag clears and comes back (a card removed).
    public var noSdObserver as Method() as Void? = null;
    private var _noSdNoticed as Boolean = false;
    // False when the connected Node runs no Recon: its HELLO_ACK reports
    // neither monitor capability, or it answered GET_CHANGED with ERROR
    // UnknownOp (the Increment 1 Link-only Node). No GET_CHANGED is sent then.
    // Reset on every new connection.
    public var reconAvailable as Boolean = true;
    public var commandsSent as Number = 0;
    public var syncs as Number = 0;
    public var strayFrames as Number = 0;
    private var _syncQueued as Boolean = false;
    private var _lastSyncMs as Number = 0;
    private var _textQueued as Boolean = false;
    // True while the client is looking for a Node (scanning, pairing or
    // setting up), as opposed to READY or lost.
    public function isSearching() as Boolean {
        return _state == STATE_SCANNING || _state == STATE_PAIRING || _state == STATE_SETUP || _state == STATE_REGISTERING;
    }
    // Milliseconds since the last Status decode, or -1 before the first.
    public function statusAgeMs() as Number {
        return lastStatusMs != 0 ? System.getTimer() - lastStatusMs : -1;
    }

    // What the face shows about the link: :ready (Status fresh), :stale
    // (connected, but no Status for STALE_MS), :reconnecting (lost a Node it
    // had, looking again), :searching (never connected this run), :failed
    // (the BLE profile could not be registered).
    public function phase() as Symbol {
        var forced = Env.forcedPhase(); // preview build only; null otherwise
        if (forced != null) { return forced; }
        if (_state == STATE_READY) {
            var age = statusAgeMs();
            return (age < 0 || age > STALE_MS) ? :stale : :ready;
        }
        if (_state == STATE_IDLE) { return :failed; }
        if (sessionId != 0 || disconnects > 0) { return :reconnecting; }
        return :searching;
    }

    // Called once per PING when it completes: the round trip in ms, or -1
    // when it was lost (timeout, token mismatch, or an ERROR reply). The
    // test-only burst (LinkBurst.mc) hooks here; production leaves it null.
    public var pingObserver as Method(rttMs as Number) as Void? = null;
    // Finding F2 (devices/garmin-tactix/README.md): true from the moment any GATT request
    // (CCCD write, Status read, Control write) is handed to the stack until
    // the stack's own completion callback for it, or a disconnect. While it
    // is true nothing else is issued: the 3 s timeout only accounts, and a
    // request released by the queue waits in _deferred. The test-only burst
    // reads it too before issuing the next PING.
    public var requestPending as Boolean = false;
    // Scan watchdog (correction B): how many times a scan that found nothing
    // for SCAN_WATCHDOG_MS was stopped and started again.
    public var scanRestarts as Number = 0;
    // Called with the Garmin-local receipt time (System.getTimer) of every
    // successfully decoded Status NOTIFICATION (not the setup-time read).
    // The test-only heartbeat measurement (HeartbeatWatch.mc) hooks here;
    // production leaves it null. Observers only record (finding F1).
    public var statusObserver as Method(receivedMs as Number) as Void? = null;

    public function initialize() {
        BleDelegate.initialize();
    }

    // --- Lifecycle ----------------------------------------------------------

    public function start() as Void {
        BluetoothLowEnergy.setDelegate(self);
        _timer.start(method(:onTick), 1000, true);
        _state = STATE_REGISTERING;
        // One profile: the LayerTime service with its four characteristics
        // (Probe is absent on release Nodes and skipped at connection time).
        // Connect IQ allows 3 registrations per app process and has no
        // unregister call, so a repeated start() (the unit-test harness
        // re-enters onStart) can be refused: that is reported on the view
        // through the same path onProfileRegister uses, not left to crash.
        try {
            BluetoothLowEnergy.registerProfile({
                :uuid => Link.serviceUuid(),
                :characteristics => [
                    {:uuid => Link.controlUuid()},
                    {:uuid => Link.statusUuid(), :descriptors => [BluetoothLowEnergy.cccdUuid()]},
                    {:uuid => Link.dataUuid(), :descriptors => [BluetoothLowEnergy.cccdUuid()]},
                    {:uuid => Link.probeUuid(), :descriptors => [BluetoothLowEnergy.cccdUuid()]}
                ]
            });
        } catch (e instanceof BluetoothLowEnergy.ProfileRegistrationException) {
            lastError = "Profile registration failed (" + e.getErrorMessage() + ")";
            _state = STATE_IDLE;
        }
        WatchUi.requestUpdate();
    }

    public function stop() as Void {
        _timer.stop();
        BluetoothLowEnergy.setScanState(BluetoothLowEnergy.SCAN_STATE_OFF);
        if (_device != null) {
            BluetoothLowEnergy.unpairDevice(_device);
            _device = null;
        }
        _queue.clear();
        _state = STATE_IDLE;
    }

    public function stateName() as String {
        if (_state == STATE_IDLE) { return "Idle"; }
        if (_state == STATE_REGISTERING) { return "Registering"; }
        if (_state == STATE_SCANNING) { return "Scanning"; }
        if (_state == STATE_PAIRING) { return "Pairing"; }
        if (_state == STATE_SETUP) { return "Connecting"; }
        if (_state == STATE_READY) { return "Connected"; }
        return "Link lost";
    }

    public function isReady() as Boolean {
        var forced = Env.forcedPhase(); // preview build only; null otherwise
        if (forced != null) { return forced == :ready || forced == :stale; }
        return _state == STATE_READY;
    }

    // --- Actions ------------------------------------------------------------

    // Sends PING with a fresh token. Returns false when not connected.
    public function ping() as Boolean {
        if (_state != STATE_READY || _control == null) { return false; }
        var reqId = takeReqId();
        var token = (System.getTimer() & 0x7FFFFFFF).toLong();
        pingsSent++;
        enqueue({:kind => :write, :op => Link.OP_PING, :reqId => reqId, :token => token,
                 :bytes => Link.encodePing(reqId, token)});
        return true;
    }

    // Sends one Recon Control. arg is null for the commands without one.
    // Returns false when not connected.
    public function sendCommand(commandType as Number, arg as Number?) as Boolean {
        if (_state != STATE_READY || _control == null) { return false; }
        var reqId = takeReqId();
        commandsSent++;
        enqueue({:kind => :write, :op => Link.OP_COMMAND, :reqId => reqId, :commandType => commandType,
                 :bytes => Link.encodeCommand(reqId, commandType, arg)});
        return true;
    }

    // Asks the Node for one text field of an event (GET_TEXT); the answer
    // lands in the mirror. Returns false when not connected, or when a text
    // request is already waiting.
    public function fetchText(eventId as Number, field as Number) as Boolean {
        if (_state != STATE_READY || _control == null || _textQueued) { return false; }
        _textQueued = true;
        var reqId = takeReqId();
        enqueue({:kind => :write, :op => Link.OP_GET_TEXT, :reqId => reqId, :eventId => eventId, :field => field,
                 :chunks => [] as Array<ByteArray>,
                 :bytes => Link.encodeGetText(reqId, Link.widen(eventId), field)});
        return true;
    }

    public function acknowledgeNewSession() as Void {
        newSession = false;
        WatchUi.requestUpdate();
    }

    // --- BleDelegate --------------------------------------------------------

    public function onProfileRegister(uuid as Uuid, status as Status) as Void {
        if (status == BluetoothLowEnergy.STATUS_SUCCESS) {
            _profileOk = true;
            startScan();
        } else {
            lastError = "Profile registration failed (" + status + ")";
            _state = STATE_IDLE;
        }
        WatchUi.requestUpdate();
    }

    public function onScanResults(scanResults as Iterator) as Void {
        if (_state != STATE_SCANNING) { return; }
        for (var r = scanResults.next(); r != null; r = scanResults.next()) {
            if (r instanceof ScanResult && advertisesLayerTime(r)) {
                var name = r.getDeviceName();
                nodeName = name != null ? name : "";
                BluetoothLowEnergy.setScanState(BluetoothLowEnergy.SCAN_STATE_OFF);
                _state = STATE_PAIRING;
                _device = BluetoothLowEnergy.pairDevice(r);
                if (_device == null) {
                    lastError = "Pairing failed";
                    startScan();
                }
                WatchUi.requestUpdate();
                return;
            }
        }
    }

    public function onConnectedStateChanged(device as Device, state as BluetoothLowEnergy.ConnectionState) as Void {
        if (device != _device) { return; }
        if (state == BluetoothLowEnergy.CONNECTION_STATE_CONNECTED) {
            setUpConnection(device);
        } else if (_state == STATE_SETUP || _state == STATE_READY) {
            disconnects++;
            _lostAtMs = System.getTimer();
            _state = STATE_LOST;
            abandonQueued();
            _queue.clear();
            requestPending = false;   // the stack has dropped everything with the link
            _issued = null;
            _deferred = null;
            _control = null;
            _status = null;
            _data = null;
            _probe = null;
        }
        WatchUi.requestUpdate();
    }

    public function onDescriptorWrite(descriptor as Descriptor, status as Status) as Void {
        if (!stackFinished()) { return; }
        if (status != BluetoothLowEnergy.STATUS_SUCCESS) {
            lastError = "CCCD write failed (" + status + ")";
        }
        completeAndSendNext();
    }

    public function onCharacteristicRead(characteristic as Characteristic, status as Status, value as ByteArray) as Void {
        if (status == BluetoothLowEnergy.STATUS_SUCCESS && characteristic.getUuid().equals(Link.statusUuid())) {
            applyStatus(value);
        }
        if (!stackFinished()) { return; }
        completeAndSendNext();
    }

    public function onCharacteristicWrite(characteristic as Characteristic, status as Status) as Void {
        if (!stackFinished()) { return; }
        if (status != BluetoothLowEnergy.STATUS_SUCCESS) {
            lastError = "Write failed (" + status + ")";
            // The reply will never come; free the queue now.
            var failed = _queue.inFlight();
            if (failed != null) { itemDone(failed, false); }
            completeAndSendNext();
        } else if (_queue.isExpired()) {
            // The write is done but its reply did not arrive within the
            // accounting timeout; the stack is free now, so move on (F2).
            var late = _queue.inFlight();
            if (late != null) { itemDone(late, false); }
            completeAndSendNext();
        }
        // Otherwise the write is complete when its reply arrives on Data.
    }

    public function onCharacteristicChanged(characteristic as Characteristic, value as ByteArray) as Void {
        var uuid = characteristic.getUuid();
        if (uuid.equals(Link.statusUuid())) {
            if (applyStatus(value) && statusObserver != null) { statusObserver.invoke(lastStatusMs); }
        } else if (uuid.equals(Link.dataUuid())) {
            applyReply(value);
        } else if (uuid.equals(Link.probeUuid())) {
            probeReceived = value.size();
            probeExpected = value.size() > 0 ? value[0] : -1;
        }
        WatchUi.requestUpdate();
    }

    // --- Timer ----------------------------------------------------------------

    public function onTick() as Void {
        var now = System.getTimer();
        var dropped = _queue.expire(now, REQUEST_TIMEOUT_MS);
        if (dropped != null) {
            // Accounting only (F2): the request is reported lost, but the
            // slot is freed by the stack's callback, not by this timer. If the
            // stack has already answered (a PING whose write succeeded but
            // whose ACK never came), the slot is free and the queue moves on.
            lastError = "Request timed out";
            if (dropped[:op] == Link.OP_PING) { notifyPing(-1); }
            if (!requestPending) {
                itemDone(dropped, false);
                completeAndSendNext();
            }
        }
        maybeSync(now);
        if (_state == STATE_READY && lastStatusMs != 0 && now - lastStatusMs > HEARTBEAT_LOST_MS) {
            lastError = "No heartbeat for " + ((now - lastStatusMs) / 1000) + " s";
        }
        if (_state == STATE_LOST && now - _lostAtMs > RESCAN_AFTER_LOST_MS) {
            // The system did not bring the paired device back; start over.
            if (_device != null) {
                BluetoothLowEnergy.unpairDevice(_device);
                _device = null;
            }
            startScan();
        }
        if (_state == STATE_SCANNING && now - _scanStartedMs > SCAN_WATCHDOG_MS) {
            // Correction B: a scan that has found nothing for 15 s is stopped
            // and started again, and counted, so a stuck scan recovers and
            // the recovery is visible in testing.
            BluetoothLowEnergy.setScanState(BluetoothLowEnergy.SCAN_STATE_OFF);
            BluetoothLowEnergy.setScanState(BluetoothLowEnergy.SCAN_STATE_SCANNING);
            _scanStartedMs = now;
            scanRestarts++;
            lastError = "Scan restarted (" + scanRestarts + ")";
        }
        WatchUi.requestUpdate();
    }

    // --- Internals ----------------------------------------------------------

    private function startScan() as Void {
        if (!_profileOk) { return; }
        _state = STATE_SCANNING;
        _scanStartedMs = System.getTimer();
        BluetoothLowEnergy.setScanState(BluetoothLowEnergy.SCAN_STATE_SCANNING);
    }

    private function advertisesLayerTime(r as ScanResult) as Boolean {
        var target = Link.serviceUuid();
        var uuids = r.getServiceUuids();
        for (var u = uuids.next(); u != null; u = uuids.next()) {
            if ((u as Uuid).equals(target)) { return true; }
        }
        return false;
    }

    private function setUpConnection(device as Device) as Void {
        _state = STATE_SETUP;
        reconAvailable = true;
        _noSdNoticed = false;
        abandonQueued();
        _queue.clear();
        requestPending = false;   // a new connection: the stack starts clean
        _issued = null;
        _deferred = null;
        var service = device.getService(Link.serviceUuid());
        if (service == null) {
            lastError = "LayerTime service missing";
            return;
        }
        _control = service.getCharacteristic(Link.controlUuid());
        _status = service.getCharacteristic(Link.statusUuid());
        _data = service.getCharacteristic(Link.dataUuid());
        _probe = service.getCharacteristic(Link.probeUuid()); // null on release Nodes
        if (_control == null || _status == null || _data == null) {
            lastError = "Characteristic missing";
            return;
        }
        // Order matters and each step waits for the previous to complete:
        // notifications on, then the current Status, then HELLO.
        enqueueCccd(_status as Characteristic);
        enqueueCccd(_data as Characteristic);
        if (_probe != null) { enqueueCccd(_probe as Characteristic); }
        enqueue({:kind => :read});
        var reqId = takeReqId();
        enqueue({:kind => :write, :op => Link.OP_HELLO, :reqId => reqId,
                 :bytes => Link.encodeHello(reqId, Link.MAJOR, Link.MINOR)});
    }

    private function enqueueCccd(c as Characteristic) as Void {
        var cccd = c.getDescriptor(BluetoothLowEnergy.cccdUuid());
        if (cccd != null) {
            enqueue({:kind => :cccd, :descriptor => cccd});
        }
    }

    private function enqueue(item as Dictionary) as Void {
        sendNext(_queue.submit(item, System.getTimer()));
    }

    private function completeAndSendNext() as Void {
        sendNext(_queue.complete(System.getTimer()));
    }

    // The queue released this item. It is handed to the stack only when the
    // stack has finished the previous request (F2); until then it waits in
    // _deferred and stackFinished() issues it.
    private function sendNext(item as Dictionary?) as Void {
        if (item == null) { return; }
        if (requestPending) { _deferred = item; return; }
        issue(item);
    }

    private function issue(item as Dictionary) as Void {
        var kind = item[:kind];
        _issued = item;
        requestPending = true;
        if (kind == :cccd) {
            (item[:descriptor] as Descriptor).requestWrite([0x01, 0x00]b);
        } else if (kind == :read) {
            if (_status != null) { _status.requestRead(); } else { stackIdle(); completeAndSendNext(); }
        } else if (kind == :write) {
            if (_control != null) {
                item[:sentAt] = System.getTimer();
                _control.requestWrite(item[:bytes] as ByteArray,
                    {:writeType => BluetoothLowEnergy.WRITE_TYPE_WITH_RESPONSE});
            } else {
                stackIdle();
                completeAndSendNext();
            }
        } else {
            stackIdle();
            completeAndSendNext();
        }
    }

    private function stackIdle() as Void {
        requestPending = false;
        _issued = null;
    }

    // Every completion callback starts here. Marks the stack free, issues a
    // request the queue had already released (a reply arrived before the
    // previous write's own callback, finding F1), and tells the caller
    // whether the callback belongs to the item still in flight, so a late
    // callback for an already-completed request is not mistaken for the
    // current one.
    private function stackFinished() as Boolean {
        var mine = _issued != null && _queue.inFlight() == _issued;
        stackIdle();
        var d = _deferred;
        if (d != null) {
            _deferred = null;
            issue(d);
            return false;
        }
        return mine;
    }

    private function takeReqId() as Number {
        var id = _nextReqId;
        _nextReqId = (_nextReqId % 255) + 1;
        return id;
    }

    // Returns true when the frame decoded.
    private function applyStatus(value as ByteArray) as Boolean {
        var s = Link.decodeStatus(value);
        if (s == null) {
            lastError = "Bad Status length " + value.size();
            return false;
        }
        heartbeat = s[:heartbeat] as Number;
        flags = s[:flags] as Number;
        if ((flags & Link.FLAG_NO_SD_LOG) == 0) {
            _noSdNoticed = false;
        } else if (!_noSdNoticed) {
            _noSdNoticed = true;
            if (noSdObserver != null) { noSdObserver.invoke(); }
        }
        selected = s[:selected] as Number;
        active = s[:active] as Number;
        eventCount = s[:eventCount] as Number;
        lastAlertEventId = Link.narrow(s[:lastAlertEventId] as Long);
        changeSeq = Link.narrow(s[:changeSeq] as Long);
        lastStatusMs = System.getTimer();
        noteSession(s[:sessionId] as Number);
        mirror.prune(eventCount, changeSeq);
        maybeSync(lastStatusMs);
        return true;
    }

    // Asks for what changed when Status's changeSeq has moved past the
    // mirror, at most once a second and never with one already queued.
    private function maybeSync(nowMs as Number) as Void {
        if (_state != STATE_READY || _control == null || _syncQueued || !reconAvailable) { return; }
        if (mirror.synced && mirror.syncedSeq == changeSeq) { return; }
        if (_lastSyncMs != 0 && nowMs - _lastSyncMs < SYNC_MIN_INTERVAL_MS) { return; }
        _syncQueued = true;
        _lastSyncMs = nowMs;
        syncs++;
        var reqId = takeReqId();
        enqueue({:kind => :write, :op => Link.OP_GET_CHANGED, :reqId => reqId,
                 :bytes => Link.encodeGetChanged(reqId, Link.widen(mirror.since()))});
    }

    // A request finished, answered (ok) or not. Clears its bookkeeping so the
    // next one can be sent, and tells the command observer about a command
    // that got no RESULT.
    private function itemDone(item as Dictionary, ok as Boolean) as Void {
        var op = item[:op];
        if (op == Link.OP_GET_CHANGED) { _syncQueued = false; }
        else if (op == Link.OP_GET_TEXT) { _textQueued = false; }
        else if (op == Link.OP_COMMAND && !ok && resultObserver != null) {
            resultObserver.invoke(item[:commandType] as Number, -1);
        }
    }

    // Everything queued or in flight is dropped (disconnect or new
    // connection): release the flags that keep requests unique.
    private function abandonQueued() as Void {
        _syncQueued = false;
        _textQueued = false;
    }

    private function applyReply(value as ByteArray) as Void {
        var reply = Link.decodeReply(value);
        var inFlight = _queue.inFlight();
        if (reply == null) {
            lastError = "Unknown frame " + Link.toHex(value);
            return;
        }
        if (inFlight == null || inFlight[:reqId] != reply[:reqId]) {
            // A frame of a reply that already timed out, or a stray: counted,
            // not treated as an error of the current request.
            strayFrames++;
            return;
        }
        var type = reply[:type] as Number;
        var status = reply[:linkStatus] as Number;
        var now = System.getTimer();

        // The frames of a multi-frame reply before its END: keep them and
        // keep waiting.
        if (type == Link.FRAME_EVENT_SUMMARY) {
            mirror.apply(reply, now);
            _queue.touch(now);
            return;
        }
        if (type == Link.FRAME_TEXT) {
            var chunks = inFlight[:chunks] as Array<ByteArray>?;
            if (chunks != null) { chunks.add(reply[:bytes] as ByteArray); }
            _queue.touch(now);
            return;
        }

        var pingResult = inFlight[:op] == Link.OP_PING ? -1 : null;
        var resend = null;
        var commandResult = null;
        var gapSeen = false;
        if (type == Link.FRAME_HELLO_ACK) {
            serverVersion = (reply[:serverMajor] as Number).toString() + "." + (reply[:serverMinor] as Number).toString();
            capabilities = reply[:capabilities] as Number;
            reconAvailable = (capabilities & (Link.CAP_LOCAL_WIFI_MONITOR | Link.CAP_LOCAL_BLE_MONITOR)) != 0;
            noteSession(reply[:sessionId] as Number);
            if (status == Link.STATUS_OK) {
                _state = STATE_READY;
                lastError = "";
            } else {
                lastError = "HELLO: " + Link.statusName(status);
            }
        } else if (type == Link.FRAME_ACK) {
            // Long compares by value only through equals(); == may compare references.
            if ((reply[:token] as Long).equals(inFlight[:token] as Long)) {
                acksReceived++;
                lastRttMs = now - (inFlight[:sentAt] as Number);
                pingResult = lastRttMs;
            } else {
                lastError = "ACK token mismatch";
            }
        } else if (type == Link.FRAME_RESULT) {
            commandResult = reply[:commandResult] as Number;
            if (commandResult != Link.RESULT_OK) {
                lastError = "Command " + (reply[:commandType] as Number) + ": " + Link.commandResultName(commandResult);
            } else if ((reply[:commandType] as Number) == Link.CMD_RECON_CLEAR_EVENTS) {
                mirror.clearGaps();
            }
        } else if (type == Link.FRAME_END) {
            if (inFlight[:op] == Link.OP_GET_CHANGED) {
                mirror.complete(reply);
                if ((reply[:gap] as Number) != 0) { gapSeen = true; }
            } else if (inFlight[:op] == Link.OP_GET_TEXT) {
                var chunks = inFlight[:chunks] as Array<ByteArray>;
                var all = []b;
                for (var i = 0; i < chunks.size(); i++) { all.addAll(chunks[i]); }
                var id = inFlight[:eventId] as Number;
                mirror.setText(id, inFlight[:field] as Number, ReconMirror.printable(all));
                if (textObserver != null) { textObserver.invoke(id); }
            }
        } else if (type == Link.FRAME_ERROR) {
            lastError = "Node: " + Link.statusName(status);
            // A HELLO that met a reply still draining on the Node is said
            // again; the connection is not ready without it.
            if (inFlight[:op] == Link.OP_HELLO && status == Link.STATUS_BUSY) { resend = inFlight; }
            // A Node without Recon: stop asking.
            if (inFlight[:op] == Link.OP_GET_CHANGED && status == Link.STATUS_UNKNOWN_OP) { reconAvailable = false; }
        }
        // Complete first, then report: an observer that sends the next PING
        // must find the queue free, so the next request is issued only after
        // this one has fully finished (one outstanding operation).
        var answered = commandResult != null || type == Link.FRAME_END || type == Link.FRAME_HELLO_ACK || type == Link.FRAME_ACK;
        itemDone(inFlight, answered);
        completeAndSendNext();
        if (resend != null) {
            var reqId = takeReqId();
            enqueue({:kind => :write, :op => Link.OP_HELLO, :reqId => reqId,
                     :bytes => Link.encodeHello(reqId, Link.MAJOR, Link.MINOR)});
        }
        if (commandResult != null && resultObserver != null) {
            resultObserver.invoke(reply[:commandType] as Number, commandResult);
        }
        if (pingResult != null) { notifyPing(pingResult); }
        if (gapSeen && gapObserver != null) { gapObserver.invoke(); }
        if (type == Link.FRAME_END || type == Link.FRAME_HELLO_ACK) { maybeSync(now); }
    }

    private function notifyPing(rttMs as Number) as Void {
        if (pingObserver != null) { pingObserver.invoke(rttMs); }
    }

    private function noteSession(id as Number) as Void {
        if (id == 0 || id == sessionId) { return; }
        if (sessionId != 0) {
            previousSessionId = sessionId;
            newSession = true;
        }
        sessionId = id;
        // Rule 6: a new session's events are not the old session's.
        mirror.reset(id);
    }
}
