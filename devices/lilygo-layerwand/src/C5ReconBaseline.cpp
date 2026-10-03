// LayerTime - passive early-warning firmware for the LILYGO T-Dongle-C5.
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

// Slice 1 Increment 2A: real Recon on the T-Dongle-C5, no-link baseline.
//
// Built by the tdongle_c5_recon environment (LAYERTIME_RECON_BASELINE).
// This firmware runs the existing LayerTime Recon pipeline on the C5's own
// radios: core's ReconScheduler drives the C5ReconRadio, the core classifiers
// produce candidates, and LayerTimeCore keeps the event log, exactly as on
// the T-Watch Ultra. The LayerTime Link is not started: no GATT server is
// created and nothing advertises, so what is measured is Recon alone.
//
// Evidence goes to USB serial. Every 10 s a [periodic] line reports the
// counters, the loop-period histogram and heap; the C5StageLog keeps a ring
// of stage timestamps in PSRAM. The BOOT button ends the current run,
// prints the run's stage records and counters, and starts the next mode:
// early warning (the boot default) -> manual ALL -> stopped -> early warning.
// Serial lines printed before the port is open are lost (Increment 1
// finding), which is why the dump is on the button and not at boot.
//
// The LCD stays a status surface: the owl, the mode, the live counters, and
// which radio is listening.

#if defined(LAYERTIME_TARGET_TDONGLE_C5) && defined(LAYERTIME_RECON_BASELINE)

#include "BringUpLogic.h"
#include "C5BootRecord.h"
#include "C5Display.h"
#include "C5Led.h"
#include "C5MonitorSource.h"
#include "C5ReconRadio.h"
#include "C5StageLog.h"
#include "TDongleC5Pins.h"

#include "core/app/LayerTimeCore.h"
#include "core/logic/ReconSelection.h"
#include "core/logic/ReconStageLog.h"
#include "core/model/LayerTimeCommand.h"

#include <Arduino.h>
#include <esp_chip_info.h>
#include <esp_heap_caps.h>
#include <esp_idf_version.h>
#include <esp_timer.h>
#include <stdio.h>
#include <string.h>

using namespace layertime;
using namespace layertime::tdongle_c5;

