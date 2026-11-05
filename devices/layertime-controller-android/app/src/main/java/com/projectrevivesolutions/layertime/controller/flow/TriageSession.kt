package com.projectrevivesolutions.layertime.controller.flow

import android.content.Context
import com.projectrevivesolutions.layertime.controller.model.EventRow
import com.projectrevivesolutions.layertime.controller.wireshark.PcapImport
import com.projectrevivesolutions.layertime.controller.wireshark.WiresharkDecoder
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

/**
 * The app's one loaded workflow and its last run (2026-11-04). Slice 1 of the
 * analytic pipeline: the shipped deauth triage, run on the frames of the capture
 * file the operator imported. The Hunter's own evidence window (Link 0.2) plugs in
 * here later as another FrameSource; nothing above this object changes for it.
 *
 * Bridging the event to a capture file: the Hunter's event carries phone receive
 * time, and a capture carries its own clock, so the two never line up. On a capture
 * the event is re-timed to the first disconnect frame from the event's transmitter
 * (or the capture's first frame), and the window then covers what follows.
 */
object TriageSession {
    private val _workflow = MutableStateFlow<Workflow?>(null)
    val workflow: StateFlow<Workflow?> = _workflow.asStateFlow()
    private val _problems = MutableStateFlow<List<Problem>>(emptyList())
    val problems: StateFlow<List<Problem>> = _problems.asStateFlow()
    private val _run = MutableStateFlow<RunResult?>(null)
    val run: StateFlow<RunResult?> = _run.asStateFlow()
    private val _runFor = MutableStateFlow<String?>(null)
    /** "session:eventId" of the event the last run was for, so a detail page knows the finding is its own. */
    val runFor: StateFlow<String?> = _runFor.asStateFlow()
    private val _error = MutableStateFlow<String?>(null)
    val error: StateFlow<String?> = _error.asStateFlow()
    /** Frames (capture indexes) a finding asked Wireshark to show; null shows everything. */
    val focusFrames = MutableStateFlow<Set<Int>?>(null)

    @Volatile private var mapped: List<AnalyticFrame>? = null
    @Volatile private var mappedFrom: List<WiresharkDecoder.PcapFrame>? = null

    fun load(context: Context) {
        if (_workflow.value != null) return
        DeauthSteps.register()
        val text = context.assets.open("workflows/deauth_triage.json").bufferedReader().use { it.readText() }
        val w = WorkflowLoader.parse(text)
        _problems.value = WorkflowValidator.validate(w)
        _workflow.value = w
    }

    fun key(e: EventRow) = "${e.sessionId}:${e.eventId}"

    /** The imported capture as analytic frames, mapped once per import. */
    fun captureFrames(): List<AnalyticFrame>? {
        val kept = (PcapImport.state.value as? PcapImport.State.Done)?.result?.kept ?: return null
        if (mappedFrom !== kept) { mapped = kept.mapIndexed { i, f -> AnalyticFrame.fromPcap(i, f) }; mappedFrom = kept }
        return mapped
    }

    /** Runs the loaded workflow for this event on the imported capture. Returns false when there is nothing to run on. */
    fun runOnCapture(e: EventRow): Boolean {
        val w = _workflow.value ?: return false
        val frames = captureFrames() ?: return false
        if (frames.isEmpty()) return false
        val address = e.sourceId?.lowercase()
        val anchor = frames.firstOrNull { it.isDisconnect && (address == null || it.sa == address) } ?: frames.first()
        val event = DetectionEvent(e.detectorName, address, e.channel, e.rssi, anchor.timeNanos, e.count)
        return try {
            val r = Engine.run(w, RunContext(event, FrameSource { frames }))
            _run.value = r
            _runFor.value = key(e)
            _error.value = r.steps.firstOrNull { it.error != null }?.let { "${it.step}: ${it.error}" }
            true
        } catch (ex: WorkflowFileError) {
            _error.value = ex.message; false
        }
    }

    const val CAPTURE_KEY = "capture"

    /**
     * Runs the loaded workflow on the imported capture with no Hunter detection behind it
     * (2026-11-04: an imported capture has no event, so the capture page needs its own
     * entry). The subject is the transmitter that sent the most disconnect frames; with
     * none in the capture the run still happens and the checks say so.
     */
    fun runOnCaptureAlone(): Boolean {
        val w = _workflow.value ?: return false
        val frames = captureFrames() ?: return false
        if (frames.isEmpty()) return false
        val disconnects = frames.filter { it.isDisconnect }
        val tx = disconnects.mapNotNull { it.sa }.groupingBy { it }.eachCount().maxByOrNull { it.value }?.key
        val anchor = disconnects.firstOrNull { it.sa == tx } ?: frames.first()
        val event = DetectionEvent("DEAUTH", tx, anchor.channel, anchor.rssi, anchor.timeNanos, disconnects.count { it.sa == tx })
        return try {
            val r = Engine.run(w, RunContext(event, FrameSource { frames }))
            _run.value = r
            _runFor.value = CAPTURE_KEY
            _error.value = r.steps.firstOrNull { it.error != null }?.let { "${it.step}: ${it.error}" }
            true
        } catch (ex: WorkflowFileError) {
            _error.value = ex.message; false
        }
    }

    /** The transmitter a capture-only run looked at, for the panel to name it. */
    fun captureSubject(): String? = (_run.value?.output("event") as? DetectionEvent)?.address

    fun clear() { _run.value = null; _runFor.value = null; _error.value = null; focusFrames.value = null }
}
