package com.projectrevivesolutions.layertime.controller.link

import java.util.ArrayDeque

/** One outstanding Android GATT operation. Owned exclusively by LinkClient's handler.
 * A reset discards queued operations; they must never leak into a new connection.
 */
internal class GattOperationQueue {
    private val waiting = ArrayDeque<() -> Unit>()
    var busy = false
        private set

    fun enqueue(operation: () -> Unit) {
        waiting.add(operation)
        drain()
    }

    fun complete() {
        busy = false
        drain()
    }

    fun reset() {
        waiting.clear()
        busy = false
    }

    private fun drain() {
        if (busy) return
        val operation = waiting.poll() ?: return
        busy = true
        operation()
    }
}
