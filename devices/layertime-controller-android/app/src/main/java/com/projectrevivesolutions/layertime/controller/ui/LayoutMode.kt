package com.projectrevivesolutions.layertime.controller.ui

/**
 * The adaptive-layout decision for LayerScope, in one place and testable without
 * Compose (claude/layerscope_analytics_design.md, "Phone layout and the tool dock").
 * The watch keeps its sliding console; every other screen is sized by the Material
 * window width-size-class breakpoints (600 and 840 dp), which is the same expanded
 * breakpoint the dock and the authoring editor key off, so it is one rule, not three.
 *
 *  - WATCH    the LOKMAT and any small near-square screen: the sliding hull console. Unchanged.
 *  - COMPACT  width < 600 dp (a phone upright, the Flip cover): a bottom navigation bar,
 *             one pane at a time, the face rolled up into a header strip off the OPS tab.
 *  - MEDIUM   600..839 dp: a left navigation rail, EVENTS as list-detail.
 *  - EXPANDED width >= 840 dp (a tablet, a phone unfolded or in landscape, a desktop):
 *             the rail, both panes always visible, and the block workflow editor enabled.
 */
enum class LayoutMode { WATCH, COMPACT, MEDIUM, EXPANDED }

object LayoutPlan {
    const val MEDIUM_MIN_DP = 600f
    const val EXPANDED_MIN_DP = 840f

    /** The layout for a screen of this size, orientation included (width is the live width). */
    fun mode(widthDp: Float, heightDp: Float): LayoutMode {
        val long = maxOf(widthDp, heightDp)
        val short = minOf(widthDp, heightDp)
        // Watch gate is aspect AND size: WatchFormFactor is aspect-only, so without the
        // size bound a large near-square screen (an 840-wide portrait tablet, a foldable
        // unfolded) would wrongly take the watch's sliding console. The real watch and the
        // Flip cover are small (short side below the medium breakpoint); a tablet is not.
        if (WatchFormFactor.matches(long, short) && short < MEDIUM_MIN_DP) return LayoutMode.WATCH
        return when {
            widthDp >= EXPANDED_MIN_DP -> LayoutMode.EXPANDED
            widthDp >= MEDIUM_MIN_DP -> LayoutMode.MEDIUM
            else -> LayoutMode.COMPACT
        }
    }

    /** A left rail instead of a bottom bar. */
    fun usesRail(mode: LayoutMode): Boolean = mode == LayoutMode.MEDIUM || mode == LayoutMode.EXPANDED

    /** A bottom navigation bar. */
    fun usesBottomBar(mode: LayoutMode): Boolean = mode == LayoutMode.COMPACT

    /** Both the list and its detail (and the dock) are shown at once, not one at a time. */
    fun showsBothPanes(mode: LayoutMode): Boolean = mode == LayoutMode.EXPANDED

    /** Builder is available on all phone/tablet layouts; the hardware watch stays separate. */
    fun authoringEnabled(mode: LayoutMode): Boolean = mode != LayoutMode.WATCH

    /** The watch keeps its own layout untouched; this is the gate the watch test pins. */
    fun isWatch(mode: LayoutMode): Boolean = mode == LayoutMode.WATCH
}

/**
 * Shared phone/tablet destinations. Builder was added as a permanent destination
 * at the user's request; individual analytic tools remain inside Tools.
 */
enum class Destination(val label: String) {
    OPS("OPS"),         // the field screen: the face and the connection and detection controls
    EVENTS("EVENTS"),   // the live detection list, with DECODE A CAPTURE FILE, and detail
    TOOLS("TOOLS"),     // the analytic windows (Wireshark and the rest), run and view here
    BUILDER("BUILDER"), // zoomable workspace for assembling analysis tools
    MISSION("MISSION"), // the sensor checklist
    SYSTEM("SYSTEM"),   // position, settings, diagnostics, the log
}
