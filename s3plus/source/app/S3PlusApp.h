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

// The S3 Plus application: LilyGoLib bring-up, the core and its ports, the
// GNSS service and clock, the screens, and the display gate. The screens talk
// only to this class; this class talks to the core.
//
// Event-log locking (recon/EventLock.h): the radio tasks write the core's
// event history under the lock. This class holds it around core.tick(),
// around ReconClearEvents and ReconAcknowledgeAlert, and while it copies the
// history into reconSnapshot() for the screens. ReconStart, ReconStop, and
// the early-warning switch drive the radios, so they run without it.

#include <stdint.h>

#include "core/app/LayerTimeCore.h"
#include "core/model/MonitorEvent.h"
#include "core/model/ReconState.h"

#include "../S3PlusAlertSink.h"
#include "../S3PlusSettingsStore.h"
#include "../gnss/GnssRules.h"
#include "../gnss/S3PlusClock.h"
#include "../gnss/S3PlusGpsService.h"
#include "../gnss/S3PlusNavigationSource.h"
#include "../gnss/Solar.h"
#include "../recon/EventLock.h"
#include "../recon/S3PlusMonitorSource.h"
#include "../recon/S3PlusReconService.h"
#include "../ui/DisplayGate.h"

namespace layertime {
namespace twatch_s3plus {

class HomeScreen;
class ReconScreen;
class TimeScreen;
class GpsScreen;
class SettingsScreen;

// What the screens draw from: a copy taken under the event-log lock.
struct ReconSnapshot {
    ReconState state;
    uint8_t count = 0;
    MonitorEvent events[ReconState::kMaxEvents];
};

class S3PlusApp {
public:
    static constexpr const char *kVersion = "0.2.3";

    S3PlusApp();
    void begin();
    void loop();

    // ---- for the screens ----
    const ReconSnapshot &recon() const { return _snapshot; }
    void reconStart(ReconTarget target);
    void reconStop();
    void reconClear();
    void acknowledgeAlert();
    bool earlyWarningEnabled() const { return _core.settings().earlyWarningEnabled; }
    void setEarlyWarning(bool enabled);

    const NavigationState &navigation() const { return _core.navigation(); }
    bool use24Hour() const { return _core.settings().use24Hour; }
    bool metricUnits() const { return _core.settings().metricUnits; }
    void setUse24Hour(bool enabled);
    void setMetricUnits(bool metric);
    bool sleepModeEnabled() const { return _core.settings().sleepModeEnabled; }
    void setSleepMode(bool enabled);
    bool gpsEnabled() const { return _gps.enabled(); }
    void setGpsEnabled(bool enabled);
    uint8_t brightness() const { return _local.brightness; }
    // Applied at once; saved only when `save` (the slider saves on release).
    void setBrightness(uint8_t value, bool save);
    // The face's DONGLE icon: shown or hidden (Settings), and whether a
    // dongle is connected. No dongle link exists on this watch yet, so it is
    // never connected and the icon reads red.
    bool dongleIconShown() const { return _local.dongleIcon; }
    void setDongleIconShown(bool shown);
    bool dongleConnected() const { return false; }
    // The watch's own temperature (BMA423), read every 10 s.
    bool watchTemperatureValid() const { return _watchTempValid; }
    int watchCelsius() const { return _watchCelsius; }
    // The next sunrise or sunset, from the last position and the clock;
    // unknown until both exist. Recomputed once a minute.
    const solar::NextEvent &nextSolarEvent() const { return _solar; }
    // The DATE / TIME page's SAVE.
    void setLocalDateTime(const gnss::DateTime &local);
    int timeZoneMinutes() const { return _clock.offsetMinutes(); }
    void stepTimeZone(int direction);
    // Set from GNSS this boot.
    bool clockSetThisBoot() const { return _clock.setThisBoot(); }
    bool clockSetManually() const { return _clock.setManuallyThisBoot(); }
    // GNSS or the wearer has set it this boot: the face shows the date.
    bool clockTrusted() const { return _clock.trustedThisBoot(); }
    uint32_t clockLastSetMs() const { return _clock.lastSetMs(); }
    const gnss::DateTime &localTime() const { return _localTime; }
    int batteryPercent() const { return _batteryPercent; }
    bool charging() const { return _charging; }

    // The watch face's double-tap: display dark until the wearer touches it
    // again (an alert still buzzes; see ui/DisplayGate.h).
    void sleepDisplay() { _display.sleepNow(); }

    void showHome();
    void showRecon();
    void showReconMonitor(ReconTarget target);
    void showTime();
    void showSettings();
    void showGps();

private:
    void refresh(uint32_t nowMs);
    void takeSnapshot();
    void readWatchTemperature(uint32_t nowMs);
    void updateSolar(uint32_t nowMs);
    void applyDisplayGate(uint32_t nowMs);
    void printStatus(uint32_t nowMs);
    CommandResult execute(CommandType type, ReconTarget target = ReconTarget::None, bool enabled = false);

    EventLock _lock;
    S3PlusReconService _recon;
    S3PlusMonitorSource _monitorSource;
    S3PlusAlertSink _alertSink;
    S3PlusGpsService _gps;
    S3PlusNavigationSource _navigationSource;
    S3PlusSettingsStore _settingsStore;
    S3PlusLocalSettings _local;
    S3PlusClock _clock;
    LayerTimeCore _core;
    ui::DisplayGate _display;

    ReconSnapshot _snapshot;
    gnss::DateTime _localTime;
    int _batteryPercent = 0;
    bool _watchTempValid = false;
    int _watchCelsius = 0;
    uint32_t _lastTempMs = 0;
    bool _tempReadOnce = false;
    solar::NextEvent _solar;
    uint32_t _lastSolarMs = 0;
    bool _solarOnce = false;
    bool _charging = false;

    HomeScreen *_home = nullptr;
    ReconScreen *_reconScreen = nullptr;
    TimeScreen *_timeScreen = nullptr;
    GpsScreen *_gpsScreen = nullptr;
    SettingsScreen *_settingsScreen = nullptr;

    uint32_t _lastRefreshMs = 0;
    uint32_t _lastStatusMs = 0;
};

} // namespace twatch_s3plus
} // namespace layertime
