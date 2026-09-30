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

// The LayerTime Link client (Slice 1 Increment 1): discovers a Node by
// service UUID, pairs, registers the profile, enables notifications, says
// HELLO, watches the Status heartbeat, answers the wearer's PINGs with a
// round-trip time, and reconnects when the link drops. Detects a new Node
// session (a C5 power cycle) through sessionId.
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

    // Called once per PING when it completes: the round trip in ms, or -1
    // when it was lost (timeout, token mismatch, or an ERROR reply). The
    // test-only burst (LinkBurst.mc) hooks here; production leaves it null.
    public var pingObserver as Method(rttMs as Number) as Void? = null;
    // Finding F2 (garmin/README.md): true from the moment any GATT request
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

    public function isReady() as Boolean { return _state == STATE_READY; }

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
            completeAndSendNext();
        } else if (_queue.isExpired()) {
            // The write is done but its reply did not arrive within the
            // accounting timeout; the stack is free now, so move on (F2).
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
            if (!requestPending) { completeAndSendNext(); }
        }
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
        lastStatusMs = System.getTimer();
        noteSession(s[:sessionId] as Number);
        return true;
    }

    private function applyReply(value as ByteArray) as Void {
        var reply = Link.decodeReply(value);
        var inFlight = _queue.inFlight();
        if (reply == null) {
            lastError = "Unknown frame " + Link.toHex(value);
            return;
        }
        if (inFlight == null || inFlight[:reqId] != reply[:reqId]) {
            lastError = "Unexpected reply id " + (reply[:reqId] as Number);
            return;
        }
        var type = reply[:type];
        var status = reply[:linkStatus] as Number;
        var pingResult = inFlight[:op] == Link.OP_PING ? -1 : null;
        if (type == Link.FRAME_HELLO_ACK) {
            serverVersion = (reply[:serverMajor] as Number).toString() + "." + (reply[:serverMinor] as Number).toString();
            capabilities = reply[:capabilities] as Number;
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
                lastRttMs = System.getTimer() - (inFlight[:sentAt] as Number);
                pingResult = lastRttMs;
            } else {
                lastError = "ACK token mismatch";
            }
        } else if (type == Link.FRAME_ERROR) {
            lastError = "Node: " + Link.statusName(status);
        }
        // Complete first, then report: an observer that sends the next PING
        // must find the queue free, so the next request is issued only after
        // this one has fully finished (one outstanding operation).
        completeAndSendNext();
        if (pingResult != null) { notifyPing(pingResult); }
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
    }
}
