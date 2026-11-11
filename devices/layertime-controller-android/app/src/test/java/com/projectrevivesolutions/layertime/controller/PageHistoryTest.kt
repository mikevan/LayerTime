package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.ui.PageHistory
import org.junit.Assert.*
import org.junit.Test

class PageHistoryTest {
    @Test fun nestedToolsReturnToTheirActualOpeningScreens() {
        var h=PageHistory().open("builder").open("timeline").open("wireshark")
        h=h.back();assertEquals("timeline",h.current)
        h=h.back();assertEquals("builder",h.current)
        h=h.back();assertEquals("main",h.current)
        assertEquals(h,h.back())
    }
    @Test fun rootBackStaysInTheApp() {assertEquals(PageHistory(),PageHistory().back())}
    @Test fun openingCurrentScreenDoesNotCreateDuplicateBackSteps() {
        val h=PageHistory().open("tools")
        assertEquals(h,h.open("tools"))
        assertEquals("main",h.open("tools").back().current)
    }
    @Test fun eventDetailAndCaptureDoNotReturnToToolsByAssumption() {
        val h=PageHistory().open("attacks").open("detail").open("wireshark")
        assertEquals("detail",h.back().current)
        assertEquals("attacks",h.back().back().current)
    }
    @Test fun historyIsBoundedAndStillReturnsToRoot() {
        var h=PageHistory()
        repeat(150){h=h.open("page$it")}
        assertEquals(100,h.previous.size)
        repeat(101){h=h.back()}
        assertEquals("main",h.current)
    }
}
