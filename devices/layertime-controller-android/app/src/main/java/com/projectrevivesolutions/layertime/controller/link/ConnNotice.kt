package com.projectrevivesolutions.layertime.controller.link

import com.projectrevivesolutions.layertime.controller.model.ConnState

/**
 * The sentence the CONNECTION panel shows under its title: what the link is doing,
 * or why it stopped and what to tap next. Pure, so it is unit-tested on the JVM.
 *
 * Added 2026-10-30: with Bluetooth off, SCAN & CONNECT stopped at once and the panel
 * stayed blank, so there was no way to tell why it never connected. [detail] is the
 * reason LinkClient passed to ControllerRepository.setConn.
 */
object ConnNotice {
    enum class Tone { INFO, PROBLEM }
    data class Notice(val text: String, val tone: Tone)

    fun of(state: ConnState, detail: String?, reconnecting: Boolean): Notice? = when (state) {
        ConnState.IDLE, ConnState.CONNECTED -> null
        ConnState.SCANNING -> Notice(
            if (reconnecting) "The link dropped. Searching for the Hunter again." else "Searching for a Hunter.",
            Tone.INFO)
        ConnState.CONNECTING -> Notice("Found a Hunter. Connecting.", Tone.INFO)
        ConnState.DISCOVERING, ConnState.HANDSHAKING -> Notice("Connected. Setting up the link.", Tone.INFO)
        ConnState.DISCONNECTED -> Notice(
            "The link to the Hunter dropped" + reason(detail) + "." +
                (if (reconnecting) " Reconnecting." else " Tap SCAN & CONNECT to connect again."),
            Tone.PROBLEM)
        ConnState.FAILED -> Notice(failed(detail), Tone.PROBLEM)
    }

    private fun failed(detail: String?): String = when {
        detail == BLUETOOTH_OFF -> "Bluetooth is off. Turn it on, then tap SCAN & CONNECT."
        detail == LinkClient.LOCATION_OFF_MESSAGE -> detail
        detail != null && detail.startsWith("no Hunter found") ->
            "No Hunter found in 20 seconds. Check that it is on and close by, then tap SCAN & CONNECT."
        detail != null && detail.startsWith("scan failed code=") ->
            "Android could not start the Bluetooth scan (code ${detail.removePrefix("scan failed code=")}). " +
                "Tap SCAN & CONNECT to try again."
        else -> "The connection failed" + reason(detail) + ". Tap SCAN & CONNECT to try again."
    }

    private fun reason(detail: String?) = if (detail.isNullOrBlank()) "" else " ($detail)"

    /** The detail LinkClient passes when the adapter is off; shared so the two cannot drift. */
    const val BLUETOOTH_OFF = "Bluetooth is off"
}
