// Copyright (C) 2026 Michael Van Geertruy
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <Arduino.h>
#include "TDongleC5Pins.h"

namespace layertime {
namespace tdongle_c5 {

// The SD chip select floats at boot. Keep the card off the shared SPI bus,
// even on beacon firmware that does not mount or use a card.
inline void holdC5CardDeselected()
{
    digitalWrite(pins::kSdCs, HIGH);
    pinMode(pins::kSdCs, OUTPUT);
}

} // namespace tdongle_c5
} // namespace layertime
