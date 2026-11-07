// LayerTime - passive early-warning firmware for the LILYGO T-Dongle-C5.
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

// Pulling the SD card's files over the USB serial port while the card stays in
// the Hunter (Michael, 2026-11-04). The ESP32-C5 has only a USB serial port, so
// it cannot appear as a USB drive; a PC script asks for files by name instead.
//
// Commands, one line each from the PC:
//   card list          every log and evidence file, with sizes
//   card get <name>    one file
// Replies are text lines that all start with "=== card ", so the script can
// pick them out of the Hunter's other serial output:
//   === card list begin
//   === card entry <name> <size>
//   === card list end <count>
//   === card file begin <name> <size>
//   === card data <base64 of up to kDataBytes bytes>
//   === card file end <name> <size> crc32 <8 hex digits>
//   === card error <name or -> <one sentence>
//
// Read only. Only the names the Hunter itself writes are served
// (hunter_NNNN.log and ev_NNNN/event_EEEEE.pcap), so a request can never
// reach any other file on the card. Hardware-free: tested with plain g++.

#include <stddef.h>
#include <stdint.h>

namespace layertime {
namespace tdongle_c5 {

// Raw bytes per data line: 48 bytes is 64 base64 characters.
constexpr size_t kCardDataBytes = 48;
constexpr size_t kCardLineSize = 160;        // longest reply line, terminator included
constexpr size_t kCardCommandSize = 64;      // longest command line accepted
constexpr size_t kCardNameSize = 40;

enum class CardCommand : uint8_t { None = 0, List = 1, Get = 2, Bad = 3 };

// Parses one command line (trailing \r and spaces ignored). For Get, the name
// goes in `name`. Bad: it starts with "card" but is not a valid command or the
// name is not one the Hunter serves. None: not a card command at all.
CardCommand parseCardCommand(const char *line, char *name, size_t nameSize);

// True for "hunter_NNNN.log" and "ev_NNNN/event_D....pcap" (5 to 10 digits),
// exactly as the Hunter writes them (lower case, no leading '/').
bool cardNameServed(const char *name);

// Base64 (RFC 4648, with padding). Returns the length written without the
// terminator, or 0 if it does not fit.
size_t base64Encode(const uint8_t *data, size_t length, char *out, size_t size);

// CRC-32 (IEEE 802.3, as zlib and PowerShell's check compute it). Start at 0,
// feed every chunk in order.
uint32_t crc32Update(uint32_t crc, const uint8_t *data, size_t length);

} // namespace tdongle_c5
} // namespace layertime
