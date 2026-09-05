// LayerTime GPS / sensor diagnostic firmware.
//
// TEMPORARY DIAGNOSTIC. This is not part of LayerTime. It replaces the
// firmware on the watch; reflash LayerTime when you are done.
//
// Answers three questions that source code cannot:
//
//   1. What is actually populated on the I2C bus, specifically whether a
//      magnetometer exists. That decides whether dead reckoning can take a
//      live heading or has to ask the user for his azimuth.
//
//   2. What the u-blox MIA-M10Q emits when it loses the constellation.
//      Does the position freeze while time keeps advancing? That is the
//      stale-fix hazard, and it is not answerable by reading GpsService.
//
//   3. Whether hAcc (the receiver's own horizontal accuracy estimate),
//      the spoofing detector, and the jamming detector report anything
//      useful. All three exist in UBX and none of them exist in NMEA,
//      which is all the firmware currently reads.
//
// Everything decoded is ALSO printed as raw hex. Do not trust the decode.
// Check it against the u-blox M10 SPG interface description before any of
// it turns into firmware.
//
// Copyright (C) 2026 Michael Van Geertruy. GPL-3.0-or-later.

#include <Arduino.h>
#include <LilyGoLib.h>
#include <Wire.h>

// ------------------------------------------------------------------ config

static constexpr uint32_t kPollIntervalMs = 2000;
static constexpr uint32_t kSummaryIntervalMs = 10000;

// UBX message class/id pairs we poll. A poll is the frame with a zero-length
// payload; the receiver answers on the port that asked. Polling avoids the
// M10 configuration-key problem entirely - no CFG-VALSET, nothing persisted,
// nothing to undo on the module.
struct UbxPoll { uint8_t cls; uint8_t id; const char *name; };
static const UbxPoll kPolls[] = {
    {0x01, 0x07, "NAV-PVT"},     // position, hAcc, fixType, numSV
    {0x01, 0x03, "NAV-STATUS"},  // spoofDetState lives here
    {0x0A, 0x38, "MON-RF"},      // jammingState, jamInd, AGC
    {0x0A, 0x04, "MON-VER"},     // firmware version, so we know what we are talking to
};

// ------------------------------------------------------------------ i2c

struct KnownAddr { uint8_t addr; const char *what; };
static const KnownAddr kKnown[] = {
    // Confirmed against LilyGo's own T-Watch Ultra hardware doc, 2026-09-05.
    {0x1A, "CST9217 capacitive touch"},
    {0x20, "XL9555 GPIO expander"},
    {0x28, "BHI260AP smart sensor (6-axis, NO magnetometer)"},
    {0x34, "AXP2101 power management"},
    {0x51, "PCF85063A RTC"},
    {0x5A, "DRV2605 haptic driver"},
    // Magnetometer addresses, listed so their ABSENCE is explicit.
    {0x0C, "AK09918 magnetometer"},
    {0x0D, "QMC5883L magnetometer"},
    {0x1E, "HMC5883L magnetometer"},
    {0x30, "MMC5603 magnetometer"},
};

static const char *describeAddr(uint8_t addr)
{
    for (const KnownAddr &k : kKnown) {
        if (k.addr == addr) return k.what;
    }
    return "unknown";
}

static void scanBus(TwoWire &bus, const char *label)
{
    Serial.printf("\n--- I2C scan: %s ---\n", label);
    int found = 0;
    for (uint8_t addr = 0x08; addr < 0x78; ++addr) {
        bus.beginTransmission(addr);
        if (bus.endTransmission() == 0) {
            Serial.printf("  0x%02X  %s\n", addr, describeAddr(addr));
            ++found;
        }
        delay(2);
    }
    if (found == 0) Serial.println("  nothing responded");
    Serial.printf("--- %d device(s) on %s ---\n", found, label);
}

// ------------------------------------------------------------------ ubx

static void ubxChecksum(const uint8_t *buf, size_t len, uint8_t &ckA, uint8_t &ckB)
{
    ckA = 0; ckB = 0;
    for (size_t i = 0; i < len; ++i) { ckA += buf[i]; ckB += ckA; }
}

