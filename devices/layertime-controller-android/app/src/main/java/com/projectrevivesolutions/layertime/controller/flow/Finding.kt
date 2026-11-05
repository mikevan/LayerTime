package com.projectrevivesolutions.layertime.controller.flow

/** Which way one check leans. UNKNOWN means the check could not run on these frames, and says why. */
enum class Lean { BENIGN, SUSPICIOUS, UNKNOWN }

/**
 * One check's answer: a name, the value it saw, which way it leans, one sentence of
 * reason, and the frames it rests on. The reason is written for the operator.
 */
data class Feature(
    val step: String,          // the step id that produced it
    val title: String,         // the step's title
    val value: String,         // what it measured, short
    val lean: Lean,
    val reason: String,
    val frames: List<Int> = emptyList(),
)

enum class Verdict(val label: String) {
    NOT_WORRISOME("NOT WORRISOME"),
    WATCH("WATCH"),
    LIKELY_ATTACK("LIKELY ATTACK"),
    ATTACK_WITH_FOLLOW_ON("ATTACK WITH FOLLOW-ON"),
}

/**
 * The verdict with its evidence chain: every feature that drove it, the checks that
 * could not run, and the frames behind it. Never says "countered" or "blocked".
 */
data class Finding(
    val verdict: Verdict,
    val summary: String,
    val features: List<Feature>,
    val couldNotRun: List<Feature>,
    val frames: List<Int>,
) {
    val suspicious get() = features.filter { it.lean == Lean.SUSPICIOUS }
    val benign get() = features.filter { it.lean == Lean.BENIGN }
}
