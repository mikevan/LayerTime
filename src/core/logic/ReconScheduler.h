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

#pragma once

// Recon radio scheduling: which radio is listening, on which channel, and
// when it changes. Expressed in core in Slice 1 Increment 2A from the
// T-Watch Ultra's ReconService (frozen reference code since then), control
// flow and timing unchanged, so the T-Dongle-C5 runs the same schedule.
//
// The three schedules, exactly as the reference ran them:
//
//   * Manual, mixed radios (ALL, or a group with Wi-Fi and BLE members):
//     hop the 2.4 GHz channels 1 to 11 every kChannelHopMs; every
//     kBleCycleMs stop Wi-Fi and run one kBleScanMs BLE scan; when it has
//     finished, start Wi-Fi again on channel 1. ALL shows Deauth while on
//     Wi-Fi and Flock while on BLE as its active detector.
//   * Manual, BLE only: one kBleScanMs scan, restarted whenever the previous
//     one has finished and kBleOnlyRescanMs have passed since it started.
//   * Manual, Wi-Fi only: hop every kChannelHopMs.
//   * Early warning (no manual session): sweep Wi-Fi for
//     kEarlyWarningActiveMs, run one kBleScanMs BLE scan, then rest with both
//     radios idle for kEarlyWarningRestMs after the scan has finished, and
//     repeat. Starting a manual session pauses it; stopManual() resumes it.
//
// The LayerWand schedule (Michael, 2026-10-02): the same four schedules,
// with two differences a platform switches on.
//
//   * A channel plan. The reference plan is 2.4 GHz channels 1 to 11. A
//     platform may supply its own: the LayerWand's is every 5 GHz channel
//     its radio accepts, lowest to highest, then every 2.4 GHz channel,
//     lowest to highest.
//   * SweepMode::FullPass. A pass is every channel in the plan once, in
//     order, kChannelHopMs each. Mixed manual (ALL) runs a full pass, then
//     one kBleScanMs BLE scan, then the next pass. Early warning runs one
//     full pass, then one BLE scan, then rests. Wi-Fi-only and BLE-only
//     manual sessions behave as in Timed mode. SweepMode::Timed is the
//     reference, unchanged, and the default.
//   * A random source (optional). Two LayerWands on the same schedule stay
//     locked in step, and with a bad offset one's BLE scan keeps landing
//     while the other is busy on Wi-Fi, so one never hears the other
//     (seen on hardware 2026-10-02; moving one wand's boot by 30 s cured
//     it). With a random source, each early-warning rest lasts an extra
//     0 to kRestJitterMaxMs, and in FullPass ALL the last channel of each
//     pass dwells an extra 0 to kPassJitterMaxMs. Without one, nothing
//     changes.
//
// Time is the platform's millisecond uptime. poll() takes the loop's own
// reading of it, and the clock function supplies the readings the old code
// took for itself part-way through a transition, so the timestamps land
// where they always did.
//
// This class owns no radio and no driver: it drives a ReconRadio port and
// reports its state as an AcquisitionStatus. The C5's C5ReconRadio is the
// port's implementation.

#include <stdint.h>

#include "../model/MonitorEvent.h"
#include "../ports/MonitorSource.h"
#include "../ports/ReconRadio.h"

namespace layertime {
namespace recon {

enum class SweepMode : uint8_t { Timed, FullPass };

class ReconScheduler {
public:
    static constexpr uint32_t kChannelHopMs = 650;
    static constexpr uint32_t kBleCycleMs = 12000;
    static constexpr uint32_t kBleScanMs = 1800;
    static constexpr uint32_t kBleOnlyRescanMs = 5000;
    static constexpr uint32_t kEarlyWarningActiveMs = 10000;
    static constexpr uint32_t kEarlyWarningRestMs = 60000;
    static constexpr uint8_t kFirstChannel = 1;
    static constexpr uint8_t kLastChannel = 11;
    static constexpr uint8_t kMaxPlanChannels = 48;
    static constexpr uint32_t kRestJitterMaxMs = 10000;
    static constexpr uint32_t kPassJitterMaxMs = 2000;