static void sendPoll(uint8_t cls, uint8_t id)
{
    uint8_t body[4] = {cls, id, 0x00, 0x00};
    uint8_t ckA, ckB;
    ubxChecksum(body, sizeof(body), ckA, ckB);
    const uint8_t frame[8] = {0xB5, 0x62, body[0], body[1], body[2], body[3], ckA, ckB};
    Serial1.write(frame, sizeof(frame));
}

static uint32_t rdU32(const uint8_t *p, size_t o) {
    return (uint32_t)p[o] | ((uint32_t)p[o+1] << 8) | ((uint32_t)p[o+2] << 16) | ((uint32_t)p[o+3] << 24);
}
static int32_t rdI32(const uint8_t *p, size_t o) { return (int32_t)rdU32(p, o); }
static uint16_t rdU16(const uint8_t *p, size_t o) { return (uint16_t)p[o] | ((uint16_t)p[o+1] << 8); }

static const char *fixTypeName(uint8_t t)
{
    switch (t) {
        case 0: return "no fix";
        case 1: return "dead reckoning only";
        case 2: return "2D";
        case 3: return "3D";
        case 4: return "GNSS + dead reckoning";
        case 5: return "time only";
        default: return "?";
    }
}

static const char *spoofName(uint8_t s)
{
    switch (s) {
        case 0: return "unknown or deactivated";
        case 1: return "no spoofing indicated";
        case 2: return "SPOOFING INDICATED";
        case 3: return "MULTIPLE SPOOFING INDICATIONS";
        default: return "?";
    }
}

static const char *jamName(uint8_t s)
{
    switch (s) {
        case 0: return "unknown or disabled";
        case 1: return "ok, no significant jamming";
        case 2: return "WARNING: interference visible, fix still held";
        case 3: return "CRITICAL: interference visible, no fix";
        default: return "?";
    }
}

static void dumpHex(const uint8_t *p, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        Serial.printf("%02X", p[i]);
        if ((i & 0x0F) == 0x0F) Serial.print(' ');
    }
}

// Last seen values, for the periodic summary line.
static bool     gSawPvt = false;
static uint32_t gPvtMillis = 0;
static int32_t  gLat = 0, gLon = 0;
static uint32_t gHAcc = 0;
static uint8_t  gFixType = 0, gNumSV = 0, gFlags = 0;
static uint16_t gPdop = 0;
static uint32_t gItow = 0;
static int32_t  gLastLat = 0, gLastLon = 0;
static uint32_t gFrozenSince = 0;
static bool     gEverHadFix = false;

static void decodeNavPvt(const uint8_t *p, uint16_t len)
{
    if (len < 92) { Serial.printf("  NAV-PVT short (%u bytes)\n", len); return; }
    gItow    = rdU32(p, 0);
    gFixType = p[20];
    gFlags   = p[21];
    gNumSV   = p[23];
    gLon     = rdI32(p, 24);
    gLat     = rdI32(p, 28);
    gHAcc    = rdU32(p, 40);
    gPdop    = rdU16(p, 76);

    // Freeze detector. Only meaningful once a real fix has existed: NAV-PVT
    // reports 0,0 with fixType 0 before first fix, and an unchanging null
    // island is not a stale position. Arm on the first valid fix.
    const bool fixValid = (gFixType >= 2) && (gFlags & 0x01);
    if (fixValid) gEverHadFix = true;
    if (!gEverHadFix) {
        gFrozenSince = 0;
    } else if (gLat == gLastLat && gLon == gLastLon) {
        if (gFrozenSince == 0) gFrozenSince = millis();
    } else {
        gFrozenSince = 0;
    }
    if (fixValid) { gLastLat = gLat; gLastLon = gLon; }

    gSawPvt = true;
    gPvtMillis = millis();

    Serial.printf("  iTOW=%lu  fix=%u(%s)  gnssFixOK=%u  numSV=%u\n",
                  (unsigned long)gItow, gFixType, fixTypeName(gFixType),
                  (unsigned)(gFlags & 0x01), gNumSV);
    if (gHAcc == 0xFFFFFFFFUL) {
        Serial.printf("  lat=%.7f lon=%.7f  hAcc=INVALID (0xFFFFFFFF)  pDOP=%.2f\n",
                      gLat * 1e-7, gLon * 1e-7, gPdop * 0.01);
    } else {
        Serial.printf("  lat=%.7f lon=%.7f  hAcc=%lu mm (%.2f m)  pDOP=%.2f\n",
                      gLat * 1e-7, gLon * 1e-7,
                      (unsigned long)gHAcc, gHAcc / 1000.0,
                      gPdop * 0.01);
    }
    if (gFrozenSince != 0) {
        Serial.printf("  *** POSITION UNCHANGED for %lu ms while iTOW advances ***\n",
                      (unsigned long)(millis() - gFrozenSince));
    }
}

