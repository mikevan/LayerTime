package com.projectrevivesolutions.layertime.controller.flow

import java.io.ByteArrayOutputStream

/**
 * Builds 802.11 frames with every knob under control (2026-11-04), so the triage is
 * tested against known truth rather than whatever a recording happened to hold. A
 * scenario is a list of AnalyticFrames (what the checks read) and the same frames
 * as real bytes in a pcap (radiotap link type), so the fixture can also be decoded
 * through the Wireshark engine on the phone and compared with the model.
 *
 * Passive line: these bytes exist in memory and in test files only. Nothing here
 * can transmit, and nothing in the app will ever be given a way to.
 */
class FrameFabricator {
    private val frames = ArrayList<AnalyticFrame>()
    private val packets = ArrayList<ByteArray>()
    private val times = ArrayList<Long>()

    val built: List<AnalyticFrame> get() = frames.toList()
    val count: Int get() = frames.size

    /** An access point's beacon. `mfp` null = open network (no RSN), else (capable, required). */
    fun beacon(atNanos: Long, bssid: String, ssid: String, channel: Int, rssi: Int, seq: Int,
               mfp: Pair<Boolean, Boolean>? = null): FrameFabricator {
        val body = ByteArrayOutputStream()
        repeat(8) { body.write(0) }                 // timestamp
        body.write(0x64); body.write(0x00)           // beacon interval 100 TU
        body.write(0x11); body.write(0x04)           // capabilities: ESS, privacy
        body.write(0); body.write(ssid.length); body.write(ssid.toByteArray(Charsets.UTF_8))
        if (mfp != null) {
            val caps = (if (mfp.first) 0x80 else 0) or (if (mfp.second) 0x40 else 0)
            val rsn = byteArrayOf(0x01, 0x00,                                  // version 1
                0x00, 0x0F, 0xAC.toByte(), 0x04,                             // group cipher CCMP
                0x01, 0x00, 0x00, 0x0F, 0xAC.toByte(), 0x04,                  // one pairwise cipher CCMP
                0x01, 0x00, 0x00, 0x0F, 0xAC.toByte(), 0x02,                  // one AKM PSK
                caps.toByte(), 0x00)                                         // RSN capabilities
            body.write(48); body.write(rsn.size); body.write(rsn)
        }
        return add(atNanos, AnalyticFrame.BEACON, da = AnalyticFrame.BROADCAST, sa = bssid, bssid = bssid, seq = seq,
            body = body.toByteArray(), channel = channel, rssi = rssi,
            extra = { it.copy(ssid = ssid, mfpCapable = mfp?.first, mfpRequired = mfp?.second) })
    }

    /** A deauthentication (or disassociation) frame. */
    fun deauth(atNanos: Long, from: String, to: String, bssid: String, reason: Int, seq: Int, channel: Int, rssi: Int,
               retry: Boolean = false, disassoc: Boolean = false): FrameFabricator =
        add(atNanos, if (disassoc) AnalyticFrame.DISASSOC else AnalyticFrame.DEAUTH, da = to, sa = from, bssid = bssid, seq = seq,
            body = byteArrayOf((reason and 0xFF).toByte(), (reason shr 8).toByte()), channel = channel, rssi = rssi, retry = retry,
            extra = { it.copy(reasonCode = reason) })

    /** A data frame from a client (what a disconnected client keeps sending if the deauth was forged). */
    fun data(atNanos: Long, from: String, to: String, bssid: String, seq: Int, channel: Int, rssi: Int): FrameFabricator =
        add(atNanos, AnalyticFrame.DATA, da = to, sa = from, bssid = bssid, seq = seq, body = ByteArray(16), channel = channel, rssi = rssi)

