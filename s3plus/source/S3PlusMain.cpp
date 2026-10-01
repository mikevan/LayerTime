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

// Phase 1: bring-up and hardware identification. Version 0.1.3.
//
// What it does:
//   * Reports, over USB serial and on the screen, what this watch actually
//     has: chip, MAC, PSRAM, both I2C buses against LilyGo's documented
//     parts, the GNSS module fitted, battery and charger, RTC time, and the
//     FFat partition and filesystem.
//   * Lets you check the display, touch (a dot follows the tap), and haptics
//     (one pulse per tap, at most once a second).
//   * 0.1.3: a storage reboot test that needs no format. When storage passes
//     its check, a button labeled "Run storage reboot test" writes a test
//     file with a random token, reads it back, and records the token in NVS.
//     On the next boot the firmware reads the file again, compares it,
//     removes it, confirms the removal, and stores the outcome and a run
//     count in NVS. Every report prints that stored outcome, so it survives a
//     stalled serial monitor or further restarts (0.1.2's result lived only
//     in RAM for one boot and was lost that way).
//   * 0.1.2: offers ONE repair of the FFat filesystem, only when all of these
//     hold: the filesystem fails its check, the partition table has exactly
//     one data/fat partition and it is "ffat" at 0x810000 with 0x7E0000 bytes,
//     this watch has never been formatted by this firmware (NVS record), and
//     no format has run since boot. The control is a red button labeled
//     "Hold to format storage." It fires only after a continuous three-second
//     hold; releasing early cancels. The format is the Arduino core's
//     FFat.format(FFAT_WIPE_FULL, "ffat"), which erases that partition only and
//     builds a new FAT sized from its wear-leveling layer. Afterwards the
//     firmware mounts it at /fs, checks a nonzero capacity, writes a test
//     file and reads it back, and records a pending check in NVS. On the next
//     boot it reads the file again, compares it, removes it, and confirms the
//     removal.
//
// What it does NOT do:
//   * The LoRa radio is never initialised (instance.begin(NO_HW_LORA)), and
//     its power rail (AXP2101 ALDO4) is switched off straight after begin.
//     Mesh stays deferred.
//   * No Wi-Fi, no BLE, no sleep, and the touch panel is never put to sleep
//     (its reset line is not connected, so it would not come back).
//   * Nothing outside the ffat partition is ever erased or written, except
//     this firmware's own NVS namespace "lt_s3plus".
//
// Why the repair exists: a read-only capture (tools/s3plus_flash_read.py,
// 2026-10-01) showed the FAT boot sector intact but the FAT table and root
// directory overwritten by an earlier firmware's data, so FatFs reports
// FR_INT_ERR on the free-space query. See s3plus/README.md.
//
// LilyGoLib's begin() does two things of its own on this board: it mounts
// FFat at /fs (formatting it if it will not mount at all), and on the first
// boot only it writes the PMU fuel-gauge battery parameters and records that
// in NVS namespace "lilygo".
//
// The S3 Plus lives in s3plus/, outside src/, so the T-Watch Ultra build
// (which compiles everything under src/) never sees these files.

#include <Arduino.h>
#include <FFat.h>
#include <LilyGoLib.h>
#include <LV_Helper.h>
#include <Preferences.h>
#include <Wire.h>
#include <esp_mac.h>
#include <esp_partition.h>
#include <esp_random.h>
#include <esp_vfs_fat.h>
#include <lvgl.h>
#include <time.h>

#include "BringUpCheck.h"
#include "S3PlusProfile.h"

#if !defined(LAYERTIME_S3PLUS_LV_CONF)
#error "LVGL is not using s3plus/lv_conf_s3plus.h"
#endif

#if !defined(ARDUINO_T_WATCH_S3)
#error "The S3 Plus target must build LilyGoLib's T-Watch S3 board class"
#endif

namespace bringup = layertime::twatch_s3plus::bringup;

