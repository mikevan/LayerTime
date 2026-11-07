package com.projectrevivesolutions.layertime.controller.link

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

/**
 * HUNTER ACTIONS (Michael, 2026-11-06): every Hunter LayerScope can reach, one row each,
 * keyed by its Bluetooth address.
 *
 * SCAN listens for Hunters for [SCAN_MS], then connects to each one it heard for a moment,
 * reads its Status, and disconnects ([HunterScanner]); the Status flag says whether that
 * Hunter keeps an SD card. A Hunter stops advertising while something is connected to it,
 * so the Hunter this phone is connected to is added from the live connection instead.
 *
 * One Hunter is connected at a time for now. The roster, and connecting by address, are
 * the groundwork for several at once (Michael: several Hunters are how LayerTime will find
 * the direction of a signal such as a deauth attack).
 */
object HunterRoster {
    enum class Card { UNKNOWN, YES, NO }

    data class Hunter(val address: String, val name: String, val rssi: Int, val card: Card = Card.UNKNOWN)

    sealed interface Scan {
        data object Idle : Scan
        data class Listening(val found: Int) : Scan
        data class Checking(val index: Int, val total: Int, val name: String) : Scan
        data class Done(val found: Int) : Scan
        data class Failed(val message: String) : Scan
    }

    const val SCAN_MS = 10_000L
    const val CHECK_MS = 6_000L

    private val _hunters = MutableStateFlow<List<Hunter>>(emptyList())
    val hunters: StateFlow<List<Hunter>> = _hunters.asStateFlow()
    private val _scan = MutableStateFlow<Scan>(Scan.Idle)
    val scan: StateFlow<Scan> = _scan.asStateFlow()

    internal fun update(f: (List<Hunter>) -> List<Hunter>) { _hunters.value = f(_hunters.value) }
    internal fun setScan(s: Scan) { _scan.value = s }

    val scanning: Boolean get() = _scan.value is Scan.Listening || _scan.value is Scan.Checking

    /** A failed attempt still names its Hunter; only a live or in-progress link blocks retry. */
    fun canConnect(address: String, currentAddress: String?, online: Boolean, connecting: Boolean, scanning: Boolean): Boolean =
        !scanning && (address != currentAddress || (!online && !connecting))

    /** Adds a Hunter heard in a scan, or updates its name and signal; its card answer is kept. Sorted by name. */
    fun seen(list: List<Hunter>, address: String, name: String?, rssi: Int): List<Hunter> {
        val shown = name?.takeIf { it.isNotBlank() } ?: "Hunter"
        val old = list.firstOrNull { it.address == address }
        val next = if (old == null) list + Hunter(address, shown, rssi)
            else list.map { if (it.address == address) it.copy(name = if (name.isNullOrBlank()) it.name else shown, rssi = rssi) else it }
        return next.sortedWith(compareBy({ it.name }, { it.address }))
    }

    fun withCard(list: List<Hunter>, address: String, card: Card): List<Hunter> =
        list.map { if (it.address == address) it.copy(card = card) else it }

    /**
     * The list as shown: the scanned Hunters, plus the Hunter this phone holds if the scan
     * did not hear it (it does not advertise while connected), with that Hunter's card taken
     * from its live Status ([noSdLog], null until a Status arrives).
     */
    fun withHeld(list: List<Hunter>, address: String?, name: String?, noSdLog: Boolean?): List<Hunter> {
        if (address == null) return list
        val card = when (noSdLog) { true -> Card.NO; false -> Card.YES; null -> null }
        val withIt = if (list.any { it.address == address }) list else seen(list, address, name, 0)
        return if (card == null) withIt else withCard(withIt, address, card)
    }

    /** The SD card answer in a Status value read from the Hunter; UNKNOWN if it is not a Status. */
    fun cardFrom(status: ByteArray?): Card {
        val s = status?.let { LinkCodec.decodeStatus(it) } ?: return Card.UNKNOWN
        return if (s.noSdLog) Card.NO else Card.YES
    }

    /** One sentence for the panel, or null when there is nothing to say. */
    fun sentence(s: Scan): String? = when (s) {
        Scan.Idle -> null
        is Scan.Listening -> if (s.found == 1) "Scanning for Hunters. Found 1 so far." else "Scanning for Hunters. Found ${s.found} so far."
        is Scan.Checking -> "Checking ${s.name} for an SD card (${s.index} of ${s.total})."
        is Scan.Done -> when (s.found) {
            0 -> "No Hunter answered. Turn a Hunter on and tap Scan again."
            1 -> "Found 1 Hunter."
            else -> "Found ${s.found} Hunters."
        }
        is Scan.Failed -> s.message
    }
}