namespace {

constexpr uint32_t kReportMs = 10000;
constexpr uint32_t kDisplayMs = 1000;
constexpr uint32_t kSerialWaitMs = 3000;
constexpr uint8_t kLedBrightness = 2;
// 65,536 records of 16 bytes: 1 MiB of the 8 MB PSRAM, about twelve hours
// of hops at 650 ms with room for everything else.
constexpr uint32_t kStageCapacity = 65536;

enum class RunMode : uint8_t { Stopped = 0, EarlyWarning = 1, ManualAll = 2 };

const char *modeName(RunMode m)
{
    switch (m) {
    case RunMode::EarlyWarning: return "early-warning";
    case RunMode::ManualAll: return "manual-all";
    default: return "stopped";
    }
}

C5BootRecord gBoot; // D3: reset reason and retained boot history
C5Display gDisplay;
C5Led gLed;
ButtonDebouncer gButton;
C5StageLog gStage;
C5ReconRadio gRadio;
C5MonitorSource gMonitor(gRadio);
LayerTimeCore gCore;

RunMode gMode = RunMode::Stopped;
uint32_t gRunStartMs = 0;
uint32_t gButtonCount = 0;
uint32_t gLastReportMs = 0;
uint32_t gLastDisplayMs = 0;
uint32_t gLastLoopMs = 0;
uint32_t gLastHopCount = 0;
uint32_t gLastHopMs = 0;
uint32_t gLastEventId = 0;
uint32_t gEventsSeen = 0;
bool gStageOk = false;
bool gHeapOk = true;
AcquisitionStatus gLastAcq;
recon::IntervalHistogram gLoopPeriod;
recon::IntervalHistogram gHopInterval;

bool heapIntact()
{
    return heap_caps_check_integrity_all(true);
}

void execute(CommandType type, ReconTarget target = ReconTarget::None, bool enabled = false)
{
    LayerTimeCommand c;
    c.type = type;
    c.reconTarget.target = target;
    c.setting.enabled = enabled;
    const CommandResult r = gCore.execute(c);
    if (r != CommandResult::Ok) {
        Serial.printf("Command %d answered %d\n", static_cast<int>(type), static_cast<int>(r));
    }
}

// The early-warning setting is core's; the platform applies it to the
// monitor, in the order WatchApp does on the T-Ultra.
void applyEarlyWarning(bool enabled)
{
    execute(CommandType::SetEarlyWarning, ReconTarget::None, enabled);
    gMonitor.setEarlyWarningEnabled(gCore.settings().earlyWarningEnabled);
}

void recordHeap()
{
    const uint32_t largest = static_cast<uint32_t>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    gStage.record(recon::Stage::Heap, static_cast<uint8_t>(ESP.getFreePsram() / (1024 * 1024)),
                  static_cast<uint16_t>(largest / 1024), ESP.getFreeHeap());
}

uint32_t acqBits(const AcquisitionStatus &a)
{
    return (a.monitoring ? 1u : 0u) | (a.earlyWarningResting ? 2u : 0u) | (a.earlyWarningEnabled ? 4u : 0u);
}

bool acqChanged(const AcquisitionStatus &a, const AcquisitionStatus &b)
{
    return a.selected != b.selected || a.active != b.active || a.monitoring != b.monitoring ||
           a.earlyWarningEnabled != b.earlyWarningEnabled || a.earlyWarningResting != b.earlyWarningResting;
}

void printHistogram(const char *name, const recon::IntervalHistogram &h)
{
    Serial.printf("%s n=%lu min=%lu max=%lu mean=%lu buckets(<=5,10,20,50,100,250,1000,more)=",
                  name, static_cast<unsigned long>(h.count()), static_cast<unsigned long>(h.minMs()),
                  static_cast<unsigned long>(h.maxMs()),
                  static_cast<unsigned long>(h.count() ? h.sumMs() / h.count() : 0));
    for (uint8_t i = 0; i < recon::IntervalHistogram::kBuckets; ++i) {
        Serial.printf("%s%lu", i ? "," : "", static_cast<unsigned long>(h.bucket(i)));
    }
    Serial.println();
}

void printCounters(const char *when)
{
    const C5ReconRadio::Counters &c = gRadio.counters();
    const ReconState r = gCore.reconState();
    const AcquisitionStatus a = gMonitor.acquisition();
    // D3: keep this boot's uptime and state where the next boot can read
    // them, and say why this boot happened.
    gBoot.touch(millis() / 1000, static_cast<uint32_t>(gMode) | (acqBits(a) << 8) |
                                     (static_cast<uint32_t>(static_cast<uint8_t>(a.active)) << 16));
    char boot[120];
    gBoot.format(boot, sizeof(boot));
    Serial.printf("[%s] %s\n", when, boot);
    const uint32_t runS = (millis() - gRunStartMs) / 1000;
    Serial.printf("[%s] mode %s run %lu s uptime %lu s sel %s act %s mon %d ew %d rest %d ch %u | "
                  "wifi starts %lu stops %lu hops %lu | ble scans %lu ends %lu | frames %lu adverts %lu | "
                  "candidates %lu events %u lastId %lu sinkMax %lu us | stage %lu/%lu lost %lu | "
                  "heap %lu largest %lu psram %lu %s\n",
                  when, modeName(gMode), static_cast<unsigned long>(runS),
                  static_cast<unsigned long>(millis() / 1000), recon::detectorName(a.selected),
                  recon::detectorName(a.active), a.monitoring ? 1 : 0, a.earlyWarningEnabled ? 1 : 0,
                  a.earlyWarningResting ? 1 : 0, gMonitor.scheduler().wifiChannel(),
                  static_cast<unsigned long>(c.wifiStarts), static_cast<unsigned long>(c.wifiStops),
                  static_cast<unsigned long>(c.hops), static_cast<unsigned long>(c.bleScanStarts),
                  static_cast<unsigned long>(c.bleScanEnds), static_cast<unsigned long>(c.frames),
                  static_cast<unsigned long>(c.adverts), static_cast<unsigned long>(c.candidates),
                  static_cast<unsigned>(r.eventCount), static_cast<unsigned long>(r.lastEventId),
                  static_cast<unsigned long>(c.sinkMaxUs), static_cast<unsigned long>(gStage.count()),
                  static_cast<unsigned long>(gStage.capacity()), static_cast<unsigned long>(gStage.lost()),
                  static_cast<unsigned long>(ESP.getFreeHeap()),
                  static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                  static_cast<unsigned long>(ESP.getFreePsram()), gHeapOk ? "intact" : "CORRUPT");
    Serial.print("frames by channel:");
    for (uint8_t ch = 1; ch <= 14; ++ch) Serial.printf(" %u=%lu", ch, static_cast<unsigned long>(c.framesByChannel[ch]));
    Serial.println();
    printHistogram("loop period ms", gLoopPeriod);
    printHistogram("hop interval ms", gHopInterval);
}

void printEvents()
{
    const uint8_t n = gCore.eventCount();
    Serial.printf("events %u:\n", static_cast<unsigned>(n));
    for (uint8_t i = 0; i < n; ++i) {
        const MonitorEvent &e = gCore.event(i);
        Serial.printf("E,%lu,%s,%s,%s,%s,%d,%u,%lu,%lu,%u,%u\n", static_cast<unsigned long>(e.eventId),
                      recon::detectorName(e.detector), recon::confidenceLabel(e.confidence), e.sourceId, e.detail,
                      static_cast<int>(e.rssi), static_cast<unsigned>(e.channel),
                      static_cast<unsigned long>(e.lastSeen.uptimeMs), static_cast<unsigned long>(e.count),
                      static_cast<unsigned>(e.sourceKind), static_cast<unsigned>(e.band));
    }
}

// Prints the run's stage records as CSV: S,stage,t_us,a8,a16,a32.
void dumpRun()
{
    gStage.record(recon::Stage::RunEnd, 0, 0, static_cast<uint32_t>(gMode));
    Serial.printf("=== run dump begin mode %s\n", modeName(gMode));
    printCounters("dump");
    printEvents();
    const uint32_t n = gStage.count();
    Serial.printf("stage records %lu of %lu offered, %lu lost, %s\n", static_cast<unsigned long>(n),
                  static_cast<unsigned long>(gStage.total()), static_cast<unsigned long>(gStage.lost()),
                  gStageOk ? "ring in PSRAM" : "NO RING (PSRAM allocation failed)");
    Serial.println("S,stage,t_us,a8,a16,a32");
    for (uint32_t i = 0; i < n; ++i) {
        const recon::StageRecord r = gStage.at(i);
        Serial.printf("S,%u,%llu,%u,%u,%lu\n", static_cast<unsigned>(r.stage),
                      static_cast<unsigned long long>(r.tUs), static_cast<unsigned>(r.a8),
                      static_cast<unsigned>(r.a16), static_cast<unsigned long>(r.a32));
        if ((i & 255) == 255) {
            Serial.flush();
            gDisplay.service();
        }
    }
    Serial.println("=== run dump end");
    Serial.flush();
}

void startRun(RunMode mode)
{
    gMode = mode;
    gRunStartMs = millis();
    gStage.clear();
    gRadio.clearCounters();
    gLoopPeriod.clear();
    gHopInterval.clear();
    gLastHopCount = 0;
    gLastHopMs = 0;
    gLastLoopMs = millis();
    gStage.record(recon::Stage::RunStart, 0, 0, static_cast<uint32_t>(mode));
    recordHeap();
    switch (mode) {
    case RunMode::EarlyWarning:
        applyEarlyWarning(true);
        break;
    case RunMode::ManualAll:
        execute(CommandType::ReconStart, ReconTarget::All);
        break;
    default:
        break;
    }
    gLastAcq = gMonitor.acquisition();
    Serial.printf("=== run start mode %s at %lu ms\n", modeName(mode), static_cast<unsigned long>(gRunStartMs));
}

// Ends the run with both radios idle, then dumps it.
void endRun()
{
    if (gMode == RunMode::ManualAll) execute(CommandType::ReconStop);
    applyEarlyWarning(false);
    gMonitor.stop();
    dumpRun();
}

RunMode nextMode(RunMode m)
{
    switch (m) {
    case RunMode::EarlyWarning: return RunMode::ManualAll;
    case RunMode::ManualAll: return RunMode::Stopped;
    default: return RunMode::EarlyWarning;
    }
}

void showStatus()
{
    // Each line must fit the 96 px status column (about 14 characters).
    char line[40];
    const C5ReconRadio::Counters &c = gRadio.counters();
    const AcquisitionStatus a = gMonitor.acquisition();
    switch (gMode) {
    case RunMode::EarlyWarning: gDisplay.setLine(1, a.earlyWarningResting ? "EarlyWarn rest" : "EarlyWarn"); break;
    case RunMode::ManualAll: gDisplay.setLine(1, "Manual ALL"); break;
    default: gDisplay.setLine(1, "Stopped"); break;
    }
    snprintf(line, sizeof(line), "F%lu A%lu", static_cast<unsigned long>(c.frames), static_cast<unsigned long>(c.adverts));
    gDisplay.setLine(2, line);
    snprintf(line, sizeof(line), "Det %lu Ev %u", static_cast<unsigned long>(c.candidates),
             static_cast<unsigned>(gCore.eventCount()));
    gDisplay.setLine(3, line);
    const char *radio = gRadio.bleScanning() ? "BLE" : (c.wifiStarts > c.wifiStops ? "WiFi" : "idle");
    // Uptime, the listening radio, then the reset-reason letter and boot
    // count (C5BootRecord::reasonLetter): "126s idle0 P3".
    snprintf(line, sizeof(line), "%lus %s%u %c%lu", static_cast<unsigned long>(millis() / 1000), radio,
             strcmp(radio, "WiFi") == 0 ? static_cast<unsigned>(gMonitor.scheduler().wifiChannel()) : 0u,
             C5BootRecord::reasonLetter(gBoot.reason()), static_cast<unsigned long>(gBoot.bootCount()));
    gDisplay.setLine(4, line);
}

} // namespace