    using Clock = uint32_t (*)();
    // Any 32-bit random value; the platform's hardware generator on a device,
    // a scripted sequence in the tests.
    using Random = uint32_t (*)();

    ReconScheduler(ReconRadio &radio, Clock clock);

    // Set before Recon starts. channels are copied. A null or empty plan
    // restores the reference plan (1 to 11). A plan longer than
    // kMaxPlanChannels is refused (returns false) and the current plan kept.
    bool setChannelPlan(const uint8_t *channels, uint8_t count);
    void setSweepMode(SweepMode mode) { _mode = mode; }
    void setRandom(Random random) { _random = random; }

    uint8_t planCount() const { return _planCount; }
    uint8_t planChannel(uint8_t index) const { return index < _planCount ? _plan[index] : 0; }
    SweepMode sweepMode() const { return _mode; }
    // Passes completed (every channel in the plan once), and how long the
    // most recent one took, from Wi-Fi start or the previous pass's end.
    uint32_t passes() const { return _passes; }
    uint32_t lastPassMs() const { return _lastPassMs; }
    // The extra rest drawn for the current or most recent early-warning rest.
    uint32_t restJitterMs() const { return _restJitterMs; }

    // Starts manual monitoring for a selection. None stops it. Any running
    // schedule, manual or early warning, is torn down first.
    void start(ReconTarget target);
    // Tears everything down: both radios idle, no selection, the
    // early-warning sweep not running (its enabled flag is kept).
    void stop();
    // Leaves manual monitoring; the early-warning sweep resumes at once if
    // it is enabled.
    void stopManual();
    void setEarlyWarningEnabled(bool enabled);
    // Once per application tick, with the tick's reading of the clock.
    void poll(uint32_t nowMs);

    const AcquisitionStatus &status() const { return _status; }
    // True while the early-warning Wi-Fi sweep is the reason the radio is
    // listening: the classifiers are gated on it (ReconSelection::wants).
    bool sweepingForBackground() const { return _earlyWarningEnabled && _earlyWarningSweeping; }
    // The channel the hop cursor is on.
    uint8_t wifiChannel() const { return _wifiChannel; }

private:
    void pollManual(uint32_t now);
    void pollEarlyWarning(uint32_t now);
    void armEarlyWarningSweep(uint32_t now);
    void startWifi();
    // Hops when the dwell is up. At the end of a pass it either wraps to the
    // plan's first channel, or, with stopAtPassEnd, stays put and returns
    // true so the caller can go to BLE.
    bool hopIfDue(uint32_t now, bool stopAtPassEnd);
    uint32_t jitter(uint32_t maxMs);

    ReconRadio &_radio;
    Clock _clock;
    AcquisitionStatus _status;
    uint32_t _lastChannelHopMs = 0;
    uint32_t _lastBleCycleMs = 0;
    // Shared Wi-Fi channel-hop cursor, used by manual Wi-Fi detectors and the
    // background sweep alike (a single physical radio, so one cursor).
    uint8_t _wifiChannel = kFirstChannel;
    uint8_t _plan[kMaxPlanChannels];
    uint8_t _planCount = 0;
    uint8_t _planIndex = 0;
    SweepMode _mode = SweepMode::Timed;
    Random _random = nullptr;
    uint32_t _passStartMs = 0;
    uint32_t _lastPassMs = 0;
    uint32_t _passes = 0;
    uint32_t _lastChannelExtraMs = 0;
    uint32_t _restJitterMs = 0;
    // Manual mixed-radio mode's own Wi-Fi/BLE alternation bookkeeping.
    bool _allBleBursting = false;
    // Background early-warning scheduler.
    bool _earlyWarningEnabled = false;
    bool _earlyWarningSweeping = false;
    uint32_t _earlyWarningPhaseStartMs = 0;
};

} // namespace recon
} // namespace layertime
