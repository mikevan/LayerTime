package com.projectrevivesolutions.layertime.controller.flow

/**
 * The workflow wiring file (2026-11-04): what an operator trades. A workflow is a
 * named, versioned list of steps and the edges between them. Every edge carries one
 * declared data type, and the validator refuses a file whose wiring does not hold
 * together, naming the step and the port, before anything runs. Shipped workflows
 * are read-only; copy one to change it.
 *
 * JSON shape:
 *   { "name": "...", "version": 1,
 *     "steps": [ { "id": "event", "type": "event_source", "settings": { "window_ms": 10000 } }, ... ],
 *     "edges": [ { "from": "event", "to": "target.frames", "carries": "frames" }, ... ] }
 *
 * "from" names a step (each step has at most one output). "to" is "stepId.port".
 */

/** What an edge carries. The names are what the file says. */
enum class DataType(val wire: String) {
    EVENT("event"),       // the Hunter's detection
    FRAMES("frames"),     // the 802.11 frames behind it
    FEATURE("feature"),   // one check's answer
    FINDING("finding");   // the verdict with its reasons and evidence

    companion object {
        fun parse(s: String): DataType? = values().firstOrNull { it.wire == s }
    }
}

/** One input a step declares. `many` lets several edges feed it (the verdict's features). */
data class PortSpec(val name: String, val type: DataType, val many: Boolean = false, val optional: Boolean = false)

/** One setting a step declares, with its default. The validator refuses unknown keys and wrong kinds. */
data class SettingSpec(val key: String, val default: Any, val description: String, val min: Long? = null, val max: Long? = null) {
    val kind: String get() = when (default) { is Boolean -> "boolean"; is String -> "text"; else -> "number" }
}

/**
 * What a step type is, for the validator and for the Builder: a plain-language
 * title and description (the operator reads these), its inputs, its output, and
 * its settings. The code that runs a step is registered separately (StepRunner),
 * so the wiring can be checked, shown, and traded without running anything.
 */
data class StepSpec(
    val type: String,
    val title: String,
    val description: String,
    val inputs: List<PortSpec> = emptyList(),
    val output: DataType? = null,
    val settings: List<SettingSpec> = emptyList(),
    /** The Tools page tool this step is ("wireshark", "attack_timeline"), or null for a step that only passes field values. */
    val tool: String? = null,
) {
    fun input(name: String): PortSpec? = inputs.firstOrNull { it.name == name }
    fun setting(key: String): SettingSpec? = settings.firstOrNull { it.key == key }
}

data class Step(val id: String, val type: String, val settings: Map<String, Any?> = emptyMap())

data class Edge(val from: String, val toStep: String, val toPort: String, val carries: DataType) {
    val to: String get() = "$toStep.$toPort"
}

data class Workflow(val name: String, val version: Int, val steps: List<Step>, val edges: List<Edge>) {
    fun step(id: String): Step? = steps.firstOrNull { it.id == id }
    fun edgesInto(stepId: String): List<Edge> = edges.filter { it.toStep == stepId }
    fun edgesFrom(stepId: String): List<Edge> = edges.filter { it.from == stepId }
}

/** One thing wrong with a wiring file. `step` and `port` are null when the problem is file-wide. */
data class Problem(val step: String?, val port: String?, val message: String) {
    override fun toString(): String = buildString {
        if (step != null) { append(step); if (port != null) append('.').append(port); append(": ") }
        append(message)
    }
}

class WorkflowFileError(message: String) : Exception(message)

