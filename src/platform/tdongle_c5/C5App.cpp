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

// Slice 1 Increment 2B: LayerWand, architecture A. Real Recon and the
// LayerTime Link on one T-Dongle-C5.
//
// Built by the tdongle_c5_wand environment (LAYERTIME_WAND_APP). Recon runs
// exactly as in the Increment 2A baseline (C5ReconBaseline.cpp): core's
// ReconScheduler drives the C5ReconRadio, the core classifiers produce
// candidates, LayerTimeCore keeps the event log. It runs whether or not a
// watch is connected, starting in early warning. The LayerTime Link
// (C5Link, contracts/link.md) runs beside it: Status with the live Recon
// fields, HELLO and PING, and through core's LinkServer the COMMAND,
// GET_CHANGED and GET_TEXT requests the watch uses for its Recon page and
// Controls.
//
// Threading. Candidates reach core's event log on the Wi-Fi task
// (promiscuous callback) and the NimBLE host task (scan results). A FreeRTOS
// mutex, gEventLock, covers every touch of the event log and alert state:
// the candidate sink, the alert half of core's tick, the LinkServer's reads,
// and the commands that write the log. It is never held around anything
// that reaches the radios (ReconStart, ReconStop, SetEarlyWarning, the
// scheduler's poll), because the Wi-Fi driver's own task can be waiting to
// deliver a frame into the sink at that moment.
//
// ReconClearEvents also resets the Wi-Fi detectors' tracking state. That
// state belongs to the Wi-Fi task, which classifies every frame, so
// C5ReconRadio::resetDetectorState() only flags the reset and the Wi-Fi task
// performs it before its next frame; the loop never touches the classifier.
//
// Evidence goes to USB serial in the Increment 2A format, so the two can be
// compared line for line: a [periodic] line every 10 s with the Recon
// counters, the loop-period and hop-interval histograms, heap, and a [link]
// line with the Link counters. A new run starts, with the counters cleared,
// whenever the Recon mode changes (early warning, manual, stopped). The
// BOOT button dumps the current run's stage records without changing the
// mode; the mode is the watch's to change.

#if defined(LAYERTIME_TARGET_TDONGLE_C5) && defined(LAYERTIME_WAND_APP)

#include "BringUpLogic.h"
#include "C5BootRecord.h"
#include "C5Display.h"
#include "C5Led.h"
#include "C5Link.h"
#include "C5MonitorSource.h"
#include "C5ReconRadio.h"
#include "C5StageLog.h"
#include "TDongleC5Pins.h"

#include "../../core/app/LayerTimeCore.h"
#include "../../core/link/LinkServer.h"
#include "../../core/logic/ReconSelection.h"
#include "../../core/logic/ReconStageLog.h"
#include "../../core/model/LayerTimeCommand.h"

#include <Arduino.h>
#include <esp_chip_info.h>
#include <esp_heap_caps.h>
#include <esp_idf_version.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stdio.h>
#include <string.h>

using namespace layertime;
using namespace layertime::tdongle_c5;

namespace {

constexpr uint32_t kReportMs = 10000;
constexpr uint32_t kDisplayMs = 1000;
constexpr uint32_t kSerialWaitMs = 3000;
constexpr uint8_t kLedBrightness = 1;
// The status LED stays dark so the LayerWand can be concealed (Michael,
// 2026-10-01). Set true to restore blue at boot and red on each new
// detection, at kLedBrightness. Either way the LED's state is sent again
// after every LCD transfer, because the LED shares the LCD's SPI bus
// (C5Led.h).
constexpr bool kLedEnabled = false;
// Concealment (Michael, 2026-10-01). The screen stays dark and unstarted until
// a short BOOT press wakes it for kScreenWakeMs, showing a snapshot of the
// status at the press (no redraws while lit). A long press (held
// ButtonGestures::kLongPressMs) dumps the run, as a press did before.
constexpr uint32_t kScreenWakeMs = 10000;
constexpr uint32_t kStageCapacity = 65536; // as in Increment 2A

enum class RunMode : uint8_t { Stopped = 0, EarlyWarning = 1, Manual = 2 };

const char *modeName(RunMode m)
{
    switch (m) {
    case RunMode::EarlyWarning: return "early-warning";
    case RunMode::Manual: return "manual";
    default: return "stopped";
    }
}

// ---- The event-log lock --------------------------------------------------------

SemaphoreHandle_t gEventLock = nullptr;

void lockEvents(void *)
{
    xSemaphoreTake(gEventLock, portMAX_DELAY);
}

void unlockEvents(void *)
{
    xSemaphoreGive(gEventLock);
}

// Stands between core and the C5 monitor source only to put the candidate
// sink under the event-log lock. Everything else passes straight through
// and is not locked: it drives the radios.
class LockedMonitor : public MonitorSource {
public:
    explicit LockedMonitor(C5MonitorSource &inner) : _inner(inner) {}

