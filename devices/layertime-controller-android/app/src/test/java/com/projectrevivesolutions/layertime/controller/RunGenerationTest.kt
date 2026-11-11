package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.flow.RunGeneration
import org.junit.Assert.*
import org.junit.Test
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicReference

class RunGenerationTest {
    @Test fun lateCompletionCannotReplaceNewEvidenceOrClearItsRunningState() {
        val gate=RunGeneration()
        val value=AtomicReference("old")
        val old=gate.begin()
        val started=CountDownLatch(1);val finish=CountDownLatch(1)
        val worker=Thread {started.countDown();finish.await();gate.publish(old){value.set("stale")}}
        worker.start();assertTrue(started.await(2,TimeUnit.SECONDS))
        val current=gate.begin {value.set("new running")}
        finish.countDown();worker.join(2000)
        assertFalse(worker.isAlive)
        assertEquals("new running",value.get())
        assertTrue(gate.publish(current){value.set("new evidence")})
        assertEquals("new evidence",value.get())
    }
    @Test fun clearedRunCannotPublishAgain() {
        val gate=RunGeneration();var result:String?="previous"
        val token=gate.begin()
        gate.begin {result=null}
        assertFalse(gate.publish(token){result="late"})
        assertNull(result)
    }
}
