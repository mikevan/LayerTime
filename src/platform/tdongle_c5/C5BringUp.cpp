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

// This file belongs to the tdongle_c5 build environment only. Every build
// environment compiles all of src/, so the T-Watch Ultra build sees this file
// as an empty translation unit. The tdongle_c5_recon environment
// (LAYERTIME_RECON_BASELINE) builds C5ReconBaseline.cpp's setup() and loop()
// instead of these, and the tdongle_c5_wand environment (LAYERTIME_WAND_APP)
// builds C5App.cpp's.
#if defined(LAYERTIME_TARGET_TDONGLE_C5) && !defined(LAYERTIME_RECON_BASELINE) && !defined(LAYERTIME_WAND_APP)

// Slice 1, Increments 0 and 1: T-Dongle-C5 bring-up and the LayerTime Link
// transport proof.
//
// Increment 0 proved, on the real board: the LCD, the APA102 LED, the BOOT
// button, PSRAM, and NimBLE initialising without corrupting the heap
// (arduino-esp32 #12821 on the C5). Increment 1 replaces the bare test
// advertisement with the LayerTime Link service (C5Link): Status heartbeat,
// HELLO and PING, re-advertising on disconnect, and on LAYERTIME_LINK_TEST
// builds the Probe characteristic, which the BOOT button drives.
//
// NVS persistence was proven during bring-up with a temporary boot counter
// (namespace "lt_bringup"), verified across power cycles on 2026-09-29 and
// then removed: it was never LayerTime state. The operational identity is
// the per-power-on sessionId (Slice 1 plan). Any "lt_bringup" keys left on
// a dongle from that proof are inert.

#include "BringUpLogic.h"
#include "C5BootRecord.h"
#include "C5Display.h"
#include "C5Led.h"
#include "C5Link.h"
#include "TDongleC5Pins.h"

#include <Arduino.h>
#include <string.h>
#include <esp_chip_info.h>
#include <esp_heap_caps.h>
#include <esp_idf_version.h>

using namespace layertime::tdongle_c5;
// No `namespace link = layertime::link;` alias here: at file scope the name
// collides with newlib's link(const char*, const char*), so the capability
// bits below are spelled out in full.

namespace {

constexpr uint32_t kLedStepMs = 500;
constexpr uint8_t kLedBrightness = 4; // of 31; enough to see, not to dazzle
constexpr uint32_t kReportMs = 10000;
constexpr uint32_t kSerialWaitMs = 3000;

C5BootRecord gBoot; // D3: reset reason and retained boot history
C5Display gDisplay;
C5Led gLed;
C5Link gLink;
ButtonDebouncer gButton;

uint32_t gButtonCount = 0;
uint32_t gLedStep = 0;
uint32_t gLastLedMs = 0;
uint32_t gLastReportMs = 0;
bool gHeapOk = true;
bool gLinkWasConnected = false;
bool gBleOk = false;

bool heapIntact()
{
    return heap_caps_check_integrity_all(true);
}

void showStatus()
{
    // Each line must fit the 96 px status column (about 14 characters).
    char line[40];
    if (!gBleOk) {
        gDisplay.setLine(1, "BLE FAILED");
    } else if (gLink.connected()) {
        // The last three bytes of the central's address: "Conn dd:ee:ff".
        const char *addr = gLink.peerAddress();
        const size_t n = strlen(addr);
        snprintf(line, sizeof(line), "Conn %s", n >= 8 ? addr + n - 8 : addr);
        gDisplay.setLine(1, line);
    } else {
        gDisplay.setLine(1, "Advertising");
    }
#if defined(LAYERTIME_LINK_TEST)
    // Test build: P pings answered, B button presses, S Status notifies sent.
    snprintf(line, sizeof(line), "P%lu B%lu S%lu", static_cast<unsigned long>(gLink.pingsAnswered()),
             static_cast<unsigned long>(gButtonCount), static_cast<unsigned long>(gLink.statusNotifies()));
#else
    snprintf(line, sizeof(line), "Ping %lu  Btn %lu", static_cast<unsigned long>(gLink.pingsAnswered()),
             static_cast<unsigned long>(gButtonCount));
#endif
    gDisplay.setLine(2, line);
    // The advertised name, then the reset-reason letter and boot count
    // (C5BootRecord::reasonLetter): "LT-C5-F412 P3".
    snprintf(line, sizeof(line), "%s %c%lu", gLink.advertisedName(), C5BootRecord::reasonLetter(gBoot.reason()),
             static_cast<unsigned long>(gBoot.bootCount()));
    gDisplay.setLine(3, line);
    snprintf(line, sizeof(line), "Up %lus  S%04X", static_cast<unsigned long>(millis() / 1000),
             static_cast<unsigned>(gLink.sessionId()));
    gDisplay.setLine(4, line);
}

void report(const char *when)
{
    // D3: keep this boot's uptime and state where the next boot can read
    // them, and say why this boot happened.
    gBoot.touch(millis() / 1000, (gLink.connected() ? 1u : 0u) | (gLink.pingsAnswered() << 8));
    char boot[120];
    gBoot.format(boot, sizeof(boot));
    Serial.printf("[%s] %s\n", when, boot);
    Serial.printf("[%s] uptime %lu s, heap free %lu, PSRAM free %lu, heap %s, button %lu, link %s, "
                  "heartbeat %u, pings %lu, status notifies %lu\n",
                  when, static_cast<unsigned long>(millis() / 1000),
                  static_cast<unsigned long>(ESP.getFreeHeap()),
                  static_cast<unsigned long>(ESP.getFreePsram()), gHeapOk ? "intact" : "CORRUPT",
                  static_cast<unsigned long>(gButtonCount), gLink.connected() ? "connected" : "advertising",
                  static_cast<unsigned>(gLink.heartbeat()), static_cast<unsigned long>(gLink.pingsAnswered()),
                  static_cast<unsigned long>(gLink.statusNotifies()));
}

} // namespace