    void start(ReconTarget target) override { _inner.start(target); }
    void stopManual() override { _inner.stopManual(); }
    void resetDetectorState() override { _inner.resetDetectorState(); }
    void setEarlyWarningEnabled(bool enabled) override { _inner.setEarlyWarningEnabled(enabled); }
    void poll() override { _inner.poll(); }
    AcquisitionStatus acquisition() const override { return _inner.acquisition(); }
    void setCandidateSink(recon::CandidateSink sink, void *context) override
    {
        _sink = sink;
        _context = context;
        _inner.setCandidateSink(lockedSink, this);
    }

private:
    static void lockedSink(const recon::Candidate &candidate, void *self)
    {
        LockedMonitor *me = static_cast<LockedMonitor *>(self);
        if (me->_sink == nullptr) return;
        lockEvents(nullptr);
        me->_sink(candidate, me->_context);
        unlockEvents(nullptr);
    }

    C5MonitorSource &_inner;
    recon::CandidateSink _sink = nullptr;
    void *_context = nullptr;
};

C5BootRecord gBoot; // D3: reset reason and retained boot history
C5Display gDisplay;
C5Led gLed;
ButtonGestures gButton;
C5StageLog gStage;
C5ReconRadio gRadio;
C5MonitorSource gMonitor(gRadio);
LockedMonitor gLockedMonitor(gMonitor);
LayerTimeCore gCore;
link::LinkServer gServer(gCore);
C5Link gLink;

RunMode gMode = RunMode::Stopped;
uint32_t gRun = 0;
uint32_t gRunStartMs = 0;
uint32_t gButtonCount = 0;
bool gDisplayStarted = false;
bool gDisplayOk = false;
bool gScreenAwake = false;
uint32_t gScreenWokeMs = 0;
// What the LED should show when kLedEnabled, and the LCD transfer count it
// was last sent after.
Rgb gLedColor{0, 0, 32};
uint32_t gLedSentAfterTransfers = 0;
uint32_t gLastReportMs = 0;
uint32_t gLastDisplayMs = 0;
uint32_t gLastLoopMs = 0;
uint32_t gLastHopCount = 0;
uint32_t gLastHopMs = 0;
uint32_t gLastEventId = 0;
uint32_t gConnects = 0;
uint32_t gDisconnects = 0;
bool gEarlyWarningApplied = false;
bool gStageOk = false;
bool gHeapOk = true;
bool gBleOk = false;
bool gWasConnected = false;
AcquisitionStatus gLastAcq;
recon::IntervalHistogram gLoopPeriod;
recon::IntervalHistogram gHopInterval;

bool heapIntact()
{
    return heap_caps_check_integrity_all(true);
}

RunMode modeOf(const AcquisitionStatus &a)
{
    if (a.monitoring) return RunMode::Manual;
    if (a.earlyWarningEnabled) return RunMode::EarlyWarning;
    return RunMode::Stopped;
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

void recordHeap()
{
    const uint32_t largest = static_cast<uint32_t>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    gStage.record(recon::Stage::Heap, static_cast<uint8_t>(ESP.getFreePsram() / (1024 * 1024)),
                  static_cast<uint16_t>(largest / 1024), ESP.getFreeHeap());
}

// The early-warning setting is core's (the watch changes it with
// SetEarlyWarning); the platform applies it to the monitor, outside the
// event-log lock.
void applyEarlyWarningIfChanged()
{
    const bool wanted = gCore.settings().earlyWarningEnabled;
    if (wanted == gEarlyWarningApplied) return;
    gEarlyWarningApplied = wanted;
    gMonitor.setEarlyWarningEnabled(wanted);
    Serial.printf("early warning %s at %lu ms\n", wanted ? "on" : "off", static_cast<unsigned long>(millis()));
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
    lockEvents(nullptr);
    const ReconState r = gCore.reconState();
    unlockEvents(nullptr);
    const AcquisitionStatus a = gMonitor.acquisition();
    gBoot.touch(millis() / 1000, static_cast<uint32_t>(gMode) | (acqBits(a) << 8) |
                                     (static_cast<uint32_t>(static_cast<uint8_t>(a.active)) << 16) |
                                     (gLink.connected() ? (1u << 24) : 0u));
    char boot[120];
    gBoot.format(boot, sizeof(boot));
    Serial.printf("[%s] %s\n", when, boot);
    const uint32_t runS = (millis() - gRunStartMs) / 1000;
    // The Increment 2A [periodic] line, field for field.
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
    const link::LinkServer::Counters &s = gServer.counters();
    Serial.printf("[%s] link %s peer %s session %04X changeSeq %lu heartbeat %u | connects %lu disconnects %lu | "
                  "pings %lu requests %lu commands %lu getChanged %lu getText %lu summaries %lu gaps %lu | "
                  "frames sent %lu refused %lu dropped %lu busy %lu | status change notifies %lu\n",
                  when, gLink.connected() ? "connected" : "advertising", gLink.connected() ? gLink.peerAddress() : "-",
                  static_cast<unsigned>(gLink.sessionId()), static_cast<unsigned long>(gLink.changeSeq()),
                  static_cast<unsigned>(gLink.heartbeat()), static_cast<unsigned long>(gConnects),
                  static_cast<unsigned long>(gDisconnects), static_cast<unsigned long>(gLink.pingsAnswered()),
                  static_cast<unsigned long>(gLink.requestsAnswered()), static_cast<unsigned long>(s.commands),
                  static_cast<unsigned long>(s.getChanged), static_cast<unsigned long>(s.getText),
                  static_cast<unsigned long>(s.summaries), static_cast<unsigned long>(s.gaps),
                  static_cast<unsigned long>(gLink.framesSent()), static_cast<unsigned long>(gLink.notifyRefused()),
                  static_cast<unsigned long>(gLink.framesDropped()), static_cast<unsigned long>(gLink.busyReplies()),
                  static_cast<unsigned long>(gLink.changeNotifies()));
    Serial.print("frames by channel:");
    for (uint8_t ch = 1; ch <= 14; ++ch) Serial.printf(" %u=%lu", ch, static_cast<unsigned long>(c.framesByChannel[ch]));
    Serial.println();
    printHistogram("loop period ms", gLoopPeriod);
    printHistogram("hop interval ms", gHopInterval);
}

// Copies the events out under the lock and prints them after it.
void printEvents()
{
    static MonitorEvent copy[ReconState::kMaxEvents];
    lockEvents(nullptr);
    const uint8_t n = gCore.eventCount();
    for (uint8_t i = 0; i < n; ++i) copy[i] = gCore.event(i);
    unlockEvents(nullptr);
    Serial.printf("events %u:\n", static_cast<unsigned>(n));
    for (uint8_t i = 0; i < n; ++i) {
        const MonitorEvent &e = copy[i];
        Serial.printf("E,%lu,%s,%s,%s,%s,%d,%u,%lu,%lu,%u,%u\n", static_cast<unsigned long>(e.eventId),
                      recon::detectorName(e.detector), recon::confidenceLabel(e.confidence), e.sourceId, e.detail,
                      static_cast<int>(e.rssi), static_cast<unsigned>(e.channel),
                      static_cast<unsigned long>(e.lastSeen.uptimeMs), static_cast<unsigned long>(e.count),
                      static_cast<unsigned>(e.sourceKind), static_cast<unsigned>(e.band));
    }
}

// The run's stage records as CSV (S,stage,t_us,a8,a16,a32), as in 2A. The
// radios keep running; the dump is evidence, not a mode change.
void dumpRun()
{
    Serial.printf("=== run dump begin mode %s run %lu\n", modeName(gMode), static_cast<unsigned long>(gRun));
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
            if (gScreenAwake) gDisplay.service();
        }
    }
    Serial.println("=== run dump end");
    Serial.flush();
}

