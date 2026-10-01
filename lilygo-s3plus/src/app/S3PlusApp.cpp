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

#include "S3PlusApp.h"

#include <Arduino.h>
#include <LV_Helper.h>
#include <LilyGoLib.h>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include <Wire.h>
#include <string.h>

#include "core/logic/ReconSelection.h"
#include "core/model/LayerTimeCommand.h"

#include "../BringUpReport.h"
#include "../ui/GpsScreen.h"
#include "../ui/HomeScreen.h"
#include "../ui/ReconScreen.h"
#include "../ui/SettingsScreen.h"
#include "../ui/TextFormat.h"
#include "../ui/TimeScreen.h"

namespace layertime {
namespace twatch_s3plus {

namespace {
constexpr uint32_t kSerialWaitMs = 5000;
constexpr uint32_t kRefreshMs = 250;
constexpr uint32_t kStatusMs = 30000;
constexpr uint32_t kTemperatureMs = 10000;
constexpr uint32_t kSolarMs = 60000;
// The BMA423 on the main I2C bus (the bring-up report lists it at 0x19).
constexpr uint8_t kBma423Address = 0x19;
constexpr uint8_t kBma423Temperature = 0x22;

void setTouchInput(bool enabled)
{
    for (lv_indev_t *i = lv_indev_get_next(nullptr); i != nullptr; i = lv_indev_get_next(i)) {
        if (lv_indev_get_type(i) == LV_INDEV_TYPE_POINTER) lv_indev_enable(i, enabled);
    }
}
}

S3PlusApp::S3PlusApp()
    : _recon(_lock), _monitorSource(_recon, _lock), _navigationSource(_gps)
{
}

void S3PlusApp::begin()
{
    Serial.begin(115200);
    // Native USB: the port opens seconds after boot. Wait for it, bounded.
    const uint32_t start = millis();
    while (!Serial && millis() - start < kSerialWaitMs) delay(10);
    Serial.printf("\n[s3plus] LayerTime T-Watch S3 Plus %s starting\n", kVersion);

    bringup_report::beforeBegin();
    // The LoRa radio is excluded from begin() and its rail is cut right
    // after. Mesh stays deferred.
    instance.begin(NO_HW_LORA);
    instance.powerControl(POWER_RADIO, false);
    bringup_report::afterBegin();

    // Before anything that can start a radio.
    _lock.begin();
    if (!_lock.ready()) Serial.printf("[s3plus] ERROR: the event-log lock could not be created\n");

    beginLvglHelper(instance);
    _local.load();
    instance.setBrightness(_local.brightness);
    // begin() powered the receiver; the Settings GPS switch decides.
    _gps.setEnabled(_local.gpsEnabled);

    CorePorts ports;
    ports.monitor = &_monitorSource;
    ports.alerts = &_alertSink;
    ports.eventLog = nullptr;  // Recon logging is R2.
    ports.navigation = &_navigationSource;
    ports.settings = &_settingsStore;
    _core.attach(ports);
    _core.loadSettings();

    _clock.begin(_local.timeZoneMinutes);
    _recon.begin();
    // Starts the Wi-Fi sweep at once when on (the default, as on the Ultra).
    _recon.setEarlyWarningEnabled(_core.settings().earlyWarningEnabled);

    _home = new HomeScreen();
    _reconScreen = new ReconScreen();
    _timeScreen = new TimeScreen();
    _gpsScreen = new GpsScreen();
    _settingsScreen = new SettingsScreen();
    _home->create(this);
    _reconScreen->create(this);
    _timeScreen->create(this);
    _gpsScreen->create(this);
    _settingsScreen->create(this);

    bringup_report::print(kVersion);
    refresh(millis());
    _home->show();
}

void S3PlusApp::loop()
{
    const uint32_t now = millis();
    instance.loop();
    _gps.poll(now);
    _clock.poll(_gps, now);
    {
        // Alerts read the event history under the lock. The monitor source
        // releases it while it drives the radios (S3PlusMonitorSource::poll).
        EventLockGuard guard(_lock);
        _core.tick(now);
    }
    applyDisplayGate(now);
    if (now - _lastRefreshMs >= kRefreshMs) {
        _lastRefreshMs = now;
        refresh(now);
    }
    lv_timer_handler();
    if (now - _lastStatusMs >= kStatusMs) {
        _lastStatusMs = now;
        printStatus(now);
    }
    delay(2);
}

void S3PlusApp::refresh(uint32_t nowMs)
{
    _core.refreshNavigation();
    _localTime = _clock.readLocal();
    _batteryPercent = instance.pmu.getBatteryPercent();
    readWatchTemperature(nowMs);
    updateSolar(nowMs);
    _charging = instance.pmu.isCharging();
    takeSnapshot();
    if (_home) _home->render(nowMs);
    if (_reconScreen) _reconScreen->render();
    if (_timeScreen) _timeScreen->render(nowMs);
    if (_gpsScreen) _gpsScreen->render();
    if (_settingsScreen) _settingsScreen->render();
}

void S3PlusApp::readWatchTemperature(uint32_t nowMs)
{
    if (_tempReadOnce && nowMs - _lastTempMs < kTemperatureMs) return;
    _tempReadOnce = true;
    _lastTempMs = nowMs;
    // Read the register directly: SensorLib's getTemperature() mis-decodes
    // anything below 23 C, and cannot report a failed read
    // (text::watchCelsius decodes it).
    Wire.beginTransmission(kBma423Address);
    Wire.write(kBma423Temperature);
    if (Wire.endTransmission(false) != 0 || Wire.requestFrom(kBma423Address, static_cast<uint8_t>(1)) != 1) {
        _watchTempValid = false;
        return;
    }
    int celsius = 0;
    _watchTempValid = text::watchCelsius(static_cast<uint8_t>(Wire.read()), celsius);
    if (_watchTempValid) _watchCelsius = celsius;
}

void S3PlusApp::updateSolar(uint32_t nowMs)
{
    if (_solarOnce && nowMs - _lastSolarMs < kSolarMs) return;
    _solarOnce = true;
    _lastSolarMs = nowMs;
    const NavigationState &nav = _core.navigation();
    // Needs a position (the last one is fine: sunrise barely moves with a few
    // km) and a clock someone vouched for.
    if (!nav.everHadFix || !_clock.trustedThisBoot()) {
        _solar = solar::NextEvent{};
        return;
    }
    const gnss::DateTime utc = gnss::addMinutes(_localTime, -_clock.offsetMinutes());
    _solar = solar::nextEvent(nav.latitudeDeg, nav.longitudeDeg, utc);
}

void S3PlusApp::setDongleIconShown(bool shown)
{
    _local.dongleIcon = shown;
    _local.save();
}

void S3PlusApp::takeSnapshot()
{
    EventLockGuard guard(_lock);
    _snapshot.state = _core.reconState();
    _snapshot.count = _core.eventCount();
    for (uint8_t i = 0; i < _snapshot.count && i < ReconState::kMaxEvents; ++i) {
        _snapshot.events[i] = _core.event(i);
    }
}

void S3PlusApp::applyDisplayGate(uint32_t nowMs)
{
    (void)nowMs;
    bool touchDown = false;
    if (!_display.inputEnabled()) {
        // Screens get no input while dark, so read the controller directly.
        int16_t x = 0;
        int16_t y = 0;
        touchDown = instance.getPoint(&x, &y, 1) > 0;
    }
    switch (_display.update(lv_display_get_inactive_time(nullptr), touchDown)) {
    case ui::DisplayGate::Action::Blank:
        setTouchInput(false);
        instance.setBrightness(0);
        break;
    case ui::DisplayGate::Action::Wake:
        instance.setBrightness(_local.brightness);
        break;
    case ui::DisplayGate::Action::WakeAndInput:
        instance.setBrightness(_local.brightness);
        setTouchInput(true);
        break;
    case ui::DisplayGate::Action::Input:
        setTouchInput(true);
        lv_display_trigger_activity(nullptr);
        break;
    case ui::DisplayGate::Action::None:
        break;
    }
}

CommandResult S3PlusApp::execute(CommandType type, ReconTarget target, bool enabled)
{
    LayerTimeCommand c;
    c.type = type;
    c.reconTarget.target = target;
    c.setting.enabled = enabled;
    return _core.execute(c);
}

void S3PlusApp::reconStart(ReconTarget target)
{
    // Drives the radios: never under the event-log lock.
    execute(CommandType::ReconStart, target);
    takeSnapshot();  // the screens see the new state at once
    Serial.printf("[s3plus] recon: start %s\n", recon::detectorName(target));
}

void S3PlusApp::reconStop()
{
    execute(CommandType::ReconStop);
    takeSnapshot();
    Serial.printf("[s3plus] recon: stop\n");
}

void S3PlusApp::reconClear()
{
    {
        EventLockGuard guard(_lock);
        execute(CommandType::ReconClearEvents);
    }
    takeSnapshot();
    Serial.printf("[s3plus] recon: events cleared\n");
}

void S3PlusApp::acknowledgeAlert()
{
    {
        EventLockGuard guard(_lock);
        execute(CommandType::ReconAcknowledgeAlert);
    }
    takeSnapshot();
}

void S3PlusApp::setEarlyWarning(bool enabled)
{
    execute(CommandType::SetEarlyWarning, ReconTarget::None, enabled);
    // The core records the setting; the platform applies it to the radios,
    // as WatchApp::settingsChanged does on the Ultra.
    _recon.setEarlyWarningEnabled(enabled);
    _core.saveSettings();
    Serial.printf("[s3plus] early warning %s\n", enabled ? "on" : "off");
}

void S3PlusApp::setUse24Hour(bool enabled)
{
    execute(CommandType::SetClockFormat, ReconTarget::None, enabled);
    _core.saveSettings();
}

void S3PlusApp::setMetricUnits(bool metric)
{
    execute(CommandType::SetUnits, ReconTarget::None, metric);
    _core.saveSettings();
}

void S3PlusApp::setSleepMode(bool enabled)
{
    execute(CommandType::SetSleepMode, ReconTarget::None, enabled);
    _core.saveSettings();
    Serial.printf("[s3plus] sleep mode %s\n", enabled ? "on" : "off");
}

void S3PlusApp::setGpsEnabled(bool enabled)
{
    _gps.setEnabled(enabled);
    _local.gpsEnabled = enabled;
    _local.save();
    Serial.printf("[s3plus] gps %s\n", enabled ? "on" : "off");
}

void S3PlusApp::setBrightness(uint8_t value, bool save)
{
    if (value < S3PlusLocalSettings::kMinBrightness) value = S3PlusLocalSettings::kMinBrightness;
    _local.brightness = value;
    // Only while lit: a dark display stays dark until the gate wakes it.
    if (_display.lit()) instance.setBrightness(value);
    if (save) _local.save();
}

void S3PlusApp::setLocalDateTime(const gnss::DateTime &local)
{
    _clock.setLocalManually(local);
    _localTime = _clock.readLocal();
}

void S3PlusApp::stepTimeZone(int direction)
{
    _clock.setOffsetMinutes(gnss::stepOffset(_clock.offsetMinutes(), direction));
    _local.timeZoneMinutes = _clock.offsetMinutes();
    _local.save();
    _localTime = _clock.readLocal();
}

void S3PlusApp::showHome()
{
    if (_home) _home->show();
}

void S3PlusApp::showRecon()
{
    if (_reconScreen) _reconScreen->show();
}

void S3PlusApp::showReconMonitor(ReconTarget target)
{
    if (_reconScreen) _reconScreen->showMonitor(target);
}

void S3PlusApp::showGps()
{
    if (_gpsScreen) _gpsScreen->show();
}

void S3PlusApp::showTime()
{
    if (_timeScreen) _timeScreen->show();
}

void S3PlusApp::showSettings()
{
    if (_settingsScreen) _settingsScreen->show();
}

void S3PlusApp::printStatus(uint32_t nowMs)
{
    const NavigationState &nav = _core.navigation();
    const ReconState &r = _snapshot.state;
    Serial.printf("[s3plus] status up=%lus heap free=%lu min=%lu largest=%lu psram free=%lu "
                  "events=%u lastId=%lu alert=%d recon=%s ew=%s fix=%s age=%lus sats=%u clock=%s "
                  "ubx ok=%lu bad=%lu gps=%s sleep=%s\n",
                  static_cast<unsigned long>(nowMs / 1000), static_cast<unsigned long>(ESP.getFreeHeap()),
                  static_cast<unsigned long>(ESP.getMinFreeHeap()),
                  static_cast<unsigned long>(
                      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                  static_cast<unsigned long>(ESP.getFreePsram()), static_cast<unsigned>(_snapshot.count),
                  static_cast<unsigned long>(r.lastEventId), r.alertPending ? 1 : 0,
                  r.monitoring ? recon::detectorName(r.selected) : "manual off",
                  !r.earlyWarningEnabled ? "off" : (r.earlyWarningResting ? "resting" : "sweeping"),
                  nav.fixUsable ? "yes" : "no", static_cast<unsigned long>(nav.fixAgeMs / 1000),
                  static_cast<unsigned>(nav.satellites), _clock.setThisBoot() ? "set" : "unset",
                  static_cast<unsigned long>(_gps.framesAccepted()),
                  static_cast<unsigned long>(_gps.framesRejected()), _gps.enabled() ? "on" : "off",
                  _core.settings().sleepModeEnabled ? "on" : "off");
}

} // namespace twatch_s3plus
} // namespace layertime
