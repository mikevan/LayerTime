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

import Toybox.Lang;

// The accounting of the 10-minute Status stability measurement (Increment 1
// acceptance item (b)), separated from BLE and timers so it is unit tested.
//
// Rules (approved 2026-09-30):
//   * The epoch is the first successfully decoded Status NOTIFICATION after
//     the test was started, not the start itself.
//   * The window is WINDOW_MS of Garmin-local monotonic time from the epoch,
//     fixed; a reconnect never restarts it.
//   * Every later Status receipt yields one interval to the previous receipt.
//     The previous receipt is never reset across a disconnect, so the first
//     Status after a reconnect measures the whole interruption.
//   * A disconnect is counted and makes the result FAIL; measuring goes on
//     to the boundary so the record stays complete.
//   * PASS = window completed, no interval over GAP_LIMIT_MS, no disconnect.
//     The number received is evidence, never a PASS condition.
class HeartbeatRun {

    const WINDOW_MS = 600000;
    const GAP_LIMIT_MS = 2000;

    private var _epochMs as Number = -1;
    private var _lastMs as Number = -1;
    private var _endMs as Number = -1;
    private var _received as Number = 0;
    private var _intervals as Array<Number> = [] as Array<Number>;
    private var _gapsOver as Number = 0;
    private var _disconnects as Number = 0;
    private var _complete as Boolean = false;

    public function initialize() {
    }

    // A Status notification decoded at receivedMs (Garmin-local). Returns the
    // interval it closed in ms, or -1 for the epoch (no interval yet).
    public function onStatus(receivedMs as Number) as Number {
        if (_complete) { return -1; }
        _received++;
        if (_epochMs < 0) {
            _epochMs = receivedMs;
            _lastMs = receivedMs;
            return -1;
        }
        var gap = receivedMs - _lastMs;
        _lastMs = receivedMs;
        _intervals.add(gap);
        if (gap > GAP_LIMIT_MS) { _gapsOver++; }
        return gap;
    }

    public function noteDisconnect() as Void {
        if (!_complete) { _disconnects++; }
    }

    // Called periodically. Returns true the first time the window has
    // elapsed; the run is complete from then on.
    public function tick(nowMs as Number) as Boolean {
        if (_complete || _epochMs < 0) { return false; }
        if (nowMs - _epochMs < WINDOW_MS) { return false; }
        _endMs = nowMs;
        _complete = true;
        return true;
    }

    public function gapLimitMs() as Number { return GAP_LIMIT_MS; }
    public function hasEpoch() as Boolean { return _epochMs >= 0; }
    public function isComplete() as Boolean { return _complete; }
    public function received() as Number { return _received; }
    public function intervals() as Number { return _intervals.size(); }
    public function gapsOverLimit() as Number { return _gapsOver; }
    public function disconnects() as Number { return _disconnects; }
    public function elapsedMs(nowMs as Number) as Number { return _epochMs < 0 ? 0 : nowMs - _epochMs; }
    public function maxInterval() as Number {
        var m = 0;
        for (var i = 0; i < _intervals.size(); i++) { if (_intervals[i] > m) { m = _intervals[i]; } }
        return m;
    }

    public function passed() as Boolean {
        return _complete && _gapsOver == 0 && _disconnects == 0;
    }

    // duration, received, intervals, min, mean, p95, max (nearest rank, as
    // the PING burst), gaps, disconnects, pass.
    public function summary() as Dictionary {
        var s = LinkBurstStats.compute(_intervals);
        s[:duration] = _endMs >= 0 ? _endMs - _epochMs : 0;
        s[:received] = _received;
        s[:intervals] = _intervals.size();
        s[:gaps] = _gapsOver;
        s[:disconnects] = _disconnects;
        s[:pass] = passed();
        return s;
    }
}