// A new run: the counters and histograms describe one Recon mode at a time,
// as the 2A runs did.
void startRun(RunMode mode, uint32_t now)
{
    if (gRun > 0) {
        gStage.record(recon::Stage::RunEnd, 0, 0, static_cast<uint32_t>(gMode));
        printCounters("run-end");
    }
    ++gRun;
    gMode = mode;
    gRunStartMs = now;
    gStage.clear();
    gRadio.clearCounters();
    gLoopPeriod.clear();
    gHopInterval.clear();
    gLastHopCount = 0;
    gLastHopMs = 0;
    gStage.record(recon::Stage::RunStart, 0, 0, static_cast<uint32_t>(mode));
    recordHeap();
    Serial.printf("=== run start mode %s run %lu at %lu ms\n", modeName(mode), static_cast<unsigned long>(gRun),
                  static_cast<unsigned long>(now));
}

void showStatus()
{
    // Each line must fit the 96 px status column (about 14 characters).
    char line[40];
    const AcquisitionStatus a = gMonitor.acquisition();
    const ReconState r = gCore.reconState(); // single-word reads for the LCD
    if (a.monitoring) {
        snprintf(line, sizeof(line), "R %s", recon::detectorName(a.selected));
    } else if (a.earlyWarningEnabled) {
        snprintf(line, sizeof(line), "%s", a.earlyWarningResting ? "EarlyWarn rest" : "EarlyWarn");
    } else {
        snprintf(line, sizeof(line), "Recon off");
    }
    gDisplay.setLine(1, line);
    snprintf(line, sizeof(line), "Ev %u%s", static_cast<unsigned>(r.eventCount), r.alertPending ? " ALERT" : "");
    gDisplay.setLine(2, line);
    if (!gBleOk) {
        gDisplay.setLine(3, "BLE FAILED");
    } else if (gLink.connected()) {
        const char *addr = gLink.peerAddress();
        const size_t n = strlen(addr);
        snprintf(line, sizeof(line), "Watch %s", n >= 5 ? addr + n - 5 : addr);
        gDisplay.setLine(3, line);
    } else {
        gDisplay.setLine(3, "No watch");
    }
    // Uptime, then the reset-reason letter and boot count.
    snprintf(line, sizeof(line), "%lus %c%lu", static_cast<unsigned long>(millis() / 1000),
             C5BootRecord::reasonLetter(gBoot.reason()), static_cast<unsigned long>(gBoot.bootCount()));
    gDisplay.setLine(4, line);
}

