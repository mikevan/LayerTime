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

// The bookkeeping of a PING burst, separated from the BLE side so it can be
// unit tested: how many were sent, what came back, and whether another
// attempt is due. It never issues a request itself; LinkBurst does that,
// from a one-shot timer, after record() says one is due.
class LinkBurstRun {

    private var _target as Number;
    private var _sent as Number = 0;
    private var _acked as Number = 0;
    private var _lost as Number = 0;
    private var _rtts as Array<Number> = [] as Array<Number>;

    public function initialize(target as Number) {
        _target = target;
    }

    // A PING has been issued.
    public function noteSent() as Void { _sent++; }

    // The outstanding PING completed: rttMs >= 0 is an ACK, -1 is lost.
    // Returns true when another PING should be issued, false when the run
    // has reached its target.
    public function record(rttMs as Number) as Boolean {
        if (rttMs >= 0) { _acked++; _rtts.add(rttMs); } else { _lost++; }
        return _sent < _target;
    }

    public function isComplete() as Boolean { return _sent >= _target; }
    public function target() as Number { return _target; }
    public function sent() as Number { return _sent; }
    public function acked() as Number { return _acked; }
    public function lost() as Number { return _lost; }
    public function rtts() as Array<Number> { return _rtts; }

    public function summary() as Dictionary {
        var s = LinkBurstStats.compute(_rtts);
        s[:sent] = _sent;
        s[:acked] = _acked;
        s[:lost] = _lost;
        return s;
    }
}
