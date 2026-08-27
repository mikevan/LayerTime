#include "WatchApp.h"

#include <Arduino.h>
#include <LilyGoLib.h>
#include <LV_Helper.h>

namespace {
constexpr uint32_t kDisplayTimeoutMs = 15000;
bool displayBlanked = false;
}

void WatchApp::begin()
{
    Serial.begin(115200);

    instance.begin();
    beginLvglHelper(instance);

    _settingsService.load(_settings);
    _settingsService.apply(_settings);
    _gps.begin(_settings.gpsEnabled);
    _mesh.begin();
    _mesh.setAdvertisingEnabled(_settings.meshAdvertiseEnabled);
    // Mesh radio always starts powered off; the user opts in each session
    // from Settings > MESH.
    _meshtastic.begin();
    _meshtastic.setIdentity(_settings.meshtasticNodeName);
    _meshtastic.setAdvertisingEnabled(_settings.meshtasticAdvertiseEnabled);
    // Meshtastic radio also always starts powered off, same reasoning as
    // Mesh above - and the two are kept mutually exclusive in settingsChanged().
    //_weatherBle.begin();
    _sdCard.begin();
    _recon.begin();
    _recon.setEarlyWarningEnabled(_settings.reconEarlyWarningEnabled);
    _recon.setDetectionSink(reconDetectionSinkThunk, this);

    _face.create();
    _face.setSettingsRequestedCallback(settingsRequestedThunk, this);
    _face.setGpsRequestedCallback(gpsRequestedThunk, this);
    _face.setMeshRequestedCallback(meshRequestedThunk, this);
    _face.setMeshtasticRequestedCallback(meshtasticRequestedThunk, this);
    _face.setReconRequestedCallback(reconRequestedThunk, this);

    // Create every secondary screen before the first render.
    _gpsScreen.create(gpsBackThunk, this);
    _meshScreen.create(&_mesh, meshBackThunk, this);
    _meshtasticScreen.create(&_meshtastic, meshtasticBackThunk, this);
    _reconScreen.create(&_recon, reconBackThunk, this);

    _settingsScreen.create(
        _settings,
        _state,
        _sdCard,
        settingsBackThunk,
        settingsChangedThunk,
        dateTimeSaveThunk,
        this);

    refreshState();
}

void WatchApp::tick()
{
    instance.loop();
    _gps.poll(_state);
    _mesh.poll();
    _meshtastic.poll();
    //_weatherBle.poll(_state);
    _recon.poll();
    lv_timer_handler();

    const uint32_t inactiveMs = lv_display_get_inactive_time(nullptr);
    if (!displayBlanked && inactiveMs >= kDisplayTimeoutMs) {
        instance.setBrightness(0);
        displayBlanked = true;
    } else if (displayBlanked && inactiveMs < kDisplayTimeoutMs) {
        instance.setBrightness(_settings.brightness);
        displayBlanked = false;
    }


    const uint32_t now = millis();
    if (now - _lastRefreshMs >= 250) {
        _lastRefreshMs = now;
        refreshState();
    }

    delay(2);
}

void WatchApp::refreshState()
{
    _clock.update(_state);
    _battery.update(_state);
    _face.render(_state, _settings);
    _gpsScreen.render(_state, _settings);
    _meshScreen.render(_mesh.status());
    _meshtasticScreen.render(_meshtastic.status());
    _reconScreen.render();
}

void WatchApp::settingsRequestedThunk(void *userData)
{
    static_cast<WatchApp *>(userData)->openSettings();
}

void WatchApp::gpsRequestedThunk(void *userData)
{
    static_cast<WatchApp *>(userData)->openGps();
}

void WatchApp::gpsBackThunk(void *userData)
{
    static_cast<WatchApp *>(userData)->closeGps();
}

void WatchApp::meshRequestedThunk(void *userData)
{
    static_cast<WatchApp *>(userData)->openMesh();
}

void WatchApp::meshBackThunk(void *userData)
{
    static_cast<WatchApp *>(userData)->closeMesh();
}

void WatchApp::meshtasticRequestedThunk(void *userData)
{
    static_cast<WatchApp *>(userData)->openMeshtastic();
}

