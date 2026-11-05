package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.ui.Destination
import com.projectrevivesolutions.layertime.controller.ui.LayoutMode
import com.projectrevivesolutions.layertime.controller.ui.LayoutPlan
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * The adaptive-layout rule, on the devices the design doc names: the LOKMAT watch, the
 * Flip upright and sideways, the Flip cover, and a tablet. The watch case is the one that
 * must never change.
 */
class LayoutModeTest {
    @Test fun lokmatWatchGetsTheWatchLayoutInEitherOrientation() {
        assertEquals(LayoutMode.WATCH, LayoutPlan.mode(640f, 480f))
        assertEquals(LayoutMode.WATCH, LayoutPlan.mode(480f, 640f))
        assertTrue(LayoutPlan.isWatch(LayoutPlan.mode(640f, 480f)))
    }

    @Test fun flipUprightIsCompactWithABottomBar() {
        val m = LayoutPlan.mode(415f, 1015f)
        assertEquals(LayoutMode.COMPACT, m)
        assertTrue(LayoutPlan.usesBottomBar(m))
        assertFalse(LayoutPlan.usesRail(m))
        assertTrue(LayoutPlan.authoringEnabled(m))
    }

    @Test fun flipSidewaysIsExpandedWithARailAndAuthoring() {
        val m = LayoutPlan.mode(1015f, 415f)
        assertEquals(LayoutMode.EXPANDED, m)
        assertTrue(LayoutPlan.usesRail(m))
        assertTrue(LayoutPlan.showsBothPanes(m))
        assertTrue(LayoutPlan.authoringEnabled(m))
    }

    @Test fun flipCoverIsSmallAndNearSquareSoItTakesTheCompactSlidingConsole() {
        assertEquals(LayoutMode.WATCH, LayoutPlan.mode(356f, 343f))
    }

    @Test fun aTabletIsExpanded() {
        val m = LayoutPlan.mode(1280f, 800f)
        assertEquals(LayoutMode.EXPANDED, m)
        assertTrue(LayoutPlan.showsBothPanes(m))
    }

    @Test fun theBreakpointsAreTheMaterialWidthClasses() {
        assertEquals(LayoutMode.COMPACT, LayoutPlan.mode(599f, 1200f))
        assertEquals(LayoutMode.MEDIUM, LayoutPlan.mode(600f, 1200f))
        assertEquals(LayoutMode.MEDIUM, LayoutPlan.mode(839f, 1200f))
        assertEquals(LayoutMode.EXPANDED, LayoutPlan.mode(840f, 1200f))
    }

    @Test fun builderIsPresentInSharedNavigationOrder() {
        assertEquals(
            listOf("OPS", "EVENTS", "TOOLS", "BUILDER", "MISSION", "SYSTEM"),
            Destination.values().map { it.label },
        )
    }

    @Test fun builderDoesNotChangeTheHardwareWatchLayout() {
        assertFalse(LayoutPlan.authoringEnabled(LayoutMode.WATCH))
        assertTrue(LayoutPlan.authoringEnabled(LayoutMode.MEDIUM))
    }
}
