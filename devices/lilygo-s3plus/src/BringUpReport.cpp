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

#include "BringUpReport.h"

#include <Arduino.h>
#include <FFat.h>
#include <LilyGoLib.h>
#include <Preferences.h>
#include <Wire.h>
#include <esp_mac.h>
#include <esp_partition.h>
#include <esp_vfs_fat.h>
#include <string.h>
#include <time.h>

#include "BringUpCheck.h"
#include "S3PlusProfile.h"

namespace layertime {
namespace twatch_s3plus {
namespace bringup_report {

namespace {

constexpr const char *kMountPoint = "/fs";
constexpr const char *kTestPath = "/s3plus_selftest.txt";
constexpr const char *kNvsNamespace = "lt_s3plus";
constexpr const char *kNvsFormatCount = "ffmt";
constexpr const char *kNvsPendingToken = "sttok";
constexpr const char *kNvsTestResult = "strs";   // bringup::RebootTestResult, as stored
constexpr const char *kNvsTestRuns = "stn";      // completed reboot tests, ever

struct Facts {
    bool psramFound = false;
    uint32_t psramSize = 0;
    uint8_t mac[6] = {0};
    uint32_t probe = 0;
    bringup::Found mainBus;
    bringup::Found touchBus;
    bringup::GnssModule gnss = bringup::GnssModule::None;
    char gnssModel[32] = {0};
};

struct StorageFacts {
    bool partitionFound = false;
    char label[17] = {0};
    uint32_t address = 0;
    uint32_t size = 0;
    unsigned fatPartitionCount = 0;
    char mountpoint[16] = {0};
    esp_err_t fsInfo = ESP_FAIL;
    uint64_t fsTotal = 0;
    uint64_t fsFree = 0;
    esp_err_t ffatInfo = ESP_FAIL;
    uint64_t ffatTotal = 0;
    uint64_t ffatFree = 0;
    bool retried = false;
    bool retryMounted = false;
};


// The storage reboot test's stored outcome (0.1.3), reloaded at boot.
struct RebootTest {
    bringup::RebootTestResult last = bringup::RebootTestResult::None;
    uint32_t runs = 0;              // completed tests, from NVS
    bool completedThisBoot = false; // a pending test was completed at this boot
    char completedNote[64] = "";
};

Facts g_facts;
StorageFacts g_storage;
RebootTest g_test;
uint8_t g_formatCount = 0;


void scan(TwoWire &bus, bringup::Found &found)
{
    for (uint8_t a = 1; a < 127; ++a) {
        bus.beginTransmission(a);
        if (bus.endTransmission() == 0) found.add(a);
    }
}

template <size_t N>
void printBus(const char *name, const bringup::ExpectedDevice (&expected)[N], const bringup::Found &found)
{
    for (const auto &e : expected) {
        Serial.printf("[s3plus] %s 0x%02X %-24s %s\n", name, e.address, e.part,
                      found.has(e.address) ? "present" : "MISSING");
    }
    uint8_t extra[16];
    const size_t n = bringup::unexpected(expected, found, extra, sizeof(extra));
    for (size_t i = 0; i < n; ++i) Serial.printf("[s3plus] %s 0x%02X not in LilyGo's list\n", name, extra[i]);
}

const char *passFail(bool ok) { return ok ? "PASS" : "FAIL"; }

void readStorage(StorageFacts &st)
{
    st.fatPartitionCount = 0;
    esp_partition_iterator_t it =
        esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, nullptr);
    while (it != nullptr) {
        ++st.fatPartitionCount;
        it = esp_partition_next(it);
    }
    esp_partition_iterator_release(it);

