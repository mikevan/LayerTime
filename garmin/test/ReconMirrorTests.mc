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
import Toybox.Test;

// ReconMirror, the watch's copy of the LayerWand event log: the GET_CHANGED
// rules of contracts/link.md.
module ReconMirrorTests {

    function summary(id as Number, count as Number, rssi as Number) as Dictionary {
        return {:type => Link.FRAME_EVENT_SUMMARY, :reqId => 1, :linkStatus => 0, :eventId => id.toLong(),
                :detector => 5, :confidence => 2, :sourceKind => 1, :band => 1, :channel => 6, :rssi => rssi,
                :count => count, :ageSeconds => 10, :flags => 3};
    }

    function end(seq as Number, gap as Number) as Dictionary {
        return {:type => Link.FRAME_END, :reqId => 1, :linkStatus => 0, :count => 0, :gap => gap, :changeSeq => seq.toLong()};
    }

    (:test)
    function summariesUpsertAndListNewestFirst(logger as Logger) as Boolean {
        var m = new ReconMirror();
        m.reset(0x1234);
        var ok = m.since() == 0 && !m.synced;
        m.apply(summary(1, 1, -60), 1000);
        m.apply(summary(2, 1, -70), 1000);
        m.apply(summary(3, 1, -80), 1000);
        m.complete(end(4, 0));
        ok = ok && m.synced && m.since() == 4 && m.count() == 3;
        var list = m.events();
        ok = ok && (list[0][:eventId] as Number) == 3 && (list[2][:eventId] as Number) == 1;
        m.setText(2, Link.TEXT_FIELD_DETAIL, "Free Public WiFi");
        m.apply(summary(2, 5, -40), 2000);             // a repeat sighting
        var e = m.get(2) as Dictionary;
        ok = ok && (e[:count] as Number) == 5 && (e[:rssi] as Number) == -40;
        ok = ok && "Free Public WiFi".equals(e[:detail] as String); // text kept
        ok = ok && ReconMirror.ageSeconds(e, 5000) == 13;           // 10 s + 3 s since receipt
        if (!ok) { logger.error("upsert or order broke"); }
        return ok;
    }

    (:test)
    function pruneKeepsTheHighestIdsOnlyWhenStatusMatches(logger as Logger) as Boolean {
        var m = new ReconMirror();
        m.reset(1);
        for (var i = 1; i <= 5; i++) { m.apply(summary(i, 1, -60), 0); }
        m.complete(end(9, 0));
        var ok = !m.prune(3, 8);                          // Status from another moment: untouched
        ok = ok && m.count() == 5;
        ok = ok && m.prune(3, 9) && m.count() == 3;       // same moment: ids 3, 4, 5 stay
        ok = ok && m.get(1) == null && m.get(2) == null && m.get(3) != null && m.get(5) != null;
        ok = ok && !m.prune(3, 9);                        // nothing more to remove
        ok = ok && m.prune(0, 9) && m.count() == 0;       // a clear
        if (!ok) { logger.error("prune broke"); }
        return ok;
    }

    (:test)
    function gapsAreCountedAndANewSessionStartsOver(logger as Logger) as Boolean {
        var m = new ReconMirror();
        m.reset(1);
        m.apply(summary(1, 1, -60), 0);
        m.complete(end(3, 1));
        var ok = m.gaps == 1 && m.syncedSeq == 3;
        m.complete(end(4, 1));
        ok = ok && m.gaps == 2;
        m.clearGaps();                                    // ReconClearEvents succeeded
        ok = ok && m.gaps == 0 && m.syncedSeq == 4 && m.count() == 1;
        m.complete(end(5, 1));
        m.reset(2);
        ok = ok && m.count() == 0 && !m.synced && m.since() == 0 && m.gaps == 0 && m.sessionId == 2;
        if (!ok) { logger.error("gap or session reset broke"); }
        return ok;
    }

    (:test)
    function textFromTheAirIsMadePrintable(logger as Logger) as Boolean {
        var ok = ReconMirror.printable([0x41, 0x42, 0x00, 0x7F, 0xC3, 0xA9, 0x20, 0x7E]b).equals("AB???? ~");
        ok = ok && ReconMirror.printable([]b).equals("");
        if (!ok) { logger.error("printable broke"); }
        return ok;
    }
}
