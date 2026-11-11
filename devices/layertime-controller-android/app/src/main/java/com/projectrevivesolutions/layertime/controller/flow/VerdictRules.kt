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
        frames.groupBy { listOf(it.typeSubtype, it.eapol, meaningOf(it), it.sa?.lowercase(), it.da?.lowercase(), it.bssid?.lowercase()) }
            .map { (_, fs) ->
                val f = fs.first()
                Group(f.typeSubtype, if(f.eapol) "EAPOL" else typeName(f.typeSubtype), meaningOf(f), f.sa, f.da, fs.size, fs.map { it.index })
            }
            .sortedWith(compareByDescending<Group> { it.count }.thenBy { it.typeName }.thenBy { it.sa ?: "" })

    /** The largest group's size, or 0 when there are no frames. For a rule such as "any group >= N". */
    fun largest(frames: List<AnalyticFrame>): Int = frames
        .groupingBy { listOf(it.typeSubtype, it.eapol, meaningOf(it), it.sa?.lowercase(), it.da?.lowercase(), it.bssid?.lowercase()) }.eachCount().values.maxOrNull() ?: 0
}

/** Fills {blanks} in a sentence the operator wrote, from the facts the verdict has. */
object Summary {
    private val BLANK = Regex("\\{([^{}]+)\\}")

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
    data class MetricAtLeast(val step: String, val metric: String, val atLeast: Long) : Cond
    data class All(val of: List<Cond>) : Cond
    data class Any(val of: List<Cond>) : Cond
    data class Not(val of: Cond) : Cond
    data object Always : Cond
}

data class VerdictRule(val cond: Cond, val verdict: Verdict, val summary: String, val label: String? = null)

class VerdictRuleSetError(message: String) : Exception(message)

data class VerdictRuleSet(val counted: List<String>, val rules: List<VerdictRule>) {

    /** The frames a rule set reads: the checks' answers, and optionally the frames behind them. */
    class View(val features: List<Feature>, val frames: List<AnalyticFrame>, val counted: List<String>) {
        fun feature(step: String): Feature? = features.firstOrNull { it.step == step }
        fun leans(lean: Lean): Int = counted.count { feature(it)?.lean == lean }
        val present: Int get() = counted.count { feature(it) != null }
    }

    private fun matches(c: Cond, v: View): Boolean? = when (c) {
        is Cond.MetricAtLeast -> v.feature(c.step)?.metrics?.get(c.metric)?.let { it >= c.atLeast }
        is Cond.CheckLeans -> v.feature(c.step)?.let { if(it.lean==Lean.UNKNOWN && c.lean!=Lean.UNKNOWN) null else it.lean == c.lean }
        is Cond.ValueContains -> v.feature(c.step)?.let { it.value.contains(c.text) }
        is Cond.CountLeans -> if(v.leans(c.lean)>=c.atLeast) true else if(v.present<v.counted.size) null else false
        is Cond.GroupAtLeast -> FrameCount.largest(v.frames) >= c.atLeast
        is Cond.All -> c.of.map { matches(it,v) }.let { if(false in it) false else if(null in it) null else true }
        is Cond.Any -> c.of.map { matches(it,v) }.let { if(true in it) true else if(null in it) null else false }
        is Cond.Not -> matches(c.of,v)?.not()
        Cond.Always -> true
    }

    fun needsFrames():Boolean {
        fun needs(c:Cond):Boolean=when(c) {
            is Cond.GroupAtLeast->true
            is Cond.All->c.of.any {needs(it)}
            is Cond.Any->c.of.any {needs(it)}
            is Cond.Not->needs(c.of)
            else->false
        }
        return rules.any {needs(it.cond)}
    }

