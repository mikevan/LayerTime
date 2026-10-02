// LayerTime - counter-intrusion and resilient-communications firmware
// for the LilyGo T-Watch Ultra.
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

#include "LinkServer.h"

#include <string.h>

#include "../model/LayerTimeCommand.h"

namespace layertime {
namespace link {

namespace {

void emitError(uint8_t reqId, LinkStatus status, LinkServer::FrameSink sink, void *context)
{
    uint8_t frame[kMaxFrame];
    Error e;
    e.reqId = reqId;
    e.status = status;
    sink(frame, encodeError(e, frame), context);
}

void emitResult(uint8_t reqId, uint8_t commandType, CommandResult result, LinkServer::FrameSink sink,
                void *context)
{
    uint8_t frame[kMaxFrame];
    Result r;
    r.reqId = reqId;
    r.commandType = commandType;
    r.commandResult = static_cast<uint8_t>(result);
    sink(frame, encodeResult(r, frame), context);
}

void emitEnd(uint8_t reqId, uint8_t count, bool gap, uint32_t changeSeq, LinkServer::FrameSink sink,
             void *context)
{
    uint8_t frame[kMaxFrame];
    End e;
    e.reqId = reqId;
    e.count = count;
    e.gap = gap ? 1 : 0;
    e.changeSeq = changeSeq;
    sink(frame, encodeEnd(e, frame), context);
}

uint16_t saturate16(uint32_t v)
{
    return v > 0xFFFF ? 0xFFFF : static_cast<uint16_t>(v);
}

// The commands a Recon Node accepts, and the length each takes.
bool commandLength(uint8_t commandType, size_t &length)
{
    switch (static_cast<CommandType>(commandType)) {
    case CommandType::ReconStart:
    case CommandType::SetSleepMode:
    case CommandType::SetEarlyWarning:
        length = kCommandMinSize + 1;
        return true;
    case CommandType::ReconStop:
    case CommandType::ReconClearEvents:
    case CommandType::ReconAcknowledgeAlert:
        length = kCommandMinSize;
        return true;
    default:
        return false;
    }
}

} // namespace

uint8_t statusFlags(const ReconState &r, bool sleepMode)
{
    uint8_t f = 0;
    if (r.monitoring) f |= kFlagMonitoring;
    if (r.earlyWarningEnabled) f |= kFlagEarlyWarningEnabled;
    if (r.earlyWarningResting) f |= kFlagEarlyWarningResting;
    if (r.alertPending) f |= kFlagAlertPending;
    if (sleepMode) f |= kFlagSleepMode;
    return f;
}

void LinkServer::setLock(LockFn lock, LockFn unlock, void *context)
{
    _lock = lock;
    _unlock = unlock;
    _lockContext = context;
}

bool LinkServer::refresh()
{
    const uint32_t before = _tracker.changeSeq();
    lock();
    refreshLocked();
    unlock();
    return _tracker.changeSeq() != before;
}

// Caller holds the lock.
void LinkServer::refreshLocked()
{
    const ReconState r = _core.reconState();
    TrackedState s;
    s.flags = statusFlags(r, _core.settings().sleepModeEnabled);
    s.selected = static_cast<uint8_t>(r.selected);
    s.active = static_cast<uint8_t>(r.active);
    s.eventCount = r.eventCount;
    s.lastAlertEventId = _core.lastAlertEventId();
    _tracker.begin();
    const uint8_t n = _core.eventCount();
    for (uint8_t i = 0; i < n; ++i) _tracker.event(_core.event(i));
    _tracker.end(s);
}

void LinkServer::fillStatus(StatusSnapshot &s) const
{
    const TrackedState &t = _tracker.state();
    s.changeSeq = _tracker.changeSeq();
    s.flags = t.flags;
    s.selected = t.selected;
    s.active = t.active;
    s.eventCount = t.eventCount;
    s.lastAlertEventId = t.lastAlertEventId;
}

void LinkServer::handle(const NodeIdentity &node, uint8_t heartbeat, const uint8_t *request, size_t len,
                        uint32_t nowMs, FrameSink sink, void *context)
{
    if (len >= 2) {
        switch (request[0]) {
        case static_cast<uint8_t>(Op::Command):
            handleCommand(request, len, sink, context);
            return;
        case static_cast<uint8_t>(Op::GetChanged):
            handleGetChanged(request, len, nowMs, sink, context);
            return;
        case static_cast<uint8_t>(Op::GetText):
            handleGetText(request, len, sink, context);
            return;
        default:
            break;
        }
    }
    uint8_t reply[kMaxFrame];
    sink(reply, dispatch(node, heartbeat, request, len, reply), context);
}

void LinkServer::handleCommand(const uint8_t *request, size_t len, FrameSink sink, void *context)
{
    const uint8_t reqId = request[1];
    Command c;
    if (!decodeCommand(request, len, c)) {
        emitError(reqId, LinkStatus::BadLength, sink, context);
        return;
    }
    size_t expected = 0;
    if (!commandLength(c.commandType, expected)) {
        emitResult(reqId, c.commandType, CommandResult::Unsupported, sink, context);
        return;
    }
    if (len != expected) {
        emitError(reqId, LinkStatus::BadLength, sink, context);
        return;
    }
    ++_counters.commands;

    LayerTimeCommand cmd;
    cmd.type = static_cast<CommandType>(c.commandType);
    CommandResult result = CommandResult::Ok;
    switch (cmd.type) {
    case CommandType::ReconStart:
        // 1 (All) to 16 (GoogleTag). None is ReconStop's job and
        // EarlyWarning is SetEarlyWarning's.
        if (c.argument < static_cast<uint8_t>(ReconTarget::All) ||
            c.argument > static_cast<uint8_t>(ReconTarget::GoogleTag)) {
            result = CommandResult::InvalidArgument;
            break;
        }
        cmd.reconTarget.target = static_cast<ReconTarget>(c.argument);
        result = _core.execute(cmd); // reaches the radios: not under the lock
        break;
    case CommandType::ReconStop:
        result = _core.execute(cmd);
        break;
    case CommandType::SetEarlyWarning:
        if (c.argument > 1) {
            result = CommandResult::InvalidArgument;
            break;
        }
        cmd.setting.enabled = c.argument == 1;
        // Changes the setting only; the platform applies it to the radios.
        result = _core.execute(cmd);
        break;
    case CommandType::SetSleepMode:
        if (c.argument > 1) {
            result = CommandResult::InvalidArgument;
            break;
        }
        cmd.setting.enabled = c.argument == 1;
        lock();
        result = _core.execute(cmd);
        unlock();
        break;
    case CommandType::ReconClearEvents:
    case CommandType::ReconAcknowledgeAlert:
        lock();
        result = _core.execute(cmd);
        unlock();
        break;
    default:
        result = CommandResult::Unsupported; // not reached: commandLength() filters
        break;
    }
    _counters.lastCommandType = c.commandType;
    _counters.lastCommandArgument = c.argument;
    _counters.lastCommandResult = static_cast<uint8_t>(result);
    emitResult(reqId, c.commandType, result, sink, context);
}

void LinkServer::handleGetChanged(const uint8_t *request, size_t len, uint32_t nowMs, FrameSink sink,
                                  void *context)
{
    GetChanged g;
    if (!decodeGetChanged(request, len, g)) {
        emitError(request[1], LinkStatus::BadLength, sink, context);
        return;
    }
    ++_counters.getChanged;
    lock();
    // The pass and the summaries come from the same moment, so END's
    // changeSeq describes exactly what was sent.
    refreshLocked();
    uint8_t sent = 0;
    const uint8_t n = _core.eventCount();
    for (uint8_t i = 0; i < n && i < _tracker.heldCount(); ++i) {
        if (_tracker.heldSeq(i) <= g.sinceChangeSeq) continue;
        const MonitorEvent &e = _core.event(i);
        EventSummary s;
        s.reqId = g.reqId;
        s.eventId = e.eventId;
        s.detector = static_cast<uint8_t>(e.detector);
        s.confidence = static_cast<uint8_t>(e.confidence);
        s.sourceKind = static_cast<uint8_t>(e.sourceKind);
        s.band = static_cast<uint8_t>(e.band);
        s.channel = e.channel;
        s.rssi = e.rssi;
        s.count = saturate16(e.count);
        // A radio task may stamp lastSeen after nowMs was read.
        const uint32_t seen = e.lastSeen.uptimeMs;
        s.ageSeconds = saturate16(nowMs > seen ? (nowMs - seen) / 1000 : 0);
        s.flags = static_cast<uint8_t>((e.sourceId[0] != '\0' ? kSummaryHasSourceId : 0) |
                                       (e.detail[0] != '\0' ? kSummaryHasDetail : 0));
        uint8_t frame[kMaxFrame];
        sink(frame, encodeEventSummary(s, frame), context);
        ++sent;
    }
    const bool gap = _tracker.gapSince(g.sinceChangeSeq);
    const uint32_t seq = _tracker.changeSeq();
    unlock();
    _counters.summaries += sent;
    if (gap) ++_counters.gaps;
    emitEnd(g.reqId, sent, gap, seq, sink, context);
}

void LinkServer::handleGetText(const uint8_t *request, size_t len, FrameSink sink, void *context)
{
    GetText g;
    if (!decodeGetText(request, len, g)) {
        emitError(request[1], LinkStatus::BadLength, sink, context);
        return;
    }
    ++_counters.getText;
    // Copied out under the lock, sent after it.
    char text[MonitorEvent::kDetailSize] = {0};
    size_t length = 0;
    lock();
    const uint8_t n = _core.eventCount();
    for (uint8_t i = 0; i < n; ++i) {
        const MonitorEvent &e = _core.event(i);
        if (e.eventId != g.eventId) continue;
        if (g.field == static_cast<uint8_t>(TextField::SourceId)) {
            length = strnlen(e.sourceId, sizeof(e.sourceId));
            memcpy(text, e.sourceId, length);
        } else if (g.field == static_cast<uint8_t>(TextField::Detail)) {
            length = strnlen(e.detail, sizeof(e.detail));
            memcpy(text, e.detail, length);
        }
        break;
    }
    const uint32_t seq = _tracker.changeSeq();
    unlock();

    const uint8_t total = static_cast<uint8_t>((length + kTextMaxChunk - 1) / kTextMaxChunk);
    for (uint8_t i = 0; i < total; ++i) {
        Text t;
        t.reqId = g.reqId;
        t.field = g.field;
        t.index = i;
        t.total = total;
        const size_t offset = static_cast<size_t>(i) * kTextMaxChunk;
        const size_t chunk = length - offset < kTextMaxChunk ? length - offset : kTextMaxChunk;
        t.length = static_cast<uint8_t>(chunk);
        memcpy(t.bytes, text + offset, chunk);
        uint8_t frame[kMaxFrame];
        sink(frame, encodeText(t, frame), context);
    }
    emitEnd(g.reqId, total, false, seq, sink, context);
}

} // namespace link
} // namespace layertime
