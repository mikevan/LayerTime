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

// Slice 1 Increment 2B: Hunter, architecture A. Real Recon and the
// LayerTime Link on one T-Dongle-C5.
//
// Built by the tdongle_c5_hunter environment (LAYERTIME_HUNTER_APP). Recon runs
// exactly as in the Increment 2A baseline (C5ReconBaseline.cpp): core's
// ReconScheduler drives the C5ReconRadio, the core classifiers produce
// candidates, LayerTimeCore keeps the event log. It runs whether or not a
// watch is connected. Since 2026-11-04 it powers up stopped and waits for
// START from the controller (HunterSettingsStore). The LayerTime Link
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
//
// SD-card log (Michael, approved 2026-10-01; from boot since 2026-10-03):
// the Hunter looks for a microSD card right after boot. With a card,
// every event, link change, mode change, and the boot line go to
// hunter_NNNN.log for the whole run (C5SdLog.h, SdLogLogic.h). The card
// shares the status LED's bus, so it is touched only at boot and when a
// write is due: half the record ring waiting, or the watch connecting or
// disconnecting (option c, 2026-10-03). Without one, Link Status carries noSdLog and the watch
// warns that events live only in memory. Core's one EventLog port feeds
// both the screen's BandTally and the SD log.
//
// Scan order (commit 2, Michael, 2026-10-02): the Wi-Fi radio runs on both
// bands. At boot the Hunter probes which channels its radio accepts and
// hands core's scheduler a plan of every accepted 5 GHz channel, lowest to
// highest, then every accepted 2.4 GHz channel, lowest to highest, in
// SweepMode::FullPass: a full pass, then BLE. The hardware random number
// generator varies each early-warning rest (and the end of each ALL pass)
// so two Hunters cannot stay locked in step and miss each other.

#if defined(LAYERTIME_TARGET_TDONGLE_C5) && defined(LAYERTIME_HUNTER_APP)

#include "BringUpLogic.h"
#include "C5BootRecord.h"
#include "C5Display.h"
#include "C5Led.h"
#include "C5Link.h"
#include "C5MonitorSource.h"
#include "C5ReconRadio.h"
#include "C5SdLog.h"
#include "CardTransferLogic.h"
#include "C5StageLog.h"
#include "TDongleC5Pins.h"

#include "core/app/LayerTimeCore.h"
#include "core/link/LinkEvidence.h"
#include "core/link/LinkHunt.h"
#include "core/link/LinkServer.h"
#include "core/logic/EvidencePcap.h"
#include "core/logic/EvidenceStore.h"
#include "core/logic/ReconSelection.h"
#include "core/logic/ReconStageLog.h"
#include "core/model/LayerTimeCommand.h"
#include "core/ports/EventLog.h"
#include "core/ports/SettingsStore.h"

#include <Arduino.h>
#include <Preferences.h>
#include <esp_chip_info.h>
#include <esp_mac.h>
#include <esp_heap_caps.h>
#include <esp_idf_version.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

using namespace layertime;
using namespace layertime::tdongle_c5;

namespace {

constexpr uint32_t kReportMs = 10000;
constexpr uint32_t kDisplayMs = 1000;
constexpr uint32_t kSerialWaitMs = 3000;
// USB serial must never stall the loop (2026-10-30). Espressif's USB serial
// driver keeps treating the port as read while the cable is in, even after
// the program reading it has closed, so each print waited up to 100 ms per
// chunk and up to 20 chunks. With the logger closed one loop pass took 182 s,
// Status stopped, and the watch dropped and reconnected every 10 s. A print
// now waits at most kSerialTxTimeoutMs per chunk (about 40 ms per print with
// nobody reading), and the 10 s report is skipped while the last one has not
// drained. Same fault as D4 (2026-10-02), which was wrongly judged lab-only.
constexpr uint32_t kSerialTxTimeoutMs = 2;
// The USB serial TX ring holds 256 bytes; a report prints only when at least
// this much of it is free, which a reading logger guarantees within ms.
constexpr int kSerialDrainedBytes = 128;
constexpr uint8_t kLedBrightness = 1;
// The status LED stays dark so the Hunter can be concealed (Michael,
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
        // An attack burst or a handshake is worth staying on this channel for:
        // tell the scheduler to hold it briefly before the next hop. atMs is
        // the uptime when the candidate was produced, the scheduler's clock.
        // Not under the event lock; noteInterest only writes one word.
        if (candidate.detector == ReconTarget::Deauth ||
            candidate.detector == ReconTarget::Eapol)
            me->_inner.scheduler().noteInterest(candidate.atMs);
    }

    C5MonitorSource &_inner;
    recon::CandidateSink _sink = nullptr;
    void *_context = nullptr;
};

// The screen's event totals by band, since power-on. Core calls append()
// once for each new event record, from the candidate sink, so always under
// the event-log lock; the loop reads the totals as single words.
class BandTally : public EventLog {
public:
    void append(const MonitorEvent &e) override { _totals.add(eventBand(e.sourceKind, e.band, e.channel)); }
    const BandTotals &totals() const { return _totals; }

private:
    BandTotals _totals;
};

BandTally gTally;
C5SdLog gSdLog;

// ---- Evidence (Link 0.2 evidence, slice 2, 2026-11-04) -------------------------
// The frames behind each Wi-Fi detection, in PSRAM (core/logic/EvidenceStore).
// The Wi-Fi task writes it (every frame through the radio's tap, and a new
// window from the event log below); the loop reads it to answer GET_FRAMES and
// GET_FRAME_BYTES. gEvidenceLock is its own mutex: never held while the event
// lock is taken, so the two cannot deadlock.
evidence::EvidenceStore gEvidence;
SemaphoreHandle_t gEvidenceLock = nullptr;
recon::ReconScheduler *gEvidenceScheduler = nullptr;
volatile uint32_t gEvidenceWindows = 0;     // windows opened since boot (read by the loop)
volatile uint32_t gEvidenceHolds = 0;       // channel holds started by a deauth window
volatile uint32_t gEvidenceRepeats = 0;     // repeat sightings that pushed a window (and a deauth's hold) out
uint32_t gEvidenceWindowsReported = 0;
uint32_t gEvidenceRepeatsReported = 0;
// The evidence hold in seconds (SetEvidenceHold, Settings.evidenceHoldSec).
// The loop copies it from core's settings when it changes; the Wi-Fi task
// reads it here as one byte, so the opener never touches core's settings.
volatile uint8_t gEvidenceHoldSec = kDefaultEvidenceHoldSec;
// The SD card copy of each window (2026-11-04): the loop copies a closed window
// into this PSRAM buffer as a pcap file under the evidence lock, then writes the
// card without holding it, so the Wi-Fi task never waits on the card.
uint8_t *gEvidencePcap = nullptr;
uint32_t gEvidenceRetryAtMs = 0; // after a failed save, wait before the next try

void lockEvidence(void *) { xSemaphoreTake(gEvidenceLock, portMAX_DELAY); }
void unlockEvidence(void *) { xSemaphoreGive(gEvidenceLock); }

// Every Wi-Fi frame, before classification (C5ReconRadio::setFrameTap).
void evidenceTap(const uint8_t *frame, uint16_t len, int8_t rssi, uint8_t channel, uint32_t nowMs, void *)
{
    if (!gEvidence.attached()) return;
    lockEvidence(nullptr);
    gEvidence.noteFrame(frame, len, rssi, channel, nowMs);
    unlockEvidence(nullptr);
}

