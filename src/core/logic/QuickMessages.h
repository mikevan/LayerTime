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

// The default quick-message library: canned phrases sent instead of typing.
// Moved from src/ui/QuickPhrases.h in Phase 0 Step 5, same phrases in the
// same order. Canonical copy: contracts/vectors/quick_messages_default.json.
//
// Typing on a watch while moving does not work; someone who needs to say
// something needs one tap, not thirty. Meshtastic's CannedMessageModule
// stores its list as a pipe-delimited string capped at 200 bytes. Joined
// with '|' this list is 199 bytes, so it can be pushed from the phone app
// later without trimming anything.
//
// The table is constant and lives in flash. Its text pointers stay valid
// for the life of the program.

#include <stdint.h>

#include "../model/QuickMessage.h"

namespace layertime {
namespace mesh {

// The whole library, in display order.
const QuickMessage *defaultQuickMessages(uint8_t &count);

// The message with this id, or nullptr if there is none.
const QuickMessage *findQuickMessage(uint8_t id);

} // namespace mesh
} // namespace layertime
