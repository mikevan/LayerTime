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

#include <stddef.h>
#include <stdint.h>

class NimBLEAdvertisedDevice;

#include "core/logic/WifiFrameClassifier.h"
#include "core/model/MonitorEvent.h"

// How much a match is worth trusting, and what Recon is pointed at.
//
// Since Phase 0 Step 3c these are the core model's types under their
// existing T-Ultra names. The enumerators and their values are identical
// (test_core_model checks every one), so every existing use compiles and
// behaves exactly as before. Group and detector semantics are documented in
// src/core/logic/ReconSelection.h.
using SignalConfidence = layertime::Confidence;
using ReconDetector = layertime::ReconTarget;

// What the radios are doing. Since Phase 0 Step 4 the detection log, its
// event serial and the pending alert live in core
// (src/core/logic/MonitorEventLog), not here: this service only acquires.
struct ReconStatus {
    ReconDetector detector = ReconDetector::None;
    ReconDetector activeDetector = ReconDetector::None;
    bool monitoring = false;

    // Background early-warning scheduler state (independent of manual
    // monitoring above).
    bool earlyWarningEnabled = false;
    // True while resting between sweeps (Wi-Fi/BLE radios idle to save power).
    bool earlyWarningResting = false;
};

class ReconService {
public:
    void begin();
    void poll();
    void startDetector(ReconDetector detector);
    void stop();
    // Leaves manual monitoring and, if the background early-warning sweep is
    // enabled, resumes it. This is what the UI should call when backing out
    // of a manual detector/leaving the Recon screen (not stop() directly).
    void exitManualMode();
    void stopActivity() { exitManualMode(); } // Existing WatchApp compatibility.
    // Forgets the Wi-Fi detectors' own tracking state (deauth burst
    // counters, per-BSSID SSID sets). Acquisition-side only: the event
    // history is core's, cleared by the ReconClearEvents command.
    void resetDetectorState();
    const ReconStatus &status() const { return _status; }
    static const char *detectorName(ReconDetector detector);
    static const char *confidenceLabel(SignalConfidence confidence);
    // Abbreviated label for the watch face's 110px-wide THREATS block, which
    // cannot fit the full group names. Menus and titles use detectorName().
    static const char *detectorShortName(ReconDetector detector);
    // Members of a group sweep, for the Recon menu's sub-pages. Returns
    // nullptr and count 0 for anything that isn't a group.
    static const ReconDetector *groupMembers(ReconDetector group, size_t &count);

    // Background low-power early-warning sweep: Deauth/Pwnagotchi/Pineapple/
    // MultiSSID on a duty-cycled Wi-Fi sweep, plus Flipper/Meta on a duty-
    // cycled BLE scan. Runs regardless of which screen is active, and is
    // automatically paused while a manual detector is running.
    void setEarlyWarningEnabled(bool enabled);

    // Called by the NimBLEScanCallbacks handler for each device found during
    // an async scan (manual or background).
    void handleBleAdvertisement(const NimBLEAdvertisedDevice *device);

    // Where each candidate a classifier finds is delivered, stamped with the
    // radio, band and millis(). Called on the Wi-Fi or NimBLE callback task.
    void setCandidateSink(layertime::recon::CandidateSink sink, void *context);

private:
    static void promiscuousThunk(void *buf, int type);
    void onPromiscuousPacket(void *buf, int type);
    void startWifiMonitoring();
    void stopWifiMonitoring();
    void startBleScan(ReconDetector detector, uint32_t durationMs);
    void deliver(const layertime::recon::Candidate &candidate, layertime::SourceKind kind,
                 layertime::Band band);
    bool wants(ReconDetector detector) const;
    static bool groupContains(ReconDetector group, ReconDetector detector);
    // Which radios a selection needs. A group can need both (COUNTER-INTRUSION
    // is four Wi-Fi detectors plus Flipper over BLE), in which case it runs
    // the same alternating sweep ALL uses rather than picking one radio.
    static bool needsWifi(ReconDetector detector);
    static bool needsBle(ReconDetector detector);
    // Whether an in-flight BLE scan started for `scanDetector` should report
    // a match on `target`.
    static bool bleScanWants(ReconDetector scanDetector, ReconDetector target);
    // Bridges from the core classifiers back into this service: the
    // classifiers ask which detectors are live and hand back candidates.
    static bool wantsThunk(ReconDetector detector, const void *self);
    static void wifiCandidateThunk(const layertime::recon::Candidate &candidate, void *self);
    static void bleCandidateThunk(const layertime::recon::Candidate &candidate, void *self);

    void pollManual(uint32_t now);
    void pollEarlyWarning(uint32_t now);
    void armEarlyWarningSweep(uint32_t now);
    static bool isBackgroundWifiDetector(ReconDetector detector);

    ReconStatus _status;
    bool _bleInitialized = false;
    uint32_t _lastChannelHopMs = 0;
    uint32_t _lastBleCycleMs = 0;
    // Deauth burst and several-SSIDs-per-BSSID tracking, in core since
    // Phase 0 Step 3e.
    layertime::recon::WifiFrameClassifier _wifiClassifier;
    static ReconService *_activeInstance;
    // Shared Wi-Fi channel-hop cursor, used by manual Wi-Fi detectors and the
    // background sweep alike (a single physical radio, so one cursor).
    uint8_t _wifiChannel = 1;

    // Manual "All" mode's own Wi-Fi/BLE alternation bookkeeping.
    bool _allBleBursting = false;

    // Background early-warning scheduler.
    bool _earlyWarningEnabled = false;
    bool _earlyWarningSweeping = false;
    uint32_t _earlyWarningPhaseStartMs = 0;
    static constexpr uint32_t kEarlyWarningActiveMs = 10000;
    static constexpr uint32_t kEarlyWarningRestMs = 60000;

    // Which detector(s) the in-flight async BLE scan is checking for.
    ReconDetector _currentBleScanDetector = ReconDetector::None;

    layertime::recon::CandidateSink _candidateSink = nullptr;
    void *_candidateSinkContext = nullptr;
};
