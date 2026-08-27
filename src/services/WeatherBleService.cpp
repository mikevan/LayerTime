#include "WeatherBleService.h"

#include <Arduino.h>
#include <NimBLEDevice.h>

#include <ctype.h>
#include <stdlib.h>
#include <string>

#include "../model/WatchState.h"

namespace {
// LayerTime Weather Bridge custom GATT service.
// Phone writes temperature as ASCII Celsius, e.g. "23.4" or "T=23.4".
static const char *SERVICE_UUID = "7e2a0001-6c74-4d54-8d47-4c4159455254";
static const char *TEMP_UUID    = "7e2a0002-6c74-4d54-8d47-4c4159455254";
static const char *STATUS_UUID  = "7e2a0003-6c74-4d54-8d47-4c4159455254";

WeatherBleService *gWeatherService = nullptr;
NimBLECharacteristic *gStatusCharacteristic = nullptr;

class WeatherCharacteristicCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &connInfo) override
    {
        if (gWeatherService != nullptr) {
            gWeatherService->handleWrite(characteristic, connInfo);
        }
    }
};

class WeatherServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer *, NimBLEConnInfo &) override
    {
        if (gWeatherService != nullptr) gWeatherService->setConnected(true);
    }

    void onDisconnect(NimBLEServer *, NimBLEConnInfo &, int) override
    {
        if (gWeatherService != nullptr) gWeatherService->setConnected(false);
    }
};

WeatherCharacteristicCallbacks gCharacteristicCallbacks;
WeatherServerCallbacks gServerCallbacks;
}

void WeatherBleService::begin()
{
    gWeatherService = this;

    if (!NimBLEDevice::isInitialized()) {
        NimBLEDevice::init("LayerTime");
    }

    NimBLEServer *server = NimBLEDevice::createServer();
    server->setCallbacks(&gServerCallbacks);
    server->advertiseOnDisconnect(true);

    NimBLEService *service = server->createService(SERVICE_UUID);

    NimBLECharacteristic *temperature = service->createCharacteristic(
        TEMP_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    temperature->setCallbacks(&gCharacteristicCallbacks);

    gStatusCharacteristic = service->createCharacteristic(
        STATUS_UUID,
        NIMBLE_PROPERTY::READ);
    gStatusCharacteristic->setValue("READY");

    service->start();

    NimBLEAdvertising *advertising = server->getAdvertising();
    advertising->setName("LayerTime");
    advertising->addServiceUUID(SERVICE_UUID);
    advertising->enableScanResponse(true);
    advertising->start();

    Serial.println("LayerTime BLE weather bridge advertising");
}

void WeatherBleService::handleWrite(
    NimBLECharacteristic *characteristic,
    NimBLEConnInfo &)
{
    if (characteristic == nullptr) return;

    std::string value = characteristic->getValue();
    if (value.empty()) return;

    const char *text = value.c_str();
    const char *number = text;

    // Accept either "23.4" or "T=23.4" / "TEMP=23.4".
    const char *equals = strchr(text, '=');
    if (equals != nullptr && *(equals + 1) != '\0') {
        number = equals + 1;
    }

    char *end = nullptr;
    const float tempC = strtof(number, &end);
    if (end == number) return;

    // Reject clearly impossible ambient values and malformed trailing data.
    while (end != nullptr && *end != '\0' && isspace(static_cast<unsigned char>(*end))) ++end;
    if (end != nullptr && *end != '\0') return;
    if (tempC < -80.0f || tempC > 80.0f) return;

    _temperatureC = tempC;
    _temperatureValid = true;
    _lastWeatherMs = millis();

    if (gStatusCharacteristic != nullptr) {
        char status[24];
        snprintf(status, sizeof(status), "TEMP %.1fC", tempC);
        gStatusCharacteristic->setValue(status);
    }

    Serial.printf("BLE weather update: %.1f C\n", tempC);
}

void WeatherBleService::setConnected(bool connected)
{
    _connected = connected;
}

void WeatherBleService::poll(WatchState &state)
{
    state.bluetoothConnected = _connected;

    const bool fresh = _temperatureValid &&
        (static_cast<uint32_t>(millis() - _lastWeatherMs) <= WEATHER_STALE_MS);

    state.weatherValid = fresh;
    if (fresh) {
        state.temperatureC = _temperatureC;
    }
}
