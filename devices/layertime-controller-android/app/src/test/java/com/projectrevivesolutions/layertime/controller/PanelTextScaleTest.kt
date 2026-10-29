package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.ui.PanelTextScale
import org.junit.Assert.assertEquals
import org.junit.Test

class PanelTextScaleTest {
    @Test fun watchPanelScalesUp() { assertEquals(1.36f, PanelTextScale.auto(596f, 464f), 0.01f) }
    @Test fun phonePortraitStaysAtOne() { assertEquals(1.0f, PanelTextScale.auto(387f, 444f), 0.001f) }
    @Test fun phoneLandscapeIsLimitedByHeight() { assertEquals(1.06f, PanelTextScale.auto(477f, 360f), 0.01f) }
    @Test fun hugePanelIsCapped() { assertEquals(1.8f, PanelTextScale.auto(2000f, 2000f), 0.001f) }
}
