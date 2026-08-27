#pragma once

#include <stdint.h>

#include "../model/AppSettings.h"
#include "../model/WatchState.h"
#include "../services/BatteryService.h"
#include "../services/ClockService.h"
#include "../services/GpsService.h"
#include "../services/MeshService.h"
#include "../services/MeshtasticService.h"
#include "../services/ReconService.h"
#include "../services/SdCardService.h"
#include "../services/SettingsService.h"
#include "../ui/GpsScreen.h"
#include "../ui/MeshScreen.h"
#include "../ui/MeshtasticScreen.h"
#include "../ui/ReconScreen.h"
#include "../ui/SettingsScreen.h"
#include "../ui/WatchFace.h"

class WatchApp {
public:
    void begin();
    void tick();

private:
    static void settingsRequestedThunk(void *userData);
    static void gpsRequestedThunk(void *userData);
    static void gpsBackThunk(void *userData);
    static void meshRequestedThunk(void *userData);
    static void meshBackThunk(void *userData);
    static void meshtasticRequestedThunk(void *userData);
    static void meshtasticBackThunk(void *userData);
    static void reconRequestedThunk(void *userData);
    static void reconBackThunk(void *userData);
    static void threatsRequestedThunk(void *userData);
    static void settingsBackThunk(void *userData);
    static void settingsChangedThunk(void *userData);
    static void reconDetectionSinkThunk(const ReconDetection &detection, void *userData);
    static void dateTimeSaveThunk(
        int year,
        int month,
        int day,
        int hour,
        int minute,
        void *userData);

    void refreshState();
    void openSettings();
    void openGps();
    void closeGps();
    void openMesh();
    void closeMesh();
    void openMeshtastic();
    void closeMeshtastic();
    void openRecon();
    void openThreatsRecon();
    void closeRecon();
    void closeSettings();
    void settingsChanged();
    void saveDateTime(int year, int month, int day, int hour, int minute);
    void logReconDetection(const ReconDetection &detection);

    WatchState _state;
    AppSettings _settings;

    ClockService _clock;
    BatteryService _battery;
    GpsService _gps;
    MeshService _mesh;
    MeshtasticService _meshtastic;
    ReconService _recon;
    SdCardService _sdCard;
    SettingsService _settingsService;

    WatchFace _face;
    GpsScreen _gpsScreen;
    MeshScreen _meshScreen;
    MeshtasticScreen _meshtasticScreen;
    ReconScreen _reconScreen;
    SettingsScreen _settingsScreen;

    uint32_t _lastRefreshMs = 0;
};
