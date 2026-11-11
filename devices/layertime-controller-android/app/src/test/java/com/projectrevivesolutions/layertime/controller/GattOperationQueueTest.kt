package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.link.GattOperationQueue
import org.junit.Assert.*
import org.junit.Test

class GattOperationQueueTest {
    @Test fun onlyOneOperationRunsUntilCompletion() {
        val queue = GattOperationQueue()
        val ran = mutableListOf<Int>()
        queue.enqueue { ran += 1 }
        queue.enqueue { ran += 2 }
        queue.enqueue { ran += 3 }
        assertEquals(listOf(1), ran)
        queue.complete()
        assertEquals(listOf(1, 2), ran)
        queue.complete()
        assertEquals(listOf(1, 2, 3), ran)
        queue.complete()
        assertFalse(queue.busy)
    }

    @Test fun disconnectDiscardsOldOperationsAndAllowsNewSession() {
        val queue = GattOperationQueue()
        val ran = mutableListOf<String>()
        queue.enqueue { ran += "old" }
        queue.enqueue { fail("old queued command leaked into new session") }
        queue.reset()
        queue.enqueue { ran += "new" }
        assertEquals(listOf("old", "new"), ran)
        assertTrue(queue.busy)
    }

    @Test fun synchronousRefusalCanResetWhileAnOperationIsStarting() {
        val queue = GattOperationQueue()
        queue.enqueue { }
        queue.enqueue { queue.reset() }
        queue.enqueue { fail("command ran after local refusal") }
        queue.complete()
        assertFalse(queue.busy)
        var recovered = false
        queue.enqueue { recovered = true }
        assertTrue(recovered)
    }
}
