package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.link.LinkConstants
import com.projectrevivesolutions.layertime.controller.service.AlertPolicy
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class AlertPolicyTest {
    @Test
    fun lowIsListedButNeverAlerted() {
        assertEquals("LOW", LinkConstants.Confidence.name(AlertPolicy.CONFIDENCE_LOW))
        assertFalse(AlertPolicy.worthAnAlert(0))
    }

    @Test
    fun mediumAndHighAlert() {
        assertTrue(AlertPolicy.worthAnAlert(1))
        assertTrue(AlertPolicy.worthAnAlert(2))
    }

    @Test
    fun anUnknownConfidenceStillAlerts() {
        assertTrue(AlertPolicy.worthAnAlert(3))
        assertTrue(AlertPolicy.worthAnAlert(255))
    }
}
