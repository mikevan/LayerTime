package com.projectrevivesolutions.layertime.controller.mission

import android.content.Context
import com.projectrevivesolutions.layertime.controller.link.LinkConstants

/**
 * The mission setup, saved on the watch so a restart or Android recreating the screen
 * does not reset it: the sensor checkboxes (sent to the Hunter as SetSensors since
 * build 87). The Recon target is no longer stored: it follows the sensor list (ONLY
 * DEAUTH when the list is Deauth alone, ALL otherwise). "target" stays in the prefs
 * file, unread.
 */
internal object MissionPrefs {
    private const val PREFS = "layertime_mission"
    fun readTarget(ctx: Context): Int = ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        .getInt("target", LinkConstants.ReconTarget.ALL)
    fun writeTarget(ctx: Context, t: Int) { ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit().putInt("target", t).apply() }
    // "sensors_v2" marks the one-time move to the Hunter-wired list (2026-11-04):
    // SensorSelection.migrate turns the old, never-sent list into every sensor on
    // unless the operator had checked real sensors. After that the stored list is
    // used as is (presets expanded), and an empty one means every sensor off.
    private const val V2 = "sensors_v2"
    // "trackers_off_once" (Michael, 2026-11-06): one time, on the first run of build 100 or
    // later, the trackers are unchecked in a list saved before trackers-off became the default.
    // After that the operator's choices are left alone; checking a tracker again sticks.
    private const val TRACKERS_OFF_ONCE = "trackers_off_once"
    fun readSensors(ctx: Context): Set<Int> {
        val p = ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        val stored = p.getStringSet("sensors", null)?.mapNotNull { it.toIntOrNull() }?.toSet()
        var selection = if (!p.getBoolean(V2, false)) SensorSelection.migrate(stored)
            else SensorSelection.normalize(stored ?: emptySet())
        if (!p.getBoolean(TRACKERS_OFF_ONCE, false)) {
            selection = SensorSelection.withoutTrackers(selection)
            writeSensors(ctx, selection)
            p.edit().putBoolean(TRACKERS_OFF_ONCE, true).apply()
        } else if (!p.getBoolean(V2, false)) writeSensors(ctx, selection)
        return selection
    }
    fun writeSensors(ctx: Context, on: Set<Int>) {
        ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit()
            .putStringSet("sensors", on.map { it.toString() }.toSet()).putBoolean(V2, true).apply()
    }
    // The evidence hold in seconds (SetEvidenceHold, 2026-11-04). 10 until the operator picks one.
    fun readEvidenceHold(ctx: Context): Int = EvidenceHold.normalize(
        ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE).getInt("evidence_hold", EvidenceHold.DEFAULT_SEC))
    fun writeEvidenceHold(ctx: Context, sec: Int) {
        ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit().putInt("evidence_hold", EvidenceHold.normalize(sec)).apply()
    }
}
