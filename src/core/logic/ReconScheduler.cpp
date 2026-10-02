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

#include "ReconScheduler.h"

#include "ReconSelection.h"

namespace layertime {
namespace recon {

// The bodies below are the T-Ultra ReconService's startDetector, poll,
// pollManual, pollEarlyWarning, armEarlyWarningSweep, setEarlyWarningEnabled,
// stop and exitManualMode as they stood at Increment 1 (the reference), with
// each radio call replaced by the port call that does the same thing. The
// order of radio calls within each transition is kept, and so is every point
// at which the reference read the clock for itself. The channel plan,
// SweepMode::FullPass, and the random rest are LayerWand additions
// (2026-10-02); with the reference plan, Timed mode, and no random source,
// every call and every timestamp is as it was.

ReconScheduler::ReconScheduler(ReconRadio &radio, Clock clock) : _radio(radio), _clock(clock)
{
    setChannelPlan(nullptr, 0);
}

bool ReconScheduler::setChannelPlan(const uint8_t *channels, uint8_t count)
{
    if (channels == nullptr || count == 0) {
        _planCount = 0;
        for (uint8_t c = kFirstChannel; c <= kLastChannel; ++c) _plan[_planCount++] = c;
    } else {
        if (count > kMaxPlanChannels) return false;
        for (uint8_t i = 0; i < count; ++i) _plan[i] = channels[i];
        _planCount = count;
    }
    _planIndex = 0;
    _wifiChannel = _plan[0];
    return true;
}

uint32_t ReconScheduler::jitter(uint32_t maxMs)
{
    return _random == nullptr ? 0 : _random() % (maxMs + 1);
}

void ReconScheduler::startWifi()
{
    // The reference's startWifiMonitoring reset the hop cursor to channel 1
    // and restarted the hop clock as part of bringing the radio up; the
    // cursor and the clock live here, so the reset is done here. The radio
    // starts on the plan's first channel (1 in the reference plan).
    _radio.startWifiMonitoring(_plan[0]);
    _planIndex = 0;
    _wifiChannel = _plan[0];
    _lastChannelHopMs = _clock();
    _passStartMs = _lastChannelHopMs;
    // FullPass ALL: the last channel of this pass dwells a little longer, so
    // two LayerWands cannot stay locked in step.
    const bool mixedManual = _status.monitoring && needsWifi(_status.selected) && needsBle(_status.selected);
    _lastChannelExtraMs = _mode == SweepMode::FullPass && mixedManual ? jitter(kPassJitterMaxMs) : 0;
}

bool ReconScheduler::hopIfDue(uint32_t now, bool stopAtPassEnd)
{
    const bool onLast = _planIndex + 1 >= _planCount;
    const uint32_t dwell = kChannelHopMs + (onLast ? _lastChannelExtraMs : 0);
    if (now - _lastChannelHopMs < dwell) return false;
    _lastChannelHopMs = now;
    if (onLast) {
        _lastPassMs = now - _passStartMs;
        _passStartMs = now;
        ++_passes;
        if (stopAtPassEnd) return true;
        _planIndex = 0;
    } else {
        ++_planIndex;
    }
    _wifiChannel = _plan[_planIndex];
    _radio.setWifiChannel(_wifiChannel);
    return false;
}

void ReconScheduler::start(ReconTarget target)
{
    stop();
    _status.selected = target;
    _status.active = target;
    _status.monitoring = target != ReconTarget::None;
    if (!_status.monitoring) return;

    _allBleBursting = false;
    if (needsWifi(target)) {
        // Both radios needed (ALL, or a group mixing Wi-Fi and BLE members):
        // start on Wi-Fi and let pollManual alternate into BLE bursts.
        startWifi();
        _lastBleCycleMs = _clock();
        if (target == ReconTarget::All) _status.active = ReconTarget::Deauth;
    } else {
        _radio.startBleScan(target, kBleScanMs);
        _lastBleCycleMs = _clock();
    }
}

void ReconScheduler::poll(uint32_t now)
{
    if (_status.monitoring) {
        pollManual(now);
        return;
    }
    pollEarlyWarning(now);
}

void ReconScheduler::pollManual(uint32_t now)
{
    const bool mixedRadios = needsWifi(_status.selected) && needsBle(_status.selected);
    if (mixedRadios) {
        const bool bleBusy = _radio.bleScanning();

        if (_allBleBursting) {
            if (bleBusy) return;
            // BLE burst finished - resume the Wi-Fi sweep.
            startWifi();
            if (_status.selected == ReconTarget::All) _status.active = ReconTarget::Deauth;
            _allBleBursting = false;
            _lastBleCycleMs = now;
            return;
        }

        if (_mode == SweepMode::FullPass) {
            // A full pass, then BLE.
            if (hopIfDue(now, true)) {
                _radio.stopWifiMonitoring();
                if (_status.selected == ReconTarget::All) _status.active = ReconTarget::Flock;
                _radio.startBleScan(_status.selected, kBleScanMs);
                _allBleBursting = true;
            }
            return;
        }
        if (now - _lastBleCycleMs >= kBleCycleMs) {
            _radio.stopWifiMonitoring();
            if (_status.selected == ReconTarget::All) _status.active = ReconTarget::Flock;
            _radio.startBleScan(_status.selected, kBleScanMs);
            _allBleBursting = true;
            return;
        }

        hopIfDue(now, false);
        return;
    }

    if (needsBle(_status.selected)) {
        const bool bleBusy = _radio.bleScanning();
        if (!bleBusy && now - _lastBleCycleMs >= kBleOnlyRescanMs) {
            _radio.startBleScan(_status.selected, kBleScanMs);
            _lastBleCycleMs = now;
        }
        return;
    }

    hopIfDue(now, false);
}

void ReconScheduler::setEarlyWarningEnabled(bool enabled)
{
    if (enabled == _earlyWarningEnabled) {
        return;
    }
    _earlyWarningEnabled = enabled;
    _status.earlyWarningEnabled = enabled;

    if (!enabled) {
        if (!_status.monitoring) {
            // No manual session running, so any radio activity right now
            // belongs to the background scheduler - tear it down.
            _radio.stopWifiMonitoring();
            _radio.stopBleScan();
        }
        _earlyWarningSweeping = false;
        _status.earlyWarningResting = false;
        return;
    }

    if (!_status.monitoring) {
        armEarlyWarningSweep(_clock());
    }
    // If a manual session is active, stopManual() will arm the sweep once
    // that session ends.
}

void ReconScheduler::armEarlyWarningSweep(uint32_t now)
{
    startWifi();
    _earlyWarningSweeping = true;
    _status.earlyWarningResting = false;
    _earlyWarningPhaseStartMs = now;
}

void ReconScheduler::pollEarlyWarning(uint32_t now)
{
    if (!_earlyWarningEnabled) return;

    if (_earlyWarningSweeping) {
        // Timed: a kEarlyWarningActiveMs sweep. FullPass: one full pass.
        const bool sweepDone = _mode == SweepMode::FullPass ? hopIfDue(now, true)
                                                            : now - _earlyWarningPhaseStartMs >= kEarlyWarningActiveMs;
        if (sweepDone) {
            // Sweep window done - a short BLE burst before resting.
            _radio.stopWifiMonitoring();
            _radio.startBleScan(ReconTarget::EarlyWarning, kBleScanMs);
            _earlyWarningSweeping = false;
            _earlyWarningPhaseStartMs = now;
            return;
        }
        if (_mode == SweepMode::Timed) hopIfDue(now, false);
        return;
    }

    if (_radio.bleScanning()) {
        return; // Let the BLE burst finish; adverts stream in meanwhile.
    }

    if (!_status.earlyWarningResting) {
        // BLE burst just finished - begin the rest window (both radios idle).
        _status.earlyWarningResting = true;
        _earlyWarningPhaseStartMs = now;
        _restJitterMs = jitter(kRestJitterMaxMs);
        return;
    }

    if (now - _earlyWarningPhaseStartMs >= kEarlyWarningRestMs + _restJitterMs) {
        armEarlyWarningSweep(now);
    }
}

void ReconScheduler::stop()
{
    _radio.stopWifiMonitoring();
    _radio.stopBleScan();
    _status.monitoring = false;
    _status.selected = ReconTarget::None;
    _status.active = ReconTarget::None;
    _allBleBursting = false;
    _earlyWarningSweeping = false;
    _status.earlyWarningResting = false;
}

void ReconScheduler::stopManual()
{
    stop();
    if (_earlyWarningEnabled) {
        armEarlyWarningSweep(_clock());
    }
}

} // namespace recon
} // namespace layertime
