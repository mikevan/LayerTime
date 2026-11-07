package com.projectrevivesolutions.layertime.controller.flow

/**
 * The pieces an operator uses to build a Verdict tile by hand (Michael, 2026-11-06).
 *
 * LayerTime exists to put the detection of common Wi-Fi and BLE attacks in the hands of
 * the people those attacks are used against, as free, open-source, passive analysis. None
 * of this transmits. Every tool here reads frames the Hunter already heard and reports what
 * it sees, so a user can build their own detector and read its result in plain language.
 *
 * Two primitives live here, both pure so they are host-tested:
 *  - [FrameCount] counts identical frames: the same kind, from the same address, to the same
 *    address, meaning the same thing. This is how a user counts "how many of these did I see?"
 *  - [Summary] fills the blanks in a sentence the user writes, so the verdict reads in their
 *    own words, the way the shipped flows read in mine.
 */

/** Counts frames that are identical: same type, same meaning, same source, same destination. */
object FrameCount {
    /**
     * One group of identical frames. [meaning] is the field that says what the frame is for:
     * the reason code for a disconnect, the network name for a beacon or probe response, the
     * handshake message number for an EAPOL-Key frame, and nothing for a plain data frame.
     */
    data class Group(
        val typeSubtype: Int,
        val typeName: String,
        val meaning: String?,
        val sa: String?,
        val da: String?,
        val count: Int,
        val indices: List<Int>,
    ) {
        /** One line an operator reads: "40 x Deauthentication, reason 7, aa:bb -> ff:ff". */
        val line: String get() = buildString {
            append(count); append(" x "); append(typeName)
            if (meaning != null) { append(", "); append(meaning) }
            append(", "); append(sa ?: "?"); append(" -> "); append(da ?: "?")
        }
    }

    /** Wireshark's name for a frame type, by its type/subtype. */
    fun typeName(typeSubtype: Int): String = when (typeSubtype) {
        AnalyticFrame.BEACON -> "Beacon"
        AnalyticFrame.PROBE_RESP -> "Probe Response"
        AnalyticFrame.DISASSOC -> "Disassociation"
        AnalyticFrame.DEAUTH -> "Deauthentication"
        else -> if ((typeSubtype shr 4) == 2) "Data" else "0x%02x".format(typeSubtype)
    }

    /** What the frame is for, in Wireshark's terms, or null when the type carries no such field. */
    fun meaningOf(f: AnalyticFrame): String? = when {
        f.isDisconnect -> f.reasonCode?.let { "reason $it" }
        f.isBeacon || f.typeSubtype == AnalyticFrame.PROBE_RESP -> f.ssid?.let { "SSID \"$it\"" }
        f.eapol -> f.eapolMsg?.let { "message $it" }
        else -> null
    }

    /** Every group of identical frames in [frames], the largest group first. */
    fun groups(frames: List<AnalyticFrame>): List<Group> =
        frames.groupBy { listOf(it.typeSubtype, meaningOf(it), it.sa, it.da) }
            .map { (_, fs) ->
                val f = fs.first()
                Group(f.typeSubtype, typeName(f.typeSubtype), meaningOf(f), f.sa, f.da, fs.size, fs.map { it.index })
            }
            .sortedWith(compareByDescending<Group> { it.count }.thenBy { it.typeName }.thenBy { it.sa ?: "" })

    /** The largest group's size, or 0 when there are no frames. For a rule such as "any group >= N". */
    fun largest(frames: List<AnalyticFrame>): Int = frames
        .groupingBy { listOf(it.typeSubtype, meaningOf(it), it.sa, it.da) }.eachCount().values.maxOrNull() ?: 0
}

/** Fills {blanks} in a sentence the operator wrote, from the facts the verdict has. */
object Summary {
    private val BLANK = Regex("\\{([^{}]+)}")

    /**
     * Replaces every {name} in [template] with vars[name]. A blank with no value is left as
     * written, so a mistyped name is visible to the operator instead of silently vanishing.
     */
    fun fill(template: String, vars: Map<String, String>): String =
        BLANK.replace(template) { m -> vars[m.groupValues[1].trim()] ?: m.value }

    /** The names a template may use, for the editor to offer and the validator to check against. */
    fun blanksIn(template: String): List<String> =
        BLANK.findAll(template).map { it.groupValues[1].trim() }.distinct().toList()
}

/**
 * A verdict rule set the operator writes on the Verdict tile (Michael, 2026-11-06).
 *
 * The rules are read top to bottom; the first whose test is met sets the verdict and the
 * sentence. A test can count the checks by how they lean, read one named check, count
 * identical frames, or combine those with all/any/not. The sentence may carry blanks the
 * engine fills: {suspicious}, {counted}, {benign}, {unknown}, {top}, {groups}, {frames},
 * and each check's value by its step id (for example {after}). Left empty on the tile, the
 * engine uses [deauthDefault], the shipped deauth triage written out as rules, so the first
 * analytic is an example an operator can read and copy.
 */

