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

#include "QuickMessages.h"

namespace layertime {
namespace mesh {

namespace {
const QuickMessage kDefault[] = {
    {0, "Yes"},
    {1, "No"},
    {2, "OK"},
    {3, "On my way"},
    {4, "Be there in 10"},
    {5, "Almost there"},
    {6, "Running late"},
    {7, "Where are you?"},
    {8, "Im here"},
    {9, "Heading back"},
    {10, "Wait for me"},
    {11, "Need help"},
    {12, "All clear"},
    {13, "Copy that"},
    {14, "Standby"},
    {15, "Call me"},
    {16, "Cant talk"},
    {17, "Radio check"},
    {18, "Low battery"},
    {19, "Good night"},
};
constexpr uint8_t kDefaultCount = sizeof(kDefault) / sizeof(kDefault[0]);
static_assert(kDefaultCount <= QuickMessageLibrary::kMaxMessages, "library over its limit");
} // namespace

const QuickMessage *defaultQuickMessages(uint8_t &count)
{
    count = kDefaultCount;
    return kDefault;
}

const QuickMessage *findQuickMessage(uint8_t id)
{
    for (const QuickMessage &m : kDefault)
        if (m.id == id) return &m;
    return nullptr;
}

} // namespace mesh
} // namespace layertime
