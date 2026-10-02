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

// The watch's copy of the LayerWand event log, kept by GET_CHANGED
// (contracts/link.md). Pure logic, no BLE and no UI, so it is unit tested.
//
// Rules, from the contract:
//   * An EVENT_SUMMARY replaces what the mirror holds for that eventId.
//   * END's changeSeq is what the mirror is synchronised to; the next
//     GET_CHANGED asks for everything after it.
//   * The Node holds the 40 most recently created events. The mirror keeps
//     only the eventCount highest eventIds, and only when the Status that
//     carried eventCount has the same changeSeq as the mirror, so the two
//     describe the same moment.
//   * A new sessionId discards everything.
class ReconMirror {

    public var sessionId as Number = 0;
    // END.changeSeq of the last complete GET_CHANGED; meaningful once
    // synced is true.
    public var syncedSeq as Number = 0;
    public var synced as Boolean = false;
    // GET_CHANGED replies whose END reported a gap: changes that were
    // dropped on the Node before the watch asked. Shown on the Recon page
    // until the detections are cleared or a new session starts.
    public var gaps as Number = 0;

    // eventId (Number) -> summary Dictionary, plus :receivedMs, and
    // :sourceId / :detail once fetched with GET_TEXT.
    private var _events as Dictionary = {};

    public function initialize() {}

    public function reset(newSessionId as Number) as Void {
        sessionId = newSessionId;
        syncedSeq = 0;
        synced = false;
        gaps = 0;
        _events = {};
    }

    // The since value for the next GET_CHANGED.
    public function since() as Number { return synced ? syncedSeq : 0; }

    // One EVENT_SUMMARY, as Link.decodeReply returns it.
    public function apply(summary as Dictionary, nowMs as Number) as Void {
        var id = Link.narrow(summary[:eventId] as Long);
        var old = _events[id] as Dictionary?;
        var e = {
            :eventId => id,
            :detector => summary[:detector],
            :confidence => summary[:confidence],
            :sourceKind => summary[:sourceKind],
            :band => summary[:band],
            :channel => summary[:channel],
            :rssi => summary[:rssi],
            :count => summary[:count],
            :ageSeconds => summary[:ageSeconds],
            :flags => summary[:flags],
            :receivedMs => nowMs
        };
        // Text does not change for an event (same detector, same source);
        // keep what was already fetched.
        if (old != null) {
            if (old[:sourceId] != null) { e[:sourceId] = old[:sourceId]; }
            if (old[:detail] != null) { e[:detail] = old[:detail]; }
        }
        _events[id] = e;
    }

    // END of a GET_CHANGED.
    public function complete(end as Dictionary) as Void {
        syncedSeq = Link.narrow(end[:changeSeq] as Long);
        synced = true;
        if ((end[:gap] as Number) != 0) { gaps++; }
    }

    // Status with this changeSeq says the Node holds eventCount events.
    // Returns true when anything was removed.
    public function prune(eventCount as Number, statusChangeSeq as Number) as Boolean {
        if (!synced || statusChangeSeq != syncedSeq) { return false; }
        var ids = sortedIds();
        if (ids.size() <= eventCount) { return false; }
        for (var i = eventCount; i < ids.size(); i++) { _events.remove(ids[i]); }
        return true;
    }

    // ReconClearEvents succeeded: the missed detections no longer matter.
    public function clearGaps() as Void { gaps = 0; }

    public function count() as Number { return _events.size(); }

    public function get(eventId as Number) as Dictionary? { return _events[eventId] as Dictionary?; }

    // Newest first (highest eventId first).
    public function events() as Array<Dictionary> {
        var ids = sortedIds();
        var out = [] as Array<Dictionary>;
        for (var i = 0; i < ids.size(); i++) { out.add(_events[ids[i]] as Dictionary); }
        return out;
    }

    public function setText(eventId as Number, field as Number, text as String) as Void {
        var e = _events[eventId] as Dictionary?;
        if (e == null) { return; }
        e[field == Link.TEXT_FIELD_SOURCE_ID ? :sourceId : :detail] = text;
    }

    // Seconds since the Node last saw the event, now.
    public static function ageSeconds(e as Dictionary, nowMs as Number) as Number {
        var age = e[:ageSeconds] as Number;
        var since = nowMs - (e[:receivedMs] as Number);
        return age + (since > 0 ? since / 1000 : 0);
    }

    // Untrusted over-the-air bytes to display text: printable ASCII kept,
    // anything else shown as '?'.
    public static function printable(bytes as ByteArray) as String {
        var s = "";
        for (var i = 0; i < bytes.size(); i++) {
            var b = bytes[i] as Number;
            s += (b >= 0x20 && b <= 0x7E) ? b.toChar().toString() : "?";
        }
        return s;
    }

    // Event ids, highest first (insertion sort; at most 40).
    private function sortedIds() as Array<Number> {
        var keys = _events.keys();
        var ids = [] as Array<Number>;
        for (var i = 0; i < keys.size(); i++) {
            var k = keys[i] as Number;
            var j = ids.size();
            ids.add(k);
            while (j > 0 && ids[j - 1] < k) {
                ids[j] = ids[j - 1];
                j--;
            }
            ids[j] = k;
        }
        return ids;
    }
}