void setup()
{
    gBoot.begin(); // before anything else: the reset reason of this boot
    Serial.begin(115200);
    const uint32_t waitStart = millis();
    while (!Serial && millis() - waitStart < kSerialWaitMs) delay(10);

    gLed.begin();
    pinMode(pins::kBootButton, INPUT); // external 10 k pull-up (R16)

    const bool displayOk = gDisplay.begin();
    // The C5 is LayerWand; LayerTime is the watch (Michael, 2026-09-30).
    gDisplay.setLine(0, "LayerWand");

    esp_chip_info_t chip;
    esp_chip_info(&chip);
    Serial.println();
    Serial.println("LayerWand (T-Dongle-C5): LayerTime Link 0.1 (Slice 1, Increment 1)");
    Serial.printf("Arduino core %s, ESP-IDF %s\n", ESP_ARDUINO_VERSION_STR, esp_get_idf_version());
    Serial.printf("Chip model %d, revision v%d.%d, %d core(s)\n", static_cast<int>(chip.model),
                  chip.revision / 100, chip.revision % 100, chip.cores);
    Serial.printf("Flash %lu bytes, PSRAM %lu bytes\n",
                  static_cast<unsigned long>(ESP.getFlashChipSize()),
                  static_cast<unsigned long>(ESP.getPsramSize()));
    Serial.printf("Display %s\n", displayOk ? "started" : "FAILED");
    {
        char boot[120];
        gBoot.format(boot, sizeof(boot));
        Serial.println(boot);
    }

    gHeapOk = heapIntact();
    Serial.printf("Heap before BLE init: %s, free %lu\n", gHeapOk ? "intact" : "CORRUPT",
                  static_cast<unsigned long>(ESP.getFreeHeap()));

    // Increment 1 capabilities: the C5 has a display, an LED and a button.
    // The monitors are reported once Increment 2A proves them.
    gBleOk = gLink.begin(layertime::link::kCapDisplay | layertime::link::kCapLed |
                        layertime::link::kCapButton);
    const bool heapAfterBle = heapIntact();
    gHeapOk = gHeapOk && heapAfterBle;
    Serial.printf("Link %s, advertising as %s, session %04X, PSRAM %lu MB\n", gBleOk ? "up" : "FAILED",
                  gLink.advertisedName(), static_cast<unsigned>(gLink.sessionId()),
                  static_cast<unsigned long>(wholeMiB(ESP.getPsramSize())));
    Serial.printf("Heap after BLE init: %s, free %lu\n", heapAfterBle ? "intact" : "CORRUPT",
                  static_cast<unsigned long>(ESP.getFreeHeap()));

    showStatus();
    report("boot");
}

void loop()
{
    const uint32_t now = millis();

    const bool pressed = digitalRead(pins::kBootButton) == LOW;
    if (gButton.update(pressed, now)) {
        ++gButtonCount;
        Serial.printf("Button press %lu\n", static_cast<unsigned long>(gButtonCount));
        const size_t probe = gLink.sendNextProbe();
        if (probe > 0) Serial.printf("Probe sent: %u bytes\n", static_cast<unsigned>(probe));
        showStatus();
    }

    if (now - gLastLedMs >= kLedStepMs) {
        gLastLedMs = now;
        gLed.show(bringUpCycleColor(gLedStep++), kLedBrightness);
    }

    gLink.service(now);
    if (gLink.connected() != gLinkWasConnected) {
        gLinkWasConnected = gLink.connected();
        Serial.printf("Link %s %s\n", gLinkWasConnected ? "connected to" : "disconnected, advertising again;",
                      gLinkWasConnected ? gLink.peerAddress() : "");
        showStatus();
    }

    if (now - gLastReportMs >= kReportMs) {
        gLastReportMs = now;
        gHeapOk = gHeapOk && heapIntact();
        showStatus();
        report("periodic");
    }

    gDisplay.service();
    delay(5);
}

#endif
