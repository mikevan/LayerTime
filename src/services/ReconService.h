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

#include "../core/logic/WifiFrameClassifier.h"
#include "../core/model/MonitorEvent.h"

// How much a match is worth trusting, and what Recon is pointed at.
//
// Since Phase 0 Step 3c these are the core model's types under their
// existing T-Ultra names. The enumerators and their values are identical
// (test_core_model checks every one), so every existing use compiles and
// behaves exactly as before. Group and detector semantics are documented in
// src/core/logic/ReconSelection.h.
using SignalConfidence = layertime::Confidence;
using ReconDetector = layertime::ReconTarget;

struct ReconDetection {
    char category[14] = {0};
    char detail[40] = {0};
    char address[19] = {0};
    int8_t rssi = 0;
    uint8_t channel = 0;
    uint32_t lastSeenMs = 0;
    // How many times this unique (category, address) signal has been seen
    // since the log was last cleared. Wide on purpose - a real attack can
    // mean thousands of repeats of the same deauth frame.
    uint32_t encounterCount = 1;
    // How much this particular match is worth trusting. Set per call site,
    // and per signature row where the table knows better than the detector
    // does - Meta on its own SIG-assigned 0xFD5F is High, the two unsourced
    // Meta UUIDs are Low, and both report as META.
    SignalConfidence confidence = SignalConfidence::High;
};

struct ReconStatus {
    // This is a persistent session log, not a live scan snapshot - it is
    // only cleared by an explicit user action (ReconScreen's CLEAR LOG
    // button), never automatically by stop()/start(). Sized generously so
    // a long monitoring session doesn't evict genuine unique threats.
    static constexpr size_t MAX_DETECTIONS = 40;
    ReconDetector detector = ReconDetector::None;
    ReconDetector activeDetector = ReconDetector::None;
    ReconDetection detections[MAX_DETECTIONS];
    size_t detectionCount = 0;
    uint32_t eventSerial = 0;
    bool monitoring = false;
    bool alertPending = false;

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
    void clearDetections();
    void acknowledgeAlert();
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

    // While true, new detections are still logged and counted normally,
    // but do not set alertPending - so ReconScreen's popup never opens and
    // poll() never vibrates or wakes the display for them. Meant for
    // overnight use; acknowledgeAlert()/clearDetections() are unaffected.
    void setSleepModeEnabled(bool enabled) { _sleepModeEnabled = enabled; }

    // Called by the NimBLEScanCallbacks handler for each device found during
    // an async scan (manual or background).
    void handleBleAdvertisement(const NimBLEAdvertisedDevice *device);

    // Optional sink invoked once for every genuinely new (non-duplicate)
    // detection - i.e. not on repeat RSSI updates of something already
    // seen. Used by WatchApp to append detections to the SD log when that
    // setting is enabled; ReconService itself has no file I/O.
    using DetectionSink = void (*)(const ReconDetection &detection, void *userData);
    void setDetectionSink(DetectionSink sink, void *userData);

private:
    static void promiscuousThunk(void *buf, int type);
    void onPromiscuousPacket(void *buf, int type);
    void startWifiMonitoring();
    void stopWifiMonitoring();
    void startBleScan(ReconDetector detector, uint32_t durationMs);
    // confidence is deliberately not defaulted - every call site states its
    // own grade, so a new detector can't silently inherit High.
    void addDetection(ReconDetector detector, const char *detail, const char *address,
                      int8_t rssi, SignalConfidence confidence, uint8_t channel = 0);
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
    static void candidateThunk(const layertime::recon::Candidate &candidate, void *self);

    void pollManual(uint32_t now);
    void pollEarlyWarning(uint32_t now);
    void armEarlyWarningSweep(uint32_t now);
    static bool isBackgroundWifiDetector(ReconDetector detector);

    ReconStatus _status;
    bool _bleInitialized = false;
    uint32_t _lastChannelHopMs = 0;
    uint32_t _lastBleCycleMs = 0;
    uint32_t _alertActuatedSerial = 0;
    bool _sleepModeEnabled = false;
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

    DetectionSink _detectionSink = nullptr;
    void *_detectionSinkUserData = nullptr;
};