static void decodeNavStatus(const uint8_t *p, uint16_t len)
{
    if (len < 16) { Serial.printf("  NAV-STATUS short (%u bytes)\n", len); return; }
    const uint8_t gpsFix = p[4];
    const uint8_t flags  = p[5];
    const uint8_t flags2 = p[7];
    const uint32_t ttff  = rdU32(p, 8);
    const uint8_t spoof  = (flags2 >> 3) & 0x03;   // VERIFY against interface description
    Serial.printf("  gpsFix=%u  flags=0x%02X  flags2=0x%02X  ttff=%lu ms\n",
                  gpsFix, flags, flags2, (unsigned long)ttff);
    Serial.printf("  spoofDetState=%u (%s)   <-- verify bit position before trusting\n",
                  spoof, spoofName(spoof));
}

static void decodeMonRf(const uint8_t *p, uint16_t len)
{
    if (len < 4) { Serial.printf("  MON-RF short (%u bytes)\n", len); return; }
    const uint8_t nBlocks = p[1];
    Serial.printf("  version=%u nBlocks=%u\n", p[0], nBlocks);
    for (uint8_t b = 0; b < nBlocks; ++b) {
        const size_t off = 4 + (size_t)b * 24;
        if (off + 24 > len) break;
        const uint8_t blockId = p[off + 0];
        const uint8_t flags   = p[off + 1];
        const uint8_t jamming = flags & 0x03;
        const uint8_t antStat = p[off + 2];
        const uint16_t noise  = rdU16(p, off + 12);
        const uint16_t agc    = rdU16(p, off + 14);
        const uint8_t jamInd  = p[off + 16];
        Serial.printf("  RF block %u: jammingState=%u (%s)\n", blockId, jamming, jamName(jamming));
        Serial.printf("     antStatus=%u noisePerMS=%u agcCnt=%u jamInd=%u\n",
                      antStat, noise, agc, jamInd);
    }
}

// ------------------------------------------------------------------ parser

static uint8_t  gUbxBuf[512];
static uint16_t gUbxLen = 0;
static uint16_t gUbxExpect = 0;
static uint8_t  gState = 0;
static uint8_t  gCls = 0, gId = 0;

static char     gNmeaLine[128];
static uint8_t  gNmeaLen = 0;
static uint32_t gNmeaCount = 0;
static uint32_t gUbxCount = 0;
static uint32_t gByteCount = 0;

static void handleUbx()
{
    ++gUbxCount;
    Serial.printf("\n[%lu ms] UBX %02X/%02X len=%u\n",
                  (unsigned long)millis(), gCls, gId, gUbxLen);
    if (gCls == 0x01 && gId == 0x07)      decodeNavPvt(gUbxBuf, gUbxLen);
    else if (gCls == 0x01 && gId == 0x03) decodeNavStatus(gUbxBuf, gUbxLen);
    else if (gCls == 0x0A && gId == 0x38) decodeMonRf(gUbxBuf, gUbxLen);
    else if (gCls == 0x0A && gId == 0x04) {
        Serial.print("  MON-VER: ");
        for (uint16_t i = 0; i < gUbxLen && i < 40; ++i) {
            const char c = (char)gUbxBuf[i];
            Serial.print((c >= 32 && c < 127) ? c : '.');
        }
        Serial.println();
    } else if (gCls == 0x05) {
        Serial.printf("  ACK/NAK class, id=%02X\n", gId);
    }
    Serial.print("  raw: ");
    dumpHex(gUbxBuf, gUbxLen < 96 ? gUbxLen : 96);
    if (gUbxLen > 96) Serial.print("...");
    Serial.println();
}