/** One test a rule applies. Kept small and in Wireshark-literate terms. */
sealed interface Cond {
    /** A named check leans this way. */
    data class CheckLeans(val step: String, val lean: Lean) : Cond
    /** A named check's value contains this text. */
    data class ValueContains(val step: String, val text: String) : Cond
    /** At least [atLeast] of the counted checks lean this way. */
    data class CountLeans(val lean: Lean, val atLeast: Int) : Cond
    /** The largest group of identical frames is at least [atLeast]. */
    data class GroupAtLeast(val atLeast: Int) : Cond
    data class All(val of: List<Cond>) : Cond
    data class Any(val of: List<Cond>) : Cond
    data class Not(val of: Cond) : Cond
    data object Always : Cond
}

data class VerdictRule(val cond: Cond, val verdict: Verdict, val summary: String)

class VerdictRuleSetError(message: String) : Exception(message)

data class VerdictRuleSet(val counted: List<String>, val rules: List<VerdictRule>) {

    /** The frames a rule set reads: the checks' answers, and optionally the frames behind them. */
    class View(val features: List<Feature>, val frames: List<AnalyticFrame>, val counted: List<String>) {
        fun feature(step: String): Feature? = features.firstOrNull { it.step == step }
        fun leans(lean: Lean): Int = counted.count { feature(it)?.lean == lean }
        val present: Int get() = counted.count { feature(it) != null }
    }

    private fun matches(c: Cond, v: View): Boolean = when (c) {
        is Cond.CheckLeans -> v.feature(c.step)?.lean == c.lean
        is Cond.ValueContains -> v.feature(c.step)?.value?.contains(c.text) == true
        is Cond.CountLeans -> v.leans(c.lean) >= c.atLeast
        is Cond.GroupAtLeast -> FrameCount.largest(v.frames) >= c.atLeast
        is Cond.All -> c.of.all { matches(it, v) }
        is Cond.Any -> c.of.any { matches(it, v) }
        is Cond.Not -> !matches(c.of, v)
        Cond.Always -> true
    }

    /** Runs the rules and builds the finding, the same shape every check and flow produces. */
    fun evaluate(features: List<Feature>, frames: List<AnalyticFrame> = emptyList()): Finding {
        val v = View(features, frames, counted)
        val couldNotRun = features.filter { it.lean == Lean.UNKNOWN }
        val match = rules.firstOrNull { matches(it.cond, v) }
        val verdict = match?.verdict ?: Verdict.NOT_WORRISOME
        val vars = HashMap<String, String>()
        vars["suspicious"] = v.leans(Lean.SUSPICIOUS).toString()
        vars["benign"] = v.leans(Lean.BENIGN).toString()
        vars["counted"] = v.present.toString()
        vars["unknown"] = couldNotRun.size.toString()
        vars["frames"] = frames.size.toString()
        val groups = FrameCount.groups(frames)
        vars["top"] = groups.firstOrNull()?.line ?: "none"
        vars["groups"] = if (groups.isEmpty()) "none" else groups.joinToString("; ") { it.line }
        for (f in features) vars[f.step] = f.value
        var summary = Summary.fill(match?.summary ?: "No rule matched these checks.", vars)
        if (couldNotRun.isNotEmpty()) summary += " ${couldNotRun.size} check${if (couldNotRun.size == 1) "" else "s"} could not run."
        val frameRefs = features.flatMap { it.frames }.distinct().sorted()
        return Finding(verdict, summary, features.filter { it.lean != Lean.UNKNOWN }, couldNotRun, frameRefs)
    }

    fun toJson(): String = FlowJson.write(linkedMapOf(
        "counted" to counted,
        "rules" to rules.map { r -> linkedMapOf("verdict" to r.verdict.label, "summary" to r.summary, "when" to condJson(r.cond)) },
    ))

    companion object {
        private fun leanOf(s: String): Lean = when (s.trim().lowercase()) {
            "benign" -> Lean.BENIGN; "suspicious" -> Lean.SUSPICIOUS; "unknown" -> Lean.UNKNOWN
            else -> throw VerdictRuleSetError("\"$s\" is not benign, suspicious, or unknown.")
        }
        private fun verdictOf(s: String): Verdict = Verdict.values().firstOrNull { it.label == s.trim() }
            ?: throw VerdictRuleSetError("\"$s\" is not a verdict. Use ${Verdict.values().joinToString(", ") { "\"${it.label}\"" }}.")

