// LayerTime - counter-intrusion and resilient-communications firmware
// for the LilyGo T-Watch Ultra.
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


// LayerVixen: the fox for a Bluetooth fox hunt (claude/hunt_v1_layervixen_plan.md).
// The same LILYGO T-Dongle-C5 board as the Hunter, built from the
// tdongle_c5_vixen environment (LAYERTIME_VIXEN_APP). Its only job is to be a
// beacon: it advertises the LayerVixen service every 100 ms at a fixed
// transmit power, accepts no connections, and receives nothing. A Hunter
// in HUNT mode reports its signal to the watch.
//
// Hideable: the screen and the LED stay dark. A short BOOT press lights the
// screen for 10 s with the name, the power, and the uptime.

#if defined(LAYERTIME_TARGET_TDONGLE_C5) && defined(LAYERTIME_VIXEN_APP)

#include "BringUpLogic.h"
#include "C5Display.h"
#include "C5Led.h"
#include "C5Board.h"
#include "TDongleC5Pins.h"

#include "core/link/LinkHunt.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <esp_mac.h>
#include <stdio.h>

using namespace layertime;
using namespace layertime::tdongle_c5;

namespace {

// Fixed, so the signal the hunter sees changes only with distance and walls.
constexpr int8_t kTxPowerDbm = 9;
// 100 ms in Bluetooth's 0.625 ms units.
constexpr uint16_t kAdvertisingInterval = 160;
constexpr uint32_t kScreenWakeMs = 10000;
constexpr uint32_t kCheckMs = 1000;
constexpr uint32_t kReportMs = 10000;
constexpr uint32_t kSerialWaitMs = 3000;
constexpr uint32_t kSerialTxTimeoutMs = 2;
constexpr int kSerialDrainedBytes = 128;

C5Display gDisplay;
C5Led gLed;
ButtonGestures gButton;
NimBLEAdvertising *gAdv = nullptr;
char gName[20] = "";
bool gAdvertising = false;
bool gDisplayStarted = false;
bool gScreenAwake = false;
uint32_t gScreenWokeMs = 0;
uint32_t gLastCheckMs = 0;
uint32_t gLastReportMs = 0;
uint32_t gRestarts = 0;
uint32_t gLedSentAfterTransfers = 0;

bool startBeacon()
{
    uint8_t mac[6] = {};
    esp_read_mac(mac, ESP_MAC_BT);
    // Every unit says what it is (Michael, 2026-11-04): "Vixen", the chip, and
    // five base-32 characters from its own address (2026-11-05), "Vixen-C5-00FX2".
    char chip[8];
    chipShortName(ESP.getChipModel(), chip, sizeof(chip));
    if (!formatUnitName("Vixen", chip, mac, gName, sizeof(gName))) snprintf(gName, sizeof(gName), "Vixen");
    if (!NimBLEDevice::init(gName)) return false;
    NimBLEDevice::setPower(kTxPowerDbm);

    gAdv = NimBLEDevice::getAdvertising();
    // Non-connectable: nothing can connect to the Vixen. Scannable, so a
    // phone's Bluetooth scanner can still show its name.
    gAdv->setConnectableMode(BLE_GAP_CONN_MODE_NON);
    NimBLEAdvertisementData primary;
    primary.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
    primary.addServiceUUID(link::hunt::kVixenServiceUuid);
    NimBLEAdvertisementData scanResponse;
    scanResponse.setName(gName);
    gAdv->setAdvertisementData(primary);
    gAdv->setScanResponseData(scanResponse);
    gAdv->enableScanResponse(true);
    gAdv->setMinInterval(kAdvertisingInterval);
    gAdv->setMaxInterval(kAdvertisingInterval);
    return gAdv->start();
}

void showStatus(uint32_t now)
{
    char line[32];
    gDisplay.setLine(0, "Vixen");
    gDisplay.setLine(1, gName); // "Vixen-C5-00FX2"
    snprintf(line, sizeof(line), "%s", gAdvertising ? "BEACON ON" : "BEACON OFF");
    gDisplay.setLine(2, line);
    snprintf(line, sizeof(line), "POWER: %d dBm", static_cast<int>(kTxPowerDbm));
    gDisplay.setLine(3, line);
    const uint32_t s = now / 1000;
    snprintf(line, sizeof(line), "UP: %lu:%02lu", static_cast<unsigned long>(s / 60), static_cast<unsigned long>(s % 60));
    gDisplay.setLine(4, line);
}

void wakeScreen(uint32_t now)
{
    if (!gDisplayStarted) {
        gDisplayStarted = true;
        const bool ok = gDisplay.begin();
        Serial.printf("Display %s\n", ok ? "started" : "FAILED");
    } else {
        gDisplay.setBacklight(true);
    }
    showStatus(now);
    gScreenAwake = true;
    gScreenWokeMs = now;
}

// The unit's name at the bench: "unit get" on USB serial, the same command
// the Hunter takes. The name comes from the chip (2026-11-05); nothing is set.
char gUnitCmd[32];
size_t gUnitCmdLen = 0;
void serviceUnitSerial()
{
    while (Serial.available() > 0) {
        const int ch = Serial.read();
        if (ch < 0) break;
        if (ch != '\n') {
            if (gUnitCmdLen + 1 < sizeof(gUnitCmd)) gUnitCmd[gUnitCmdLen++] = static_cast<char>(ch);
            continue;
        }
        gUnitCmd[gUnitCmdLen] = '\0';
        gUnitCmdLen = 0;
        const UnitCommand c = parseUnitCommand(gUnitCmd);
        if (c == UnitCommand::None) continue;
        if (c == UnitCommand::Bad) {
            Serial.println("=== unit error The name comes from the chip and cannot be set. Send \"unit get\" to read it.");
            continue;
        }
        uint8_t mac[6] = {};
        esp_read_mac(mac, ESP_MAC_BT);
        Serial.printf("=== unit name %s mac %02X:%02X:%02X:%02X:%02X:%02X\n", gName, mac[0], mac[1], mac[2], mac[3],
                      mac[4], mac[5]);
    }
}

} // namespace

