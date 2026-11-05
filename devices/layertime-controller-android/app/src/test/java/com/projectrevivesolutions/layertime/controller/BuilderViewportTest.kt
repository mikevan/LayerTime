package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.ui.BuilderViewport
import org.junit.Assert.*
import org.junit.Test

class BuilderViewportTest {
    @Test fun pinchKeepsTheSameWorldPointUnderTheFinger() {
        val before = BuilderViewport(1f, -1000f, -500f)
        val after = before.transform(2f, 125f, 80f, 0f, 0f, 400f, 600f, 10000f, 6000f)
        assertEquals((125f-before.x)/before.zoom, (125f-after.x)/after.zoom, .001f)
        assertEquals((80f-before.y)/before.zoom, (80f-after.y)/after.zoom, .001f)
    }
    @Test fun dragIsAppliedInScreenPixelsAtAnyZoom() {
        val after = BuilderViewport(2f,-1000f,-1000f).transform(1f,0f,0f,30f,-40f,400f,600f,10000f,6000f)
        assertEquals(-970f, after.x, .001f)
        assertEquals(-1040f, after.y, .001f)
    }
    @Test fun zoomIsClampedWithoutMovingPastWorldEdges() {
        val after = BuilderViewport().transform(100f,0f,0f,500f,-100000f,400f,600f,10000f,6000f)
        assertEquals(4f,after.zoom,.001f)
        assertEquals(0f,after.x,.001f)
        assertEquals(600f-6000f*4f,after.y,.001f)
    }
    @Test fun fitShowsTheWholeWorkspaceAndCentersIt() {
        val fit=BuilderViewport.fit(400f,600f,10000f,6000f)
        assertTrue(fit.x>=0f && fit.y>=0f)
        assertTrue(fit.x+10000f*fit.zoom<=400f)
        assertTrue(fit.y+6000f*fit.zoom<=600f)
        assertEquals(200f,fit.x+5000f*fit.zoom,.001f)
        assertEquals(300f,fit.y+3000f*fit.zoom,.001f)
    }
    @Test fun resizeRetainsAReachableWorkspace() {
        val after=BuilderViewport(.025f,-4000f,-4000f).bounded(600f,400f,10000f,6000f)
        assertEquals(175f,after.x,.001f)
        assertEquals(125f,after.y,.001f)
    }
}
