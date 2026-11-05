package com.projectrevivesolutions.layertime.controller.mission

/**
 * The MISSION page's sensor selection (2026-11-04), pure and host-tested.
 *
 * The three groups and their members mirror the Hunter's own group table,
 * src/core/logic/ReconSelection.cpp (kTrackerMembers, kCounterSurveilMembers,
 * kCounterIntrusionMembers), in the same order. That table is the single source
 * for the Hunter's radio scheduling and menus; SensorSelectionTest pins this copy
 * to it number by number, so a change on one side fails the build of the other.
 *
 * A preset is not stored. Its checkbox is computed from its members: all on is
 * checked, some on is partial, none on is clear. Tapping a preset turns every
 * member on, unless every member is already on, in which case it turns them off.
 * So a preset and its sensors can never disagree.
 *
 * The selection goes to the Hunter as a SetSensors mask (contracts/link.md): one
 * bit per sensor at its ReconTarget value.
 */
object SensorSelection {
    const val TRACKERS = 2
    const val COUNTER_SURVEIL = 3
    const val COUNTER_INTRUSION = 4

    data class Group(val preset: Int, val label: String, val members: List<Int>)

    val GROUPS: List<Group> = listOf(
        // AirTag, Tile, SmartTag, Google Tag
        Group(TRACKERS, "TRACKERS", listOf(10, 14, 15, 16)),
        // Flock, Axon, Meta
        Group(COUNTER_SURVEIL, "COUNTER-SURVEIL", listOf(8, 13, 12)),
        // Deauth, Pwnagotchi, MultiSSID, Pineapple, Flipper, EAPOL
        Group(COUNTER_INTRUSION, "COUNTER-INTRUSION", listOf(5, 6, 7, 9, 11, 18)),
    )

    /** Every sensor, grouped, in display order. */
    val ALL_SENSORS: List<Int> = GROUPS.flatMap { it.members }

    /** kAllSensorsMask in src/core/model/MonitorEvent.h: bits 5 to 16, and 18. */
    const val ALL_MASK: Long = 0x5FFE0L

    enum class PresetState { NONE, PARTIAL, ALL }

    fun group(preset: Int): Group? = GROUPS.firstOrNull { it.preset == preset }

    fun presetState(preset: Int, selected: Set<Int>): PresetState {
        val members = group(preset)?.members ?: return PresetState.NONE
        val on = members.count { it in selected }
        return when (on) {
            0 -> PresetState.NONE
            members.size -> PresetState.ALL
            else -> PresetState.PARTIAL
        }
    }

    /** A preset tap: everything in the group on, or off when it is already all on. */
    fun togglePreset(preset: Int, selected: Set<Int>): Set<Int> {
        val members = group(preset)?.members ?: return selected
        return if (presetState(preset, selected) == PresetState.ALL) selected - members.toSet()
        else selected + members
    }

    /** Only real sensors, with any stored preset id expanded into its members. */
    fun normalize(stored: Set<Int>): Set<Int> {
        val out = mutableSetOf<Int>()
        for (id in stored) {
            val g = group(id)
            if (g != null) out += g.members else if (id in ALL_SENSORS) out += id
        }
        return out
    }

    /**
     * The one-time move from the old local-only checklist. Before 2026-11-04 the
     * selection was never sent anywhere, so an empty or missing one meant nothing,
     * and the Hunter was running every sensor. Starting it as "every sensor on"
     * keeps the Hunter doing exactly what it did; switching the list off silently
     * on first connect would blind it. Anything else the operator checked is kept,
     * with presets expanded.
     */
    fun migrate(oldStored: Set<Int>?): Set<Int> {
        val kept = normalize(oldStored ?: emptySet())
        return if (kept.isEmpty()) ALL_SENSORS.toSet() else kept
    }

    /** The SetSensors mask for a selection. */
    fun mask(selected: Set<Int>): Long =
        normalize(selected).fold(0L) { m, id -> m or (1L shl id) }
}
