// LayerTime - passive early-warning system. Connect IQ Device App for the
// Garmin tactix 8 AMOLED.
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

import Toybox.BluetoothLowEnergy;
import Toybox.Lang;

// LayerTime Link 0.1 constants and codec: the Monkey C binding of
// contracts/link.md. Checked byte for byte against LinkVectors (generated
// from contracts/vectors/link_frames.json) by test/LinkCodecTests.mc.
//
// Numbers wider than 31 bits (tokens, changeSeq, lastAlertEventId) are
// carried as Long because Monkey C's Number is a signed 32-bit value.
module Link {

    const MAJOR = 0;
    const MINOR = 1;
    const VERSION_BYTE = 0x01;
    const MAX_FRAME = 20;
    const STATUS_SIZE = 18;

    // UUIDs, fixed (contracts/link.md).
    const UUID_SERVICE = "8ac60001-a08b-4851-b89f-e6b39081268e";
    const UUID_CONTROL = "8ac60002-a08b-4851-b89f-e6b39081268e";
    const UUID_STATUS = "8ac60003-a08b-4851-b89f-e6b39081268e";
    const UUID_DATA = "8ac60004-a08b-4851-b89f-e6b39081268e";
    const UUID_PROBE = "8ac600f0-a08b-4851-b89f-e6b39081268e";

    const OP_HELLO = 0x01;
    const OP_PING = 0x02;
    const OP_COMMAND = 0x03;
    const OP_GET_CHANGED = 0x04;
    const OP_GET_TEXT = 0x05;
    const OP_TEST_FIRST = 0xF0;
    const OP_RX_MARK = 0xF1;
    const OP_TEST_LAST = 0xFE;

    const FRAME_HELLO_ACK = 0x81;
    const FRAME_ACK = 0x82;
    const FRAME_RESULT = 0x83;
    const FRAME_EVENT_SUMMARY = 0x84;
    const FRAME_END = 0x85;
    const FRAME_TEXT = 0x86;
    const FRAME_ERROR = 0x8F;

    const STATUS_OK = 0;
    const STATUS_UNKNOWN_OP = 1;
    const STATUS_BAD_LENGTH = 2;
    const STATUS_VERSION_MISMATCH = 3;
    const STATUS_BUSY = 4;

    const FLAG_MONITORING = 0x01;
    const FLAG_EARLY_WARNING_ENABLED = 0x02;
    const FLAG_EARLY_WARNING_RESTING = 0x04;
    const FLAG_ALERT_PENDING = 0x08;
    const FLAG_SLEEP_MODE = 0x10;

    const SCHEDULE_SIMULTANEOUS = 0;
    const SCHEDULE_RECON = 1;
    const SCHEDULE_REPORT = 2;

    const CAP_LOCAL_WIFI_MONITOR = 0x0001;
    const CAP_LOCAL_BLE_MONITOR = 0x0002;
    const CAP_DISPLAY = 0x0004;
    const CAP_LED = 0x0008;
    const CAP_BUTTON = 0x0010;

    const HELLO_SIZE = 4;
    const PING_SIZE = 6;
    const HELLO_ACK_SIZE = 10;
    const ACK_SIZE = 8;
    const ERROR_SIZE = 3;

    function serviceUuid() as Uuid { return BluetoothLowEnergy.stringToUuid(UUID_SERVICE); }
    function controlUuid() as Uuid { return BluetoothLowEnergy.stringToUuid(UUID_CONTROL); }
    function statusUuid() as Uuid { return BluetoothLowEnergy.stringToUuid(UUID_STATUS); }
    function dataUuid() as Uuid { return BluetoothLowEnergy.stringToUuid(UUID_DATA); }
    function probeUuid() as Uuid { return BluetoothLowEnergy.stringToUuid(UUID_PROBE); }

    // --- Encoders (interface to Node) --------------------------------------

    function encodeHello(reqId as Number, clientMajor as Number, clientMinor as Number) as ByteArray {
        return [OP_HELLO, reqId & 0xFF, clientMajor & 0xFF, clientMinor & 0xFF]b;
    }

    function encodePing(reqId as Number, token as Long) as ByteArray {
        var b = new [PING_SIZE]b;
        b[0] = OP_PING;
        b[1] = reqId & 0xFF;
        b.encodeNumber(token, Lang.NUMBER_FORMAT_UINT32, {:offset => 2, :endianness => Lang.ENDIAN_LITTLE});
        return b;
    }

    // --- Decoders (Node to interface) --------------------------------------

    // Returns null when the length is wrong.
    function decodeStatus(value as ByteArray) as Dictionary? {
        if (value.size() != STATUS_SIZE) { return null; }
        return {
            :linkVersion => value[0],
            :sessionId => u16(value, 1),
            :changeSeq => u32(value, 3),
            :flags => value[7],
            :selected => value[8],
            :active => value[9],
            :eventCount => value[10],
            :lastAlertEventId => u32(value, 11),
            :heartbeat => value[15],
            :schedule => value[16],
            :nextReportS => value[17]
        };
    }

    // Returns null when the frame is not one this binding knows or its
    // length is wrong. Every dictionary carries :type, :reqId, :linkStatus.
    function decodeReply(value as ByteArray) as Dictionary? {
        if (value.size() < 3) { return null; }
        var type = value[0];
        if (type == FRAME_HELLO_ACK) {
            if (value.size() != HELLO_ACK_SIZE) { return null; }
            return {
                :type => type, :reqId => value[1], :linkStatus => value[2],
                :serverMajor => value[3], :serverMinor => value[4],
                :sessionId => u16(value, 5), :capabilities => u16(value, 7), :maxFrame => value[9]
            };
        }
        if (type == FRAME_ACK) {
            if (value.size() != ACK_SIZE) { return null; }
            return {
                :type => type, :reqId => value[1], :linkStatus => value[2],
                :token => u32(value, 3), :heartbeat => value[7]
            };
        }
        if (type == FRAME_ERROR) {
            if (value.size() != ERROR_SIZE) { return null; }
            return {:type => type, :reqId => value[1], :linkStatus => value[2]};
        }
        return null;
    }

    function u16(b as ByteArray, offset as Number) as Number {
        return b.decodeNumber(Lang.NUMBER_FORMAT_UINT16, {:offset => offset, :endianness => Lang.ENDIAN_LITTLE}) as Number;
    }

    function u32(b as ByteArray, offset as Number) as Long {
        return b.decodeNumber(Lang.NUMBER_FORMAT_UINT32, {:offset => offset, :endianness => Lang.ENDIAN_LITTLE}).toLong();
    }

    function toHex(b as ByteArray) as String {
        var s = "";
        for (var i = 0; i < b.size(); i++) {
            s += (b[i] as Number).format("%02X");
        }
        return s;
    }

    function statusName(status as Number) as String {
        if (status == STATUS_OK) { return "OK"; }
        if (status == STATUS_UNKNOWN_OP) { return "Unknown op"; }
        if (status == STATUS_BAD_LENGTH) { return "Bad length"; }
        if (status == STATUS_VERSION_MISMATCH) { return "Version mismatch"; }
        if (status == STATUS_BUSY) { return "Busy"; }
        return "Status " + status;
    }
}
