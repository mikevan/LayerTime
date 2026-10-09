// TEST-ONLY stand-in for ESP-IDF's esp_wifi.h. Never part of the firmware
// build.
//
// The promiscuous receive callback is captured instead of registered with a
// radio, so a test can hand the production code a crafted 802.11 frame the
// same way the Wi-Fi driver would. Field names in the packet header follow
// ESP-IDF's wifi_pkt_rx_ctrl_t; only the fields LayerTime reads are kept.

#pragma once

#include <stdint.h>

using esp_err_t = int;
constexpr esp_err_t ESP_OK = 0;

constexpr int WIFI_SECOND_CHAN_NONE = 0;

struct wifi_pkt_rx_ctrl_t {
    signed rssi : 8;
    unsigned channel : 4;
    unsigned sig_len : 12;
    unsigned rx_state : 8;
};

struct wifi_promiscuous_pkt_t {
    wifi_pkt_rx_ctrl_t rx_ctrl;
    uint8_t payload[0];
};

using wifi_promiscuous_cb_t = void (*)(void *buf, int type);

namespace fake_wifi {
inline wifi_promiscuous_cb_t g_rxCallback = nullptr;
inline bool g_promiscuous = false;
inline uint8_t g_channel = 0;
}

inline esp_err_t esp_wifi_set_promiscuous(bool on)
{
    fake_wifi::g_promiscuous = on;
    return ESP_OK;
}
inline esp_err_t esp_wifi_set_promiscuous_rx_cb(wifi_promiscuous_cb_t cb)
{
    fake_wifi::g_rxCallback = cb;
    return ESP_OK;
}
inline esp_err_t esp_wifi_set_channel(uint8_t channel, int)
{
    fake_wifi::g_channel = channel;
    return ESP_OK;
}
