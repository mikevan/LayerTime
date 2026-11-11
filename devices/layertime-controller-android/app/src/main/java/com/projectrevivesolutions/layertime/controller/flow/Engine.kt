package com.projectrevivesolutions.layertime.controller.flow

/**
 * Runs a validated workflow (2026-11-04). The engine knows nothing about deauths:
 * it orders the steps from the wiring, hands each runner its inputs by port name
 * and its settings with defaults filled in, and keeps every step's output so the
 * Builder can light the steps up and a finding can point back at its evidence.
 */
class RunContext(val event: DetectionEvent, val source: FrameSource, val coverage: EvidenceCoverage? = null)

/** What one step type does when it runs. Registered by type name, beside the StepSpec. */
fun interface StepRunner {
    fun run(inputs: Map<String, List<Any>>, settings: Map<String, Any>, ctx: RunContext): Any
}

data class StepResult(val step: String, val type: String, val output: Any?, val error: String?)

data class RunResult(val workflow: Workflow, val steps: List<StepResult>, val finding: Finding?) {
    fun output(stepId: String): Any? = steps.firstOrNull { it.step == stepId }?.output
}

object Engine {
    private val runners = HashMap<String, StepRunner>()
    fun register(type: String, runner: StepRunner) { runners[type] = runner }
    fun runner(type: String): StepRunner? = runners[type]

    /** Fails fast on a wiring problem; a step that throws stops the run with its error recorded. */
    fun run(w: Workflow, ctx: RunContext, registry: StepRegistry = StepRegistry): RunResult {
        val problems = WorkflowValidator.validate(w, registry)
        if (problems.isNotEmpty()) throw WorkflowFileError("the workflow does not validate: " + problems.joinToString("; "))
        val order = WorkflowValidator.order(w)!!
        val outputs = HashMap<String, Any>()
        val results = ArrayList<StepResult>()
        for (id in order) {
            val step = w.step(id)!!
            val spec = registry.spec(step.type)!!
            val runner = runners[step.type]
                ?: return RunResult(w, results + StepResult(id, step.type, null, "This build cannot run \"${spec.title}\"."), null)
            val inputs = HashMap<String, List<Any>>()
            for (p in spec.inputs) inputs[p.name] = w.edgesInto(id).filter { it.toPort == p.name }.mapNotNull { outputs[it.from] }
            val settings = LinkedHashMap<String, Any>()
            for (s in spec.settings) settings[s.key] = step.settings[s.key] ?: s.default
            if(step.type=="event_source") {
                val reachable=HashSet<String>()
                fun visit(node:String) { if(reachable.add(node)) w.edgesFrom(node).forEach { visit(it.toStep) } }
                visit(id)
                val horizon=reachable.mapNotNull { node -> w.step(node) }.maxOfOrNull { downstream ->
                    val ds=registry.spec(downstream.type)!!
                    listOf("watch_ms","follow_on_ms").maxOf { key ->
                        ((downstream.settings[key] ?: ds.setting(key)?.default) as? Number)?.toLong() ?: 0L
                    }
                } ?: 0L
                settings["after_ms"]=maxOf((settings["after_ms"] as? Number)?.toLong() ?: 30000L,horizon)
            }
            val out = try { runner.run(inputs, settings, ctx) } catch (e: Exception) {
                results += StepResult(id, step.type, null, e.message ?: e.toString())
                return RunResult(w, results, null)
            }
            val identified = if (out is Feature) out.copy(step = id, checkType = step.type) else out
            outputs[id] = identified
            results += StepResult(id, step.type, identified, null)
        }
        val finding = results.mapNotNull { it.output as? Finding }.lastOrNull()
        return RunResult(w, results, finding?.copy(coverage = ctx.coverage))
    }
}

/** Settings helpers for runners. */
internal fun Map<String, Any>.long(key: String): Long = (this[key] as Number).toLong()
internal fun Map<String, Any>.int(key: String): Int = (this[key] as Number).toInt()
