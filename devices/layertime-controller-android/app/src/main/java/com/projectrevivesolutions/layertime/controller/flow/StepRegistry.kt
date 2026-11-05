package com.projectrevivesolutions.layertime.controller.flow

/**
 * Every step type this build of LayerScope knows (2026-11-04). The validator and the
 * Builder read the specs; the engine looks up runners by the same type name. A
 * workflow file that names a type missing here is refused, so a traded file never
 * runs half of itself.
 *
 * The first set is the deauth triage from claude/layerscope_analytics_design.md:
 * seven deterministic checks feeding one verdict. Every description is written for
 * the operator, not the programmer.
 */
object StepRegistry {
    private val specs = LinkedHashMap<String, StepSpec>()

    fun spec(type: String): StepSpec? = specs[type]
    fun all(): List<StepSpec> = specs.values.toList()
    fun register(spec: StepSpec) { specs[spec.type] = spec }

    private val frames = PortSpec("frames", DataType.FRAMES)

    init {
        register(StepSpec("event_source", "Detection",
            "The detection the Hunter reported: which sensor fired, the address it heard, the channel, the signal, and when.",
            output = DataType.EVENT))
        register(StepSpec("load_frames", "Frames behind it",
            "Gathers the frames around the detection: the ones from the same transmitter, and everything else heard in the window around it.",
            inputs = listOf(PortSpec("event", DataType.EVENT)), output = DataType.FRAMES,
            settings = listOf(SettingSpec("window_ms", 10000L, "How far before and after the detection to look, in milliseconds."))))
        register(StepSpec("deauth_target", "Who it targets",
            "Were the disconnects sent to every client at once (broadcast), or aimed at one client?",
            inputs = listOf(frames), output = DataType.FEATURE))
        register(StepSpec("deauth_rate", "Rate and shape",
            "One housekeeping frame, or a burst? Counts disconnect frames per second and how long the burst lasted.",
            inputs = listOf(frames), output = DataType.FEATURE,
            settings = listOf(
                SettingSpec("burst_frames", 6L, "How many disconnect frames inside the burst window count as a burst."),
                SettingSpec("burst_ms", 3000L, "The burst window, in milliseconds."))))
        register(StepSpec("deauth_reason", "Reason code",
            "Reads the reason each disconnect gives. Attack tools send reason 7 by default; codes outside the standard are a tell.",
            inputs = listOf(frames), output = DataType.FEATURE))
        register(StepSpec("seq_integrity", "Sequence numbers",
            "A real sender numbers its frames in order. Disconnects whose numbers do not fit the access point's own count came from a second radio.",
            inputs = listOf(frames), output = DataType.FEATURE))
        register(StepSpec("signal_match", "Signal consistency",
            "Compares the disconnects' signal strength with the access point's own beacons. Supporting evidence only; a copied signal is possible.",
            inputs = listOf(frames), output = DataType.FEATURE,
            settings = listOf(SettingSpec("tolerance_db", 8L, "How many dB apart the two signals may be before it counts as a mismatch."))))
        register(StepSpec("aftermath", "What happened next",
            "Did the disconnected client keep talking? Did a copy of the network appear, or a password handshake follow? An evil twin often follows a deauth.",
            inputs = listOf(frames), output = DataType.FEATURE,
            settings = listOf(SettingSpec("watch_ms", 30000L, "How long after the disconnects to watch, in milliseconds."))))
        register(StepSpec("mgmt_protection", "Protection",
            "Was the network using management frame protection? A deauth flood only works on networks without it.",
            inputs = listOf(frames), output = DataType.FEATURE))
        register(StepSpec("deauth_verdict", "Verdict",
            "Weighs every check into one call: NOT WORRISOME, WATCH, LIKELY ATTACK, or ATTACK WITH FOLLOW-ON, with the reasons and any check that could not run.",
            inputs = listOf(PortSpec("features", DataType.FEATURE, many = true)), output = DataType.FINDING))
    }
}