// A new Wi-Fi event opens its window, collecting for the evidence hold (never
// less than 10 s). A deauth also holds the channel for the evidence hold
// (Michael's option 1, 2026-11-04), so the window hears what follows the flood.
// A repeat sighting of the same event pushes its window out again, and a
// repeat deauth restarts the hold, so the Hunter stays on a sustained flood
// and resumes the sweep when it stops (2026-11-04). Off (0) never holds.
// Called by core from the candidate sink, on the Wi-Fi task, under the event
// lock; takes only the evidence lock. No Serial here: a print can wait up to
// kSerialTxTimeoutMs, and this is the radio's task.
class EvidenceOpener : public EventLog {
public:
    void append(const MonitorEvent &e) override
    {
        if (!gEvidence.attached() || e.sourceKind != SourceKind::Wifi) return;
        uint8_t mac[6];
        if (!evidence::EvidenceStore::parseMac(e.sourceId, mac)) return;
        const uint32_t now = millis();
        const uint8_t holdSec = gEvidenceHoldSec;
        lockEvidence(nullptr);
        const bool opened = gEvidence.openWindow(e.eventId, mac, now, evidence::EvidenceStore::followMsFor(holdSec));
        unlockEvidence(nullptr);
        if (!opened) return;
        gEvidenceWindows = gEvidenceWindows + 1;
        if (hold(e, now, holdSec)) gEvidenceHolds = gEvidenceHolds + 1;
    }
    void repeated(const MonitorEvent &e) override
    {
        if (!gEvidence.attached() || e.sourceKind != SourceKind::Wifi) return;
        const uint32_t now = millis();
        const uint8_t holdSec = gEvidenceHoldSec;
        lockEvidence(nullptr);
        const bool extended = gEvidence.extendWindow(e.eventId, now, evidence::EvidenceStore::followMsFor(holdSec));
        unlockEvidence(nullptr);
        if (!extended) return; // its window was reused for a newer detection
        gEvidenceRepeats = gEvidenceRepeats + 1;
        if (hold(e, now, holdSec)) gEvidenceHolds = gEvidenceHolds + 1;
    }

private:
    static bool hold(const MonitorEvent &e, uint32_t now, uint8_t holdSec)
    {
        if (e.detector != ReconTarget::Deauth || holdSec == 0 || gEvidenceScheduler == nullptr) return false;
        gEvidenceScheduler->holdChannel(now, static_cast<uint32_t>(holdSec) * 1000u);
        return true;
    }
};
EvidenceOpener gEvidenceOpener;

// Core has one EventLog port; the screen's totals, the SD log, and the
// evidence store all need every new event, so the port feeds all three.
// Repeat sightings go to all three too (only the evidence opener uses them).
class EventLogFanout : public EventLog {
public:
    EventLogFanout(EventLog &first, EventLog &second, EventLog &third) : _first(first), _second(second), _third(third) {}
    void append(const MonitorEvent &e) override
    {
        _first.append(e);
        _second.append(e);
        _third.append(e);
    }
    void repeated(const MonitorEvent &e) override
    {
        _first.repeated(e);
        _second.repeated(e);
        _third.repeated(e);
    }

private:
    EventLog &_first;
    EventLog &_second;
    EventLog &_third;
};

EventLogFanout gEventLogs(gTally, gSdLog, gEvidenceOpener);
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
uint32_t gReportsSkipped = 0; // 10 s reports skipped while nothing read the port
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
// Full passes of the channel plan (Wi-Fi start to the last channel's end),
// and the time from one pass's end to the next (pass, BLE, and any rest).
recon::IntervalHistogram gPassTime;
recon::IntervalHistogram gPassToPass;
uint32_t gLastPasses = 0;
uint32_t gLastPassEndMs = 0;
uint8_t gPlan[recon::ReconScheduler::kMaxPlanChannels];
uint8_t gPlanCount = 0;

// HUNT v1 (claude/hunt_v1_layervixen_plan.md). Recon pauses for a hunt and
// comes back as it was: manual monitoring of the same selection, or early
// warning (re-armed by stopManual when enabled).
// A hunt nobody polls for this long ends by itself, so a watch that went
// away does not leave the Hunter deaf to Recon.
constexpr uint32_t kHuntIdleStopMs = 30000;
// The hunt scans in short scans (C5ReconRadio::kHuntScanMs); the next starts
// once one ends, but never sooner than this after the last start.
constexpr uint32_t kHuntScanRetryMs = 50;
bool gHuntPrevMonitoring = false;
ReconTarget gHuntPrevTarget = ReconTarget::None;
uint32_t gHuntStartMs = 0;
uint32_t gLastHuntPollMs = 0;
uint32_t gLastHuntScanTryMs = 0;
uint32_t gHuntScanRestarts = 0;

uint32_t hardwareRandom()
{
    return esp_random();
}

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

// The Hunter's settings store (2026-11-04). It keeps one thing between boots:
// the SetSensors allow-list, in NVS namespace "hunter", key "sensors". Every
// other ApplicationSettings field is set on each boot: early warning OFF (the
// Hunter powers up stopped), sleep mode off, as core's defaults otherwise. A stored value that is not a valid mask (a corrupt
// or future one) is ignored and every sensor stays on.
class HunterSettingsStore : public SettingsStore {
public:
    void load(ApplicationSettings &out) override
    {
        out = ApplicationSettings{};
        // The Hunter powers up stopped (Michael, 2026-11-04): no background
        // early-warning sweep until the operator presses START. Core's default
        // (early warning on) still holds for the watches, which boot without a
        // controller; the Hunter always has one.
        out.earlyWarningEnabled = false;
        // Trackers off until the operator turns them on (2026-11-06, BringUpLogic.h).
        out.sensorMask = kHunterDefaultSensorMask;
        Preferences prefs;
        // Read-only open fails until the namespace has been written once.
        if (!prefs.begin("hunter", true)) return;
        const uint32_t mask = prefs.getULong("sensors", kHunterDefaultSensorMask);
        const uint8_t holdSec = prefs.getUChar("evhold", kDefaultEvidenceHoldSec);
        prefs.end();
        if (recon::validSensorMask(mask)) out.sensorMask = mask;
        // A stored hold above the maximum (corrupt or future) is ignored.
        if (holdSec <= kMaxEvidenceHoldSec) out.evidenceHoldSec = holdSec;
    }
    void save(const ApplicationSettings &settings) override
    {
        Preferences prefs;
        if (!prefs.begin("hunter", false)) return;
        prefs.putULong("sensors", settings.sensorMask);
        prefs.putUChar("evhold", settings.evidenceHoldSec);
        prefs.end();
    }
};
HunterSettingsStore gSettingsStore;
uint32_t gSensorMaskSaved = kAllSensorsMask;

// SetSensors changes core's setting (and core's candidate sink applies it at
// once); the platform writes it to flash here, only when it changed, so a
// Hunter that reboots without a watch nearby keeps the operator's choice.
void saveSensorsIfChanged()
{
    const uint32_t mask = gCore.settings().sensorMask;
    if (mask == gSensorMaskSaved) return;
    gSensorMaskSaved = mask;
    gCore.saveSettings();
    Serial.printf("sensors 0x%05lX saved at %lu ms\n", static_cast<unsigned long>(mask),
                  static_cast<unsigned long>(millis()));
}

// SetEvidenceHold the same way: the loop hands the new value to the Wi-Fi
// task's opener (gEvidenceHoldSec) and writes it to flash, only when it
// changed. A window already collecting keeps the time it opened with.
void applyEvidenceHoldIfChanged()
{
    const uint8_t holdSec = gCore.settings().evidenceHoldSec;
    if (holdSec == gEvidenceHoldSec) return;
    gEvidenceHoldSec = holdSec;
    gCore.saveSettings();
    if (holdSec == 0) Serial.printf("evidence hold Off saved at %lu ms\n", static_cast<unsigned long>(millis()));
    else Serial.printf("evidence hold %u s saved at %lu ms\n", static_cast<unsigned>(holdSec),
                       static_cast<unsigned long>(millis()));
}