    fun references(): Set<String> {
        fun refs(c:Cond):Set<String> = when(c) {
            is Cond.CheckLeans -> setOf(c.step)
            is Cond.ValueContains -> setOf(c.step)
            is Cond.MetricAtLeast -> setOf(c.step)
            is Cond.All -> c.of.flatMap { refs(it) }.toSet()
            is Cond.Any -> c.of.flatMap { refs(it) }.toSet()
            is Cond.Not -> refs(c.of)
            else -> emptySet()
        }
        return counted.toSet()+rules.flatMap { refs(it.cond) }
    }

    /** Bind shipped rules to actual block IDs. Explicit user rules always retain their own IDs. */
    fun forFeatures(features: List<Feature>): VerdictRuleSet {
        val aliases = mapOf("target" to "deauth_target", "rate" to "deauth_rate", "reason" to "deauth_reason",
            "seq" to "seq_integrity", "signal" to "signal_match", "after" to "aftermath", "mfp" to "mgmt_protection")
        fun id(name: String) = features.firstOrNull { it.checkType == aliases[name] }?.step ?: name
        fun bind(c: Cond): Cond = when(c) {
            is Cond.CheckLeans -> c.copy(step=id(c.step))
            is Cond.ValueContains -> c.copy(step=id(c.step))
            is Cond.MetricAtLeast -> c.copy(step=id(c.step))
            is Cond.All -> Cond.All(c.of.map { bind(it) })
            is Cond.Any -> Cond.Any(c.of.map { bind(it) })
            is Cond.Not -> Cond.Not(bind(c.of))
            else -> c
        }
        return copy(counted=counted.map { id(it) }, rules=rules.map { rule ->
            var sentence=rule.summary
            for (name in aliases.keys) sentence=sentence.replace("{$name}", "{${id(name)}}")
            rule.copy(cond=bind(rule.cond),summary=sentence)
        })
    }

    /** Runs the rules and builds the finding, the same shape every check and flow produces. */
    fun evaluate(features: List<Feature>, frames: List<AnalyticFrame> = emptyList()): Finding {
        val v = View(features, frames, counted)
        val couldNotRun = features.filter { it.lean == Lean.UNKNOWN }
        val match = rules.firstOrNull { matches(it.cond, v) == true }
        val verdict = match?.verdict ?: Verdict.WATCH
        val vars = HashMap<String, String>()
        vars["suspicious"] = v.leans(Lean.SUSPICIOUS).toString()
        vars["benign"] = v.leans(Lean.BENIGN).toString()
        vars["counted"] = v.present.toString()
        vars["unknown"] = couldNotRun.size.toString()
        vars["frames"] = frames.size.toString()
        val groups = FrameCount.groups(frames)
        vars["top"] = groups.firstOrNull()?.line ?: "none"
        vars["groups"] = if (groups.isEmpty()) "none" else groups.joinToString("; ") { it.line }
        for (f in features) { vars[f.step] = f.value; for ((name, value) in f.metrics) vars["${f.step}.$name"] = value.toString() }
        var summary = Summary.fill(match?.summary ?: "No rule matched these checks.", vars)
        if (couldNotRun.isNotEmpty()) summary += " ${couldNotRun.size} check${if (couldNotRun.size == 1) "" else "s"} could not run."
        val frameRefs = features.flatMap { it.frames }.distinct().sorted()
        return Finding(verdict, summary, features.filter { it.lean != Lean.UNKNOWN }, couldNotRun, frameRefs, assessmentLabel = match?.label ?: if (match == null) "INSUFFICIENT EVIDENCE / NO MATCHING RULE" else verdict.label)
    }

    fun toJson(): String = FlowJson.write(linkedMapOf(
        "counted" to counted,
        "rules" to rules.map { r -> linkedMapOf("verdict" to r.verdict.label, "summary" to r.summary, "when" to condJson(r.cond), "label" to r.label) },
    ))

