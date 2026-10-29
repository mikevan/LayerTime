package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.ui.WatchFormFactor
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class WatchFormFactorTest {
    @Test fun lokmatWatchIsWatchLayout() { assertTrue(WatchFormFactor.matches(640f, 480f)) }
    @Test fun squareIsWatchLayout() { assertTrue(WatchFormFactor.matches(454f, 454f)) }
    @Test fun portraitPhoneIsNot() { assertFalse(WatchFormFactor.matches(387f, 800f)) }
    @Test fun wideLandscapePhoneIsNot() { assertFalse(WatchFormFactor.matches(800f, 387f)) }
    @Test fun exactlyMaxAspectIsNot() { assertFalse(WatchFormFactor.matches(160f, 100f)) }
}
