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

// LayerTime Link 0.1 conformance for the Monkey C binding, against the same
// byte-exact vectors the C++ suite uses (LinkVectors.mc is generated from
// contracts/vectors/link_frames.json).
module LinkCodecTests {

    function same(logger as Logger, name as String, expected as ByteArray, actual as ByteArray) as Boolean {
        if (expected.equals(actual)) { return true; }
        logger.error(name + ": expected " + Link.toHex(expected) + " got " + Link.toHex(actual));
        return false;
    }

    function field(logger as Logger, name as String, key as Symbol, expected as Object?, actual as Object?) as Boolean {
        if (expected == null) { return true; }
        if (expected.equals(actual)) { return true; }
        logger.error(name + "." + key + ": expected " + expected + " got " + actual);
        return false;
    }

    (:test)
    function constantsMatchTheVectors(logger as Logger) as Boolean {
        var ok = true;
        ok = ok && Link.MAX_FRAME == LinkVectors.MAXFRAME;
        ok = ok && Link.STATUS_SIZE == LinkVectors.STATUSSIZE;
        ok = ok && Link.VERSION_BYTE == LinkVectors.LINKVERSIONBYTE;
        ok = ok && Link.MAJOR == LinkVectors.SERVERMAJOR && Link.MINOR == LinkVectors.SERVERMINOR;
        ok = ok && Link.OP_HELLO == LinkVectors.OPS_HELLO && Link.OP_PING == LinkVectors.OPS_PING;
        ok = ok && Link.OP_COMMAND == LinkVectors.OPS_COMMAND && Link.OP_GET_CHANGED == LinkVectors.OPS_GET_CHANGED;
        ok = ok && Link.OP_GET_TEXT == LinkVectors.OPS_GET_TEXT && Link.OP_RX_MARK == LinkVectors.OPS_RX_MARK;
        ok = ok && Link.OP_TEST_FIRST == LinkVectors.OPS_TESTRANGEFIRST && Link.OP_TEST_LAST == LinkVectors.OPS_TESTRANGELAST;
        ok = ok && Link.FRAME_HELLO_ACK == LinkVectors.FRAMETYPES_HELLO_ACK && Link.FRAME_ACK == LinkVectors.FRAMETYPES_ACK;
        ok = ok && Link.FRAME_RESULT == LinkVectors.FRAMETYPES_RESULT && Link.FRAME_EVENT_SUMMARY == LinkVectors.FRAMETYPES_EVENT_SUMMARY;
        ok = ok && Link.FRAME_END == LinkVectors.FRAMETYPES_END && Link.FRAME_TEXT == LinkVectors.FRAMETYPES_TEXT;
        ok = ok && Link.FRAME_ERROR == LinkVectors.FRAMETYPES_ERROR;
        ok = ok && Link.STATUS_OK == LinkVectors.LINKSTATUS_OK && Link.STATUS_UNKNOWN_OP == LinkVectors.LINKSTATUS_UNKNOWNOP;
        ok = ok && Link.STATUS_BAD_LENGTH == LinkVectors.LINKSTATUS_BADLENGTH;
        ok = ok && Link.STATUS_VERSION_MISMATCH == LinkVectors.LINKSTATUS_VERSIONMISMATCH && Link.STATUS_BUSY == LinkVectors.LINKSTATUS_BUSY;
        ok = ok && Link.FLAG_MONITORING == LinkVectors.STATUSFLAGS_MONITORING && Link.FLAG_SLEEP_MODE == LinkVectors.STATUSFLAGS_SLEEPMODE;
        ok = ok && Link.FLAG_NO_SD_LOG == LinkVectors.STATUSFLAGS_NOSDLOG;
        ok = ok && Link.CAP_LOCAL_WIFI_MONITOR == LinkVectors.CAPABILITIES_LOCALWIFIMONITOR && Link.CAP_BUTTON == LinkVectors.CAPABILITIES_BUTTON;
        ok = ok && Link.UUID_SERVICE.equals(LinkVectors.UUID_SERVICE) && Link.UUID_CONTROL.equals(LinkVectors.UUID_CONTROL);
        ok = ok && Link.UUID_STATUS.equals(LinkVectors.UUID_STATUS) && Link.UUID_DATA.equals(LinkVectors.UUID_DATA);
        ok = ok && Link.UUID_PROBE.equals(LinkVectors.UUID_PROBE);
        ok = ok && Link.TEXT_MAX_CHUNK == LinkVectors.TEXTMAXCHUNK;
        ok = ok && Link.SUMMARY_HAS_SOURCE_ID == LinkVectors.SUMMARYFLAGS_SOURCEID && Link.SUMMARY_HAS_DETAIL == LinkVectors.SUMMARYFLAGS_DETAIL;
        ok = ok && Link.TEXT_FIELD_SOURCE_ID == LinkVectors.TEXTFIELDS_SOURCEID && Link.TEXT_FIELD_DETAIL == LinkVectors.TEXTFIELDS_DETAIL;
        if (!ok) { logger.error("a constant differs from link_frames.json"); }
        return ok;
    }

