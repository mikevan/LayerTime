package com.projectrevivesolutions.layertime.controller.capture

/** Shared capture encoding; independent of analysis and connection lifecycle. */
object Radiotap {
        fun header(channel: Int, rssi: Int): ByteArray {
            val freq = when {
                channel == 14 -> 2484
                channel in 1..13 -> 2407 + channel * 5
                else -> 5000 + channel * 5
            }
            val chanFlags = if (channel <= 14) 0x0480 else 0x0140 // 2 GHz + CCK, or 5 GHz + OFDM
            return byteArrayOf(0, 0, 16, 0, 0x2A, 0, 0, 0,
                0,                                   // flags: no FCS
                0,                                   // pad to align the channel field
                (freq and 0xFF).toByte(), (freq shr 8).toByte(),
                (chanFlags and 0xFF).toByte(), (chanFlags shr 8).toByte(),
                rssi.toByte(),
                0)                                   // pad
        }
}