namespace {

constexpr const char *kVersion = "0.1.3";
constexpr uint32_t kSerialWaitMs = 5000;
constexpr uint32_t kReportIntervalMs = 10000;
constexpr uint32_t kHapticGapMs = 1000;
constexpr uint32_t kFormatHoldMs = 3000;
constexpr uint8_t kBrightness = 200;
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

// Everything the repair did, for the report.
struct RepairLog {
    uint8_t formatCount = 0;        // from NVS: formats this firmware has ever run
    bool eligible = false;          // control shown this boot
    char why[96] = "not needed";    // why the control is or is not shown
    bool ran = false;
    bool formatOk = false;
    bool remounted = false;
    uint64_t total = 0;
    bool writeOk = false;
    bool readOk = false;
};

// The storage reboot test. The outcome is kept in NVS and reloaded at boot.
struct RebootTest {
    bringup::RebootTestResult last = bringup::RebootTestResult::None;
    uint32_t runs = 0;              // completed tests, from NVS
    bool completedThisBoot = false; // the boot check ran on this boot
    char completedNote[64] = "";
    uint32_t pendingToken = 0;      // written this boot; restart to complete
    bool requested = false;         // the button was tapped; handled in loop()
};

Facts g_facts;
StorageFacts g_storage;
RepairLog g_repair;
RebootTest g_test;
bringup::HoldGate g_hold(kFormatHoldMs);

lv_obj_t *g_status = nullptr;
lv_obj_t *g_touch = nullptr;
lv_obj_t *g_dot = nullptr;
lv_obj_t *g_formatBtn = nullptr;
lv_obj_t *g_formatBar = nullptr;
lv_obj_t *g_testBtn = nullptr;
uint32_t g_lastReportMs = 0;
uint32_t g_lastHapticMs = 0;
int16_t g_lastX = -1;
int16_t g_lastY = -1;

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
    const RepairLog &r = g_repair;
    Serial.printf("[s3plus] repair: formats recorded=%u, control %s (%s)\n", r.formatCount,
                  r.eligible ? "shown" : "not shown", r.why);
    if (r.ran) {
        Serial.printf("[s3plus] repair: format=%s remount=%s capacity=%llu write=%s read=%s\n",
                      r.formatOk ? "ok" : "FAILED", r.remounted ? "ok" : "FAILED",
                      static_cast<unsigned long long>(r.total), r.writeOk ? "ok" : "FAILED",
                      r.readOk ? "ok" : "FAILED");
    }
    Serial.printf("[s3plus] storage reboot test: completed runs=%lu, last result: %s\n",
                  static_cast<unsigned long>(g_test.runs), bringup::rebootTestText(g_test.last));
    if (g_test.completedThisBoot) {
        Serial.printf("[s3plus] storage reboot test: completed at this boot (%s)\n", g_test.completedNote);
    }
    if (g_test.pendingToken != 0) {
        Serial.printf("[s3plus] storage reboot test: test file written this boot (token %08lX); "
                      "restart the watch to complete the test\n",
                      static_cast<unsigned long>(g_test.pendingToken));
    }
    Serial.printf("[s3plus] FFat %s\n", passFail(storageOk(st)));
}

void printReport()
{
    const bool psramOk = bringup::psramAsExpected(g_facts.psramFound, g_facts.psramSize);
    const bool mainOk = bringup::allPresent(bringup::kMainBus, g_facts.mainBus);
    const bool touchOk = bringup::allPresent(bringup::kTouchBus, g_facts.touchBus);

    Serial.printf("\n[s3plus] ===== LayerTime T-Watch S3 Plus bring-up %s =====\n", kVersion);
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
    Serial.printf("[s3plus] ===== end of report; it repeats every %lu s =====\n",
                  static_cast<unsigned long>(kReportIntervalMs / 1000));
}

// ---- test file ---------------------------------------------------------

