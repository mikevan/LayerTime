// TEST-ONLY stand-in for NimBLE-Arduino. Never part of the firmware build.
//
// The advertised-device fake carries exactly the fields ReconService reads.
// Two behaviours are copied from NimBLE-Arduino 2.2.3 as installed in this
// project's libdeps, because the tests depend on them being faithful:
//
//   * NimBLEAddress stores its six bytes least-significant first. getVal()
//     returns that raw array, and toString() prints it most-significant
//     first (val[5] down to val[0]). See NimBLEAddress.cpp, operator
//     std::string().
//   * toString() prints lowercase hex with ':' separators, because
//     CONFIG_NIMBLE_CPP_ADDR_FMT_UPPERCASE is not defined in nimconfig.h.

#pragma once

#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>

class NimBLEAddress {
public:
    NimBLEAddress() = default;
    // Takes the address as printed, most-significant octet first, and stores
    // it the way NimBLE does.
    static NimBLEAddress fromPrinted(const uint8_t printed[6])
    {
        NimBLEAddress a;
        for (int i = 0; i < 6; ++i) a.val[i] = printed[5 - i];
        return a;
    }
    const uint8_t *getVal() const { return val; }
    std::string toString() const
    {
        char buf[18];
        snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
                 val[5], val[4], val[3], val[2], val[1], val[0]);
        return std::string(buf);
    }

private:
    uint8_t val[6] = {0};
};

class NimBLEUUID {
public:
    NimBLEUUID() = default;
    NimBLEUUID(uint16_t uuid) : _value(uuid), _bits(16) {}
    static NimBLEUUID with128Bits()
    {
        NimBLEUUID u;
        u._bits = 128;
        return u;
    }
    uint8_t bitSize() const { return _bits; }
    bool equals(const NimBLEUUID &other) const
    {
        return _bits == 16 && other._bits == 16 && _value == other._value;
    }

private:
    uint16_t _value = 0;
    uint8_t _bits = 0;
};

class NimBLEAdvertisedDevice {
public:
    std::string name;
    bool hasName = false;
    NimBLEAddress address;
    int rssi = 0;
    std::vector<std::string> manufacturerData;
    std::vector<NimBLEUUID> serviceUuids;

    bool haveName() const { return hasName; }
    std::string getName() const { return name; }
    const NimBLEAddress &getAddress() const { return address; }
    int getRSSI() const { return rssi; }
    bool haveManufacturerData() const { return !manufacturerData.empty(); }
    uint8_t getManufacturerDataCount() const { return static_cast<uint8_t>(manufacturerData.size()); }
    std::string getManufacturerData(uint8_t i) const { return manufacturerData[i]; }
    bool haveServiceUUID() const { return !serviceUuids.empty(); }
    uint8_t getServiceUUIDCount() const { return static_cast<uint8_t>(serviceUuids.size()); }
    NimBLEUUID getServiceUUID(uint8_t i) const { return serviceUuids[i]; }
};

class NimBLEScanCallbacks {
public:
    virtual ~NimBLEScanCallbacks() = default;
    virtual void onResult(const NimBLEAdvertisedDevice *) {}
};

class NimBLEScan {
public:
    bool scanning = false;
    NimBLEScanCallbacks *callbacks = nullptr;
    bool isScanning() const { return scanning; }
    void stop() { scanning = false; }
    void clearResults() {}
    void setScanCallbacks(NimBLEScanCallbacks *cb) { callbacks = cb; }
    void setActiveScan(bool) {}
    void setInterval(int) {}
    void setWindow(int) {}
    void setMaxResults(int) {}
    bool start(uint32_t, bool) { scanning = true; return true; }
};

namespace fake_nimble {
inline NimBLEScan g_scan;
inline bool g_initialized = false;
}

class NimBLEDevice {
public:
    static bool init(const std::string &) { fake_nimble::g_initialized = true; return true; }
    static bool isInitialized() { return fake_nimble::g_initialized; }
    static NimBLEScan *getScan() { return &fake_nimble::g_scan; }
};
