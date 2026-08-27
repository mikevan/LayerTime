#include "BatteryService.h"

#include <LilyGoLib.h>

void BatteryService::update(WatchState &state)
{
    state.batteryConnected = instance.pmu.isBatteryConnect();

    if (state.batteryConnected) {
        state.batteryPercent = instance.pmu.getBatteryPercent();
    } else {
        state.batteryPercent = 0;
    }
}
