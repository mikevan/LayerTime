// LayerTime - counter-intrusion and resilient-communications firmware
// for the LilyGo T-Watch Ultra.
//
// Copyright (C) 2026 Michael Van Geertruy
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

// Canonical definition: contracts/models.md, "QuickMessage". The default
// library is contracts/vectors/quick_messages_default.json.

#include <stdint.h>

namespace layertime {

// A pre-made message sent with one selection instead of typing.
struct QuickMessage {
    static constexpr uint8_t kTextSize = 32;

    // Stable within a library. A command refers to a message by id, not by
    // position in a list the wearer may have scrolled.
    uint8_t id = 0;
    char text[kTextSize] = {0};
};

struct QuickMessageLibrary {
    // The T-Ultra ships 20. Joined with '|' they fit Meshtastic's 200-byte
    // canned-message limit, which is why the ceiling is where it is.
    static constexpr uint8_t kMaxMessages = 20;

    QuickMessage messages[kMaxMessages];
    uint8_t count = 0;
};

} // namespace layertime
