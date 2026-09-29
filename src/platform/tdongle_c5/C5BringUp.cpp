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
// as an empty translation unit.
#if defined(LAYERTIME_TARGET_TDONGLE_C5)

// Slice 1, Increment 0: T-Dongle-C5 bring-up on a pinned toolchain.
//
// Proves, on the real board: the LCD, the APA102 LED, the BOOT button,
// PSRAM, and NimBLE initialising and advertising a test name without
// corrupting the heap (arduino-esp32 #12821 on the C5).
//
// NVS persistence was proven during bring-up with a temporary boot counter
// (namespace "lt_bringup"), verified across power cycles on 2026-09-29 and
// then removed: it was never LayerTime state. The operational identity is
// the per-power-on sessionId (Slice 1 plan). Any "lt_bringup" keys left on
// a dongle from that proof are inert.

#include "BringUpLogic.h"
#include "C5Display.h"
#include "C5Led.h"
#include "TDongleC5Pins.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <esp_chip_info.h>
#include <esp_heap_caps.h>
#include <esp_idf_version.h>
#include <esp_mac.h>

using namespace layertime::tdongle_c5;

namespace {

constexpr uint32_t kLedStepMs = 500;
constexpr uint8_t kLedBrightness = 4; // of 31; enough to see, not to dazzle
constexpr uint32_t kReportMs = 10000;
constexpr uint32_t kSerialWaitMs = 3000;

C5Display gDisplay;
C5Led gLed;
ButtonDebouncer gButton;

uint32_t gButtonCount = 0;
uint32_t gLedStep = 0;
uint32_t gLastLedMs = 0;
uint32_t gLastReportMs = 0;
bool gHeapOk = true;
bool gBleOk = false;
char gName[kAdvertisedNameSize] = "";

bool heapIntact()
{
    return heap_caps_check_integrity_all(true);
}

void showStatus()
{
    // Each line must fit the 96 px status column (about 14 characters).
    char line[40];
    snprintf(line, sizeof(line), "Button %lu", static_cast<unsigned long>(gButtonCount));
    gDisplay.setLine(1, line);
    snprintf(line, sizeof(line), "%lu MB Heap %s",
             static_cast<unsigned long>(wholeMiB(ESP.getPsramSize())), gHeapOk ? "OK" : "BAD");
    gDisplay.setLine(2, line);
    gDisplay.setLine(3, gBleOk ? gName : "BLE FAILED");
    snprintf(line, sizeof(line), "Up %lus", static_cast<unsigned long>(millis() / 1000));
    gDisplay.setLine(4, line);
}

void report(const char *when)
{
    Serial.printf("[%s] uptime %lu s, heap free %lu, PSRAM free %lu, heap %s, button %lu\n", when,
                  static_cast<unsigned long>(millis() / 1000),
                  static_cast<unsigned long>(ESP.getFreeHeap()),
                  static_cast<unsigned long>(ESP.getFreePsram()), gHeapOk ? "intact" : "CORRUPT",
                  static_cast<unsigned long>(gButtonCount));
}

} // namespace

void setup()
{
    Serial.begin(115200);
    const uint32_t waitStart = millis();
    while (!Serial && millis() - waitStart < kSerialWaitMs) delay(10);

    gLed.begin();
    pinMode(pins::kBootButton, INPUT); // external 10 k pull-up (R16)

    const bool displayOk = gDisplay.begin();
    gDisplay.setLine(0, "LayerTime C5");

    esp_chip_info_t chip;
    esp_chip_info(&chip);
    Serial.println();
    Serial.println("LayerTime T-Dongle-C5 bring-up (Slice 1, Increment 0)");
    Serial.printf("Arduino core %s, ESP-IDF %s\n", ESP_ARDUINO_VERSION_STR, esp_get_idf_version());
    Serial.printf("Chip model %d, revision v%d.%d, %d core(s)\n", static_cast<int>(chip.model),
                  chip.revision / 100, chip.revision % 100, chip.cores);
    Serial.printf("Flash %lu bytes, PSRAM %lu bytes\n",
                  static_cast<unsigned long>(ESP.getFlashChipSize()),
                  static_cast<unsigned long>(ESP.getPsramSize()));
    Serial.printf("Display %s\n", displayOk ? "started" : "FAILED");

    gHeapOk = heapIntact();
    Serial.printf("Heap before BLE init: %s, free %lu\n", gHeapOk ? "intact" : "CORRUPT",
                  static_cast<unsigned long>(ESP.getFreeHeap()));

    uint8_t mac[6] = {};
    esp_read_mac(mac, ESP_MAC_BT);
    formatAdvertisedName(mac, gName);
    if (NimBLEDevice::init(gName)) {
        NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
        adv->setName(gName);
        adv->enableScanResponse(false);
        gBleOk = adv->start();
    }
    const bool heapAfterBle = heapIntact();
    gHeapOk = gHeapOk && heapAfterBle;
    Serial.printf("BLE init %s, advertising as %s, address %s\n", gBleOk ? "ok" : "FAILED", gName,
                  NimBLEDevice::getAddress().toString().c_str());
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
        showStatus();
    }

    if (now - gLastLedMs >= kLedStepMs) {
        gLastLedMs = now;
        gLed.show(bringUpCycleColor(gLedStep++), kLedBrightness);
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
