#include "WatchApp.h"

#include <Arduino.h>
#include <LilyGoLib.h>
#include <LV_Helper.h>

// Prints one line per qualifying press so a still-broken gesture can be
// diagnosed from the serial monitor instead of guessed at. Comment out
// once the gesture is confirmed working.
#define LAYERTIME_DEBUG_DOUBLETAP

namespace {
constexpr uint32_t kDisplayTimeoutMs = 15000;
// Two background taps closer together than this put the display to sleep.
constexpr uint32_t kDoubleTapWindowMs = 350;
// How stale LVGL's activity timer has to be, relative to the moment sleep
// was forced, before a fresh touch counts as a wake. Absorbs the release
// edge of the double-tap itself and any bounce right after it.
constexpr uint32_t kForcedSleepGraceMs = 250;
bool displayBlanked = false;
}

void WatchApp::begin()
{
    Serial.begin(115200);

    instance.begin();
    beginLvglHelper(instance);

    _settingsService.load(_settings);
    _settingsService.apply(_settings);
    _sleepModeActive = _settings.sleepModeEnabled;
    if (_sleepModeActive) {
        // Loaded from a previous session with sleep mode already on -
        // start dark immediately rather than lighting up for the normal
        // 15s auto-blank window.
        instance.setBrightness(0);
        displayBlanked = true;
    }
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
    _sdCard.begin();
    _recon.begin();
    _recon.setEarlyWarningEnabled(_settings.reconEarlyWarningEnabled);
    _recon.setSleepModeEnabled(_settings.sleepModeEnabled);
    _recon.setDetectionSink(reconDetectionSinkThunk, this);

    _face.create();
    _face.setSettingsRequestedCallback(settingsRequestedThunk, this);
    _face.setGpsRequestedCallback(gpsRequestedThunk, this);
    _face.setMeshRequestedCallback(meshRequestedThunk, this);
    _face.setMeshtasticRequestedCallback(meshtasticRequestedThunk, this);
    _face.setReconRequestedCallback(reconRequestedThunk, this);
    _face.setThreatsRequestedCallback(threatsRequestedThunk, this);
    // Hook the touch device itself rather than the watch-face screen object.
    // LV_EVENT_PRESSED fires before the indev decides whether a press is a
    // drag, so unlike LV_EVENT_CLICKED it cannot be cancelled by scrolling,
    // and it survives lv_screen_load() swapping screens underneath it.
    // Scope is re-established explicitly in handleFaceBackgroundTap().
    for (lv_indev_t *indev = lv_indev_get_next(nullptr); indev != nullptr;
         indev = lv_indev_get_next(indev)) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
            lv_indev_add_event_cb(indev, touchPressedThunk, LV_EVENT_PRESSED, this);
        }
    }

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
    _recon.poll();
    lv_timer_handler();

    const uint32_t inactiveMs = lv_display_get_inactive_time(nullptr);

    // Release a forced sleep as soon as LVGL reports activity newer than the
    // moment it was forced - i.e. a genuine new touch. Deliberately measured
    // against the inactivity timer rather than hooking the tap itself, so a
    // tap anywhere wakes the watch, including one that lands on a button
    // rather than the background.
    if (_forcedSleep && inactiveMs + kForcedSleepGraceMs < millis() - _forcedSleepAtMs) {
        _forcedSleep = false;
    }

    if (!displayBlanked && (_forcedSleep || inactiveMs >= kDisplayTimeoutMs)) {
        instance.setBrightness(0);
        displayBlanked = true;
    } else if (displayBlanked && !_forcedSleep && inactiveMs < kDisplayTimeoutMs) {
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
    _face.render(_state, _settings, _recon.status());
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

void WatchApp::threatsRequestedThunk(void *userData)
{
    static_cast<WatchApp *>(userData)->openThreatsRecon();
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

void WatchApp::openThreatsRecon()
{
    _reconScreen.show(ReconDetector::All);
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
    if (_settings.sleepModeEnabled && !_sleepModeActive) {
        // Just turned on - darken right away instead of waiting out the
        // normal auto-blank timeout. Tapping the screen still wakes it
        // normally afterward (e.g. to check the time or turn this back off).
        instance.setBrightness(0);
        displayBlanked = true;
    }
    _sleepModeActive = _settings.sleepModeEnabled;
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
    _recon.setSleepModeEnabled(_settings.sleepModeEnabled);
    _settingsService.save(_settings);
    _face.render(_state, _settings, _recon.status());
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

void WatchApp::touchPressedThunk(lv_event_t *event)
{
    auto *self = static_cast<WatchApp *>(lv_event_get_user_data(event));
    if (self != nullptr) {
        self->handleFaceBackgroundTap();
    }
}

void WatchApp::handleFaceBackgroundTap()
{
    // This now fires for every touch anywhere, so the watch-face-background
    // scope that used to come free from the screen-object callback has to be
    // re-established here: right screen, and the press landed on the face
    // itself rather than one of its buttons or the GPS/THREATS labels.
    lv_obj_t *face = _face.screen();
    if (face == nullptr || lv_screen_active() != face) return;
    lv_obj_t *hit = lv_indev_get_active_obj();
#ifdef LAYERTIME_DEBUG_DOUBLETAP
    // Logged before the hit test rejects anything, so the serial trace
    // distinguishes "guard rejected this press" from "no event at all".
    Serial.printf("[dtap] hit=%p face=%p %s blanked=%d forced=%d gap=%ld\n",
                  static_cast<void *>(hit), static_cast<void *>(face),
                  (hit == nullptr || hit == face) ? "BG" : "child",
                  static_cast<int>(displayBlanked), static_cast<int>(_forcedSleep),
                  _lastFaceTapMs ? static_cast<long>(millis() - _lastFaceTapMs) : -1L);
#endif
    if (hit != nullptr && hit != face) return;

    const uint32_t now = millis();

    // A tap that arrives while the screen is already dark only ever wakes it.
    // Waking is handled by the inactivity timer in tick(); all this has to do
    // is make sure that tap isn't also counted as the first half of a pair,
    // or waking with two quick taps would immediately sleep the watch again.
    if (displayBlanked) {
        _lastFaceTapMs = 0;
        return;
    }

    if (_lastFaceTapMs != 0 && now - _lastFaceTapMs <= kDoubleTapWindowMs) {
        _lastFaceTapMs = 0;
        _forcedSleep = true;
        _forcedSleepAtMs = now;
        instance.setBrightness(0);
        displayBlanked = true;
        return;
    }

    _lastFaceTapMs = now;
}