// Saves the oldest closed evidence window that has not reached the card, one
// per loop pass, while a log file is open (no card: the windows wait in PSRAM).
// A failed save waits kEvidenceRetryMs, so a bad card does not flash the LED
// every pass.
constexpr uint32_t kEvidenceRetryMs = 10000;
void applyLed(); // below: card traffic scrambles the LED, so it is resent after a save
void saveEvidenceIfDue(uint32_t now)
{
    if (gEvidencePcap == nullptr || !gSdLog.hasCardLog()) return;
    if (gEvidenceRetryAtMs != 0 && static_cast<int32_t>(now - gEvidenceRetryAtMs) < 0) return;
    gEvidenceRetryAtMs = 0;
    uint32_t eventId = 0;
    evidence::WindowInfo info;
    size_t n = 0;
    lockEvidence(nullptr);
    if (gEvidence.nextToSave(now, eventId))
        n = evidence::buildWindowPcap(gEvidence, eventId, now, gEvidencePcap, evidence::kMaxWindowPcapBytes, info);
    if (eventId != 0 && n == 0) gEvidence.markSaved(eventId, info.seen); // cannot build: never retry it
    unlockEvidence(nullptr);
    if (eventId == 0 || n == 0) return;
    SdEvidenceCounts counts;
    counts.seen = info.seen;
    counts.kept = info.retained;
    counts.dropped = info.dropped;
    counts.truncated = info.truncated;
    counts.collecting = info.collecting;
    bool usedBus = false;
    const bool saved = gSdLog.saveEvidence(eventId, gEvidencePcap, n, counts, now, gBoot.bootCount(), usedBus);
    if (usedBus) applyLed();
    if (saved) {
        lockEvidence(nullptr);
        gEvidence.markSaved(eventId, info.seen);
        unlockEvidence(nullptr);
    } else {
        gEvidenceRetryAtMs = now + kEvidenceRetryMs;
        if (gEvidenceRetryAtMs == 0) gEvidenceRetryAtMs = 1;
    }
}

// ---- Pulling card files over USB serial (2026-11-04, CardTransferLogic.h) ----
// The PC sends "card list" or "card get <name>"; replies are "=== card ..." lines.
// A file goes out a few lines per loop pass, only as fast as the PC reads, so
// scanning and the Link keep running. The 10 s report pauses while a file is
// sent. A PC that stops reading for kCardStallMs ends the send, so the card
// log is never left closed.
constexpr uint32_t kCardStallMs = 5000;
constexpr size_t kCardLinesPerPass = 16;
char gCardCmd[kCardCommandSize];
size_t gCardCmdLen = 0;
bool gCardCmdOverflow = false;
struct CardSend {
    bool active = false;
    char name[kCardNameSize] = {0};
    uint32_t size = 0;
    uint32_t sent = 0;
    uint32_t crc = 0;
    uint32_t lastProgressMs = 0;
};
CardSend gCardSend;
bool cardLinkActive(); // below: a card read over the Link holds the card's one open file

void cardLine(const char *fmt, ...)
{
    char line[kCardLineSize];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    if (n <= 0) return;
    const size_t len = static_cast<size_t>(n) < sizeof(line) ? static_cast<size_t>(n) : sizeof(line) - 1;
    Serial.write(reinterpret_cast<const uint8_t *>(line), len);
}

void cardListEntry(const char *name, uint32_t size, void *context)
{
    ++*static_cast<uint32_t *>(context);
    cardLine("=== card entry %s %lu\n", name, static_cast<unsigned long>(size));
}

void finishCardSend(const char *error)
{
    bool usedBus = false;
    gSdLog.endRead(usedBus);
    if (usedBus) applyLed();
    if (error != nullptr) cardLine("=== card error %s %s\n", gCardSend.name, error);
    else cardLine("=== card file end %s %lu crc32 %08lx\n", gCardSend.name, static_cast<unsigned long>(gCardSend.sent),
                  static_cast<unsigned long>(gCardSend.crc));
    gCardSend = CardSend{};
}

void handleCardCommand(const char *line, uint32_t now)
{
    char name[kCardNameSize];
    const CardCommand c = parseCardCommand(line, name, sizeof(name));
    if (c == CardCommand::None) return;
    if (gCardSend.active) {
        cardLine("=== card error - A file is already being sent. Wait for its end line.\n");
        return;
    }
    if (c == CardCommand::Bad) {
        cardLine("=== card error - That is not a card command, or not a file the Hunter serves.\n");
        return;
    }
    if (cardLinkActive()) {
        cardLine("=== card error - LayerScope is pulling a file over the Link. Try again when it finishes.\n");
        if (c == CardCommand::List) cardLine("=== card list end 0\n");
        return;
    }
    bool usedBus = false;
    if (c == CardCommand::List) {
        uint32_t count = 0;
        cardLine("=== card list begin\n");
        const bool ok = gSdLog.listCard(cardListEntry, &count, usedBus);
        if (usedBus) applyLed();
        if (!ok) cardLine("=== card error - No SD card log is open, so the card cannot be listed.\n");
        cardLine("=== card list end %lu\n", static_cast<unsigned long>(count));
        return;
    }
    uint32_t size = 0;
    const bool ok = gSdLog.beginRead(name, size, usedBus);
    if (usedBus) applyLed();
    if (!ok) {
        cardLine("=== card error %s The Hunter could not open this file. It may not exist, or no card log is open.\n", name);
        return;
    }
    gCardSend = CardSend{};
    gCardSend.active = true;
    strncpy(gCardSend.name, name, sizeof(gCardSend.name) - 1);
    gCardSend.size = size;
    gCardSend.lastProgressMs = now;
    cardLine("=== card file begin %s %lu\n", name, static_cast<unsigned long>(size));
}

// The unit's name at the bench (2026-11-04; from the chip since 2026-11-05):
// "unit get" on USB serial prints the name and the Bluetooth address it is
// built from. Nothing is numbered or saved. True when the line was a unit command.
bool handleUnitCommand(const char *line)
{
    const UnitCommand c = parseUnitCommand(line);
    if (c == UnitCommand::None) return false;
    if (c == UnitCommand::Bad) {
        Serial.println("=== unit error The name comes from the chip and cannot be set. Send \"unit get\" to read it.");
        return true;
    }
    uint8_t mac[6] = {};
    esp_read_mac(mac, ESP_MAC_BT);
    Serial.printf("=== unit name %s mac %02X:%02X:%02X:%02X:%02X:%02X\n", gLink.advertisedName(), mac[0], mac[1], mac[2],
                  mac[3], mac[4], mac[5]);
    return true;
}

void serviceCardSerial(uint32_t now)
{
    while (Serial.available() > 0) {
        const int ch = Serial.read();
        if (ch < 0) break;
        if (ch == '\n') {
            if (!gCardCmdOverflow) {
                gCardCmd[gCardCmdLen] = '\0';
                if (!handleUnitCommand(gCardCmd)) handleCardCommand(gCardCmd, now);
            }
            gCardCmdLen = 0;
            gCardCmdOverflow = false;
        } else if (gCardCmdLen + 1 < sizeof(gCardCmd)) {
            gCardCmd[gCardCmdLen++] = static_cast<char>(ch);
        } else {
            gCardCmdOverflow = true;
        }
    }
    if (!gCardSend.active) return;
    uint8_t buf[kCardDataBytes];
    char b64[kCardLineSize];
    bool usedBus = false;
    for (size_t i = 0; i < kCardLinesPerPass; ++i) {
        if (gCardSend.sent >= gCardSend.size) {
            if (usedBus) applyLed();
            finishCardSend(nullptr);
            return;
        }
        if (Serial.availableForWrite() < 96) break; // the PC has not read the last lines yet
        bool bus = false;
        const size_t n = gSdLog.readChunk(buf, sizeof(buf), bus);
        usedBus = usedBus || bus;
        if (n == 0) {
            if (usedBus) applyLed();
            finishCardSend("The card stopped returning data before the end of the file.");
            return;
        }
        gCardSend.crc = crc32Update(gCardSend.crc, buf, n);
        gCardSend.sent += static_cast<uint32_t>(n);
        gCardSend.lastProgressMs = now;
        base64Encode(buf, n, b64, sizeof(b64));
        cardLine("=== card data %s\n", b64);
    }
    if (usedBus) applyLed();
    if (gCardSend.active && now - gCardSend.lastProgressMs > kCardStallMs)
        finishCardSend("The PC stopped reading, so the send was ended.");
}

// The 10 s report stops at its first print that cannot get through (2026-10-31). With the
// cable in and nobody reading, one print waits at most about 40 ms (kSerialTxTimeoutMs per
// chunk) and comes back short; the rest of that report is dropped instead of each line
// waiting in turn, which held the loop about 1 s on the first report after the logger closed.
// Connect, disconnect, detection, boot, and dump lines do not go through here.
bool gReportCut = false;     // the report in progress has been cut short
uint32_t gReportsCut = 0;    // reports cut short, for the notice when a reader returns
void reportf(const char *fmt, ...)
{
    if (gReportCut) return;
    char line[512];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    if (n <= 0) return;
    const size_t len = static_cast<size_t>(n) < sizeof(line) ? static_cast<size_t>(n) : sizeof(line) - 1;
    if (Serial.write(reinterpret_cast<const uint8_t *>(line), len) < len) gReportCut = true;
}

