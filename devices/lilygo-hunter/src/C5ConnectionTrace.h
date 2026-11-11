#pragma once

// Diagnostic builds only. Observe radio state without restarting either radio.
#if defined(LAYERTIME_DIAG_CONNECTION_TRACE)
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <esp_heap_caps.h>
#include <esp_wifi.h>

inline void c5ConnectionTrace(const char *event, int result = 0)
{
    wifi_mode_t mode = WIFI_MODE_NULL;
    uint8_t channel = 0;
    wifi_second_chan_t secondary = WIFI_SECOND_CHAN_NONE;
    const int modeRc = esp_wifi_get_mode(&mode);
    const int channelRc = esp_wifi_get_channel(&channel, &secondary);
    const bool initialized = NimBLEDevice::isInitialized();
    Serial.printf("[conn-trace] ms=%lu event=%s result=%d ble=%u adv=%u scan=%u peers=%u "
                  "wifi=%d modeRc=%d channel=%u channelRc=%d heap=%lu largest=%lu\n",
                  static_cast<unsigned long>(millis()), event, result, initialized,
                  initialized && NimBLEDevice::getAdvertising()->isAdvertising(),
                  initialized && NimBLEDevice::getScan()->isScanning(),
                  initialized && NimBLEDevice::getServer() ? NimBLEDevice::getServer()->getConnectedCount() : 0,
                  static_cast<int>(mode), modeRc, channel, channelRc,
                  static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                  static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
}
#else
inline void c5ConnectionTrace(const char *, int = 0) {}
#endif
