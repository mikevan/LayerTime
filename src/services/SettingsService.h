#pragma once

#include "../model/AppSettings.h"

class SettingsService {
public:
    void load(AppSettings &settings);
    void save(const AppSettings &settings);
    void apply(const AppSettings &settings);
};
