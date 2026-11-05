package com.projectrevivesolutions.layertime.controller.link

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import java.io.File

/**
 * Where a pull of the Hunter's frames stands (slice 2, 2026-11-04). LinkClient
 * drives it; the TRIAGE panel watches it. Done carries the pcap written from the
 * window, which then goes through the ordinary capture import.
 */
object EvidencePull {
    sealed interface State {
        data object Idle : State
        data class Pulling(val eventId: Long, val phase: String, val listed: Int, val bytes: Int, val expected: Int) : State
        data class Done(val eventId: Long, val window: LinkEvidence.Window, val frames: Int, val file: File, val elapsedMs: Long) : State
        data class Failed(val eventId: Long, val message: String) : State
    }
    private val _state = MutableStateFlow<State>(State.Idle)
    val state: StateFlow<State> = _state.asStateFlow()
    internal fun set(s: State) { _state.value = s }
    fun reset() { _state.value = State.Idle }
}
