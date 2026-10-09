// LayerTime - counter-intrusion and resilient-communications firmware
// for the LilyGo T-Watch S3 Plus.
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

// Recon acquisition on the S3 Plus: Wi-Fi promiscuous capture, NimBLE
// scanning, and the radio schedule (manual sweeps and the early-warning duty
// cycle). An S3 Plus copy of the T-Ultra's ReconService
// (src/platform/twatch_ultra/services/ReconService), per the S3 Plus port
// rule. Both watches are ESP32-S3, so the radio code carries over as is.
//
// Detector behaviour is unchanged: classification goes through core
// (ReconClassification and ReconSelection, which run the LayerTime-Sensors
// classifiers), and the schedule's timings are the Ultra's (650 ms channel
// hop over 1 to 11, 1.8 s BLE scan every 12 s, early warning 10 s active and
// 60 s rest).
//
// Intentional differences from the Ultra's copy:
//   * The two radio callbacks classify under the S3 Plus event-log lock
//     (EventLock.h). The Ultra has no lock.
//   * The unused LilyGoLib and LVGL includes are gone, and so are the name
//     forwarders the Ultra's screens call; the S3 Plus screens call the core's
//     ReconSelection directly.

#include <stddef.h>
#include <stdint.h>

#include "core/logic/ReconCandidate.h"
#include "core/logic/ReconClassification.h"
#include "core/model/MonitorEvent.h"

#include "EventLock.h"

class NimBLEAdvertisedDevice;

namespace layertime {
namespace twatch_s3plus {

struct ReconStatus {
    ReconTarget detector = ReconTarget::None;
    ReconTarget activeDetector = ReconTarget::None;
    bool monitoring = false;
    bool earlyWarningEnabled = false;
    // True while resting between sweeps (both radios idle to save power).
    bool earlyWarningResting = false;
};

class S3PlusReconService {
public:
    // The lock must exist (EventLock::begin) before any radio starts.
    explicit S3PlusReconService(EventLock &lock) : _lock(&lock) {}

    void begin();
    // Radio scheduling. Never call with the event-log lock held.
    void poll();
    void startDetector(ReconTarget detector);
    void stop();
    // Leaves manual monitoring and, if early warning is on, resumes it.
    void exitManualMode();
    // Forgets the Wi-Fi detectors' own tracking state. Call with the
    // event-log lock held.
    void resetDetectorState();
    const ReconStatus &status() const { return _status; }

    void setEarlyWarningEnabled(bool enabled);

    // From the NimBLE scan callback, for each device found.
    void handleBleAdvertisement(const NimBLEAdvertisedDevice *device);

    // Where each candidate goes, stamped with radio, band, and millis().
    // Called on the Wi-Fi or NimBLE task with the event-log lock held.
    void setCandidateSink(layertime::recon::CandidateSink sink, void *context);

private:
    static void promiscuousThunk(void *buf, int type);
    void onPromiscuousPacket(void *buf, int type);
    void startWifiMonitoring();
    void stopWifiMonitoring();
    void startBleScan(ReconTarget detector, uint32_t durationMs);
    void deliver(const layertime::recon::Candidate &candidate, layertime::SourceKind kind,
                 layertime::Band band);
    bool wants(ReconTarget detector) const;
    static bool needsWifi(ReconTarget detector);
    static bool needsBle(ReconTarget detector);
    static bool wantsThunk(ReconTarget detector, const void *self);
    static void wifiCandidateThunk(const layertime::recon::Candidate &candidate, void *self);
    static void bleCandidateThunk(const layertime::recon::Candidate &candidate, void *self);

    void pollManual(uint32_t now);
    void pollEarlyWarning(uint32_t now);
    void armEarlyWarningSweep(uint32_t now);

    EventLock *_lock;
    ReconStatus _status;
    bool _bleInitialized = false;
    uint32_t _lastChannelHopMs = 0;
    uint32_t _lastBleCycleMs = 0;
    layertime::recon::WifiFrameClassifier _wifiClassifier;
    static S3PlusReconService *_activeInstance;
    // One physical radio, so one channel cursor for manual and background.
    uint8_t _wifiChannel = 1;

    bool _allBleBursting = false;

    bool _earlyWarningEnabled = false;
    bool _earlyWarningSweeping = false;
    uint32_t _earlyWarningPhaseStartMs = 0;
    static constexpr uint32_t kEarlyWarningActiveMs = 10000;
    static constexpr uint32_t kEarlyWarningRestMs = 60000;

    ReconTarget _currentBleScanDetector = ReconTarget::None;

    layertime::recon::CandidateSink _candidateSink = nullptr;
    void *_candidateSinkContext = nullptr;
};

} // namespace twatch_s3plus
} // namespace layertime