    /** An EAPOL-Key frame (a password handshake message). */
    fun eapol(atNanos: Long, from: String, to: String, bssid: String, seq: Int, channel: Int, rssi: Int): FrameFabricator {
        val llc = byteArrayOf(0xAA.toByte(), 0xAA.toByte(), 0x03, 0, 0, 0, 0x88.toByte(), 0x8E.toByte())
        val eapol = byteArrayOf(0x02, 0x03, 0x00, 0x5F) + ByteArray(95) // version 2, EAPOL-Key, length 95
        return add(atNanos, AnalyticFrame.DATA, da = to, sa = from, bssid = bssid, seq = seq, body = llc + eapol, channel = channel, rssi = rssi,
            extra = { it.copy(eapol = true) })
    }

    private fun add(atNanos: Long, typeSubtype: Int, da: String, sa: String, bssid: String, seq: Int, body: ByteArray,
                    channel: Int, rssi: Int, retry: Boolean = false, extra: (AnalyticFrame) -> AnalyticFrame = { it }): FrameFabricator {
        val f = AnalyticFrame(index = frames.size, timeNanos = atNanos, typeSubtype = typeSubtype, sa = sa.lowercase(), da = da.lowercase(),
            bssid = bssid.lowercase(), seq = seq, retry = retry, rssi = rssi, channel = channel)
        frames += extra(f)
        packets += radiotap(channel, rssi) + dot11(typeSubtype, da, sa, bssid, seq, retry, body)
        times += atNanos
        return this
    }

    /** The frames as a pcap file (link type 127, radiotap), microsecond timestamps. */
    fun pcap(): ByteArray {
        val out = ByteArrayOutputStream()
        out.write(le32(0xA1B2C3D4L)); out.write(le16(2)); out.write(le16(4))
        out.write(le32(0)); out.write(le32(0)); out.write(le32(65535)); out.write(le32(127))
        for (i in packets.indices) {
            val p = packets[i]
            out.write(le32(times[i] / 1_000_000_000L)); out.write(le32((times[i] % 1_000_000_000L) / 1000))
            out.write(le32(p.size.toLong())); out.write(le32(p.size.toLong())); out.write(p)
        }
        return out.toByteArray()
    }

    companion object {
        /** Radiotap: flags, channel, antenna signal (present 0x2A); 16 bytes. */
        fun radiotap(channel: Int, rssi: Int): ByteArray {
            val freq = AnalyticFrame.freqOf(channel)
            val chanFlags = if (channel <= 14) 0x0480 else 0x0140 // 2 GHz + CCK, or 5 GHz + OFDM
            return byteArrayOf(0, 0, 16, 0, 0x2A, 0, 0, 0,
                0,                                   // flags: no FCS
                0,                                   // pad to align the channel field
                (freq and 0xFF).toByte(), (freq shr 8).toByte(),
                (chanFlags and 0xFF).toByte(), (chanFlags shr 8).toByte(),
                rssi.toByte(),
                0)                                   // pad
        }

        /** An 802.11 management or data frame: control, duration, three addresses, sequence control, body. */
        fun dot11(typeSubtype: Int, da: String, sa: String, bssid: String, seq: Int, retry: Boolean, body: ByteArray): ByteArray {
            val type = (typeSubtype shr 4) and 0x3
            val subtype = typeSubtype and 0xF
            val fc0 = (subtype shl 4) or (type shl 2)
            val fc1 = if (retry) 0x08 else 0
            val seqCtl = (seq and 0xFFF) shl 4
            return byteArrayOf(fc0.toByte(), fc1.toByte(), 0, 0) + mac(da) + mac(sa) + mac(bssid) +
                byteArrayOf((seqCtl and 0xFF).toByte(), (seqCtl shr 8).toByte()) + body
        }

        fun mac(s: String): ByteArray = s.split(':').map { it.toInt(16).toByte() }.toByteArray()
        private fun le16(v: Int) = byteArrayOf((v and 0xFF).toByte(), (v shr 8).toByte())
        private fun le32(v: Long) = byteArrayOf((v and 0xFF).toByte(), ((v shr 8) and 0xFF).toByte(), ((v shr 16) and 0xFF).toByte(), ((v shr 24) and 0xFF).toByte())
    }
}