    const esp_partition_t *part =
        esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, nullptr);
    st.partitionFound = part != nullptr;
    if (part != nullptr) {
        strncpy(st.label, part->label, sizeof(st.label) - 1);
        st.address = part->address;
        st.size = part->size;
    }
    const char *mp = FFat.mountpoint();
    strncpy(st.mountpoint, mp != nullptr ? mp : "", sizeof(st.mountpoint) - 1);
    st.fsInfo = esp_vfs_fat_info(kMountPoint, &st.fsTotal, &st.fsFree);
    st.ffatInfo = esp_vfs_fat_info("/ffat", &st.ffatTotal, &st.ffatFree);
}

bool storageOk(const StorageFacts &st) { return st.fsInfo == ESP_OK && st.fsTotal > 0; }

bool partitionExpected(const StorageFacts &st)
{
    return st.partitionFound &&
           bringup::ffatPartitionIsExpected(st.label, st.address, st.size, st.fatPartitionCount);
}


void printStorage()
{
    readStorage(g_storage);
    const StorageFacts &st = g_storage;
    if (st.partitionFound) {
        Serial.printf("[s3plus] FFat partition label=%s address=0x%06lX size=%lu bytes, data/fat partitions=%u, %s\n",
                      st.label, static_cast<unsigned long>(st.address), static_cast<unsigned long>(st.size),
                      st.fatPartitionCount, partitionExpected(st) ? "as expected" : "NOT as expected");
    } else {
        Serial.printf("[s3plus] FFat partition: none of type data/fat in the partition table\n");
    }
    Serial.printf("[s3plus] FFat mountpoint=\"%s\" totalBytes=%lu usedBytes=%lu\n", st.mountpoint,
                  static_cast<unsigned long>(FFat.totalBytes()), static_cast<unsigned long>(FFat.usedBytes()));
    Serial.printf("[s3plus] vfs /fs: %s total=%llu free=%llu\n", esp_err_to_name(st.fsInfo),
                  static_cast<unsigned long long>(st.fsTotal), static_cast<unsigned long long>(st.fsFree));
    Serial.printf("[s3plus] vfs /ffat: %s total=%llu free=%llu\n", esp_err_to_name(st.ffatInfo),
                  static_cast<unsigned long long>(st.ffatTotal), static_cast<unsigned long long>(st.ffatFree));
    if (st.retried) {
        Serial.printf("[s3plus] FFat mount retry at /fs without format: %s\n",
                      st.retryMounted ? "mounted" : "failed");
    }
    Serial.printf("[s3plus] repair: formats recorded=%u; the format control was removed in 0.2.0\n",
                  g_formatCount);
    Serial.printf("[s3plus] storage reboot test: completed runs=%lu, last result: %s\n",
                  static_cast<unsigned long>(g_test.runs), bringup::rebootTestText(g_test.last));
    if (g_test.completedThisBoot) {
        Serial.printf("[s3plus] storage reboot test: completed at this boot (%s)\n", g_test.completedNote);
    }
    Serial.printf("[s3plus] FFat %s\n", passFail(storageOk(st)));
}


void printReportBody(const char *version)
{
    const bool psramOk = bringup::psramAsExpected(g_facts.psramFound, g_facts.psramSize);
    const bool mainOk = bringup::allPresent(bringup::kMainBus, g_facts.mainBus);
    const bool touchOk = bringup::allPresent(bringup::kTouchBus, g_facts.touchBus);

    Serial.printf("\n[s3plus] ===== LayerTime T-Watch S3 Plus %s, hardware report =====\n", version);
    Serial.printf("[s3plus] profile %s\n", layertime::twatch_s3plus::capabilities().profileId);
    Serial.printf("[s3plus] chip %s rev %u, flash %lu bytes\n", ESP.getChipModel(),
                  static_cast<unsigned>(ESP.getChipRevision()),
                  static_cast<unsigned long>(ESP.getFlashChipSize()));
    Serial.printf("[s3plus] MAC %02X:%02X:%02X:%02X:%02X:%02X\n", g_facts.mac[0], g_facts.mac[1],
                  g_facts.mac[2], g_facts.mac[3], g_facts.mac[4], g_facts.mac[5]);
    Serial.printf("[s3plus] PSRAM found=%d size=%lu bytes free=%lu %s\n", g_facts.psramFound ? 1 : 0,
                  static_cast<unsigned long>(g_facts.psramSize),
                  static_cast<unsigned long>(ESP.getFreePsram()), passFail(psramOk));
    Serial.printf("[s3plus] LilyGoLib device probe 0x%08lX\n", static_cast<unsigned long>(g_facts.probe));
    printBus("I2C main", bringup::kMainBus, g_facts.mainBus);
    Serial.printf("[s3plus] I2C main bus %s\n", passFail(mainOk));
    printBus("I2C touch", bringup::kTouchBus, g_facts.touchBus);
    Serial.printf("[s3plus] I2C touch bus %s\n", passFail(touchOk));
    Serial.printf("[s3plus] GNSS model \"%s\" = %s\n", g_facts.gnssModel, bringup::gnssName(g_facts.gnss));

    auto &pmu = instance.pmu;
    Serial.printf("[s3plus] battery connected=%d percent=%d voltage=%u mV charging=%d usb=%d\n",
                  pmu.isBatteryConnect() ? 1 : 0, pmu.getBatteryPercent(),
                  static_cast<unsigned>(pmu.getBattVoltage()), pmu.isCharging() ? 1 : 0,
                  pmu.isVbusIn() ? 1 : 0);

    struct tm now = {};
    instance.rtc.getDateTime(&now);
    Serial.printf("[s3plus] RTC %04d-%02d-%02d %02d:%02d:%02d\n", now.tm_year + 1900, now.tm_mon + 1,
                  now.tm_mday, now.tm_hour, now.tm_min, now.tm_sec);

    printStorage();
    Serial.printf("[s3plus] LoRa radio: not initialised, power rail off (mesh deferred)\n");
    Serial.printf("[s3plus] heap free=%lu min=%lu\n", static_cast<unsigned long>(ESP.getFreeHeap()),
                  static_cast<unsigned long>(ESP.getMinFreeHeap()));
    Serial.printf("[s3plus] ===== end of hardware report =====\n");
}


void testContent(uint32_t token, char *out, size_t outSize)
{
    snprintf(out, outSize, "LayerTime S3 Plus FFat self-test. Token %08lX. MAC %02X%02X%02X%02X%02X%02X.\n",
             static_cast<unsigned long>(token), g_facts.mac[0], g_facts.mac[1], g_facts.mac[2],
             g_facts.mac[3], g_facts.mac[4], g_facts.mac[5]);
}


bool readTestFile(const char *expected)
{
    File f = FFat.open(kTestPath, FILE_READ);
    if (!f) return false;
    char buf[160] = {0};
    const size_t n = f.read(reinterpret_cast<uint8_t *>(buf), sizeof(buf) - 1);
    f.close();
    return n == strlen(expected) && memcmp(buf, expected, n) == 0;
}

void storeRebootTestResult(bringup::RebootTestResult result)
{
    Preferences prefs;
    prefs.begin(kNvsNamespace, false);
    const uint32_t runs = prefs.getUInt(kNvsTestRuns, 0) + 1;
    prefs.putUInt(kNvsTestRuns, runs);
    prefs.putUChar(kNvsTestResult, static_cast<uint8_t>(result));
    prefs.remove(kNvsPendingToken);
    prefs.end();
    g_test.runs = runs;
    g_test.last = result;
}

// Runs once at boot, after LilyGoLib has mounted storage. Loads the stored
// outcome, and if the previous boot left a test file (a reboot test or the
// 0.1.2 format), checks it survived, removes it, and stores the outcome.
void completeRebootTest()
{
    Preferences prefs;
    prefs.begin(kNvsNamespace, true);
    const uint32_t token = prefs.getUInt(kNvsPendingToken, 0);
    g_test.runs = prefs.getUInt(kNvsTestRuns, 0);
    g_test.last = bringup::rebootTestFromStored(prefs.getUChar(kNvsTestResult, 0));
    prefs.end();
    if (token == 0) return;

    char expected[160];
    testContent(token, expected, sizeof(expected));
    const bool mounted = FFat.mountpoint() != nullptr;
    const bool existed = mounted && FFat.exists(kTestPath);
    const bool matched = existed && readTestFile(expected);
    if (existed) FFat.remove(kTestPath);
    const bool removed = mounted && !FFat.exists(kTestPath);
    const bringup::RebootTestResult result = bringup::classifyRebootTest(mounted, existed, matched, removed);
    // Stored (and the pending token cleared) whatever the outcome, so a
    // failure is reported rather than retried silently.
    storeRebootTestResult(result);
    g_test.completedThisBoot = true;
    snprintf(g_test.completedNote, sizeof(g_test.completedNote),
             "token %08lX, mounted=%d existed=%d matched=%d removed=%d", static_cast<unsigned long>(token),
             mounted ? 1 : 0, existed ? 1 : 0, matched ? 1 : 0, removed ? 1 : 0);
}

} // namespace

void beforeBegin()
{
    g_facts.psramFound = psramFound();
    g_facts.psramSize = ESP.getPsramSize();
    esp_efuse_mac_get_default(g_facts.mac);
}

void afterBegin()
{
    g_facts.probe = instance.getDeviceProbe();

    // If LilyGoLib's begin() left nothing mounted, try one plain mount at /fs
    // (no format) and record the result.
    readStorage(g_storage);
    if (g_storage.fsInfo != ESP_OK && g_storage.ffatInfo != ESP_OK && g_storage.mountpoint[0] == '\0') {
        g_storage.retried = true;
        g_storage.retryMounted = FFat.begin(false, kMountPoint);
    }
    {
        Preferences prefs;
        if (prefs.begin(kNvsNamespace, true)) {
            g_formatCount = prefs.getUChar(kNvsFormatCount, 0);
            prefs.end();
        }
    }
    completeRebootTest();

    scan(Wire, g_facts.mainBus);
    scan(Wire1, g_facts.touchBus);
    const String model = instance.gps.getModel();
    strncpy(g_facts.gnssModel, model.c_str(), sizeof(g_facts.gnssModel) - 1);
    g_facts.gnss = bringup::classifyGnss(g_facts.gnssModel);
}

void print(const char *version) { printReportBody(version); }

bool psramOk() { return bringup::psramAsExpected(g_facts.psramFound, g_facts.psramSize); }
bool mainBusOk() { return bringup::allPresent(bringup::kMainBus, g_facts.mainBus); }
bool touchBusOk() { return bringup::allPresent(bringup::kTouchBus, g_facts.touchBus); }
bool storageOk()
{
    readStorage(g_storage);
    return storageOk(g_storage);
}
const char *gnssName() { return bringup::gnssName(g_facts.gnss); }

} // namespace bringup_report
} // namespace twatch_s3plus
} // namespace layertime
