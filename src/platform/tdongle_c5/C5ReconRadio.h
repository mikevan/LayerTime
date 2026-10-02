// LayerTime - passive early-warning firmware for the LILYGO T-Dongle-C5.
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

// Recon acquisition on the T-Dongle-C5: the Wi-Fi promiscuous capture and
// the NimBLE passive scan, handed to the core classifiers
// (src/core/logic/WifiFrameClassifier, BleAdvertClassifier), plus the six
// radio primitives core's ReconScheduler drives (src/core/ports/ReconRadio).
//
// This is the C5's Recon acquisition, ported in Slice 1 Increment 2A from
// the T-Watch Ultra's ReconService (frozen reference code) and deliberately
// a close copy of its acquisition half: the same NimBLE unpacking, the same
// driver call order, the same settle delays, the same candidate stamping.
// Two things differ because the radio does:
//
//   * The C5's Wi-Fi is dual band. The existing Recon model hops channels
//     1 to 11 on 2.4 GHz, so the band mode is pinned to 2.4 GHz before the
//     capture starts. 5 GHz is not observed in Increment 2A.
//   * The band a frame is stamped with is read from its channel rather than
//     assumed, since the T-Ultra's assumption does not hold here.
//
// The BLE address byte-order KNOWN_DEFECT (test_recon:
// ble_oui_path_compares_the_low_three_bytes_KNOWN_DEFECT) is carried over
// unchanged, on purpose: the C5 runs the current detectors as they are.
//
// Every stage the radios pass through is recorded in the C5StageLog when one
// is attached, and counted whether or not one is.

#include <stddef.h>
#include <stdint.h>

#include <atomic>

#include "../../core/logic/ReconCandidate.h"
#include "../../core/logic/ReconScheduler.h"
#include "../../core/logic/WifiFrameClassifier.h"
#include "../../core/model/MonitorEvent.h"
#include "../../core/ports/ReconRadio.h"

class NimBLEAdvertisedDevice;

namespace layertime {
namespace tdongle_c5 {

class C5StageLog;

class C5ReconRadio : public recon::ReconRadio {
public:
    struct Counters {
        uint32_t wifiStarts = 0;
        uint32_t wifiStops = 0;
        uint32_t hops = 0;
        uint32_t bleScanStarts = 0;
        uint32_t bleScanEnds = 0;
        uint32_t frames = 0;
        uint32_t framesByChannel[15] = {0}; // index = channel, 0 unused
        uint32_t adverts = 0;
        uint32_t candidates = 0;
        uint32_t candidatesByDetector[18] = {0}; // index = ReconTarget value
        uint32_t sinkMaxUs = 0;                  // longest core sink call
    };

    // Brings the Wi-Fi driver up in station mode, disconnected, pinned to
    // 2.4 GHz. NimBLE starts on the first BLE scan, as on the T-Ultra.
    void begin();

    // The scheduler whose state gates the classifiers (ReconSelection::wants).
    void bindScheduler(const recon::ReconScheduler *scheduler) { _scheduler = scheduler; }
    void setStageLog(C5StageLog *log) { _stage = log; }
    void setCandidateSink(recon::CandidateSink sink, void *context);
    // Forgets the Wi-Fi detectors' own tracking state. Acquisition-side only.
    // Safe from any task: it only asks for the reset, and the Wi-Fi task
    // performs it before it classifies the next frame, so the classifier is
    // only ever touched by the task that runs it (Increment 2B: a
    // ReconClearEvents from the watch arrives on the loop task while the
    // Wi-Fi task may be inside classify()).
    void resetDetectorState();

    // ReconRadio.
    void startWifiMonitoring() override;
    void stopWifiMonitoring() override;
    void setWifiChannel(uint8_t channel) override;
    void startBleScan(ReconTarget detector, uint32_t durationMs) override;
    void stopBleScan() override;
    bool bleScanning() const override;

    // NimBLE callbacks route here.
    void handleBleAdvertisement(const NimBLEAdvertisedDevice *device);
    void handleBleScanEnd();

    const Counters &counters() const { return _counters; }
    void clearCounters() { _counters = Counters{}; }

private:
    static void promiscuousThunk(void *buf, int type);
    void onPromiscuousPacket(void *buf, int type);
    void deliver(const recon::Candidate &candidate, SourceKind kind, Band band);
    bool wants(ReconTarget detector) const;
    static bool wantsThunk(ReconTarget detector, const void *self);
    static void wifiCandidateThunk(const recon::Candidate &candidate, void *self);
    static void bleCandidateThunk(const recon::Candidate &candidate, void *self);

    const recon::ReconScheduler *_scheduler = nullptr;
    C5StageLog *_stage = nullptr;
    recon::WifiFrameClassifier _wifiClassifier;
    // Set by resetDetectorState(), taken by the Wi-Fi task (onPromiscuousPacket).
    std::atomic<bool> _resetRequested{false};
    bool _bleInitialized = false;
    ReconTarget _currentBleScanDetector = ReconTarget::None;
    recon::CandidateSink _candidateSink = nullptr;
    void *_candidateSinkContext = nullptr;
    // Which channel the capture is on, for the first-frame-after-hop record.
    uint8_t _channel = 0;
    bool _firstFrameOnChannel = false;
    bool _firstAdvertInScan = false;
    Counters _counters;
    static C5ReconRadio *_activeInstance;
};

} // namespace tdongle_c5
} // namespace layertime