    (:test)
    function statusDecodesEveryVector(logger as Logger) as Boolean {
        var ok = true;
        for (var i = 0; i < LinkVectors.STATUS.size(); i++) {
            var v = LinkVectors.STATUS[i];
            var name = v[:name] as String;
            var f = v[:fields] as Dictionary;
            var s = Link.decodeStatus(v[:bytes] as ByteArray);
            if (s == null) { logger.error(name + ": decodeStatus returned null"); ok = false; continue; }
            var keys = [:linkVersion, :sessionId, :flags, :selected, :active, :eventCount, :heartbeat, :schedule, :nextReportS];
            for (var k = 0; k < keys.size(); k++) {
                ok = field(logger, name, keys[k], f[keys[k]] as Object?, s[keys[k]] as Object?) && ok;
            }
            ok = field(logger, name, :changeSeq, (f[:changeSeq] as Numeric).toLong(), s[:changeSeq] as Object?) && ok;
            ok = field(logger, name, :lastAlertEventId, (f[:lastAlertEventId] as Numeric).toLong(), s[:lastAlertEventId] as Object?) && ok;
        }
        ok = ok && Link.decodeStatus(new [17]b) == null && Link.decodeStatus(new [19]b) == null;
        return ok;
    }

    (:test)
    function requestsEncodeToEveryVector(logger as Logger) as Boolean {
        var ok = true;
        for (var i = 0; i < LinkVectors.REQUESTS.size(); i++) {
            var v = LinkVectors.REQUESTS[i];
            var name = v[:name] as String;
            var f = v[:fields] as Dictionary;
            var op = v[:op] as String;
            var bytes = null;
            if (op.equals("HELLO")) {
                bytes = Link.encodeHello(f[:reqId] as Number, f[:clientMajor] as Number, f[:clientMinor] as Number);
            } else if (op.equals("PING")) {
                bytes = Link.encodePing(f[:reqId] as Number, (f[:token] as Numeric).toLong());
            } else if (op.equals("COMMAND")) {
                bytes = Link.encodeCommand(f[:reqId] as Number, f[:commandType] as Number, f[:argument] as Number?);
            } else if (op.equals("GET_CHANGED")) {
                bytes = Link.encodeGetChanged(f[:reqId] as Number, (f[:sinceChangeSeq] as Numeric).toLong());
            } else if (op.equals("GET_TEXT")) {
                bytes = Link.encodeGetText(f[:reqId] as Number, (f[:eventId] as Numeric).toLong(), f[:field] as Number);
            } else {
                logger.error(name + ": unknown op " + op);
                ok = false;
                continue;
            }
            ok = same(logger, name, v[:bytes] as ByteArray, bytes) && ok;
            ok = ok && bytes.size() <= Link.MAX_FRAME;
        }
        return ok;
    }