void setup()
{
    holdC5CardDeselected(); // shared SPI board setup; no Hunter SD tooling
    Serial.begin(115200);
    Serial.setTxTimeoutMs(kSerialTxTimeoutMs);
    const uint32_t waitStart = millis();
    while (!Serial && millis() - waitStart < kSerialWaitMs) delay(10);

    // The LED shares the LCD's SPI bus (C5Led.h): dark from boot.
    gLed.begin();
    gLed.off();
    pinMode(pins::kBootButton, INPUT); // external 10 k pull-up (R16)

    gAdvertising = startBeacon();
    Serial.println();
    Serial.println("LayerVixen (T-Dongle-C5): Bluetooth fox-hunt beacon. Non-connectable; receives nothing.");
    Serial.printf("Beacon %s as %s, service %s, every %u ms at %d dBm\n", gAdvertising ? "on" : "FAILED", gName,
                  link::hunt::kVixenServiceUuid, static_cast<unsigned>(kAdvertisingInterval * 5 / 8),
                  static_cast<int>(kTxPowerDbm));
    Serial.println("Display: dark until a short BOOT press.");
    Serial.printf("Unit name %s, from the chip. Over USB: \"unit get\".\n", gName);
}

void loop()
{
    const uint32_t now = millis();
    serviceUnitSerial();
    const bool pressed = digitalRead(pins::kBootButton) == LOW;
    if (gButton.update(pressed, now) == ButtonGesture::Short) wakeScreen(now);
    if (gScreenAwake && now - gScreenWokeMs >= kScreenWakeMs) {
        gDisplay.setBacklight(false);
        gScreenAwake = false;
    }

    // The beacon must never go quiet: if advertising ever stopped, start it again.
    if (now - gLastCheckMs >= kCheckMs) {
        gLastCheckMs = now;
        if (gAdv == nullptr) {
            gAdvertising = startBeacon();
        } else if (!gAdv->isAdvertising()) {
            ++gRestarts;
            gAdvertising = gAdv->start();
            Serial.printf("Beacon had stopped; restart %lu %s\n", static_cast<unsigned long>(gRestarts),
                          gAdvertising ? "ok" : "FAILED");
        }
    }

    if (now - gLastReportMs >= kReportMs) {
        gLastReportMs = now;
        if (Serial.availableForWrite() >= kSerialDrainedBytes) {
            Serial.printf("[%lu s] beacon %s, restarts %lu\n", static_cast<unsigned long>(now / 1000),
                          gAdv != nullptr && gAdv->isAdvertising() ? "on" : "OFF", static_cast<unsigned long>(gRestarts));
        }
    }

    if (gScreenAwake) gDisplay.service();
    // Any LCD transfer scrambles the LED; send it dark again.
    if (gDisplay.transfers() != gLedSentAfterTransfers) {
        gLed.off();
        gLedSentAfterTransfers = gDisplay.transfers();
    }
    delay(10);
}

#endif
