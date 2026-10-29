package com.projectrevivesolutions.layertime.controller.ui

/**
 * The watch form factor of the LayerTime Android design standard
 * (project doc claude/android_watch_design_standard.md): a landscape screen at
 * most [MAX_ASPECT] times as wide as it is tall. Such screens get the sliding
 * layout, the Garmin-coordinate face over the fixed HUD console. The reference
 * device is the LOKMAT APPLLP 5 MAX (640 x 480 px). Every new Android watch of
 * this shape, and every new Android build, uses this layout.
 */
object WatchFormFactor {
    const val MAX_ASPECT = 1.6f

    /** True for a landscape screen no more than [MAX_ASPECT] times as wide as tall. */
    fun matches(widthDp: Float, heightDp: Float): Boolean =
        widthDp >= heightDp && widthDp < heightDp * MAX_ASPECT
}