void printHistogram(const char *name, const recon::IntervalHistogram &h)
{
    reportf("%s n=%lu min=%lu max=%lu mean=%lu buckets(<=5,10,20,50,100,250,1000,more)=",
                  name, static_cast<unsigned long>(h.count()), static_cast<unsigned long>(h.minMs()),
                  static_cast<unsigned long>(h.maxMs()),
                  static_cast<unsigned long>(h.count() ? h.sumMs() / h.count() : 0));
    for (uint8_t i = 0; i < recon::IntervalHistogram::kBuckets; ++i) {
        reportf("%s%lu", i ? "," : "", static_cast<unsigned long>(h.bucket(i)));
    }
    reportf("\n");
}

#if defined(LAYERTIME_MEASURE)
// ---- Link 0.2 Phase 0 measurement (claude/link_0_2_phase0_plan.md) ------------
// Every line starts with "M<n>" so the log can be read back by machine.

uint32_t meanOf(uint32_t sum, uint32_t n) { return n ? sum / n : 0; }

void printMeasure(const char *when)
{
    const C5ReconRadio::Measure &m = gRadio.measure();
    reportf("[%s] M1 wifi frames %lu fcs match %lu mismatch %lu short %lu max len %lu "
                  "len<=64,128,256,512,1024,1600,more %lu,%lu,%lu,%lu,%lu,%lu,%lu\n", when,
                  static_cast<unsigned long>(m.wifiFrames), static_cast<unsigned long>(m.fcsMatch),
                  static_cast<unsigned long>(m.fcsMismatch), static_cast<unsigned long>(m.tooShort),
                  static_cast<unsigned long>(m.maxLen), static_cast<unsigned long>(m.lenBuckets[0]),
                  static_cast<unsigned long>(m.lenBuckets[1]), static_cast<unsigned long>(m.lenBuckets[2]),
                  static_cast<unsigned long>(m.lenBuckets[3]), static_cast<unsigned long>(m.lenBuckets[4]),
                  static_cast<unsigned long>(m.lenBuckets[5]), static_cast<unsigned long>(m.lenBuckets[6]));
    reportf("[%s] M2 us crc mean %lu max %lu | classify mean %lu max %lu | copy psram mean %lu max %lu n %lu | "
                  "rings %s | peak/s mgmt %lu deauth %lu\n", when,
                  static_cast<unsigned long>(meanOf(m.crcUsSum, m.fcsMatch + m.fcsMismatch)),
                  static_cast<unsigned long>(m.crcUsMax),
                  static_cast<unsigned long>(meanOf(m.classifyUsSum, m.classified)),
                  static_cast<unsigned long>(m.classifyUsMax),
                  static_cast<unsigned long>(meanOf(m.copyPsramUsSum, m.copyPsram)),
                  static_cast<unsigned long>(m.copyPsramUsMax), static_cast<unsigned long>(m.copyPsram),
                  m.ringsOk ? "ok" : "NOT ALLOCATED", static_cast<unsigned long>(m.mgmtPerSecPeak),
                  static_cast<unsigned long>(m.deauthPerSecPeak));
    reportf("[%s] M3 free internal %lu largest %lu | free psram %lu largest %lu\n", when,
                  static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                  static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                  static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
                  static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)));
    reportf("[%s] M4 adverts %lu extended %lu max len %lu len<=31,62,255,more %lu,%lu,%lu,%lu | "
                  "peak/s %lu | addresses this min %lu peak/min %lu\n", when,
                  static_cast<unsigned long>(m.adverts), static_cast<unsigned long>(m.advExtended),
                  static_cast<unsigned long>(m.advMaxLen), static_cast<unsigned long>(m.advLenBuckets[0]),
                  static_cast<unsigned long>(m.advLenBuckets[1]), static_cast<unsigned long>(m.advLenBuckets[2]),
                  static_cast<unsigned long>(m.advLenBuckets[3]), static_cast<unsigned long>(m.advertsPerSecPeak),
                  static_cast<unsigned long>(m.addressesThisMin), static_cast<unsigned long>(m.addressesPerMinPeak));
    reportf("[%s] M5 mtu %u bulk runs %lu\n", when, static_cast<unsigned>(gLink.mtu()),
                  static_cast<unsigned long>(gLink.bulkRuns()));
}

// M3: can evidence windows of these sizes be allocated now, with Recon and
// the Link running? Each pool is freed again at once.
void measureAllocation()
{
    static const uint16_t kWindows[3] = {8, 16, 32};
    constexpr size_t kFramesPerWindow = 16;
    constexpr size_t kFrameBytes = 256;
    for (uint16_t w : kWindows) {
        const size_t bytes = static_cast<size_t>(w) * kFramesPerWindow * kFrameBytes;
        void *p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
        void *i = heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        Serial.printf("M3 pool %u windows x %u frames x %u bytes = %lu bytes: psram %s, internal %s\n",
                      static_cast<unsigned>(w), static_cast<unsigned>(kFramesPerWindow),
                      static_cast<unsigned>(kFrameBytes), static_cast<unsigned long>(bytes),
                      p ? "ok" : "FAILED", i ? "ok" : "FAILED");
        heap_caps_free(p);
        heap_caps_free(i);
    }
}
#endif

// The boot record's last-known state (gBoot), kept current every report
// period whether or not the report is printed.
void touchBootState()
{
    const AcquisitionStatus a = gMonitor.acquisition();
    gBoot.touch(millis() / 1000, static_cast<uint32_t>(gMode) | (acqBits(a) << 8) |
                                     (static_cast<uint32_t>(static_cast<uint8_t>(a.active)) << 16) |
                                     (gLink.connected() ? (1u << 24) : 0u));
}