// Short BOOT press: start the screen the first time, light it, and show the
// current status (it is drawn on the next service()).
void wakeScreen(uint32_t now)
{
    if (!gDisplayStarted) {
        gDisplayStarted = true;
        gDisplayOk = gDisplay.begin();
        gDisplay.setLine(0, "LayerWand");
        Serial.printf("Display %s\n", gDisplayOk ? "started" : "FAILED");
    } else {
        gDisplay.setBacklight(true);
    }
    gScreenAwake = true;
    gScreenWokeMs = now;
    showStatus();
    Serial.println("Screen on");
}

// Sends the LED its state: off, or gLedColor when kLedEnabled.
void applyLed()
{
    if (kLedEnabled) gLed.show(gLedColor, kLedBrightness);
    else gLed.off();
    gLedSentAfterTransfers = gDisplay.transfers();
}

// kScreenWakeMs after the press: backlight off and no more LCD transfers;
// the LED was already sent its state after the last one.
void sleepScreen()
{
    gDisplay.setBacklight(false);
    gScreenAwake = false;
    applyLed();
    Serial.println("Screen dark");
}

} // namespace

void setup()
{
    gBoot.begin(); // before anything else: the reset reason of this boot
    Serial.begin(115200);
    const uint32_t waitStart = millis();
    while (!Serial && millis() - waitStart < kSerialWaitMs) delay(10);

    gEventLock = xSemaphoreCreateMutex();

    // The LED shares the LCD's SPI bus (C5Led.h): start the bus, then send
    // the LED its state, so it is dark from boot even if it powered up lit.
    gLed.begin();
    applyLed();
    pinMode(pins::kBootButton, INPUT); // external 10 k pull-up (R16)


    esp_chip_info_t chip;
    esp_chip_info(&chip);
    Serial.println();
    Serial.println("LayerWand (T-Dongle-C5): Recon and LayerTime Link 0.1 (Slice 1, Increment 2B, architecture A)");
    Serial.printf("Arduino core %s, ESP-IDF %s\n", ESP_ARDUINO_VERSION_STR, esp_get_idf_version());
    Serial.printf("Chip model %d, revision v%d.%d, %d core(s)\n", static_cast<int>(chip.model),
                  chip.revision / 100, chip.revision % 100, chip.cores);
    Serial.printf("Flash %lu bytes, PSRAM %lu bytes\n", static_cast<unsigned long>(ESP.getFlashChipSize()),
                  static_cast<unsigned long>(ESP.getPsramSize()));
    Serial.println("Display: dark until a short BOOT press (concealment)");
    {
        char boot[120];
        gBoot.format(boot, sizeof(boot));
        Serial.println(boot);
    }

    gStageOk = gStage.begin(kStageCapacity);
    Serial.printf("Stage log: %s, %lu records of %u bytes\n", gStageOk ? "ring in PSRAM" : "PSRAM allocation FAILED",
                  static_cast<unsigned long>(gStage.capacity()), static_cast<unsigned>(sizeof(recon::StageRecord)));
    gRadio.setStageLog(&gStage);
    gRadio.begin(); // Wi-Fi first, as in 2A

    CorePorts ports;
    ports.monitor = &gLockedMonitor;
    gCore.attach(ports);
    gServer.setLock(lockEvents, unlockEvents, nullptr);

    gHeapOk = heapIntact();
    Serial.printf("Heap before BLE init: %s, free %lu\n", gHeapOk ? "intact" : "CORRUPT",
                  static_cast<unsigned long>(ESP.getFreeHeap()));
    // NimBLE comes up here, for the Link; C5ReconRadio's BLE scans find it
    // initialised and use the same host.
    gLink.attachServer(&gServer);
    gBleOk = gLink.begin(link::kCapLocalWifiMonitor | link::kCapLocalBleMonitor | link::kCapDisplay |
                         link::kCapLed | link::kCapButton);
    const bool heapAfterBle = heapIntact();
    gHeapOk = gHeapOk && heapAfterBle;
    Serial.printf("Link %s, advertising as %s, session %04X\n", gBleOk ? "up" : "FAILED", gLink.advertisedName(),
                  static_cast<unsigned>(gLink.sessionId()));
    Serial.printf("Heap after BLE init: %s, free %lu\n", heapAfterBle ? "intact" : "CORRUPT",
                  static_cast<unsigned long>(ESP.getFreeHeap()));
    Serial.printf("Schedule: hop %lu ms, BLE cycle %lu ms, BLE scan %lu ms, BLE-only rescan %lu ms, "
                  "early warning %lu ms sweep / %lu ms rest\n",
                  static_cast<unsigned long>(recon::ReconScheduler::kChannelHopMs),
                  static_cast<unsigned long>(recon::ReconScheduler::kBleCycleMs),
                  static_cast<unsigned long>(recon::ReconScheduler::kBleScanMs),
                  static_cast<unsigned long>(recon::ReconScheduler::kBleOnlyRescanMs),
                  static_cast<unsigned long>(recon::ReconScheduler::kEarlyWarningActiveMs),
                  static_cast<unsigned long>(recon::ReconScheduler::kEarlyWarningRestMs));
    Serial.println("Recon starts in early warning and runs with or without a watch. BOOT button: dump the current run.");

    // Early warning is core's default setting; apply it, and start run 1.
    applyEarlyWarningIfChanged();
    gLastAcq = gMonitor.acquisition();
    gLastLoopMs = millis();
    startRun(modeOf(gLastAcq), gLastLoopMs);
    gServer.refresh();
    gLink.updateStatus();
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
    const ButtonGesture gesture = gButton.update(pressed, now);
    if (gesture == ButtonGesture::Short) {
        ++gButtonCount;
        Serial.printf("Button press %lu (short): screen on\n", static_cast<unsigned long>(gButtonCount));
        wakeScreen(now);
    } else if (gesture == ButtonGesture::Long) {
        ++gButtonCount;
        Serial.printf("Button press %lu (long): run dump\n", static_cast<unsigned long>(gButtonCount));
        dumpRun();
        return;
    }
    if (gScreenAwake && now - gScreenWokeMs >= kScreenWakeMs) sleepScreen();

    // Core's tick, in its order: the alert half under the event-log lock,
    // then the monitor's schedule outside it.
    lockEvents(nullptr);
    gCore.raiseAlerts(now);
    unlockEvents(nullptr);
    gCore.pollMonitor();

    // The Link: answer the waiting request (a command may change the Recon
    // mode or the early-warning setting), apply the setting, then refresh
    // Status and send.
    const uint32_t commandsBefore = gServer.counters().commands;
    gLink.serviceRequests(now);
    if (gServer.counters().commands != commandsBefore) {
        const link::LinkServer::Counters &s = gServer.counters();
        Serial.printf("command %u arg %u result %u at %lu ms\n", static_cast<unsigned>(s.lastCommandType),
                      static_cast<unsigned>(s.lastCommandArgument), static_cast<unsigned>(s.lastCommandResult),
                      static_cast<unsigned long>(now));
    }
    applyEarlyWarningIfChanged();
    gServer.refresh();
    gLink.updateStatus();
    gLink.service(now);

    if (gLink.connected() != gWasConnected) {
        gWasConnected = gLink.connected();
        if (gWasConnected) ++gConnects; else ++gDisconnects;
        Serial.printf("Link %s %s at %lu ms\n", gWasConnected ? "connected to" : "disconnected, advertising again;",
                      gWasConnected ? gLink.peerAddress() : "", static_cast<unsigned long>(now));
        if (!gScreenAwake) showStatus(); // no redraw while lit (snapshot)
    }

    const AcquisitionStatus acq = gMonitor.acquisition();
    if (acqChanged(acq, gLastAcq)) {
        const RunMode was = modeOf(gLastAcq);
        gLastAcq = acq;
        gStage.record(recon::Stage::PhaseChange, static_cast<uint8_t>(acq.selected),
                      static_cast<uint16_t>(acq.active), acqBits(acq));
        Serial.printf("phase sel %s act %s mon %d rest %d at %lu ms\n", recon::detectorName(acq.selected),
                      recon::detectorName(acq.active), acq.monitoring ? 1 : 0, acq.earlyWarningResting ? 1 : 0,
                      static_cast<unsigned long>(now));
        if (modeOf(acq) != was) startRun(modeOf(acq), now);
    }

    const uint32_t hops = gRadio.counters().hops;
    if (hops != gLastHopCount) {
        if (gLastHopMs != 0 && hops == gLastHopCount + 1) gHopInterval.add(now - gLastHopMs);
        gLastHopCount = hops;
        gLastHopMs = now;
    }

    // New events, copied out under the lock and logged after it.
    MonitorEvent latest;
    bool fresh = false;
    lockEvents(nullptr);
    const uint32_t lastId = gCore.reconState().lastEventId;
    if (lastId != gLastEventId && gCore.eventCount() > 0) {
        latest = gCore.event(gCore.eventCount() - 1);
        fresh = true;
    }
    gLastEventId = lastId;
    unlockEvents(nullptr);
    if (fresh) {
        Serial.printf("event %lu %s %s %s rssi %d ch %u\n", static_cast<unsigned long>(latest.eventId),
                      recon::detectorName(latest.detector), latest.sourceId, latest.detail,
                      static_cast<int>(latest.rssi), static_cast<unsigned>(latest.channel));
        gLedColor = Rgb{32, 0, 0};
        applyLed();
    }

    if (now - gLastReportMs >= kReportMs) {
        gLastReportMs = now;
        gHeapOk = gHeapOk && heapIntact();
        recordHeap();
        printCounters("periodic");
    }

    // While the screen is lit it holds the snapshot wakeScreen() drew: every
    // redraw also flashes the LED (it shares the LCD's bus), so the status is
    // drawn once per press (Michael, 2026-10-01). While dark, the labels keep
    // up without sending anything, ready for the next press.
    if (!gScreenAwake && now - gLastDisplayMs >= kDisplayMs) {
        gLastDisplayMs = now;
        showStatus();
    }

    if (gScreenAwake) gDisplay.service();
    // Any LCD transfer (here, or in a run dump) scrambled the LED; send it
    // its state again.
    if (gDisplay.transfers() != gLedSentAfterTransfers) applyLed();
    delay(5);
}

#endif