void setup()
{
    gBoot.begin(); // before anything else: the reset reason of this boot
    Serial.begin(115200);
    const uint32_t waitStart = millis();
    while (!Serial && millis() - waitStart < kSerialWaitMs) delay(10);

    gLed.begin();
    gLed.show(Rgb{0, 0, 32}, kLedBrightness);
    pinMode(pins::kBootButton, INPUT); // external 10 k pull-up (R16)

    const bool displayOk = gDisplay.begin();
    // The C5 is LayerWand; LayerTime is the watch (Michael, 2026-09-30).
    gDisplay.setLine(0, "LayerWand");

    esp_chip_info_t chip;
    esp_chip_info(&chip);
    Serial.println();
    Serial.println("LayerWand (T-Dongle-C5): Recon no-link baseline (Slice 1, Increment 2A)");
    Serial.printf("Arduino core %s, ESP-IDF %s\n", ESP_ARDUINO_VERSION_STR, esp_get_idf_version());
    Serial.printf("Chip model %d, revision v%d.%d, %d core(s)\n", static_cast<int>(chip.model),
                  chip.revision / 100, chip.revision % 100, chip.cores);
    Serial.printf("Flash %lu bytes, PSRAM %lu bytes\n", static_cast<unsigned long>(ESP.getFlashChipSize()),
                  static_cast<unsigned long>(ESP.getPsramSize()));
    Serial.printf("Display %s\n", displayOk ? "started" : "FAILED");
    Serial.println("Link: not started. No GATT server, no advertising.");
    {
        char boot[120];
        gBoot.format(boot, sizeof(boot));
        Serial.println(boot);
    }

    gStageOk = gStage.begin(kStageCapacity);
    Serial.printf("Stage log: %s, %lu records of %u bytes\n", gStageOk ? "ring in PSRAM" : "PSRAM allocation FAILED",
                  static_cast<unsigned long>(gStage.capacity()), static_cast<unsigned>(sizeof(recon::StageRecord)));
    gRadio.setStageLog(&gStage);
    gRadio.begin();

    CorePorts ports;
    ports.monitor = &gMonitor;
    gCore.attach(ports);
    gLastEventId = gCore.reconState().lastEventId;

    gHeapOk = heapIntact();
    Serial.printf("Heap before Recon: %s, free %lu\n", gHeapOk ? "intact" : "CORRUPT",
                  static_cast<unsigned long>(ESP.getFreeHeap()));
    Serial.printf("Schedule: hop %lu ms, BLE cycle %lu ms, BLE scan %lu ms, BLE-only rescan %lu ms, "
                  "early warning %lu ms sweep / %lu ms rest\n",
                  static_cast<unsigned long>(recon::ReconScheduler::kChannelHopMs),
                  static_cast<unsigned long>(recon::ReconScheduler::kBleCycleMs),
                  static_cast<unsigned long>(recon::ReconScheduler::kBleScanMs),
                  static_cast<unsigned long>(recon::ReconScheduler::kBleOnlyRescanMs),
                  static_cast<unsigned long>(recon::ReconScheduler::kEarlyWarningActiveMs),
                  static_cast<unsigned long>(recon::ReconScheduler::kEarlyWarningRestMs));
    Serial.println("BOOT button: end the run, dump it, start the next mode (early-warning -> manual-all -> stopped).");

    startRun(RunMode::EarlyWarning);
    showStatus();
    printCounters("boot");
}