    (:test)
    function repliesDecodeEveryVector(logger as Logger) as Boolean {
        var ok = true;
        for (var i = 0; i < LinkVectors.REPLIES.size(); i++) {
            var v = LinkVectors.REPLIES[i];
            var name = v[:name] as String;
            var f = v[:fields] as Dictionary;
            var r = Link.decodeReply(v[:bytes] as ByteArray);
            if (r == null) { logger.error(name + ": decodeReply returned null"); ok = false; continue; }
            var type = v[:type] as String;
            var names = ["HELLO_ACK", "ACK", "ERROR", "RESULT", "EVENT_SUMMARY", "END", "TEXT"];
            var types = [Link.FRAME_HELLO_ACK, Link.FRAME_ACK, Link.FRAME_ERROR, Link.FRAME_RESULT,
                         Link.FRAME_EVENT_SUMMARY, Link.FRAME_END, Link.FRAME_TEXT];
            var expectedType = -1;
            for (var t = 0; t < names.size(); t++) { if (type.equals(names[t])) { expectedType = types[t]; } }
            ok = field(logger, name, :type, expectedType, r[:type] as Object?) && ok;
            ok = field(logger, name, :reqId, f[:reqId] as Object?, r[:reqId] as Object?) && ok;
            ok = field(logger, name, :linkStatus, f[:linkStatus] as Object?, r[:linkStatus] as Object?) && ok;
            if (type.equals("HELLO_ACK")) {
                ok = field(logger, name, :serverMajor, f[:serverMajor] as Object?, r[:serverMajor] as Object?) && ok;
                ok = field(logger, name, :serverMinor, f[:serverMinor] as Object?, r[:serverMinor] as Object?) && ok;
                ok = field(logger, name, :sessionId, f[:sessionId] as Object?, r[:sessionId] as Object?) && ok;
                ok = field(logger, name, :capabilities, f[:capabilities] as Object?, r[:capabilities] as Object?) && ok;
                ok = field(logger, name, :maxFrame, f[:maxFrame] as Object?, r[:maxFrame] as Object?) && ok;
            } else if (type.equals("ACK")) {
                ok = field(logger, name, :token, (f[:token] as Numeric).toLong(), r[:token] as Object?) && ok;
                ok = field(logger, name, :heartbeat, f[:heartbeat] as Object?, r[:heartbeat] as Object?) && ok;
            } else if (type.equals("RESULT")) {
                ok = field(logger, name, :commandType, f[:commandType] as Object?, r[:commandType] as Object?) && ok;
                ok = field(logger, name, :commandResult, f[:commandResult] as Object?, r[:commandResult] as Object?) && ok;
            } else if (type.equals("EVENT_SUMMARY")) {
                ok = field(logger, name, :eventId, (f[:eventId] as Numeric).toLong(), r[:eventId] as Object?) && ok;
                var keys = [:detector, :confidence, :sourceKind, :band, :channel, :rssi, :count, :ageSeconds, :flags];
                for (var k = 0; k < keys.size(); k++) {
                    ok = field(logger, name, keys[k], f[keys[k]] as Object?, r[keys[k]] as Object?) && ok;
                }
            } else if (type.equals("END")) {
                ok = field(logger, name, :count, f[:count] as Object?, r[:count] as Object?) && ok;
                ok = field(logger, name, :gap, f[:gap] as Object?, r[:gap] as Object?) && ok;
                ok = field(logger, name, :changeSeq, (f[:changeSeq] as Numeric).toLong(), r[:changeSeq] as Object?) && ok;
            } else if (type.equals("TEXT")) {
                ok = field(logger, name, :field, f[:field] as Object?, r[:field] as Object?) && ok;
                ok = field(logger, name, :index, f[:index] as Object?, r[:index] as Object?) && ok;
                ok = field(logger, name, :total, f[:total] as Object?, r[:total] as Object?) && ok;
                ok = field(logger, name, :length, f[:length] as Object?, r[:length] as Object?) && ok;
                var bytes = v[:bytes] as ByteArray;
                ok = same(logger, name + ".bytes", bytes.slice(Link.TEXT_HEADER_SIZE, bytes.size()), r[:bytes] as ByteArray) && ok;
            }
        }
        return ok;
    }

