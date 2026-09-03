#pragma once

#include <stddef.h>
#include <stdint.h>

class NimBLEAdvertisedDevice;

enum class ReconDetector : uint8_t {
    None, All, Deauth, Pwnagotchi, MultiSSID, Flock, Pineapple, AirTag, Flipper, Meta,
    // Not user-selectable from the menu - used internally to scope the
    // background early-warning BLE scan to Flipper + Meta only.
    EarlyWarning
};

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
    struct MultiSsidTracker {
        uint8_t bssid[6] = {0};
        uint16_t hashes[4] = {0};
        uint8_t count = 0;
    };

    static void promiscuousThunk(void *buf, int type);
    void onPromiscuousPacket(void *buf, int type);
    void startWifiMonitoring();
    void stopWifiMonitoring();
    void startBleScan(ReconDetector detector, uint32_t durationMs);
    void addDetection(ReconDetector detector, const char *detail, const char *address,
                      int8_t rssi, uint8_t channel = 0);
    bool wants(ReconDetector detector) const;
    void inspectBeacon(const uint8_t *payload, uint16_t length, int8_t rssi, uint8_t channel);

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
    MultiSsidTracker _multiSsid[8];
    size_t _multiSsidCount = 0;
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
