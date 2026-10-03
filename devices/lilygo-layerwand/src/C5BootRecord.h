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

// Boot record for the T-Dongle-C5: why the chip last reset, and what the
// previous boot was doing when it last checked in. Added for open finding
// D3 (an uncommanded C5 restart, 2026-09-30).
//
// esp_reset_reason() is the authority on the reset cause. The RTC record
// is supplemental history: a struct in RTC fast memory (RTC_NOINIT_ATTR)
// survives every warm reset but not a power loss. A valid record means the
// previous boot's history survived; an invalid one means no usable history,
// and nothing more. The record is never used to guess a reset reason.
//
// Cost: one esp_reset_reason() read at boot, 20 bytes of RTC memory
// written at boot and on each periodic report. No task, timer, interrupt,
// allocation, radio call or scheduler change.

#include <stdint.h>
#include <stdio.h>

#include <esp_attr.h>
#include <esp_system.h>

namespace layertime {
namespace tdongle_c5 {

class C5BootRecord {
public:
    // What the previous boot reported before it ended, as read at this boot.
    struct Previous {
        bool valid = false;      // the RTC record carried a good magic and checksum
        uint32_t bootCount = 0;  // boots since the record was last initialised
        uint32_t uptimeS = 0;    // the previous boot's last reported uptime
        uint32_t state = 0;      // the previous boot's last reported state word
    };

    // First thing in setup(): captures the reset reason, reads and validates
    // the retained record, then starts this boot's entry in it.
    void begin()
    {
        _reason = esp_reset_reason();
        Record &r = record();
        if (r.magic == kMagic && r.checksum == checksumOf(r)) {
            _previous.valid = true;
            _previous.bootCount = r.bootCount;
            _previous.uptimeS = r.uptimeS;
            _previous.state = r.state;
            r.bootCount += 1;
        } else {
            _previous = Previous{};
            r.magic = kMagic;
            r.bootCount = 1;
        }
        r.uptimeS = 0;
        r.state = 0;
        r.checksum = checksumOf(r);
        _bootCount = r.bootCount;
    }

    // From the periodic report: what this boot is doing now, kept for the
    // next boot to read if this one ends unexpectedly.
    void touch(uint32_t uptimeS, uint32_t state)
    {
        Record &r = record();
        r.uptimeS = uptimeS;
        r.state = state;
        r.checksum = checksumOf(r);
    }

    esp_reset_reason_t reason() const { return _reason; }
    uint32_t bootCount() const { return _bootCount; }
    const Previous &previous() const { return _previous; }

    static const char *reasonName(esp_reset_reason_t reason)
    {
        switch (reason) {
        case ESP_RST_POWERON: return "power-on";
        case ESP_RST_EXT: return "external pin";
        case ESP_RST_SW: return "software (esp_restart)";
        case ESP_RST_PANIC: return "panic";
        case ESP_RST_INT_WDT: return "interrupt watchdog";
        case ESP_RST_TASK_WDT: return "task watchdog";
        case ESP_RST_WDT: return "other watchdog";
        case ESP_RST_DEEPSLEEP: return "deep-sleep wake";
        case ESP_RST_BROWNOUT: return "brownout";
        case ESP_RST_SDIO: return "SDIO";
        case ESP_RST_USB: return "USB peripheral";
        case ESP_RST_JTAG: return "JTAG";
        case ESP_RST_EFUSE: return "efuse error";
        case ESP_RST_PWR_GLITCH: return "power glitch";
        case ESP_RST_CPU_LOCKUP: return "CPU lockup";
        default: return "unknown";
        }
    }

    // One letter for the LCD: P power-on, E external, S software, X panic,
    // W any watchdog, D deep sleep, B brownout, U USB, J JTAG, F efuse,
    // G power glitch, L CPU lockup, ? unknown.
    static char reasonLetter(esp_reset_reason_t reason)
    {
        switch (reason) {
        case ESP_RST_POWERON: return 'P';
        case ESP_RST_EXT: return 'E';
        case ESP_RST_SW: return 'S';
        case ESP_RST_PANIC: return 'X';
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT: return 'W';
        case ESP_RST_DEEPSLEEP: return 'D';
        case ESP_RST_BROWNOUT: return 'B';
        case ESP_RST_SDIO: return 'I';
        case ESP_RST_USB: return 'U';
        case ESP_RST_JTAG: return 'J';
        case ESP_RST_EFUSE: return 'F';
        case ESP_RST_PWR_GLITCH: return 'G';
        case ESP_RST_CPU_LOCKUP: return 'L';
        default: return '?';
        }
    }

    // "reset <name> (<code>), boot N, previous: up S s, state 0x..." or
    // "..., previous: no retained history". Human-readable for serial.
    void format(char *out, size_t size) const
    {
        if (_previous.valid) {
            snprintf(out, size, "reset %s (%d), boot %lu, previous boot: up %lu s, state 0x%08lx",
                     reasonName(_reason), static_cast<int>(_reason), static_cast<unsigned long>(_bootCount),
                     static_cast<unsigned long>(_previous.uptimeS), static_cast<unsigned long>(_previous.state));
        } else {
            snprintf(out, size, "reset %s (%d), boot %lu, previous boot: no retained history",
                     reasonName(_reason), static_cast<int>(_reason), static_cast<unsigned long>(_bootCount));
        }
    }

private:
    struct Record {
        uint32_t magic;
        uint32_t bootCount;
        uint32_t uptimeS;
        uint32_t state;
        uint32_t checksum;
    };
    static constexpr uint32_t kMagic = 0x4C54424Fu; // "LTBO"

    static Record &record()
    {
        static RTC_NOINIT_ATTR Record sRecord;
        return sRecord;
    }
    static uint32_t checksumOf(const Record &r)
    {
        return r.magic ^ (r.bootCount * 0x9E3779B9u) ^ (r.uptimeS * 0x85EBCA6Bu) ^ (r.state * 0xC2B2AE35u) ^ 0xA5A5A5A5u;
    }

    esp_reset_reason_t _reason = ESP_RST_UNKNOWN;
    uint32_t _bootCount = 0;
    Previous _previous;
};

} // namespace tdongle_c5
} // namespace layertime