/** Reads the file's structure. Shape errors throw; wiring errors are the validator's job. */
object WorkflowLoader {
    fun parse(json: String): Workflow {
        val root = try { FlowJson.parse(json) } catch (e: FlowJsonError) { throw WorkflowFileError("not valid JSON: ${e.message}") }
        val o = root as? Map<*, *> ?: throw WorkflowFileError("the file must be a JSON object")
        val name = o["name"] as? String ?: throw WorkflowFileError("\"name\" is missing")
        val version = (o["version"] as? Number)?.toInt() ?: throw WorkflowFileError("\"version\" is missing")
        val steps = (o["steps"] as? List<*> ?: throw WorkflowFileError("\"steps\" is missing")).mapIndexed { i, s ->
            val m = s as? Map<*, *> ?: throw WorkflowFileError("steps[$i] must be an object")
            val id = m["id"] as? String ?: throw WorkflowFileError("steps[$i] has no \"id\"")
            val type = m["type"] as? String ?: throw WorkflowFileError("step \"$id\" has no \"type\"")
            val settings = (m["settings"] as? Map<*, *>)?.entries?.associate { (k, v) -> k.toString() to v } ?: emptyMap()
            Step(id, type, settings)
        }
        val edges = (o["edges"] as? List<*> ?: throw WorkflowFileError("\"edges\" is missing")).mapIndexed { i, e ->
            val m = e as? Map<*, *> ?: throw WorkflowFileError("edges[$i] must be an object")
            val from = m["from"] as? String ?: throw WorkflowFileError("edges[$i] has no \"from\"")
            val to = m["to"] as? String ?: throw WorkflowFileError("edges[$i] has no \"to\"")
            val carriesText = m["carries"] as? String ?: throw WorkflowFileError("edge $from -> $to has no \"carries\"")
            val carries = DataType.parse(carriesText)
                ?: throw WorkflowFileError("edge $from -> $to carries \"$carriesText\", which is not a data type")
            val dot = to.indexOf('.')
            if (dot <= 0 || dot == to.length - 1) throw WorkflowFileError("edge $from -> $to: \"to\" must be \"step.port\"")
            Edge(from, to.substring(0, dot), to.substring(dot + 1), carries)
        }
        return Workflow(name, version, steps, edges)
    }

    fun write(w: Workflow): String = FlowJson.write(linkedMapOf(
        "name" to w.name, "version" to w.version.toLong(),
        "steps" to w.steps.map { s ->
            linkedMapOf<String, Any?>("id" to s.id, "type" to s.type).also { if (s.settings.isNotEmpty()) it["settings"] = s.settings }
        },
        "edges" to w.edges.map { e -> linkedMapOf("from" to e.from, "to" to e.to, "carries" to e.carries.wire) },
    ))
}

/**
 * Checks that a wiring holds together. An empty result means it runs. Each problem
 * names the step and port it is about, in words an operator can act on.
 */
