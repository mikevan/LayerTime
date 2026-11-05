package com.projectrevivesolutions.layertime.controller.wireshark

/**
 * The display filter for the on-device Wireshark window. The native epan display-filter
 * compiler is not exposed through the JNI layer yet, so this is a text filter over a frame's
 * decoded fields, which is honest and fast: it matches the query against the frame's one-line
 * title, its addresses, and every field name and value in the protocol tree. A door left open
 * for later: wiring epan's real dfilter syntax ("wlan.fc.type_subtype == 0x0c") through JNI.
 *
 * Rules:
 *  - Blank query matches everything.
 *  - Case-insensitive substring match.
 *  - Space-separated terms are ANDed: every term must appear somewhere in the frame.
 */
object WiresharkFilter {
    fun matches(title: String, addresses: String, fieldText: List<String>, query: String): Boolean {
        val terms = query.trim().lowercase().split(' ').filter { it.isNotEmpty() }
        if (terms.isEmpty()) return true
        val hay = buildString {
            append(title.lowercase()); append('\n')
            append(addresses.lowercase()); append('\n')
            for (f in fieldText) { append(f.lowercase()); append('\n') }
        }
        return terms.all { hay.contains(it) }
    }
}