void WatchApp::meshtasticBackThunk(void *userData)
{
    static_cast<WatchApp *>(userData)->closeMeshtastic();
}

void WatchApp::reconRequestedThunk(void *userData)
{
    static_cast<WatchApp *>(userData)->openRecon();
}

void WatchApp::reconBackThunk(void *userData)
{
    static_cast<WatchApp *>(userData)->closeRecon();
}

void WatchApp::settingsBackThunk(void *userData)
{
    static_cast<WatchApp *>(userData)->closeSettings();
}

void WatchApp::settingsChangedThunk(void *userData)
{
    static_cast<WatchApp *>(userData)->settingsChanged();
}

void WatchApp::dateTimeSaveThunk(
    int year,
    int month,
    int day,
    int hour,
    int minute,
    void *userData)
{
    static_cast<WatchApp *>(userData)->saveDateTime(year, month, day, hour, minute);
}

void WatchApp::openSettings()
{
    _settingsScreen.show();
}

void WatchApp::openGps()
{
    _gpsScreen.show(_state, _settings);
}

void WatchApp::closeGps()
{
    lv_screen_load(_face.screen());
}

void WatchApp::openMesh()
{
    _meshScreen.show(_mesh.status());
}

void WatchApp::closeMesh()
{
    lv_screen_load(_face.screen());
}

void WatchApp::openMeshtastic()
{
    _meshtasticScreen.show(_meshtastic.status());
}

void WatchApp::closeMeshtastic()
{
    lv_screen_load(_face.screen());
}

void WatchApp::openRecon()
{
    _reconScreen.show();
}

void WatchApp::closeRecon()
{
    _recon.exitManualMode();
    lv_screen_load(_face.screen());
}

void WatchApp::closeSettings()
{
    lv_screen_load(_face.screen());
}

void WatchApp::settingsChanged()
{
    _settingsService.apply(_settings);
    _gps.setEnabled(_settings.gpsEnabled);
    // MeshCore and Meshtastic share one physical SX1262 radio. The settings
    // screen already keeps meshEnabled/meshtasticEnabled mutually exclusive,
    // but sequence the disable calls first here too, defensively, so we never
    // have both drivers touching the radio at once.
    if (!_settings.meshEnabled) {
        _mesh.setRadioEnabled(false);
    }
    if (!_settings.meshtasticEnabled) {
        _meshtastic.setRadioEnabled(false);
    }
    if (_settings.meshEnabled) {
        _mesh.setRadioEnabled(true);
    }
    if (_settings.meshtasticEnabled) {
        _meshtastic.setRadioEnabled(true);
    }
    _mesh.setAdvertisingEnabled(_settings.meshAdvertiseEnabled);
    _meshtastic.setIdentity(_settings.meshtasticNodeName);
    _meshtastic.setAdvertisingEnabled(_settings.meshtasticAdvertiseEnabled);
    _recon.setEarlyWarningEnabled(_settings.reconEarlyWarningEnabled);
    _settingsService.save(_settings);
    _face.render(_state, _settings);
    _gpsScreen.render(_state, _settings);
    _meshScreen.render(_mesh.status());
    _meshtasticScreen.render(_meshtastic.status());
    _reconScreen.render();
}

void WatchApp::saveDateTime(int year, int month, int day, int hour, int minute)
{
    _clock.setDateTime(year, month, day, hour, minute, 0);
    refreshState();
}

void WatchApp::reconDetectionSinkThunk(const ReconDetection &detection, void *userData)
{
    static_cast<WatchApp *>(userData)->logReconDetection(detection);
}

void WatchApp::logReconDetection(const ReconDetection &detection)
{
    if (!_settings.reconSdLoggingEnabled) {
        return;
    }

    char row[128];
    snprintf(
        row,
        sizeof(row),
        "%04d-%02d-%02d %02d:%02d:%02d,%s,%s,%s,%d,%u",
        _state.year,
        _state.month,
        _state.day,
        _state.hour,
        _state.minute,
        _state.second,
        detection.category,
        detection.detail,
        detection.address,
        detection.rssi,
        static_cast<unsigned>(detection.channel));

    _sdCard.appendCsvRow(
        "/recon_log.csv",
        "timestamp,category,detail,address,rssi,channel",
        row);
}