void loop()
{
    const uint32_t now = millis();
    if (gLastLoopMs != 0) {
        const uint32_t period = now - gLastLoopMs;
        gLoopPeriod.add(period);
        if (period > 1000) gStage.record(recon::Stage::PollLate, 0, 0, period);
    }
    gLastLoopMs = now;

    const bool pressed = digitalRead(pins::kBootButton) == LOW;
    if (gButton.update(pressed, now)) {
        ++gButtonCount;
        Serial.printf("Button press %lu\n", static_cast<unsigned long>(gButtonCount));
        endRun();
        startRun(nextMode(gMode));
        showStatus();
        return;
    }

    // Alert actuation, then the monitor's schedule: core's tick, as on the
    // T-Ultra. The C5 has no AlertSink attached, so only the schedule runs.
    gCore.tick(now);

    const AcquisitionStatus acq = gMonitor.acquisition();
    if (acqChanged(acq, gLastAcq)) {
        gLastAcq = acq;
        gStage.record(recon::Stage::PhaseChange, static_cast<uint8_t>(acq.selected),
                      static_cast<uint16_t>(acq.active), acqBits(acq));
        Serial.printf("phase sel %s act %s mon %d rest %d at %lu ms\n", recon::detectorName(acq.selected),
                      recon::detectorName(acq.active), acq.monitoring ? 1 : 0, acq.earlyWarningResting ? 1 : 0,
                      static_cast<unsigned long>(now));
    }

    const uint32_t hops = gRadio.counters().hops;
    if (hops != gLastHopCount) {
        if (gLastHopMs != 0 && hops == gLastHopCount + 1) gHopInterval.add(now - gLastHopMs);
        gLastHopCount = hops;
        gLastHopMs = now;
    }

    const uint32_t lastId = gCore.reconState().lastEventId;
    if (lastId != gLastEventId) {
        gLastEventId = lastId;
        ++gEventsSeen;
        const uint8_t n = gCore.eventCount();
        if (n > 0) {
            const MonitorEvent &e = gCore.event(n - 1);
            Serial.printf("event %lu %s %s %s rssi %d ch %u\n", static_cast<unsigned long>(e.eventId),
                          recon::detectorName(e.detector), e.sourceId, e.detail, static_cast<int>(e.rssi),
                          static_cast<unsigned>(e.channel));
        }
        gLed.show(Rgb{32, 0, 0}, kLedBrightness);
    }

    if (now - gLastReportMs >= kReportMs) {
        gLastReportMs = now;
        gHeapOk = gHeapOk && heapIntact();
        recordHeap();
        printCounters("periodic");
    }

    if (now - gLastDisplayMs >= kDisplayMs) {
        gLastDisplayMs = now;
        showStatus();
    }

    gDisplay.service();
    delay(5);
}

#endif
