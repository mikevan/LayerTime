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

#include <stddef.h>

// Canned phrases that replace the on-screen keyboard in the MeshCore and
// Meshtastic composers. Typing on a watch while moving does not work; someone
// who needs to say something needs one tap, not thirty.
//
// Meshtastic's own CannedMessageModule stores its list as a pipe-delimited
// string capped at 200 bytes. Joined with '|' this list is 199 bytes, so it
// can be pushed from the phone app later without trimming anything.
namespace QuickPhrases {

static const char *const kPhrases[] = {
    "Yes",
    "No",
    "OK",
    "On my way",
    "Be there in 10",
    "Almost there",
    "Running late",
    "Where are you?",
    "Im here",
    "Heading back",
    "Wait for me",
    "Need help",
    "All clear",
    "Copy that",
    "Standby",
    "Call me",
    "Cant talk",
    "Radio check",
    "Low battery",
    "Good night",
};

static const size_t kCount = sizeof(kPhrases) / sizeof(kPhrases[0]);

} // namespace QuickPhrases
