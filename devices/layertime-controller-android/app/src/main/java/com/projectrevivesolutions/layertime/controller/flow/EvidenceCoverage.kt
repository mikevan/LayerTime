package com.projectrevivesolutions.layertime.controller.flow

/** Describes the supplied recording, never continuous coverage of the radio environment. */
data class EvidenceCoverage(
    val captureId: String, val decodedFrames: Int, val analyzedFrames: Int,
    val cancelled: Boolean = false, val readError: String? = null,
    val malformedFrames: Int = 0, val untimedFrames: Int = 0, val training:Boolean=false,
) {
    val limitations: List<String> get() = buildList {
        if (analyzedFrames < decodedFrames) add("Analysis limit reached: $analyzedFrames of $decodedFrames decoded frames are available.")
        if (cancelled) add("The import was cancelled; the recording was only partly decoded.")
        if (readError != null) add("The recording could not be fully read: $readError")
        if (malformedFrames > 0) add("$malformedFrames malformed frames were observed; malformed observations are excluded from behavioral correlation.")
        if (untimedFrames > 0) add("$untimedFrames frames have no valid timestamp and cannot establish temporal order.")
        add("A recording does not establish continuous channel coverage. Unobserved activity is unknown.")
    }
    val sentence: String get() = (if(training) "TRAINING DATA - generated offline example, not live Hunter evidence. " else "") + "Capture $captureId: $analyzedFrames of $decodedFrames decoded frames available for analysis. " + limitations.joinToString(" ")
}
