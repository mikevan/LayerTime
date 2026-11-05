package com.projectrevivesolutions.layertime.controller.mission

/**
 * MISSION's evidence hold (2026-11-04), pure and host-tested.
 *
 * How long, in whole seconds, the Hunter stays on the channel after a deauth
 * detection, and how long that detection's evidence window keeps collecting
 * (never less than 10 s; collecting costs no radio time). A repeat sighting of
 * the same flood restarts the hold, so the Hunter stays on a sustained flood
 * and resumes its sweep when it stops. Off never holds the channel.
 *
 * The limits mirror core: kDefaultEvidenceHoldSec and kMaxEvidenceHoldSec in
 * src/core/model/MonitorEvent.h. The Hunter takes any whole second from 0 to 30
 * (COMMAND SetEvidenceHold, capability bit 8); the page offers these presets.
 */
object EvidenceHold {
    const val DEFAULT_SEC = 10
    const val MAX_SEC = 30
    val PRESETS = listOf(0, 5, 10, 20, 30)

    /** A stored or received value the Hunter would take, else the default. */
    fun normalize(sec: Int?): Int = if (sec != null && sec in 0..MAX_SEC) sec else DEFAULT_SEC

    /** The button label: "OFF" or "20 S". */
    fun label(sec: Int): String = if (sec == 0) "OFF" else "$sec S"

    /** What the Hunter does with this hold, as a full sentence. */
    fun meaning(sec: Int): String = if (sec == 0)
        "The Hunter never holds the channel. It keeps sweeping, so each Wi-Fi detection's window catches less."
    else
        "After a deauth detection the Hunter stays on that channel for $sec seconds, and stays as long as the flood lasts."

    enum class Link { OFFLINE, OLD_FIRMWARE, SENDING, APPLIED }

    /** The status line under the buttons, as a full sentence. */
    fun status(link: Link): String = when (link) {
        Link.OFFLINE -> "Saved on this device. The Hunter gets this setting when it connects."
        Link.OLD_FIRMWARE -> "This Hunter does not take an evidence hold, so it holds for a fixed 10 seconds. Update the Hunter's firmware to use this setting."
        Link.SENDING -> "The Hunter has not confirmed this setting yet."
        Link.APPLIED -> "Running on the Hunter."
    }
}
