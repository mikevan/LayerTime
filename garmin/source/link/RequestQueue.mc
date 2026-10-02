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

// One outstanding GATT operation at a time (contracts/link.md rule 2, and
// the Connect IQ BLE driver's own limit). Pure logic, no BLE, so it is unit
// tested: LinkClient feeds it requests and tells it when each completes.
//
// An item is a Dictionary the caller understands; the queue only orders them.
//
// Finding F2 (garmin/README.md): expiry is accounting only. expire() reports
// an in-flight item that has waited too long, once, but does NOT free the
// slot; only complete() (driven by the stack's completion callback or by a
// disconnect) does. The caller decides, from the stack's own state, whether
// completing is safe.
class RequestQueue {

    private var _items as Array<Dictionary> = [] as Array<Dictionary>;
    private var _inFlight as Dictionary? = null;
    private var _startedAt as Number = 0;
    private var _expired as Boolean = false;

    // Adds an item. Returns the item to send now, or null if one is already
    // in flight (the caller sends nothing and waits for complete()).
    public function submit(item as Dictionary, nowMs as Number) as Dictionary? {
        _items.add(item);
        return next(nowMs);
    }

    // The in-flight item finished (success or failure). Returns the next
    // item to send, or null when the queue is empty.
    public function complete(nowMs as Number) as Dictionary? {
        _inFlight = null;
        _expired = false;
        return next(nowMs);
    }

    // Reports the in-flight item once it has waited timeoutMs, exactly once.
    // The item stays in flight (F2); the caller completes it when the stack
    // has finished with it.
    public function expire(nowMs as Number, timeoutMs as Number) as Dictionary? {
        if (_inFlight == null || _expired || nowMs - _startedAt < timeoutMs) { return null; }
        _expired = true;
        return _inFlight;
    }

    // The in-flight item made progress (a frame of a multi-frame reply
    // arrived): its accounting timeout starts again from nowMs.
    public function touch(nowMs as Number) as Void {
        if (_inFlight != null) { _startedAt = nowMs; }
    }

    public function inFlight() as Dictionary? { return _inFlight; }
    public function isBusy() as Boolean { return _inFlight != null; }
    public function isExpired() as Boolean { return _inFlight != null && _expired; }
    public function pending() as Number { return _items.size(); }

    public function clear() as Void {
        _items = [] as Array<Dictionary>;
        _inFlight = null;
        _expired = false;
    }

    private function next(nowMs as Number) as Dictionary? {
        if (_inFlight != null || _items.size() == 0) { return null; }
        _inFlight = _items[0];
        _items = _items.slice(1, _items.size());
        _startedAt = nowMs;
        _expired = false;
        return _inFlight;
    }
}