object WorkflowValidator {
    fun validate(w: Workflow, registry: StepRegistry = StepRegistry): List<Problem> {
        val problems = ArrayList<Problem>()
        if (w.name.isBlank()) problems += Problem(null, null, "The workflow has no name.")
        if (w.version < 1) problems += Problem(null, null, "The version must be 1 or higher.")
        if (w.steps.isEmpty()) problems += Problem(null, null, "The workflow has no steps.")

        // Steps: unique ids, known types, settings that the type declares and of the right kind.
        val seen = HashSet<String>()
        for (s in w.steps) {
            if (s.id.isBlank() || s.id.any { it == '.' || it.isWhitespace() }) problems += Problem(s.id, null, "Step ids must be one word with no dot.")
            if (!seen.add(s.id)) problems += Problem(s.id, null, "Two steps share this id.")
            val spec = registry.spec(s.type)
            if (spec == null) { problems += Problem(s.id, null, "This app has no step type \"${s.type}\"."); continue }
            for ((k, v) in s.settings) {
                val ss = spec.setting(k)
                if (ss == null) { problems += Problem(s.id, null, "\"${spec.title}\" has no setting \"$k\"."); continue }
                val ok = when (ss.default) { is Boolean -> v is Boolean; is String -> v is String; else -> v is Number }
                if (v is Number && (v.toDouble().isNaN() || v.toDouble().isInfinite() || v.toDouble() != v.toLong().toDouble() || (ss.min != null && v.toLong() < ss.min) || (ss.max != null && v.toLong() > ss.max)))
                    problems += Problem(s.id, null, "Setting $k must be a whole number within ${ss.min ?: Long.MIN_VALUE}..${ss.max ?: Long.MAX_VALUE}.")
                if (k in listOf("target_mac", "network_mac") && v is String && v.isNotBlank() && TargetBehavior.address(v) == null)
                    problems += Problem(s.id, null, "Setting $k must be a MAC address or empty.")
                if (!ok) problems += Problem(s.id, null, "Setting \"$k\" must be a ${ss.kind}.")
            }
        }
        if (problems.isNotEmpty()) return problems // the rest assumes the steps exist

        // Edges: both ends exist, the port is declared, and the type matches at both ends.
        val fed = HashMap<Pair<String, String>, Int>()
        for (e in w.edges) {
            val from = w.step(e.from)
            val to = w.step(e.toStep)
            if (from == null) { problems += Problem(e.toStep, e.toPort, "The edge comes from \"${e.from}\", which is not a step."); continue }
            if (to == null) { problems += Problem(e.from, null, "The edge goes to \"${e.toStep}\", which is not a step."); continue }
            val fromSpec = registry.spec(from.type)!!
            val toSpec = registry.spec(to.type)!!
            val out = fromSpec.output
            if (out == null) { problems += Problem(e.from, null, "\"${fromSpec.title}\" produces nothing, so nothing can come from it."); continue }
            val port = toSpec.input(e.toPort)
            if (port == null) { problems += Problem(e.toStep, e.toPort, "\"${toSpec.title}\" has no input \"${e.toPort}\"."); continue }
            if (e.carries != out) problems += Problem(e.from, null,
                "The edge says it carries ${e.carries.wire}, but \"${fromSpec.title}\" produces ${out.wire}.")
            if (e.carries != port.type) problems += Problem(e.toStep, e.toPort,
                "This input takes ${port.type.wire}, but the edge carries ${e.carries.wire}.")
            fed[e.toStep to e.toPort] = (fed[e.toStep to e.toPort] ?: 0) + 1
        }

        // Every input is fed: exactly once, or at least once for a many-input.
        for (s in w.steps) {
            val spec = registry.spec(s.type)!!
            for (p in spec.inputs) {
                val n = fed[s.id to p.name] ?: 0
                if (n == 0 && !p.optional) problems += Problem(s.id, p.name, "Nothing feeds this input; \"${spec.title}\" needs ${p.type.wire} here.")
                else if (n > 1 && !p.many) problems += Problem(s.id, p.name, "$n edges feed this input; it takes one.")
            }
        }

        for(step in w.steps.filter { it.type=="deauth_verdict" }) {
            val text=step.settings["rules"] as? String ?: continue
            if(text.isBlank()) continue
            try {
                val rules=VerdictRuleSet.parse(text)
                if(rules.needsFrames() && w.edgesInto(step.id).none {it.toPort=="frames"})
                    problems+=Problem(step.id,"frames","A frame-count condition needs frames connected to this report.")
                val fedIds=w.edgesInto(step.id).filter { it.toPort=="features" }.map { it.from }.toSet()
                for(ref in rules.references()-fedIds) problems+=Problem(step.id,"rules","Rule refers to $ref, but that block does not feed this report.")
                val firstAlways=rules.rules.indexOfFirst { it.cond==Cond.Always }
                if(firstAlways>=0 && firstAlways<rules.rules.lastIndex) problems+=Problem(step.id,"rules","Rules after an unconditional rule can never run. Move it to the end.")
            } catch(e:VerdictRuleSetError) { problems+=Problem(step.id,"rules",e.message ?: "Invalid report rules.") }
        }

        // No cycles: a run order must exist.
        if (order(w) == null) problems += Problem(null, null, "The wiring loops back on itself, so there is no order to run it in.")
        return problems
    }

    /** The run order (inputs before the steps they feed), or null when the wiring has a cycle. */
    fun order(w: Workflow): List<String>? {
        val ids = w.steps.map { it.id }
        val indeg = ids.associateWith { id -> w.edgesInto(id).count { it.from in ids }.let { n -> n } }.toMutableMap()
        val ready = ArrayDeque(ids.filter { indeg[it] == 0 })
        val out = ArrayList<String>()
        while (ready.isNotEmpty()) {
            val id = ready.removeFirst()
            out += id
            for (e in w.edgesFrom(id)) if (e.toStep in indeg) {
                indeg[e.toStep] = indeg[e.toStep]!! - 1
                if (indeg[e.toStep] == 0) ready += e.toStep
            }
        }
        return if (out.size == ids.size) out else null
    }
}