void printCounters(const char *when)
{
    gReportCut = false;
    const C5ReconRadio::Counters &c = gRadio.counters();
    lockEvents(nullptr);
    const ReconState r = gCore.reconState();
    unlockEvents(nullptr);
    const AcquisitionStatus a = gMonitor.acquisition();
    touchBootState();
    char boot[120];
    gBoot.format(boot, sizeof(boot));
    reportf("[%s] %s\n", when, boot);
    if (gRadio.hunting()) {
        reportf("[%s] hunt on for %lu s | sightings %lu | scan cycles %lu | adverts %lu | heap %lu\n", when,
                static_cast<unsigned long>((millis() - gHuntStartMs) / 1000),
                static_cast<unsigned long>(gRadio.huntSightings()), static_cast<unsigned long>(gHuntScanRestarts),
                static_cast<unsigned long>(c.adverts), static_cast<unsigned long>(ESP.getFreeHeap()));
    }
    const uint32_t runS = (millis() - gRunStartMs) / 1000;
    // The Increment 2A [periodic] line, field for field.
    reportf("[%s] mode %s run %lu s uptime %lu s sel %s act %s mon %d ew %d rest %d ch %u | "
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
    reportf("[%s] link %s peer %s session %04X changeSeq %lu heartbeat %u | connects %lu disconnects %lu | "
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
    const BandTotals &t = gTally.totals();
    reportf("[%s] screen totals 5GHz %lu 2.4GHz %lu BLE %lu other %lu | nodes %u\n", when,
                  static_cast<unsigned long>(t.wifi5GHz), static_cast<unsigned long>(t.wifi2_4GHz),
                  static_cast<unsigned long>(t.ble), static_cast<unsigned long>(t.other),
                  static_cast<unsigned>(gRadio.peerCount(millis())));
    const C5SdLog::Counters &sd = gSdLog.counters();
    reportf("[%s] sd ring %s queued %u dropped %lu discarded %lu | card %s file %s | mounts %lu failures %lu "
                  "files %lu lines %lu writes %lu errors %lu unformatted %lu | heap at mount %lu to %lu\n",
                  when, sd.ringOk ? "ok" : "FAILED", static_cast<unsigned>(gSdLog.queued()),
                  static_cast<unsigned long>(gSdLog.dropped()), static_cast<unsigned long>(sd.discarded),
                  sd.mounted ? "mounted" : "-", sd.fileOpen ? sd.fileName : "-", static_cast<unsigned long>(sd.mounts),
                  static_cast<unsigned long>(sd.mountFailures), static_cast<unsigned long>(sd.files),
                  static_cast<unsigned long>(sd.lines), static_cast<unsigned long>(sd.writes),
                  static_cast<unsigned long>(sd.writeErrors), static_cast<unsigned long>(sd.unformatted),
                  static_cast<unsigned long>(sd.heapBeforeMount), static_cast<unsigned long>(sd.heapAfterMount));
    reportf("[%s] sd evidence files %lu bytes %lu errors %lu | repeats %lu | windows lost unsaved %lu\n", when,
                  static_cast<unsigned long>(sd.evidenceFiles), static_cast<unsigned long>(sd.evidenceBytes),
                  static_cast<unsigned long>(sd.evidenceErrors), static_cast<unsigned long>(sd.repeats),
                  static_cast<unsigned long>(gEvidence.lostUnsaved()));
    const recon::ReconScheduler &sched = gMonitor.scheduler();
    reportf("[%s] plan %u channels | passes %lu last %lu ms | rest jitter %lu ms | channel refusals %lu\n", when,
                  static_cast<unsigned>(sched.planCount()), static_cast<unsigned long>(sched.passes()),
                  static_cast<unsigned long>(sched.lastPassMs()), static_cast<unsigned long>(sched.restJitterMs()),
                  static_cast<unsigned long>(c.channelRefusals));
    reportf("frames by channel:");
    for (uint8_t i = 0; i < sched.planCount(); ++i) {
        const uint8_t ch = sched.planChannel(i);
        reportf(" %u=%lu", ch, static_cast<unsigned long>(c.framesByChannel[ch]));
    }
    reportf("\n");
    printHistogram("loop period ms", gLoopPeriod);
    printHistogram("hop interval ms", gHopInterval);
    printHistogram("pass ms", gPassTime);
    printHistogram("pass to pass ms", gPassToPass);
#if defined(LAYERTIME_MEASURE)
    printMeasure(when);
#endif
    if (gReportCut) ++gReportsCut;
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
    gPassTime.clear();
    gPassToPass.clear();
    gLastPassEndMs = 0;
    gLastHopCount = 0;
    gLastHopMs = 0;
    gStage.record(recon::Stage::RunStart, 0, 0, static_cast<uint32_t>(mode));
    gSdLog.noteMode(modeName(mode), now);
    recordHeap();
    Serial.printf("=== run start mode %s run %lu at %lu ms\n", modeName(mode), static_cast<unsigned long>(gRun),
                  static_cast<unsigned long>(now));
}

void showStatus()
{
    // The Hunter screen (Michael, 2026-10-02; renamed 2026-11-01). Line 0, "Hunter", is set
    // once when the screen starts. Each line must fit the 96 px status
    // column (about 14 characters). The 5 GHz total counts only if the
    // channel plan holds 5 GHz channels (the boot line "Channel plan").
    char line[40];
    const BandTotals &t = gTally.totals(); // single-word reads for the LCD
    snprintf(line, sizeof(line), "5GHz EV: %lu", static_cast<unsigned long>(t.wifi5GHz));
    gDisplay.setLine(1, line);
    snprintf(line, sizeof(line), "2.4GHz EV: %lu", static_cast<unsigned long>(t.wifi2_4GHz));
    gDisplay.setLine(2, line);
    snprintf(line, sizeof(line), "BLE EV: %lu", static_cast<unsigned long>(t.ble));
    gDisplay.setLine(3, line);
    // During a hunt the last line counts the Vixen's sightings, so a BOOT
    // press on the Hunter shows whether it hears the Vixen at all.
    if (gRadio.hunting()) snprintf(line, sizeof(line), "VIXEN: %lu", static_cast<unsigned long>(gRadio.huntSightings()));
    else snprintf(line, sizeof(line), "NODES: %u", static_cast<unsigned>(gRadio.peerCount(millis())));
    gDisplay.setLine(4, line);
    // The eye: green with a LayerTime or LayerScope connected, red without.
    // The cheek: the mesh, red until the mesh exists.
    gDisplay.setIndicators(gLink.connected() ? LinkIndicator::Connected : LinkIndicator::NoWatch,
                           MeshIndicator::NotConnected);
}

// Short BOOT press: start the screen the first time, light it, and show the
// current status (it is drawn on the next service()).
void wakeScreen(uint32_t now)
{
    if (!gDisplayStarted) {
        gDisplayStarted = true;
        gDisplayOk = gDisplay.begin();
        gDisplay.setLine(0, gLink.advertisedName()[0] != '\0' ? gLink.advertisedName() : "Hunter");
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

// HUNT v1: Recon stops (Wi-Fi off, early warning paused) and the
// continuous Bluetooth scan for the LayerVixen starts.
void beginHunt(uint32_t now)
{
    const AcquisitionStatus a = gMonitor.acquisition();
    gHuntPrevMonitoring = a.monitoring;
    gHuntPrevTarget = a.selected;
    gMonitor.stop();
    gRadio.suspendWifiForHunt();
    delay(120); // the radio settles after Wi-Fi stops, as before every Recon BLE scan
    gRadio.startHuntScan();
    gLink.setHunting(true);
    gHuntStartMs = now;
    gLastHuntScanTryMs = now;
    gHuntScanRestarts = 0;
    Serial.printf("hunt started (any LayerVixen) at %lu ms; Recon paused, was %s\n", static_cast<unsigned long>(now),
                  a.monitoring ? recon::detectorName(a.selected) : (a.earlyWarningEnabled ? "early warning" : "stopped"));
}

// HUNT v1: the scan stops and Recon resumes as it was before the hunt.
void finishHunt(uint32_t now, const char *why, uint32_t sightings)
{
    gRadio.stopHuntScan();
    gRadio.resumeWifiAfterHunt();
    if (gHuntPrevMonitoring) gMonitor.start(gHuntPrevTarget);
    else gMonitor.stopManual();
    gLink.setHunting(false);
    Serial.printf("hunt stopped (%s) at %lu ms after %lu s, %lu sightings, %lu scan cycles; Recon resumed\n", why,
                  static_cast<unsigned long>(now), static_cast<unsigned long>((now - gHuntStartMs) / 1000),
                  static_cast<unsigned long>(sightings), static_cast<unsigned long>(gHuntScanRestarts));
}

// C5Link hands GET_FRAMES and GET_FRAME_BYTES here, on the loop task, with
// this connection's notification size and a send that waits for buffers.
// ---- Card over the Link (2026-11-04, core/link/LinkCard.h) --------------------
// LayerScope pulls the SD card's files in the field. CARD_LIST answers at once
// (one short frame per file); CARD_READ starts a stream that serviceCardLink()
// continues a few frames per loop pass, so the loop never stalls on a large
// log. One card read at a time, shared with the USB pull.
link::card::Streamer gCardLink;
C5Link::EvidenceSend gCardLinkSend = nullptr;
void *gCardLinkSendContext = nullptr;
bool gCardLinkUsedBus = false;
uint32_t gCardLinkFiles = 0;

// Clear Card (2026-11-06): the request that started the clear, answered when
// it ends. One clear at a time; reads and pulls are refused while it runs.
uint8_t gCardClearReqId = 0;
C5Link::EvidenceSend gCardClearSend = nullptr;
void *gCardClearSendContext = nullptr;

bool cardLinkActive() { return gCardLink.state() == link::card::Streamer::State::Running; }

void sendCardResult(uint8_t reqId, uint8_t op, CommandResult r, C5Link::EvidenceSend send, void *ctx)
{
    uint8_t out[link::kMaxFrame];
    link::Result res;
    res.reqId = reqId;
    res.commandType = op;
    res.commandResult = static_cast<uint8_t>(r);
    send(out, link::encodeResult(res, out), ctx);
}

struct CardListContext {
    uint8_t reqId;
    C5Link::EvidenceSend send;
    void *sendContext;
    uint8_t count;
    bool ok;
};
void cardListToLink(const char *name, uint32_t size, void *context)
{
    CardListContext *c = static_cast<CardListContext *>(context);
    link::card::FileId id;
    if (!c->ok || c->count == 255 || !link::card::fileIdFromName(name, id)) return;
    uint8_t out[link::card::kCardEntrySize];
    c->ok = c->send(out, link::card::encodeCardEntry(c->reqId, id, size, out), c->sendContext);
    if (c->ok) ++c->count;
}

size_t cardLinkRead(uint8_t *out, size_t max, void *)
{
    bool bus = false;
    const size_t n = gSdLog.readChunk(out, max, bus);
    gCardLinkUsedBus = gCardLinkUsedBus || bus;
    return n;
}

void answerCardRequest(const uint8_t *request, size_t len, size_t frameSize, C5Link::EvidenceSend send, void *sendContext)
{
    const uint8_t reqId = len >= 2 ? request[1] : 0;
    const bool busy = gCardSend.active || cardLinkActive() || gSdLog.reading();
    if (request[0] == link::card::kOpCardList) {
        if (len != link::card::kCardListSize) {
            uint8_t out[link::kMaxFrame];
            link::Error e;
            e.reqId = reqId;
            e.status = link::LinkStatus::BadLength;
            send(out, link::encodeError(e, out), sendContext);
            return;
        }
        if (busy || !gSdLog.hasCardLog()) {
            sendCardResult(reqId, request[0], CommandResult::NotReady, send, sendContext);
            return;
        }
        CardListContext c{reqId, send, sendContext, 0, true};
        bool usedBus = false;
        const bool listed = gSdLog.listCard(cardListToLink, &c, usedBus);
        if (usedBus) applyLed();
        if (!listed) {
            sendCardResult(reqId, request[0], CommandResult::NotReady, send, sendContext);
            return;
        }
        if (!c.ok) return; // the connection went
        uint8_t out[link::kMaxFrame];
        link::End e;
        e.reqId = reqId;
        e.count = c.count;
        send(out, link::encodeEnd(e, out), sendContext);
        return;
    }
    if (request[0] == link::card::kOpCardClear) {
        // Clear Card (Michael, 2026-11-06): erases the card completely, every
        // file and folder. LayerScope has already asked the operator to
        // confirm. Refused while any card read runs.
        if (len != link::card::kCardClearSize) {
            uint8_t out[link::kMaxFrame];
            link::Error e;
            e.reqId = reqId;
            e.status = link::LinkStatus::BadLength;
            send(out, link::encodeError(e, out), sendContext);
            return;
        }
        if (busy || !gSdLog.hasCardLog()) {
            sendCardResult(reqId, request[0], CommandResult::NotReady, send, sendContext);
            return;
        }
        bool usedBus = false;
        const bool started = gSdLog.beginClear(usedBus);
        if (usedBus) applyLed();
        if (!started) {
            sendCardResult(reqId, request[0], CommandResult::NotReady, send, sendContext);
            return;
        }
        // serviceCardClear() erases a few files per loop pass and answers at the end.
        gCardClearReqId = reqId;
        gCardClearSend = send;
        gCardClearSendContext = sendContext;
        Serial.printf("Card clear over the Link started at %lu ms\n", static_cast<unsigned long>(millis()));
        return;
    }
    link::card::FileId id;
    uint32_t offset = 0;
    uint8_t rid = 0;
    const link::card::ReadCheck check = link::card::decodeCardRead(request, len, rid, id, offset);
    if (check == link::card::ReadCheck::BadLength) {
        uint8_t out[link::kMaxFrame];
        link::Error e;
        e.reqId = reqId;
        e.status = link::LinkStatus::BadLength;
        send(out, link::encodeError(e, out), sendContext);
        return;
    }
    if (check == link::card::ReadCheck::BadFile) {
        sendCardResult(reqId, request[0], CommandResult::InvalidArgument, send, sendContext);
        return;
    }
    if (busy || !gSdLog.hasCardLog()) {
        sendCardResult(reqId, request[0], CommandResult::NotReady, send, sendContext);
        return;
    }
    char name[link::card::kCardNameSize];
    uint32_t size = 0;
    bool usedBus = false;
    if (!link::card::fileName(id, name, sizeof(name)) || !gSdLog.beginRead(name, size, usedBus)) {
        if (usedBus) applyLed();
        sendCardResult(reqId, request[0], CommandResult::InvalidArgument, send, sendContext);
        return;
    }
    if (usedBus) applyLed();
    gCardLinkSend = send;
    gCardLinkSendContext = sendContext;
    gCardLink.start(reqId, size, offset, frameSize);
    ++gCardLinkFiles;
}

// Once per loop pass while a clear runs: erase a few more files.
void serviceCardClear()
{
    if (!gSdLog.clearing()) return;
    bool usedBus = false;
    const uint32_t now = millis();
    const C5SdLog::ClearStep s =
        gSdLog.stepClear(now, gBoot.bootCount(), C5BootRecord::reasonName(gBoot.reason()), usedBus);
    if (usedBus) applyLed();
    if (s == C5SdLog::ClearStep::Running || s == C5SdLog::ClearStep::Idle) return;
    gServer.setMemoryOnly(!gSdLog.hasCardLog());
    const uint32_t erased = gSdLog.clearErased();
    Serial.printf("Card clear over the Link %s: %lu files erased, now logging to %s at %lu ms\n",
                  s == C5SdLog::ClearStep::Done ? "done" : "ended with files that would not erase",
                  static_cast<unsigned long>(erased),
                  gSdLog.hasCardLog() ? gSdLog.counters().fileName : "nothing (no card log)",
                  static_cast<unsigned long>(now));
    for (uint8_t i = 0; gSdLog.clearLeftover(i) != nullptr; ++i)
        Serial.printf("Card clear could not erase %s\n", gSdLog.clearLeftover(i));
    if (gCardClearSend == nullptr) return;
    if (s == C5SdLog::ClearStep::Done) {
        uint8_t out[link::kMaxFrame];
        link::End e;
        e.reqId = gCardClearReqId;
        e.count = static_cast<uint8_t>(erased > 255 ? 255 : erased);
        gCardClearSend(out, link::encodeEnd(e, out), gCardClearSendContext);
    } else {
        sendCardResult(gCardClearReqId, link::card::kOpCardClear, CommandResult::NotReady, gCardClearSend,
                       gCardClearSendContext);
    }
    gCardClearSend = nullptr;
}

// Once per loop pass: the next few frames of a card read, then close the file.
void serviceCardLink()
{
    if (!cardLinkActive()) return;
    gCardLinkUsedBus = false;
    const link::card::Streamer::State s = gCardLink.step(cardLinkRead, nullptr, gCardLinkSend, gCardLinkSendContext, 8);
    if (s != link::card::Streamer::State::Running) {
        bool bus = false;
        gSdLog.endRead(bus);
        gCardLinkUsedBus = gCardLinkUsedBus || bus;
        if (s == link::card::Streamer::State::Failed)
            Serial.printf("card read over the Link stopped after %lu bytes (connection gone or card error)\n",
                          static_cast<unsigned long>(gCardLink.sent()));
    }
    if (gCardLinkUsedBus) applyLed();
}

void answerEvidenceRequest(const uint8_t *request, size_t len, uint32_t nowMs, size_t frameSize,
                           C5Link::EvidenceSend send, void *sendContext, void *)
{
    if (link::card::isCardOp(request[0])) {
        answerCardRequest(request, len, frameSize, send, sendContext);
        return;
    }
    const uint32_t id = link::evidence::requestEventId(request, len);
    bool known = false;
    uint8_t kind = 0;
    lockEvents(nullptr);
    for (uint8_t i = 0; i < gCore.eventCount(); ++i) {
        if (gCore.event(i).eventId == id) {
            known = true;
            kind = static_cast<uint8_t>(gCore.event(i).sourceKind);
            break;
        }
    }
    unlockEvents(nullptr);
    link::evidence::Access a;
    a.store = &gEvidence;
    a.lock = lockEvidence;
    a.unlock = unlockEvidence;
    if (request[0] == link::evidence::kOpGetFrames)
        link::evidence::answerGetFrames(a, request, len, known, kind, nowMs, send, sendContext);
    else
        link::evidence::answerGetFrameBytes(a, request, len, known, frameSize, send, sendContext);
}

// C5Link hands HUNT_START, HUNT_STOP and GET_HUNT here, on the loop task.
size_t answerHuntRequest(const uint8_t *request, size_t len, uint32_t nowMs, uint8_t *reply, void *)
{
    const bool before = gRadio.hunting();
    const uint32_t sightings = gRadio.huntSightings();
    const size_t n = gRadio.answerHunt(request, len, nowMs, reply);
    const bool after = gRadio.hunting();
    gLastHuntPollMs = nowMs;
    if (!before && after) beginHunt(nowMs);
    else if (before && !after) finishHunt(nowMs, "the watch stopped it", sightings);
    else if (before && after && request[0] == link::hunt::kOpHuntStart) {
        Serial.printf("hunt restarted by the watch at %lu ms\n", static_cast<unsigned long>(nowMs));
    }
    return n;
}

// HUNT v1, once per loop pass: ends a hunt nobody polls, and restarts the
// scan if it ever stopped.
void serviceHunt(uint32_t now)
{
    if (!gRadio.hunting()) return;
    if (now - gLastHuntPollMs >= kHuntIdleStopMs) {
        const uint32_t sightings = gRadio.huntSightings();
        gRadio.endHunt();
        finishHunt(now, "no poll from the watch for 30 s", sightings);
        return;
    }
    if (!gRadio.bleScanning() && now - gLastHuntScanTryMs >= kHuntScanRetryMs) {
        // The last short scan ended (normal, twice a second): start the next.
        gLastHuntScanTryMs = now;
        ++gHuntScanRestarts;
        gRadio.startHuntScan();
    }
}

} // namespace

void setup()
{
    gBoot.begin(); // before anything else: the reset reason of this boot
    C5SdLog::holdCardDeselected(); // the card's CS floats otherwise (TDongleC5Pins.h)
    Serial.begin(115200);
    Serial.setTxTimeoutMs(kSerialTxTimeoutMs); // see kSerialTxTimeoutMs
    const uint32_t waitStart = millis();
    while (!Serial && millis() - waitStart < kSerialWaitMs) delay(10);

    gEventLock = xSemaphoreCreateMutex();
    gEvidenceLock = xSemaphoreCreateMutex();

    // Evidence store in PSRAM (about 2.4 MB of the 7 MB free; Phase 0 M3).
    {
        const size_t bytes = evidence::EvidenceStore::poolBytes();
        void *pool = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        const bool ok = pool != nullptr && gEvidence.attach(pool, bytes);
        Serial.printf("Evidence store %s: %lu KB in PSRAM, %u windows of %u frames, %u-frame ring, %u-byte frame cap\n",
                      ok ? "ready" : "NOT AVAILABLE", static_cast<unsigned long>(bytes / 1024),
                      static_cast<unsigned>(evidence::kWindows), static_cast<unsigned>(evidence::kWindowFrames),
                      static_cast<unsigned>(evidence::kRingFrames), static_cast<unsigned>(evidence::kMaxFrameBytes));
        gEvidenceScheduler = &gMonitor.scheduler();
        if (ok) gRadio.setFrameTap(evidenceTap, nullptr);
        // The card copy's buffer: one whole window as a pcap file.
        if (ok) gEvidencePcap = static_cast<uint8_t *>(
            heap_caps_malloc(evidence::kMaxWindowPcapBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        Serial.printf("Evidence to SD card %s: %lu KB buffer, files ev_NNNN/event_EEEEE.pcap beside the log\n",
                      gEvidencePcap != nullptr ? "ready" : "NOT AVAILABLE",
                      static_cast<unsigned long>(evidence::kMaxWindowPcapBytes / 1024));
    }

    // The LED shares the LCD's SPI bus (C5Led.h): start the bus, then send
    // the LED its state, so it is dark from boot even if it powered up lit.
    gLed.begin();
    applyLed();
    pinMode(pins::kBootButton, INPUT); // external 10 k pull-up (R16)


    esp_chip_info_t chip;
    esp_chip_info(&chip);
    Serial.println();
    Serial.println("Hunter (T-Dongle-C5): Recon and LayerTime Link 0.1 (Slice 1, Increment 2B, architecture A)");
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
    {
        const bool sdOk = gSdLog.begin();
        Serial.printf("SD log: %s, %u records of %u bytes; card looked for at boot and at each write; "
                      "writes when %u records wait or the watch connects or disconnects\n",
                      sdOk ? "ring in PSRAM" : "PSRAM allocation FAILED", static_cast<unsigned>(C5SdLog::kRingCapacity),
                      static_cast<unsigned>(sizeof(SdRecord)), static_cast<unsigned>(C5SdLog::kRingCapacity / 2));
        // The card is the first thing that sets one Hunter apart from another
        // (Michael, 2026-11-05), so it is looked for here, before the radio and
        // the Link start: the first Status the phone gets already says whether
        // this Hunter keeps a card. A card put in later is still found at the
        // next write. No card is not an error and the screen says nothing.
        if (gSdLog.service(millis(), gBoot.bootCount(), C5BootRecord::reasonName(gBoot.reason()), false)) applyLed();
        if (gSdLog.hasCardLog()) Serial.printf("SD card found, logging to %s\n", gSdLog.counters().fileName);
        if (gSdLog.counters().legacyRenamed > 0)
            Serial.printf("Renamed %lu old layerwand_NNNN.log files to hunter_NNNN.log\n",
                          static_cast<unsigned long>(gSdLog.counters().legacyRenamed));
        else Serial.println("No SD card: events and evidence are kept in memory only");
    }
    gRadio.setStageLog(&gStage);
    gRadio.begin(true); // Wi-Fi first, as in 2A; both bands
#if defined(LAYERTIME_MEASURE)
    {
        // The measurement must not take internal RAM from the Link; this line
        // shows it does not (both numbers should match within a few bytes).
        const size_t before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        gRadio.beginMeasure();
        const size_t after = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        Serial.printf("MEASURE build: Link 0.2 Phase 0. PSRAM ring and address set %s. Internal RAM free %lu before, %lu after.\n",
                      gRadio.measure().ringsOk ? "allocated" : "NOT ALLOCATED",
                      static_cast<unsigned long>(before), static_cast<unsigned long>(after));
    }
#endif
    // The channel plan: what this radio accepts under the regulatory rules it
    // runs with (no country is set here, so nothing new is written to
    // flash), 5 GHz first. An empty probe leaves the reference plan.
    gPlanCount = gRadio.probeChannelPlan(gPlan, recon::ReconScheduler::kMaxPlanChannels);
    {
        recon::ReconScheduler &sched = gMonitor.scheduler();
        sched.setChannelPlan(gPlan, gPlanCount);
        sched.setSweepMode(recon::SweepMode::FullPass);
        sched.setRandom(hardwareRandom);
        uint8_t n5 = 0;
        for (uint8_t i = 0; i < sched.planCount(); ++i) if (sched.planChannel(i) >= 32) ++n5;
        Serial.printf("Channel plan: %u channels (%u at 5 GHz, %u at 2.4 GHz)%s:", static_cast<unsigned>(sched.planCount()),
                      static_cast<unsigned>(n5), static_cast<unsigned>(sched.planCount() - n5),
                      gPlanCount == 0 ? ", PROBE FOUND NONE, reference plan" : "");
        for (uint8_t i = 0; i < sched.planCount(); ++i) Serial.printf(" %u", static_cast<unsigned>(sched.planChannel(i)));
        Serial.println();
    }

    CorePorts ports;
    ports.monitor = &gLockedMonitor;
    ports.eventLog = &gEventLogs;
    ports.settings = &gSettingsStore;
    gCore.attach(ports);
    // The sensor allow-list from the last run (every sensor but the trackers if none was saved).
    gCore.loadSettings();
    gSensorMaskSaved = gCore.settings().sensorMask;
    Serial.printf("Sensors 0x%05lX%s\n", static_cast<unsigned long>(gSensorMaskSaved),
                  gSensorMaskSaved == kAllSensorsMask ? " (all on)"
                  : gSensorMaskSaved == kHunterDefaultSensorMask ? " (trackers off)" : "");
    gEvidenceHoldSec = gCore.settings().evidenceHoldSec;
    if (gEvidenceHoldSec == 0) Serial.println("Evidence hold Off");
    else Serial.printf("Evidence hold %u s\n", static_cast<unsigned>(gEvidenceHoldSec));
    gServer.setLock(lockEvents, unlockEvents, nullptr);
    // The boot look above decided this; the loop keeps it current.
    gServer.setMemoryOnly(!gSdLog.hasCardLog());

#if defined(LAYERTIME_DIAG_WIFI_OFF)
    // Connection isolation only: retain the full app but release Wi-Fi before BLE.
    gRadio.suspendWifiForHunt();
    Serial.println("DIAGNOSTIC: Wi-Fi off before BLE; do not start monitoring");
#endif
    gHeapOk = heapIntact();
    Serial.printf("Heap before BLE init: %s, free %lu\n", gHeapOk ? "intact" : "CORRUPT",
                  static_cast<unsigned long>(ESP.getFreeHeap()));
    // NimBLE comes up here, for the Link; C5ReconRadio's BLE scans find it
    // initialised and use the same host.
    gLink.attachServer(&gServer);
    gLink.attachHunt(answerHuntRequest, nullptr);
    gLink.attachEvidence(answerEvidenceRequest, nullptr);
    gBleOk = gLink.begin(link::kCapLocalWifiMonitor | link::kCapLocalBleMonitor | link::kCapDisplay |
                         link::kCapLed | link::kCapButton | link::hunt::kCapHunt | link::kCapSensorSet |
                         link::card::kCapCardRead | link::card::kCapCardClear |
                         (gEvidence.attached() ? (link::evidence::kCapRawCapture | link::kCapEvidenceHold) : 0));
    const bool heapAfterBle = heapIntact();
    gHeapOk = gHeapOk && heapAfterBle;
    {
        uint8_t mac[6] = {};
        esp_read_mac(mac, ESP_MAC_BT);
        Serial.printf("Unit name %s, Bluetooth address %02X:%02X:%02X:%02X:%02X:%02X\n", gLink.advertisedName(),
                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
    Serial.printf("Link %s, advertising as %s, session %04X\n", gBleOk ? "up" : "FAILED", gLink.advertisedName(),
                  static_cast<unsigned>(gLink.sessionId()));
    Serial.printf("Heap after BLE init: %s, free %lu\n", heapAfterBle ? "intact" : "CORRUPT",
                  static_cast<unsigned long>(ESP.getFreeHeap()));
    Serial.printf("Schedule: hop %lu ms, a full pass then a %lu ms BLE scan, BLE-only rescan %lu ms, "
                  "early warning rest %lu ms + 0 to %lu ms, ALL last channel + 0 to %lu ms\n",
                  static_cast<unsigned long>(recon::ReconScheduler::kChannelHopMs),
                  static_cast<unsigned long>(recon::ReconScheduler::kBleScanMs),
                  static_cast<unsigned long>(recon::ReconScheduler::kBleOnlyRescanMs),
                  static_cast<unsigned long>(recon::ReconScheduler::kEarlyWarningRestMs),
                  static_cast<unsigned long>(recon::ReconScheduler::kRestJitterMaxMs),
                  static_cast<unsigned long>(recon::ReconScheduler::kPassJitterMaxMs));
    Serial.println("Recon starts stopped; START from LayerScope begins a scan. BOOT button: dump the current run.");
    Serial.println("Card over USB: send \"card list\" or \"card get <name>\" (read only).");

    // Early warning is core's default setting; apply it, and start run 1.
    applyEarlyWarningIfChanged();
    gLastAcq = gMonitor.acquisition();
    gLastLoopMs = millis();
    startRun(modeOf(gLastAcq), gLastLoopMs);
    gServer.refresh();
    gLink.updateStatus();
    showStatus();
#if defined(LAYERTIME_MEASURE)
    measureAllocation();
#endif
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

    serviceCardSerial(now);
    serviceCardLink();
    serviceCardClear();

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
    // A hunt owns the radios: the Recon schedule does not run under it.
    if (!gRadio.hunting()) gCore.pollMonitor();

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
#if defined(LAYERTIME_MEASURE)
    static uint32_t lastBulkRuns = 0;
    static uint16_t lastMtu = 0;
    if (gLink.mtu() != lastMtu) {
        lastMtu = gLink.mtu();
        Serial.printf("M5 mtu now %u at %lu ms\n", static_cast<unsigned>(lastMtu), static_cast<unsigned long>(now));
    }
    if (gLink.bulkRuns() != lastBulkRuns) {
        lastBulkRuns = gLink.bulkRuns();
        const link::measure::BulkDone &b = gLink.lastBulk();
        const uint32_t bytes = static_cast<uint32_t>(b.sent) * b.size;
        Serial.printf("M5 bulk run %lu: sent %u x %u bytes = %lu bytes in %lu ms (%lu bytes/s), mtu %u, "
                      "notify refused %lu\n", static_cast<unsigned long>(lastBulkRuns), static_cast<unsigned>(b.sent),
                      static_cast<unsigned>(b.size), static_cast<unsigned long>(bytes),
                      static_cast<unsigned long>(b.elapsedMs),
                      static_cast<unsigned long>(b.elapsedMs ? bytes * 1000ull / b.elapsedMs : 0),
                      static_cast<unsigned>(b.mtu), static_cast<unsigned long>(b.refused));
    }
#endif
    serviceHunt(now);
    // A change to early warning during a hunt is applied when the hunt ends.
    if (!gRadio.hunting()) applyEarlyWarningIfChanged();
    saveSensorsIfChanged();
    applyEvidenceHoldIfChanged();
    // Evidence windows are opened on the Wi-Fi task, which never prints; report here.
    // Only a new window prints: a chatty detector repeats several times a second,
    // so the repeat count rides along on the next window's line.
    if (gEvidenceWindows != gEvidenceWindowsReported) {
        gEvidenceWindowsReported = gEvidenceWindows;
        gEvidenceRepeatsReported = gEvidenceRepeats;
        Serial.printf("evidence windows %lu, repeats %lu, channel holds %lu at %lu ms\n",
                      static_cast<unsigned long>(gEvidenceWindowsReported),
                      static_cast<unsigned long>(gEvidenceRepeatsReported),
                      static_cast<unsigned long>(gEvidenceHolds), static_cast<unsigned long>(millis()));
    }
    gServer.refresh();
    gLink.updateStatus();
    gLink.service(now);

    bool linkChanged = false;
    if (gLink.connected() != gWasConnected) {
        linkChanged = true;
        gWasConnected = gLink.connected();
        if (gWasConnected) ++gConnects; else ++gDisconnects;
        gSdLog.noteLink(gWasConnected, gWasConnected ? gLink.peerAddress() : "", now);
        Serial.printf("Link %s %s at %lu ms\n", gWasConnected ? "connected to" : "disconnected, advertising again;",
                      gWasConnected ? gLink.peerAddress() : "", static_cast<unsigned long>(now));
        if (!gScreenAwake) showStatus(); // no redraw while lit (snapshot)
    }

    // The SD log: looks for the card at boot, then touches it only when a
    // write is due (half the ring waiting, or the watch connected or
    // disconnected this pass). Card traffic scrambles the LED. Status tells
    // the watch when events live only in memory.
    if (gSdLog.service(now, gBoot.bootCount(), C5BootRecord::reasonName(gBoot.reason()), linkChanged)) {
        applyLed();
    }
    saveEvidenceIfDue(now);
    gServer.setMemoryOnly(!gSdLog.hasCardLog());

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

    const uint32_t passes = gMonitor.scheduler().passes();
    if (passes != gLastPasses) {
        gLastPasses = passes;
        gPassTime.add(gMonitor.scheduler().lastPassMs());
        if (gLastPassEndMs != 0) gPassToPass.add(now - gLastPassEndMs);
        gLastPassEndMs = now;
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
        if (gCardSend.active) {
            // A card file is going out: the report waits so it cannot land in the file's lines.
            touchBootState();
        } else if (Serial.availableForWrite() >= kSerialDrainedBytes) {
            if (gReportsSkipped > 0 || gReportsCut > 0) {
                Serial.printf("serial: %lu reports skipped and %lu cut short while nothing read the port\n",
                              static_cast<unsigned long>(gReportsSkipped), static_cast<unsigned long>(gReportsCut));
                gReportsSkipped = 0;
                gReportsCut = 0;
            }
            printCounters("periodic");
        } else {
            ++gReportsSkipped;
            touchBootState();
        }
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