static void feed(uint8_t c)
{
    ++gByteCount;

    // UBX sync
    switch (gState) {
        case 0: if (c == 0xB5) { gState = 1; return; } break;
        case 1: if (c == 0x62) { gState = 2; return; } gState = 0; break;
        case 2: gCls = c; gState = 3; return;
        case 3: gId = c; gState = 4; return;
        case 4: gUbxExpect = c; gState = 5; return;
        case 5: gUbxExpect |= ((uint16_t)c << 8); gUbxLen = 0; gState = (gUbxExpect == 0) ? 7 : 6;
                if (gUbxExpect > sizeof(gUbxBuf)) { gState = 0; }
                return;
        case 6: gUbxBuf[gUbxLen++] = c; if (gUbxLen >= gUbxExpect) gState = 7; return;
        case 7: gState = 8; return;              // ck_a, not verified here
        case 8: gState = 0; handleUbx(); return; // ck_b
        default: gState = 0; break;
    }

    // NMEA
    if (c == '$') { gNmeaLen = 0; gNmeaLine[gNmeaLen++] = c; return; }
    if (gNmeaLen > 0) {
        if (c == '\r' || c == '\n') {
            gNmeaLine[gNmeaLen] = '\0';
            ++gNmeaCount;
            // Only echo GGA and RMC. GSV floods the log and tells us nothing new.
            if (strstr(gNmeaLine, "GGA") || strstr(gNmeaLine, "RMC")) {
                Serial.printf("[%lu ms] %s\n", (unsigned long)millis(), gNmeaLine);
            }
            gNmeaLen = 0;
            return;
        }
        if (gNmeaLen < sizeof(gNmeaLine) - 1) gNmeaLine[gNmeaLen++] = c;
    }
}

// ------------------------------------------------------------------ setup

static void tryBaud(uint32_t baud)
{
    // Explicit begin on the variant's own pins. The previous version called
    // updateBaudRate(), which assumes something already opened Serial1. If
    // nothing had, that was undefined behaviour.
    Serial.printf("  trying %lu baud on RX=%d TX=%d ... ",
                  (unsigned long)baud, (int)GPS_RX, (int)GPS_TX);
    Serial.flush();
    Serial1.end();
    delay(20);
    Serial1.begin(baud, SERIAL_8N1, GPS_RX, GPS_TX);
    delay(50);
    while (Serial1.available()) Serial1.read();
    const uint32_t deadline = millis() + 1500;
    uint32_t n = 0, starts = 0;
    while (millis() < deadline) {
        while (Serial1.available()) {
            const uint8_t c = (uint8_t)Serial1.read();
            ++n;
            if (c == '$' || c == 0xB5) ++starts;
        }
    }
    Serial.printf("%lu bytes, %lu frame starts\n",
                  (unsigned long)n, (unsigned long)starts);
}

