// TEST-ONLY stand-in for the Arduino WiFi class. Never part of the firmware
// build.

#pragma once

constexpr int WIFI_STA = 1;

struct FakeWiFi {
    void mode(int) {}
    void disconnect(bool, bool) {}
};

inline FakeWiFi WiFi;