    companion object {
        private fun leanOf(s: String): Lean = when (s.trim().lowercase()) {
            "benign" -> Lean.BENIGN; "suspicious" -> Lean.SUSPICIOUS; "unknown" -> Lean.UNKNOWN
            else -> throw VerdictRuleSetError("\"$s\" is not benign, suspicious, or unknown.")
        }
        private fun verdictOf(s: String): Verdict = Verdict.values().firstOrNull { it.label == s.trim() || it.name == s.trim().replace(" ", "_").replace("-", "_") }
            ?: throw VerdictRuleSetError("\"$s\" is not a verdict. Use ${Verdict.values().joinToString(", ") { "\"${it.label}\"" }}.")

        /** The deauth triage, written as rules: the first analytic an operator can read and copy. */
        fun deauthDefault(): VerdictRuleSet {
            val core = listOf("target", "rate", "reason", "seq", "signal")
            fun count(n: Int) = Cond.CountLeans(Lean.SUSPICIOUS, n)
            return VerdictRuleSet(core, listOf(
                VerdictRule(Cond.CountLeans(Lean.UNKNOWN, 5), Verdict.WATCH, "Too many checks lack evidence for a posture assessment.", "INSUFFICIENT EVIDENCE"),
                VerdictRule(
                    Cond.All(listOf(Cond.ValueContains("rate", "1 frame"), Cond.CheckLeans("reason", Lean.BENIGN))),
                    Verdict.NOT_WORRISOME, "One disconnect with an ordinary reason code was observed. Review the surrounding evidence; this does not establish safety."),
                VerdictRule(
                    Cond.All(listOf(count(3), Cond.CheckLeans("after", Lean.SUSPICIOUS),
                        Cond.Any(listOf(Cond.ValueContains("after", "clone"), Cond.ValueContains("after", "handshake"))))),
                    Verdict.ATTACK_WITH_FOLLOW_ON, "Disconnect behavior and follow-on activity warrant review: {after}."),
                VerdictRule(
                    Cond.All(listOf(count(3), Cond.ValueContains("mfp", "required"))),
                    Verdict.LIKELY_ATTACK,
                    "{suspicious} of {counted} checks lean suspicious: an elevated disconnect posture. Management protection is advertised; its enforcement and outcome are unknown."),
                VerdictRule(count(3), Verdict.LIKELY_ATTACK,
                    "{suspicious} of {counted} checks lean suspicious: an elevated disconnect posture. Inspect the supporting observations."),
                VerdictRule(count(1), Verdict.WATCH,
                    "{suspicious} of {counted} checks lean suspicious; review the evidence and follow-on behavior for this target."),
                VerdictRule(Cond.Always, Verdict.NOT_WORRISOME, "No elevated posture was identified by these rules in the supplied evidence. This is not an assessment of safety."),
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
                VerdictRule(cond(m["when"], "rules[$i].when"), verdict, summary, m["label"] as? String)
            }
            return VerdictRuleSet(counted, rules)
        }

        private fun cond(node: Any?, where: String): Cond {
            val m = node as? Map<*, *> ?: throw VerdictRuleSetError("$where must be a test object.")
            fun intOf(k: String): Int {
                val n=m[k] as? Number ?: throw VerdictRuleSetError("$where needs a number $k.")
                if(!n.toDouble().isFinite() || n.toDouble()!=n.toLong().toDouble() || n.toLong() !in 0..Int.MAX_VALUE.toLong()) throw VerdictRuleSetError("$where.$k must be a nonnegative whole number.")
                return n.toInt()
            }
            fun list(k: String) = (m[k] as? List<*>)?.mapIndexed { i, c -> cond(c, "$where.$k[$i]") }
                ?: throw VerdictRuleSetError("$where.$k must be a list of tests.")
            return when {
                m.containsKey("metric") -> Cond.MetricAtLeast(m["step"] as? String ?: throw VerdictRuleSetError("$where needs a step."), m["metric"].toString(), intOf("atLeast").toLong())
                m.containsKey("always") -> if(m["always"]==true) Cond.Always else throw VerdictRuleSetError("$where.always must be true.")
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
            is Cond.MetricAtLeast -> linkedMapOf("step" to c.step, "metric" to c.metric, "atLeast" to c.atLeast)
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
