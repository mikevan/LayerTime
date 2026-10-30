package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.link.LinkHealth
import com.projectrevivesolutions.layertime.controller.link.LinkHealth.State
import org.junit.Assert.assertEquals
import org.junit.Test

class LinkHealthTest {
    @Test fun heartbeatOnTimeIsOk() { assertEquals(State.OK, LinkHealth.state(1_200, 0, 0)) }
    @Test fun threeSecondsSilentIsStale() { assertEquals(State.STALE, LinkHealth.state(3_000, 0, 0)) }
    @Test fun eightSecondsSilentIsLost() { assertEquals(State.LOST, LinkHealth.state(8_000, 0, 0)) }
    @Test fun reconWindowSilenceIsExpected() {
        // schedule 1 (Recon) with the next Report in 20 s: 15 s of silence is normal
        assertEquals(State.OK, LinkHealth.state(15_000, 1, 20))
        assertEquals(State.LOST, LinkHealth.state(28_000, 1, 20))
    }
    @Test fun reportWindowUsesNormalTimeout() { assertEquals(State.LOST, LinkHealth.state(8_000, 2, 30)) }
}
