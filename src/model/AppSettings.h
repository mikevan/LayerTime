#pragma once

#include <stdint.h>

struct AppSettings {
    uint8_t brightness = 80;
    bool use24Hour = false;
    bool metricUnits = false;
    bool gpsEnabled = true;

    // Mesh/LoRa radio power + listening. Deliberately NOT persisted: every boot
    // starts with the radio off, and the user opts in each session from Settings.
    bool meshEnabled = false;
    // Whether Mesh also transmits signed adverts. Only takes effect while
    // meshEnabled is true. This one *is* persisted.
    bool meshAdvertiseEnabled = false;

    // Meshtastic listener - deliberately separate/parallel to MeshCore, kept
    // mutually exclusive with it since both would need the one physical
    // SX1262 radio. Deliberately NOT persisted, same reasoning as meshEnabled.
    bool meshtasticEnabled = false;

    // Meshtastic node identity, user-configurable in Settings > MESHTASTIC
    // NAME. Used as the NodeInfo long_name (and a derived short_name) when
    // meshtasticAdvertiseEnabled is on. Persisted; empty until the user sets
    // one, in which case a name is auto-generated from the chip ID instead.
    char meshtasticNodeName[20] = {0};

    // Whether Meshtastic also periodically transmits a NodeInfo advert
    // announcing meshtasticNodeName, so others see a name instead of a raw
    // node number. Only takes effect while meshtasticEnabled is true. This
    // one *is* persisted, mirroring meshAdvertiseEnabled.
    bool meshtasticAdvertiseEnabled = false;

    // Background Wi-Fi/BLE early-warning sweep (Deauth/Pwnagotchi/Pineapple/
    // MultiSSID + Flipper/Meta), duty-cycled for low power draw. Persisted;
    // defaults on.
    bool reconEarlyWarningEnabled = true;

    // Append every new (non-duplicate) Recon detection to a CSV file on the
    // SD card. Persisted; defaults off (no card assumed present).
    bool reconSdLoggingEnabled = false;

    // Sleep mode: forces the backlight off immediately (instead of waiting
    // out the normal auto-blank timeout) and silences Recon's vibration/
    // popup alert. Detections still get logged normally while it's on -
    // only the disruptive alert is muted, so nothing is missed overnight.
    // Persisted; defaults off.
    bool sleepModeEnabled = false;
};