    (:test)
    function decoderRejectsWrongLengthsAndUnknownTypes(logger as Logger) as Boolean {
        var ok = Link.decodeReply([0x82, 0x01, 0x00]b) == null;             // ACK too short
        ok = ok && Link.decodeReply([0x81, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x14, 0x00]b) == null; // HELLO_ACK too long
        ok = ok && Link.decodeReply([0x7E, 0x01, 0x00]b) == null;           // not a frame type
        ok = ok && Link.decodeReply([]b) == null;
        ok = ok && Link.decodeReply([0x85, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]b) == null;  // END too short
        ok = ok && Link.decodeReply([0x86, 0x01, 0x00, 0x01, 0x00, 0x01]b) == null;              // TEXT with no payload
        ok = ok && Link.decodeReply(new [17]b) == null;
        var summary = new [18]b;
        summary[0] = Link.FRAME_EVENT_SUMMARY;
        ok = ok && Link.decodeReply(summary.slice(0, 17)) == null;                               // EVENT_SUMMARY short
        if (!ok) { logger.error("a bad frame was accepted"); }
        return ok;
    }

    (:test)
    function narrowKeepsValuesAndWidenInverts(logger as Logger) as Boolean {
        var ok = Link.narrow(7l) == 7 && Link.narrow(0l) == 0 && Link.narrow(2147483647l) == 2147483647;
        ok = ok && Link.widen(7).equals(7l) && Link.widen(Link.narrow(4294967295l)).equals(4294967295l);
        if (!ok) { logger.error("narrow/widen broke"); }
        return ok;
    }

    (:test)
    function queueTouchRestartsTheTimeout(logger as Logger) as Boolean {
        var q = new RequestQueue();
        var a = {:kind => :write, :reqId => 1};
        var ok = q.submit(a, 0) == a;
        q.touch(2500);                                  // a frame of the reply arrived
        ok = ok && q.expire(3000, 3000) == null;       // not yet: 500 ms since the frame
        ok = ok && q.expire(5500, 3000) == a;          // 3000 ms after the last frame
        if (!ok) { logger.error("touch did not restart the timeout"); }
        return ok;
    }

    (:test)
    function burstStatsUseNearestRankOnTheSortedList(logger as Logger) as Boolean {
        // Ten values, unsorted: sorted they are 100..1000 in steps of 100.
        var s = LinkBurstStats.compute([500, 100, 1000, 300, 200, 900, 400, 800, 700, 600] as Array<Number>);
        var ok = field(logger, "ten", :count, 10, s[:count] as Object?);
        ok = field(logger, "ten", :min, 100, s[:min] as Object?) && ok;
        ok = field(logger, "ten", :max, 1000, s[:max] as Object?) && ok;
        ok = field(logger, "ten", :median, 550, s[:median] as Object?) && ok;   // (500 + 600) / 2
        ok = field(logger, "ten", :mean, 550, s[:mean] as Object?) && ok;
        ok = field(logger, "ten", :p95, 1000, s[:p95] as Object?) && ok;        // ceil(9.5) = rank 10
        var one = LinkBurstStats.compute([498] as Array<Number>);
        ok = field(logger, "one", :median, 498, one[:median] as Object?) && ok;
        ok = field(logger, "one", :p95, 498, one[:p95] as Object?) && ok;
        var odd = LinkBurstStats.compute([3, 1, 2] as Array<Number>);
        ok = field(logger, "odd", :median, 2, odd[:median] as Object?) && ok;
        ok = field(logger, "odd", :mean, 2, odd[:mean] as Object?) && ok;
        var hundred = [] as Array<Number>;
        for (var i = 1; i <= 100; i++) { hundred.add(i * 10); }
        var h = LinkBurstStats.compute(hundred);
        ok = field(logger, "hundred", :p95, 950, h[:p95] as Object?) && ok;      // rank 95 of 100
        ok = field(logger, "hundred", :median, 505, h[:median] as Object?) && ok; // (500 + 510) / 2
        ok = field(logger, "hundred", :mean, 505, h[:mean] as Object?) && ok;
        var none = LinkBurstStats.compute([] as Array<Number>);
        ok = field(logger, "none", :count, 0, none[:count] as Object?) && ok;
        return ok;
    }

