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

#include "CardTransferLogic.h"
#include "SdLogLogic.h"

#include "core/link/LinkCard.h"

#include <string.h>

namespace layertime {
namespace tdongle_c5 {


bool cardNameServed(const char *name)
{
    // One rule for the USB pull and the Link pull: the names the Hunter writes.
    link::card::FileId id;
    return link::card::fileIdFromName(name, id);
}

CardCommand parseCardCommand(const char *line, char *name, size_t nameSize)
{
    if (name != nullptr && nameSize > 0) name[0] = '\0';
    if (line == nullptr) return CardCommand::None;
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n' || line[len - 1] == ' ')) --len;
    if (len < 4 || strncmp(line, "card", 4) != 0) return CardCommand::None;
    if (len == 9 && strncmp(line, "card list", 9) == 0) return CardCommand::List;
    if (len > 9 && strncmp(line, "card get ", 9) == 0) {
        const size_t n = len - 9;
        if (name == nullptr || n + 1 > nameSize || n + 1 > kCardNameSize) return CardCommand::Bad;
        memcpy(name, line + 9, n);
        name[n] = '\0';
        if (!cardNameServed(name)) {
            name[0] = '\0';
            return CardCommand::Bad;
        }
        return CardCommand::Get;
    }
    return CardCommand::Bad;
}

size_t base64Encode(const uint8_t *data, size_t length, char *out, size_t size)
{
    static const char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const size_t need = ((length + 2) / 3) * 4;
    if (out == nullptr || need + 1 > size || (data == nullptr && length > 0)) return 0;
    size_t o = 0;
    for (size_t i = 0; i < length; i += 3) {
        const uint32_t b0 = data[i];
        const uint32_t b1 = i + 1 < length ? data[i + 1] : 0;
        const uint32_t b2 = i + 2 < length ? data[i + 2] : 0;
        const uint32_t v = (b0 << 16) | (b1 << 8) | b2;
        out[o++] = kAlphabet[(v >> 18) & 0x3F];
        out[o++] = kAlphabet[(v >> 12) & 0x3F];
        out[o++] = i + 1 < length ? kAlphabet[(v >> 6) & 0x3F] : '=';
        out[o++] = i + 2 < length ? kAlphabet[v & 0x3F] : '=';
    }
    out[o] = '\0';
    return o;
}

uint32_t crc32Update(uint32_t crc, const uint8_t *data, size_t length)
{
    return link::card::crc32Update(crc, data, length);
}

} // namespace tdongle_c5
} // namespace layertime
