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
// Widened from 350ms. With two hooks live, a genuinely fast double-tap had
// its second press swallowed by the duplicate guard, which read as "the
// watch needs a harder tap". One hook now, so the guard is pure contact
// debounce and the window can be generous.
constexpr uint32_t kDoubleTapWindowMs = 500;
constexpr uint32_t kPressDebounceMs = 40;
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
    _face.setMappingRequestedCallback(mappingRequestedThunk, this);
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
    _mappingScreen.create(mappingBackThunk, this);
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
    _mappingScreen.render(_state, _settings);
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

void WatchApp::mappingRequestedThunk(void *userData)
{
    static_cast<WatchApp *>(userData)->openMapping();
}

void WatchApp::mappingBackThunk(void *userData)
{
    static_cast<WatchApp *>(userData)->closeMapping();
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

void WatchApp::openMapping()
{
    _mappingScreen.show(_state, _settings);
}

void WatchApp::closeMapping()
{
    lv_screen_load(_face.screen());
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
    _mappingScreen.render(_state, _settings);
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

    // Worst case is ~107 bytes with the confidence column; sized well clear
    // of that so a long detail string truncates the field, never the row.
    char row[160];
    snprintf(
        row,
        sizeof(row),
        "%04d-%02d-%02d %02d:%02d:%02d,%s,%s,%s,%d,%u,%s",
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
        static_cast<unsigned>(detection.channel),
        ReconService::confidenceLabel(detection.confidence));

    _sdCard.appendCsvRow(
        "/recon_log.csv",
        "timestamp,category,detail,address,rssi,channel,confidence",
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
    lv_obj_t *hit = lv_indev_get_active_obj();

#ifdef LAYERTIME_DEBUG_DOUBLETAP
    // Printed BEFORE any early return, and counted, so a press that gets
    // rejected still shows up - the whole question is which stage drops it.
    //   p    presses that reached this handler at all
    //   scr  1 = the active screen really is the watch face
    //   hit  F = face, c = a child widget, 0 = null
    //   br   what getBrightness() actually reports
    //   gap  ms since the last accepted tap, -1 if no pair is pending
    ++_debugPressCount;
    Serial.printf("[dtap] p=%lu scr=%d hit=%s br=%u gap=%ld\n",
                  static_cast<unsigned long>(_debugPressCount),
                  lv_screen_active() == face ? 1 : 0,
                  (hit == nullptr) ? "0" : (hit == face ? "F" : "c"),
                  static_cast<unsigned>(instance.getBrightness()),
                  _lastFaceTapMs ? static_cast<long>(millis() - _lastFaceTapMs) : -1L);
#endif

    const uint32_t pressNow = millis();
    if (_lastPressHandledMs != 0 && pressNow - _lastPressHandledMs < kPressDebounceMs) return;
    _lastPressHandledMs = pressNow;

    // Any touch anywhere ends a forced sleep, and is consumed doing it. This
    // runs before the watch-face scoping below on purpose: the indev hook
    // sees every press, so tapping a button wakes the watch too. Waking used
    // to be inferred from LVGL's inactivity timer, which Recon's alert path
    // resets via lv_display_trigger_activity() - so any detection seconds
    // after a double-tap yanked the screen straight back on.
    if (_forcedSleep) {
        _forcedSleep = false;
        _lastFaceTapMs = 0;
        return;
    }

    if (face == nullptr || lv_screen_active() != face) return;
    if (hit != nullptr && hit != face) return;

    const uint32_t now = pressNow;

    // A tap that arrives while the screen is already dark only ever wakes it.
    // Waking is handled by the inactivity timer in tick(); all this has to do
    // is make sure that tap isn't also counted as the first half of a pair,
    // or waking with two quick taps would immediately sleep the watch again.
    //
    // Read the panel's actual brightness rather than the cached displayBlanked
    // flag. That flag getting stuck true was one of the two live suspects for
    // this gesture dying - it is the handler's only early return, and if
    // _forcedSleep were also stuck, tick() skips both the blank and un-blank
    // branches and never clears it. A hardware read can't get stuck.
    if (instance.getBrightness() == 0) {
        _lastFaceTapMs = 0;
        return;
    }

    if (_lastFaceTapMs != 0 && now - _lastFaceTapMs <= kDoubleTapWindowMs) {
        _lastFaceTapMs = 0;
        _forcedSleep = true;
        instance.setBrightness(0);
        displayBlanked = true;
        return;
    }

    _lastFaceTapMs = now;
}
