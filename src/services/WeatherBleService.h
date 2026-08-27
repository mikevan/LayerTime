#pragma once

#include <stdint.h>

struct WatchState;
class NimBLECharacteristic;
class NimBLEConnInfo;

class WeatherBleService {
public:
    void begin();
    void poll(WatchState &state);

    // Called by the BLE characteristic callback.
    void handleWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &connInfo);
    void setConnected(bool connected);

    bool connected() const { return _connected; }

private:
    static constexpr uint32_t WEATHER_STALE_MS = 30UL * 60UL * 1000UL;

    bool _connected = false;
    bool _temperatureValid = false;
    float _temperatureC = 0.0f;
    uint32_t _lastWeatherMs = 0;
};
