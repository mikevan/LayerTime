#pragma once

#include "../model/WatchState.h"

class GpsService {
public:
    void begin(bool enabled);
    void setEnabled(bool enabled);
    void poll(WatchState &state);
    bool enabled() const { return _enabled; }

private:
    void applyEnabled(bool enabled);

    bool _enabled = true; // LilyGoLib powers/initializes GNSS during instance.begin().
};
