package com.projectrevivesolutions.layertime.controller.hunt

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

/**
 * HUNT v1: the one hunt the watch runs, shared by the link (readings), the step
 * sensor (steps), and the hunt screen (the view). Everything calls it on the main
 * thread; the locks only guard against a stray caller.
 */
object HuntSession {
    private val engine = HuntEngine()
    private val _open = MutableStateFlow(false)
    /** True while the hunt screen is up. */
    val open: StateFlow<Boolean> = _open.asStateFlow()
    private val _problem = MutableStateFlow<String?>(null)
    /** A sentence for the hunt screen when something is wrong, or null. */
    val problem: StateFlow<String?> = _problem.asStateFlow()

    val bands get() = engine.bands

    @Synchronized fun begin(nowMs: Long) { engine.start(nowMs); _problem.value = null; _open.value = true }
    @Synchronized fun end() { _open.value = false; _problem.value = null }
    @Synchronized fun onReading(readings: Int, mean: Int?, latest: Int?, strongest: Int?, hunting: Boolean, lost: Boolean, nowMs: Long) {
        if (!_open.value) return
        engine.onReading(readings, mean, latest, strongest, hunting, lost, nowMs)
        if (hunting) _problem.value = null
    }
    @Synchronized fun onStep(nowMs: Long) { if (_open.value) engine.onStep(nowMs) }
    @Synchronized fun view(nowMs: Long): HuntView = engine.view(nowMs)
    @Synchronized fun toneFor(v: HuntView): Tone? = engine.toneFor(v)
    fun setProblem(text: String?) { _problem.value = text }
}
