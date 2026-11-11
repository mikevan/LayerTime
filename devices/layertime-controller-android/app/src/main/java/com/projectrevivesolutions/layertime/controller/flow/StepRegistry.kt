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

    /** The step type the Wireshark palette tile drops in before a check is chosen. */
    const val WIRESHARK_PLACEHOLDER = "wireshark_check"
    /** The checks the Wireshark menu offers, in the order it shows them. */
    val WIRESHARK_CHECKS = listOf("deauth_target", "deauth_rate", "deauth_reason", "seq_integrity", "signal_match", "aftermath", "mgmt_protection")
    /** The palette items that are not Wireshark checks: the source and the verdict. */
    val PALETTE_FIXED = listOf("event_source", "capture_source", "target_behavior", "deauth_verdict")
    /** The palette, in order: the source, the one Wireshark check tile, the verdict. */
    val PALETTE = listOf("event_source", "capture_source", WIRESHARK_PLACEHOLDER, "target_behavior", "deauth_verdict")
    fun register(spec: StepSpec) { specs[spec.type] = spec }

    private val frames = PortSpec("frames", DataType.FRAMES)

    init {
        register(StepSpec("capture_source", "Capture",
            "All available observations from the imported recording, with its capture identity and limitations. This source is independent of any Hunter detection.",
            output=DataType.FRAMES))
        register(StepSpec("target_behavior", "Behavior Timeline",
            "Groups disconnects by client and network, then correlates later EAPOL traffic for that same pair. Explains the observed behavior and keeps the supporting frames.",
            inputs = listOf(frames), output = DataType.FEATURE,
            settings = listOf(SettingSpec("follow_on_ms", 30000L, "How long after each disconnect to look for EAPOL traffic, in milliseconds.", 1L, 3600000L), SettingSpec("episode_gap_ms", 3000L, "Separate disconnect episodes when the gap exceeds this many milliseconds.", 1L, 3600000L),
                SettingSpec("target_mac", "", "Only this client address, or empty for all clients."),
                SettingSpec("network_mac", "", "Only this network address, or empty for all networks.")),
            tool = "attack_timeline"))
        register(StepSpec("wireshark_check", "Wireshark",
            "A Wireshark check waiting to be chosen. Long-press it and hold on a check to say which one it is.",
            output = null, tool = "wireshark"))
        register(StepSpec("event_source", "Detection",
            "The detection the Hunter reported and the frames around it: which sensor fired, the address it heard, the channel, the signal, and when, plus the frames from that transmitter and everything else heard in the window. Every check reads from here.",
            output = DataType.FRAMES,
            settings = listOf(SettingSpec("window_ms", 10000L, "How far before and after the detection to look, in milliseconds. The after window is at least after_ms.", 1L, 3600000L), SettingSpec("after_ms", 30000L, "Minimum evidence time after the detection, in milliseconds.", 1L, 3600000L)),
            tool = null)) // the frames come through LayerScope's Wireshark decoder
        register(StepSpec("deauth_target", "Who it targets",
            "Were the disconnects sent to every client at once (broadcast), or aimed at one client?",
            inputs = listOf(frames), output = DataType.FEATURE))
        register(StepSpec("deauth_rate", "Rate and shape",
            "One housekeeping frame, or a burst? Counts disconnect frames per second and how long the burst lasted.",
            inputs = listOf(frames), output = DataType.FEATURE,
            settings = listOf(
                SettingSpec("burst_frames", 6L, "How many disconnect frames inside the burst window count as a burst.", 1L, 1000000L),
                SettingSpec("burst_ms", 3000L, "The burst window, in milliseconds.", 1L, 3600000L))))
        register(StepSpec("deauth_reason", "Reason code",
            "Reads the reason each disconnect gives. Reason codes describe the stated disconnect reason; combine them with other observations.",
            inputs = listOf(frames), output = DataType.FEATURE))
        register(StepSpec("seq_integrity", "Sequence numbers",
            "A real sender numbers its frames in order. Differences from the observed beacon stream are supporting observations; capture loss and separate sequence streams can also cause gaps.",
            inputs = listOf(frames), output = DataType.FEATURE))
        register(StepSpec("signal_match", "Signal consistency",
            "Compares the disconnects' signal strength with the access point's own beacons. Supporting evidence only; a copied signal is possible.",
            inputs = listOf(frames), output = DataType.FEATURE,
            settings = listOf(SettingSpec("tolerance_db", 8L, "How many dB apart the two signals may be before it counts as a mismatch.", 0L, 127L))))
        register(StepSpec("aftermath", "What happened next",
            "Did the disconnected client keep talking? Did a copy of the network appear, or a password handshake follow? An evil twin often follows a deauth.",
            inputs = listOf(frames), output = DataType.FEATURE,
            settings = listOf(SettingSpec("watch_ms", 30000L, "How long after the disconnects to watch, in milliseconds.", 1L, 3600000L))))
        register(StepSpec("mgmt_protection", "Protection",
            "Was the network using management frame protection? Advertisements describe capabilities; they do not establish client negotiation, enforcement, or the outcome.",
            inputs = listOf(frames), output = DataType.FEATURE))
        register(StepSpec("deauth_verdict", "Report",
            "Weighs the checks into one call by the rules you write: each rule sets a verdict and a sentence when its test is met, read top to bottom, first match wins. It can count checks by how they lean, count identical frames (same type, meaning, source, and destination), and fill blanks like {suspicious}, {counted}, and {top} into your sentence. Left empty, it uses the deauth triage rules.",
            inputs = listOf(PortSpec("features", DataType.FEATURE, many = true), PortSpec("frames", DataType.FRAMES, optional = true)),
            output = DataType.FINDING,
            settings = listOf(SettingSpec("rules", "", "The verdict rules, as JSON. Empty means the built-in deauth triage rules."))))
    }
}
