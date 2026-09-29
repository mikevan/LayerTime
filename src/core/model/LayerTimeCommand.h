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

// Canonical definition: contracts/commands.md.

#include <stdint.h>

#include "Mesh.h"
#include "MonitorEvent.h"

namespace layertime {

// What a screen asks the application to do. Screens issue these instead of
// calling a service directly.
//
// The set is exactly what the T-Ultra screens call on services today, plus
// MeshSendQuickMessage, which the architecture names as the model example.
// Phase 0 Step 6 split the settings: application settings change by the
// commands from 9 on; T-Ultra-only ones (brightness, GPS power, SD logging,
// Squachify, the shared-radio switch) stay with the platform.
//
// Numeric values are part of the contract. Do not renumber; append only.
enum class CommandType : uint8_t {
    None = 0,
    ReconStart = 1,             // args: reconTarget
    ReconStop = 2,              // leave manual mode; early warning resumes if enabled
    ReconClearEvents = 3,
    ReconAcknowledgeAlert = 4,
    MeshSendText = 5,           // args: meshText
    MeshSendQuickMessage = 6,   // args: meshQuick
    MeshSetChannel = 7,         // args: meshChannel. Meshtastic only.
    MeshRemoveChannel = 8,      // args: meshChannel.index. Meshtastic only.
    SetClockFormat = 9,         // args: setting.enabled (true = 24-hour)
    SetUnits = 10,              // args: setting.enabled (true = metric)
    SetSleepMode = 11,          // args: setting.enabled
    SetEarlyWarning = 12,       // args: setting.enabled
    MeshSetAdvertising = 13,    // args: setting.network, setting.enabled
    MeshSetOwnName = 14,        // args: setting.network, setting.name. Meshtastic only.
};

// Every command gets exactly one of these back. Nothing is silently ignored.
enum class CommandResult : uint8_t {
    Ok = 0,
    Unsupported = 1,      // this platform or network cannot do it at all
    InvalidArgument = 2,  // it can, but not with these arguments
    NotReady = 3,         // it can, but not right now (radio off, not ready)
    Failed = 4,           // it tried and the attempt failed
};

struct ReconStartArgs {
    ReconTarget target = ReconTarget::None;
};

struct MeshSendTextArgs {
    // Longest text a command carries: what the T-Ultra's Meshtastic composer
    // allows. The buffer has room for the terminating zero on top.
    static constexpr uint8_t kMaxTextChars = 160;
    static constexpr uint8_t kTextSize = kMaxTextChars + 1;

    MeshNetwork network = MeshNetwork::Meshtastic;
    MeshDestination destination;
    char text[kTextSize] = {0};
};

struct MeshSendQuickArgs {
    MeshNetwork network = MeshNetwork::Meshtastic;
    MeshDestination destination;
    uint8_t quickMessageId = 0;
};

struct MeshChannelArgs {
    // Buffer sizes, including the terminating zero.
    static constexpr uint8_t kNameSize = 12; // Meshtastic: 11 characters max
    static constexpr uint8_t kKeySize = 49;  // key as the user typed it, up to 48 characters

    uint8_t index = 0;
    char name[kNameSize] = {0};
    char key[kKeySize] = {0};
};

// For the settings commands. Only the fields a command names are read.
struct SettingArgs {
    static constexpr uint8_t kNameSize = 20; // 19 characters, as the T-Ultra takes it

    MeshNetwork network = MeshNetwork::Meshtastic;
    bool enabled = false;
    char name[kNameSize] = {0};
};

// A plain struct rather than a union: every argument block has default
// initialisers, commands are short-lived, and about 300 bytes on the stack
// is cheaper than getting union lifetime rules wrong. Only the block named
// for `type` is read; the rest stay at their defaults.
struct LayerTimeCommand {
    CommandType type = CommandType::None;
    ReconStartArgs reconTarget;
    MeshSendTextArgs meshText;
    MeshSendQuickArgs meshQuick;
    MeshChannelArgs meshChannel;
    SettingArgs setting;
};

} // namespace layertime