void setup()
{
    Serial.begin(115200);

    // Native USB CDC: Serial goes true when the HOST opens the port, which is
    // seconds after boot. The previous version waited 3 s and pressed on, so a
    // crash at 1 s printed into a port nobody was listening to and looked like
    // a dead watch. Wait for the host. If nobody connects, carry on after 20 s.
    const uint32_t t0 = millis();
    while (!Serial && millis() - t0 < 20000) delay(50);
    delay(300);

    Serial.println();
    Serial.println("========================================");
    Serial.println(" LayerTime GPS / sensor diagnostic");
    Serial.println(" TEMPORARY FIRMWARE - reflash LayerTime when done");
    Serial.println("========================================");
    Serial.printf(" boot at %lu ms, host attached at %lu ms\n",
                  (unsigned long)t0, (unsigned long)millis());
    Serial.flush();

    // Every step below prints BEFORE it runs. If the log stops after a marker,
    // that marker is the thing that died. No guessing.
    Serial.println("\n[step] instance.begin() ...");
    Serial.flush();
    instance.begin();
    Serial.println("[ok]   instance.begin()");
    Serial.flush();

    Serial.printf("\n[step] Wire.begin(SDA=%d, SCL=%d) ...\n", (int)SDA, (int)SCL);
    Serial.flush();
    Wire.begin(SDA, SCL);
    Serial.println("[ok]   Wire.begin()");

    scanBus(Wire, "Wire");
    // Wire1 is deliberately NOT scanned. Nothing has begun it, and calling
    // beginTransmission on an unopened bus can hang with no timeout.

    Serial.println("\n[step] powerControl(POWER_GPS, true) ...");
    Serial.flush();
    instance.powerControl(POWER_GPS, true);
    delay(500);
    Serial.println("[ok]   GNSS rail on");

    Serial.println("\n[step] probing UART1 baud rates ...");
    Serial.flush();
    tryBaud(38400);
    tryBaud(9600);
    tryBaud(115200);
    Serial.println("  settling on 38400 (u-blox M10 UART1 default). If the byte");
    Serial.println("  counts above say otherwise, tell Claude which one won.");
    Serial1.end();
    delay(20);
    Serial1.begin(38400, SERIAL_8N1, GPS_RX, GPS_TX);
    delay(100);
    while (Serial1.available()) Serial1.read();

    Serial.println("\n--- streaming. NMEA GGA/RMC echoed, UBX polled every 2 s ---");
    Serial.println("--- I2C rescans every 30 s, so you cannot miss it ---");
    Serial.println("--- get a fix by a window, then walk inside and watch ---\n");
}

// ------------------------------------------------------------------ loop

void loop()
{
    static uint32_t lastPoll = 0;
    static uint32_t lastSummary = 0;
    static uint32_t lastScan = 0;
    static uint8_t pollIndex = 0;

    while (Serial1.available()) feed((uint8_t)Serial1.read());

    const uint32_t now = millis();

    if (now - lastPoll >= kPollIntervalMs) {
        lastPoll = now;
        const UbxPoll &p = kPolls[pollIndex];
        Serial.printf("\n>>> poll %s (%02X/%02X)\n", p.name, p.cls, p.id);
        sendPoll(p.cls, p.id);
        pollIndex = (uint8_t)((pollIndex + 1) % (sizeof(kPolls) / sizeof(kPolls[0])));
    }

    if (now - lastSummary >= kSummaryIntervalMs) {
        lastSummary = now;
        Serial.printf("\n===== %lu s  bytes=%lu nmea=%lu ubx=%lu =====\n",
                      (unsigned long)(now / 1000), (unsigned long)gByteCount,
                      (unsigned long)gNmeaCount, (unsigned long)gUbxCount);
        if (gSawPvt) {
            if (gHAcc == 0xFFFFFFFFUL) {
                Serial.printf("  last PVT %lu ms ago: fix=%s numSV=%u hAcc=INVALID\n",
                              (unsigned long)(now - gPvtMillis), fixTypeName(gFixType), gNumSV);
            } else {
                Serial.printf("  last PVT %lu ms ago: fix=%s numSV=%u hAcc=%.2f m\n",
                              (unsigned long)(now - gPvtMillis), fixTypeName(gFixType),
                              gNumSV, gHAcc / 1000.0);
            }
            if (!gEverHadFix) {
                Serial.println("  NO FIX HELD YET - go outside. The stale-fix test");
                Serial.println("  needs a real fix before it means anything.");
            }
            if (gFrozenSince != 0) {
                Serial.printf("  POSITION FROZEN for %lu ms\n",
                              (unsigned long)(now - gFrozenSince));
            }
        } else {
            Serial.println("  no UBX NAV-PVT seen yet. If this persists, the module");
            Serial.println("  is not answering polls and we need CFG-VALSET instead.");
        }
        Serial.println("=====");
    }

    if (now - lastScan >= 30000) {
        lastScan = now;
        scanBus(Wire, "Wire (rescan)");
    }
}