    (:test)
    function burstRunRecordsWithoutSendingAndStopsAtTarget(logger as Logger) as Boolean {
        // record() only accounts for the completed PING and says whether
        // another is due; it never issues one (that is LinkBurst's timer).
        var run = new LinkBurstRun(3);
        var ok = !run.isComplete() && run.sent() == 0;
        run.noteSent();                                   // PING 1 issued
        ok = ok && run.record(400) == true && run.sent() == 1 && run.acked() == 1;
        run.noteSent();                                   // PING 2 issued
        ok = ok && run.record(-1) == true && run.lost() == 1 && run.sent() == 2;
        run.noteSent();                                   // PING 3 issued
        ok = ok && !run.isComplete() == false;            // target reached once sent
        ok = ok && run.record(600) == false;              // last result: no more
        ok = ok && run.sent() == 3 && run.acked() == 2 && run.lost() == 1;
        ok = ok && run.rtts().size() == 2 && run.rtts()[0] == 400 && run.rtts()[1] == 600;
        var s = run.summary();
        ok = field(logger, "run", :sent, 3, s[:sent] as Object?) && ok;
        ok = field(logger, "run", :acked, 2, s[:acked] as Object?) && ok;
        ok = field(logger, "run", :lost, 1, s[:lost] as Object?) && ok;
        ok = field(logger, "run", :min, 400, s[:min] as Object?) && ok;
        ok = field(logger, "run", :max, 600, s[:max] as Object?) && ok;
        ok = field(logger, "run", :median, 500, s[:median] as Object?) && ok;
        if (!ok) { logger.error("burst run accounting broke"); }
        return ok;
    }

    (:test)
    function heartbeatRunMeasuresFromTheFirstStatusAndClosesAtTheWindow(logger as Logger) as Boolean {
        var run = new HeartbeatRun();
        var ok = !run.hasEpoch() && !run.tick(50000);          // no epoch yet: the window cannot elapse
        ok = ok && run.onStatus(10000) == -1 && run.hasEpoch(); // first Status is the epoch, no interval
        ok = ok && run.onStatus(11000) == 1000;
        ok = ok && run.onStatus(12000) == 1000;
        ok = ok && run.onStatus(14000) == 2000;                 // exactly 2000 is not over the limit
        ok = ok && run.gapsOverLimit() == 0;
        ok = ok && run.onStatus(16001) == 2001;                 // 2001 is
        ok = ok && run.gapsOverLimit() == 1 && run.maxInterval() == 2001;
        ok = ok && !run.tick(10000 + 599999);                   // one ms short of the window
        ok = ok && run.tick(10000 + 600000);                    // the window, measured from the epoch
        ok = ok && run.isComplete() && !run.tick(10000 + 700000); // reported once
        ok = ok && run.onStatus(700000) == -1 && run.received() == 5; // nothing recorded after completion
        var s = run.summary();
        ok = field(logger, "hb", :duration, 600000, s[:duration] as Object?) && ok;
        ok = field(logger, "hb", :received, 5, s[:received] as Object?) && ok;
        ok = field(logger, "hb", :intervals, 4, s[:intervals] as Object?) && ok;
        ok = field(logger, "hb", :min, 1000, s[:min] as Object?) && ok;
        ok = field(logger, "hb", :max, 2001, s[:max] as Object?) && ok;
        ok = field(logger, "hb", :mean, 1500, s[:mean] as Object?) && ok;   // (1000+1000+2000+2001+2)/4 = 1500.25 -> 1500
        ok = field(logger, "hb", :p95, 2001, s[:p95] as Object?) && ok;    // ceil(3.8) = rank 4
        ok = field(logger, "hb", :gaps, 1, s[:gaps] as Object?) && ok;
        ok = ok && !run.passed();                               // one gap over the limit: FAIL
        if (!ok) { logger.error("heartbeat run accounting broke"); }
        return ok;
    }