        /** The deauth triage, written as rules: the first analytic an operator can read and copy. */
        fun deauthDefault(): VerdictRuleSet {
            val core = listOf("target", "rate", "reason", "seq", "signal")
            fun count(n: Int) = Cond.CountLeans(Lean.SUSPICIOUS, n)
            return VerdictRuleSet(core, listOf(
                VerdictRule(
                    Cond.All(listOf(Cond.ValueContains("rate", "1 frame"), Cond.CheckLeans("reason", Lean.BENIGN))),
                    Verdict.NOT_WORRISOME, "A single ordinary disconnect; housekeeping."),
                VerdictRule(
                    Cond.All(listOf(count(3), Cond.CheckLeans("after", Lean.SUSPICIOUS),
                        Cond.Any(listOf(Cond.ValueContains("after", "clone"), Cond.ValueContains("after", "handshake"))))),
                    Verdict.ATTACK_WITH_FOLLOW_ON, "A forged disconnect flood, and then what it was for: {after}."),
                VerdictRule(
                    Cond.All(listOf(count(3), Cond.ValueContains("mfp", "required"))),
                    Verdict.LIKELY_ATTACK,
                    "{suspicious} of {counted} checks lean suspicious: a forged disconnect flood from a second radio. The network requires management frame protection, so the attack most likely failed."),
                VerdictRule(count(3), Verdict.LIKELY_ATTACK,
                    "{suspicious} of {counted} checks lean suspicious: a forged disconnect flood from a second radio."),
                VerdictRule(count(1), Verdict.WATCH,
                    "{suspicious} of {counted} checks lean suspicious; not enough to call an attack. Keep watching this transmitter."),
                VerdictRule(Cond.Always, Verdict.NOT_WORRISOME, "Nothing in the disconnects points to an attack."),
            ))
        }

        /** Reads a rule set the operator wrote. Throws [VerdictRuleSetError] with a sentence on a bad file. */
        fun parse(json: String): VerdictRuleSet {
            val root = (runCatching { FlowJson.parse(json) }.getOrNull() as? Map<*, *>)
                ?: throw VerdictRuleSetError("The rules must be a JSON object with \"counted\" and \"rules\".")
            val counted = (root["counted"] as? List<*>)?.map { it.toString() }
                ?: throw VerdictRuleSetError("\"counted\" must list the checks to count, by their step ids.")
            val ruleList = (root["rules"] as? List<*>)
                ?: throw VerdictRuleSetError("\"rules\" must be a list of rules.")
            val rules = ruleList.mapIndexed { i, r ->
                val m = r as? Map<*, *> ?: throw VerdictRuleSetError("rules[$i] must be an object.")
                val verdict = verdictOf(m["verdict"] as? String ?: throw VerdictRuleSetError("rules[$i] has no \"verdict\"."))
                val summary = m["summary"] as? String ?: throw VerdictRuleSetError("rules[$i] has no \"summary\".")
                VerdictRule(cond(m["when"], "rules[$i].when"), verdict, summary)
            }
            return VerdictRuleSet(counted, rules)
        }

        private fun cond(node: Any?, where: String): Cond {
            val m = node as? Map<*, *> ?: throw VerdictRuleSetError("$where must be a test object.")
            fun intOf(k: String) = (m[k] as? Number)?.toInt() ?: throw VerdictRuleSetError("$where needs a number \"$k\".")
            fun list(k: String) = (m[k] as? List<*>)?.mapIndexed { i, c -> cond(c, "$where.$k[$i]") }
                ?: throw VerdictRuleSetError("$where.$k must be a list of tests.")
            return when {
                m.containsKey("always") -> Cond.Always
                m.containsKey("all") -> Cond.All(list("all"))
                m.containsKey("any") -> Cond.Any(list("any"))
                m.containsKey("not") -> Cond.Not(cond(m["not"], "$where.not"))
                m.containsKey("count") -> Cond.CountLeans(leanOf(m["count"].toString()), intOf("atLeast"))
                m.containsKey("group") -> Cond.GroupAtLeast(intOf("atLeast"))
                m.containsKey("lean") -> Cond.CheckLeans(m["lean"].toString(), leanOf(m["is"]?.toString() ?: ""))
                m.containsKey("value") -> Cond.ValueContains(m["value"].toString(), m["contains"]?.toString()
                    ?: throw VerdictRuleSetError("$where needs \"contains\" text."))
                else -> throw VerdictRuleSetError("$where is not a known test.")
            }
        }

        private fun condJson(c: Cond): Any = when (c) {
            is Cond.CheckLeans -> linkedMapOf("lean" to c.step, "is" to c.lean.name.lowercase())
            is Cond.ValueContains -> linkedMapOf("value" to c.step, "contains" to c.text)
            is Cond.CountLeans -> linkedMapOf("count" to c.lean.name.lowercase(), "atLeast" to c.atLeast.toLong())
            is Cond.GroupAtLeast -> linkedMapOf("group" to "identical", "atLeast" to c.atLeast.toLong())
            is Cond.All -> linkedMapOf("all" to c.of.map { condJson(it) })
            is Cond.Any -> linkedMapOf("any" to c.of.map { condJson(it) })
            is Cond.Not -> linkedMapOf("not" to condJson(c.of))
            Cond.Always -> linkedMapOf("always" to true)
        }
    }
}