void testContent(uint32_t token, char *out, size_t outSize)
{
    snprintf(out, outSize, "LayerTime S3 Plus FFat self-test. Token %08lX. MAC %02X%02X%02X%02X%02X%02X.\n",
             static_cast<unsigned long>(token), g_facts.mac[0], g_facts.mac[1], g_facts.mac[2],
             g_facts.mac[3], g_facts.mac[4], g_facts.mac[5]);
}

bool writeTestFile(const char *content)
{
    File f = FFat.open(kTestPath, FILE_WRITE);
    if (!f) return false;
    const size_t n = strlen(content);
    const bool ok = f.write(reinterpret_cast<const uint8_t *>(content), n) == n;
    f.close();
    return ok;
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

// The button: write a fresh test file, read it back, and arm the boot check.
void startRebootTest()
{
    g_test.requested = false;
    lv_obj_add_flag(g_testBtn, LV_OBJ_FLAG_HIDDEN);
    readStorage(g_storage);
    bool ok = false;
    uint32_t token = 0;
    if (storageOk(g_storage)) {
        token = esp_random();
        if (token == 0) token = 1;
        char content[160];
        testContent(token, content, sizeof(content));
        ok = writeTestFile(content) && readTestFile(content);
    }
    if (ok) {
        Preferences prefs;
        prefs.begin(kNvsNamespace, false);
        prefs.putUInt(kNvsPendingToken, token);
        prefs.end();
        g_test.pendingToken = token;
        lv_label_set_text(g_touch, "Test file written. Restart the watch to finish the test.");
        Serial.printf("[s3plus] storage reboot test: test file written and read back (token %08lX)\n",
                      static_cast<unsigned long>(token));
    } else {
        if (FFat.mountpoint() != nullptr && FFat.exists(kTestPath)) FFat.remove(kTestPath);
        storeRebootTestResult(bringup::RebootTestResult::WriteFailed);
        lv_label_set_text(g_touch, "The storage test could not write its file.");
        Serial.printf("[s3plus] storage reboot test: write or read-back failed\n");
    }
    printReport();
}

void onTestButton(lv_event_t *) { g_test.requested = true; }

// ---- the repair ---------------------------------------------------------

void decideEligibility()
{
    Preferences prefs;
    prefs.begin(kNvsNamespace, true);
    g_repair.formatCount = prefs.getUChar(kNvsFormatCount, 0);
    prefs.end();

    readStorage(g_storage);
    g_repair.eligible = false;
    if (storageOk(g_storage)) {
        strncpy(g_repair.why, "storage passes its check", sizeof(g_repair.why) - 1);
    } else if (!partitionExpected(g_storage)) {
        strncpy(g_repair.why, "partition is not ffat at 0x810000 size 0x7E0000; refusing",
                sizeof(g_repair.why) - 1);
    } else if (g_repair.formatCount != 0) {
        strncpy(g_repair.why, "this watch was already formatted once by this firmware; refusing",
                sizeof(g_repair.why) - 1);
    } else {
        strncpy(g_repair.why, "storage fails its check and the partition is verified",
                sizeof(g_repair.why) - 1);
        g_repair.eligible = true;
    }
}

void runFormat()
{
    g_repair.ran = true;
    lv_obj_add_flag(g_formatBtn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_formatBar, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(g_touch, "Formatting storage. Please wait.");
    lv_refr_now(nullptr);
    Serial.printf("[s3plus] repair: formatting partition \"ffat\" at 0x%06lX, %lu bytes (full wipe)\n",
                  static_cast<unsigned long>(bringup::kFfatAddress),
                  static_cast<unsigned long>(bringup::kFfatSize));

    // Record the attempt before formatting, so a crash or power loss part way
    // through can never lead to the control being offered again.
    {
        Preferences prefs;
        prefs.begin(kNvsNamespace, false);
        prefs.putUChar(kNvsFormatCount, static_cast<uint8_t>(g_repair.formatCount + 1));
        prefs.end();
        g_repair.formatCount++;
    }

    // Re-verify the partition immediately before touching it.
    readStorage(g_storage);
    if (!partitionExpected(g_storage)) {
        strncpy(g_repair.why, "partition check failed at format time; nothing was formatted",
                sizeof(g_repair.why) - 1);
        lv_label_set_text(g_touch, "Format refused. Partition check failed.");
        return;
    }

    FFat.end();
    g_repair.formatOk = FFat.format(FFAT_WIPE_FULL, const_cast<char *>(bringup::kFfatLabel));
    g_repair.remounted = FFat.begin(false, kMountPoint, 10, bringup::kFfatLabel);
    readStorage(g_storage);
    g_repair.total = g_storage.fsTotal;

    if (g_repair.remounted && storageOk(g_storage)) {
        uint32_t token = esp_random();
        if (token == 0) token = 1;
        char content[160];
        testContent(token, content, sizeof(content));
        g_repair.writeOk = writeTestFile(content);
        g_repair.readOk = g_repair.writeOk && readTestFile(content);
        if (g_repair.readOk) {
            Preferences prefs;
            prefs.begin(kNvsNamespace, false);
            prefs.putUInt(kNvsPendingToken, token);
            prefs.end();
            g_test.pendingToken = token;
        }
    }
    const bool allOk = g_repair.formatOk && g_repair.remounted && storageOk(g_storage) &&
                       g_repair.writeOk && g_repair.readOk;
    lv_label_set_text(g_touch, allOk ? "Storage formatted and tested. Restart the watch."
                                     : "Storage repair failed. See the serial report.");
    strncpy(g_repair.why, "format ran this boot", sizeof(g_repair.why) - 1);
    g_repair.eligible = false;
    printReport();
}

void pollFormatControl(uint32_t nowMs)
{
    if (!g_repair.eligible || g_formatBtn == nullptr) return;
    const bool pressed = lv_obj_has_state(g_formatBtn, LV_STATE_PRESSED);
    switch (g_hold.update(pressed, nowMs)) {
    case bringup::HoldGate::Event::Started:
        lv_obj_remove_flag(g_formatBar, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(g_touch, "Keep holding for three seconds.");
        Serial.printf("[s3plus] repair: hold started\n");
        break;
    case bringup::HoldGate::Event::Cancelled:
        lv_bar_set_value(g_formatBar, 0, LV_ANIM_OFF);
        lv_obj_add_flag(g_formatBar, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(g_touch, "Format cancelled. Nothing was changed.");
        Serial.printf("[s3plus] repair: hold released early, cancelled\n");
        break;
    case bringup::HoldGate::Event::Fired:
        lv_bar_set_value(g_formatBar, 100, LV_ANIM_OFF);
        runFormat();
        return;
    case bringup::HoldGate::Event::None:
        break;
    }
    if (g_hold.holding()) lv_bar_set_value(g_formatBar, g_hold.percent(nowMs), LV_ANIM_OFF);
}

// ---- screen -------------------------------------------------------------

void buildScreen()
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text_fmt(title, "LayerTime S3 Plus\nBring-up %s", kVersion);
    lv_obj_set_style_text_color(title, lv_color_hex(0xD99A24), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    g_status = lv_label_create(screen);
    lv_obj_set_width(g_status, 228);
    lv_label_set_long_mode(g_status, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(g_status, lv_color_hex(0xE7ECEB), 0);
    lv_obj_set_style_text_font(g_status, &lv_font_montserrat_14, 0);
    lv_obj_align(g_status, LV_ALIGN_TOP_LEFT, 6, 52);

    g_touch = lv_label_create(screen);
    lv_obj_set_width(g_touch, 228);
    lv_obj_set_style_text_align(g_touch, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(g_touch, "Tap the screen to test touch.");
    lv_obj_set_style_text_color(g_touch, lv_color_hex(0x1CB7B0), 0);
    lv_obj_set_style_text_font(g_touch, &lv_font_montserrat_14, 0);
    lv_obj_align(g_touch, LV_ALIGN_BOTTOM_MID, 0, -4);

    g_formatBtn = lv_button_create(screen);
    lv_obj_set_size(g_formatBtn, 210, 34);
    lv_obj_align(g_formatBtn, LV_ALIGN_BOTTOM_MID, 0, -26);
    lv_obj_set_style_bg_color(g_formatBtn, lv_color_hex(0xE0524A), 0);
    lv_obj_t *btnLabel = lv_label_create(g_formatBtn);
    lv_label_set_text(btnLabel, "Hold to format storage");
    lv_obj_set_style_text_font(btnLabel, &lv_font_montserrat_14, 0);
    lv_obj_center(btnLabel);
    lv_obj_add_flag(g_formatBtn, LV_OBJ_FLAG_HIDDEN);

    g_formatBar = lv_bar_create(screen);
    lv_obj_set_size(g_formatBar, 210, 6);
    lv_obj_align(g_formatBar, LV_ALIGN_BOTTOM_MID, 0, -64);
    lv_bar_set_range(g_formatBar, 0, 100);
    lv_bar_set_value(g_formatBar, 0, LV_ANIM_OFF);
    lv_obj_add_flag(g_formatBar, LV_OBJ_FLAG_HIDDEN);

    // Same place as the format button; the two are never shown together (the
    // format button needs storage to fail, this one needs it to pass).
    g_testBtn = lv_button_create(screen);
    lv_obj_set_size(g_testBtn, 210, 34);
    lv_obj_align(g_testBtn, LV_ALIGN_BOTTOM_MID, 0, -26);
    lv_obj_set_style_bg_color(g_testBtn, lv_color_hex(0x1C7F7A), 0);
    lv_obj_t *testLabel = lv_label_create(g_testBtn);
    lv_label_set_text(testLabel, "Run storage reboot test");
    lv_obj_set_style_text_font(testLabel, &lv_font_montserrat_14, 0);
    lv_obj_center(testLabel);
    lv_obj_add_event_cb(g_testBtn, onTestButton, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(g_testBtn, LV_OBJ_FLAG_HIDDEN);

    g_dot = lv_obj_create(screen);
    lv_obj_set_size(g_dot, 14, 14);
    lv_obj_set_style_radius(g_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(g_dot, lv_color_hex(0x63E06B), 0);
    lv_obj_set_style_border_width(g_dot, 0, 0);
    lv_obj_add_flag(g_dot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(g_dot, LV_OBJ_FLAG_CLICKABLE);

    if (g_repair.eligible) {
        lv_obj_remove_flag(g_formatBtn, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(g_touch, "Storage failed its check.");
    } else if (storageOk(g_storage)) {
        lv_obj_remove_flag(g_testBtn, LV_OBJ_FLAG_HIDDEN);
    }
}

// One word for the status screen.
const char *rebootTestWord()
{
    if (g_test.pendingToken != 0) return "pending";
    switch (g_test.last) {
    case bringup::RebootTestResult::None: return "not run";
    case bringup::RebootTestResult::Pass: return "PASS";
    default: return "FAIL";
    }
}

void updateStatus()
{
    readStorage(g_storage);
    const bool psramOk = bringup::psramAsExpected(g_facts.psramFound, g_facts.psramSize);
    const bool mainOk = bringup::allPresent(bringup::kMainBus, g_facts.mainBus);
    const bool touchOk = bringup::allPresent(bringup::kTouchBus, g_facts.touchBus);
    lv_label_set_text_fmt(g_status,
                          "PSRAM: %lu KB. %s.\n"
                          "Main I2C bus: %s.\n"
                          "Touch I2C bus: %s.\n"
                          "GNSS: %s.\n"
                          "Battery: %d%%%s.\n"
                          "FFat: %s.\n"
                          "Reboot test: %s.",
                          static_cast<unsigned long>(g_facts.psramSize / 1024), passFail(psramOk),
                          passFail(mainOk), passFail(touchOk), bringup::gnssName(g_facts.gnss),
                          instance.pmu.getBatteryPercent(), instance.pmu.isCharging() ? ", charging" : "",
                          passFail(storageOk(g_storage)), rebootTestWord());
}

void pollTouch(uint32_t nowMs)
{
    int16_t x = 0;
    int16_t y = 0;
    if (instance.getPoint(&x, &y, 1) == 0) return;
    if (x == g_lastX && y == g_lastY) return;
    g_lastX = x;
    g_lastY = y;
    if (!g_hold.holding() && !g_repair.ran && !g_repair.eligible && g_test.pendingToken == 0) {
        lv_label_set_text_fmt(g_touch, "Touch at x %d, y %d.", x, y);
    }
    lv_obj_remove_flag(g_dot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(g_dot, x - 7, y - 7);
    Serial.printf("[s3plus] touch x=%d y=%d\n", x, y);
    if (!g_hold.holding() && nowMs - g_lastHapticMs >= kHapticGapMs) {
        g_lastHapticMs = nowMs;
        instance.vibrator();
    }
}

} // namespace

void setup()
{
    Serial.begin(115200);
    // Native USB: the port opens seconds after boot. Wait for it (bounded),
    // so the first report is not printed into a closed port.
    const uint32_t start = millis();
    while (!Serial && millis() - start < kSerialWaitMs) delay(10);
    Serial.printf("\n[s3plus] LayerTime T-Watch S3 Plus bring-up %s starting\n", kVersion);

    // Before instance.begin(): LilyGoLib waits forever if there is no PSRAM.
    g_facts.psramFound = psramFound();
    g_facts.psramSize = ESP.getPsramSize();
    Serial.printf("[s3plus] PSRAM found=%d size=%lu bytes\n", g_facts.psramFound ? 1 : 0,
                  static_cast<unsigned long>(g_facts.psramSize));
    esp_efuse_mac_get_default(g_facts.mac);

    // The radio is excluded from begin() and its rail is cut right after.
    instance.begin(NO_HW_LORA);
    instance.powerControl(POWER_RADIO, false);
    g_facts.probe = instance.getDeviceProbe();

    // If LilyGoLib's begin() left nothing mounted, try one plain mount at /fs
    // (no format) and record the result.
    readStorage(g_storage);
    if (g_storage.fsInfo != ESP_OK && g_storage.ffatInfo != ESP_OK && g_storage.mountpoint[0] == '\0') {
        g_storage.retried = true;
        g_storage.retryMounted = FFat.begin(false, kMountPoint);
    }
    completeRebootTest();
    decideEligibility();

    scan(Wire, g_facts.mainBus);
    scan(Wire1, g_facts.touchBus);
    const String model = instance.gps.getModel();
    strncpy(g_facts.gnssModel, model.c_str(), sizeof(g_facts.gnssModel) - 1);
    g_facts.gnss = bringup::classifyGnss(g_facts.gnssModel);

    beginLvglHelper(instance);
    instance.setBrightness(kBrightness);
    buildScreen();
    updateStatus();

    instance.vibrator();
    g_lastHapticMs = millis();
    printReport();
    g_lastReportMs = millis();
}

void loop()
{
    instance.loop();
    const uint32_t now = millis();
    pollTouch(now);
    pollFormatControl(now);
    if (g_test.requested) {
        startRebootTest();
        updateStatus();
    }
    if (now - g_lastReportMs >= kReportIntervalMs) {
        g_lastReportMs = now;
        updateStatus();
        printReport();
    }
    lv_timer_handler();
    delay(5);
}
