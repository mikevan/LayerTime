#pragma once

#include "../model/WatchState.h"

class ClockService {
public:
    void update(WatchState &state);
    void setDateTime(int year, int month, int day, int hour, int minute, int second = 0);
};