    (:test)
    function heartbeatRunPassesOnlyWithNoGapAndNoDisconnect(logger as Logger) as Boolean {
        // Clean run: 601 receipts one second apart, then the window closes.
        var clean = new HeartbeatRun();
        for (var i = 0; i <= 600; i++) { clean.onStatus(1000 * i); }
        var ok = clean.tick(600000) && clean.passed() && clean.received() == 601 && clean.intervals() == 600;
        // Fewer receipts than seconds is still a PASS: the count is evidence only.
        var sparse = new HeartbeatRun();
        for (var i = 0; i <= 300; i++) { sparse.onStatus(1900 * i); }
        ok = ok && sparse.tick(600000) && sparse.passed() && sparse.received() == 301;
        // A disconnect fails the run even with no gap over the limit, and the
        // previous receipt is kept across it, so the first Status after the
        // reconnect measures the whole interruption.
        var dc = new HeartbeatRun();
        dc.onStatus(0); dc.onStatus(1000);
        dc.noteDisconnect();
        ok = ok && dc.onStatus(9000) == 8000 && dc.gapsOverLimit() == 1;
        ok = ok && dc.tick(600000) && !dc.passed() && dc.disconnects() == 1;
        // A disconnect alone, no gap, is still a FAIL.
        var dc2 = new HeartbeatRun();
        dc2.onStatus(0); dc2.onStatus(1000); dc2.noteDisconnect(); dc2.onStatus(2000);
        ok = ok && dc2.tick(600000) && !dc2.passed() && dc2.gapsOverLimit() == 0;
        // Not complete: never a PASS, whatever was measured.
        var open = new HeartbeatRun();
        open.onStatus(0); open.onStatus(1000);
        ok = ok && !open.passed();
        if (!ok) { logger.error("heartbeat pass rule broke"); }
        return ok;
    }

    (:test)
    function queueExpiryAccountsButDoesNotFreeTheSlot(logger as Logger) as Boolean {
        // Finding F2: after the timeout the item is reported once and stays
        // in flight; a pending item is not released until complete().
        var q = new RequestQueue();
        var a = {:kind => :cccd, :n => 1};
        var b = {:kind => :cccd, :n => 2};
        var ok = q.submit(a, 0) == a;
        ok = ok && q.submit(b, 100) == null && q.pending() == 1;
        ok = ok && q.expire(3000, 3000) == a;            // reported
        ok = ok && q.inFlight() == a && q.isBusy() && q.pending() == 1; // b not released
        ok = ok && q.expire(9000, 3000) == null;         // not reported again
        ok = ok && q.submit({:kind => :read}, 9100) == null && q.pending() == 2; // still nothing released
        ok = ok && q.complete(9200) == b && q.inFlight() == b && !q.isExpired(); // the stack finished a: b goes
        q.clear();
        ok = ok && !q.isBusy() && !q.isExpired() && q.pending() == 0;
        if (!ok) { logger.error("expiry freed the slot"); }
        return ok;
    }

    (:test)
    function queueAllowsOneOutstandingOperation(logger as Logger) as Boolean {
        var q = new RequestQueue();
        var a = {:kind => :write, :reqId => 1};
        var b = {:kind => :write, :reqId => 2};
        var first = q.submit(a, 1000);
        var ok = first == a && q.isBusy();
        var second = q.submit(b, 1100);
        ok = ok && second == null && q.pending() == 1;
        var next = q.complete(1200);
        ok = ok && next == b && q.pending() == 0 && q.isBusy();
        ok = ok && q.expire(1300, 3000) == null;       // not yet
        ok = ok && q.expire(4300, 3000) == b && q.isBusy() && q.isExpired(); // timed out: reported, still in flight (F2)
        ok = ok && q.expire(4400, 3000) == null;       // reported once
        ok = ok && q.complete(4400) == null && !q.isBusy() && !q.isExpired();
        if (!ok) { logger.error("queue ordering broke"); }
        return ok;
    }
}
