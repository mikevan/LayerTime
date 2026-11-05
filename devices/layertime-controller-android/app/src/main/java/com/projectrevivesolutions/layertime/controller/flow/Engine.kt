package com.projectrevivesolutions.layertime.controller.flow

/**
 * Runs a validated workflow (2026-11-04). The engine knows nothing about deauths:
 * it orders the steps from the wiring, hands each runner its inputs by port name
 * and its settings with defaults filled in, and keeps every step's output so the
 * Builder can light the steps up and a finding can point back at its evidence.
 */
class RunContext(val event: DetectionEvent, val source: FrameSource)

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
            val out = try { runner.run(inputs, settings, ctx) } catch (e: Exception) {
                results += StepResult(id, step.type, null, e.message ?: e.toString())
                return RunResult(w, results, null)
            }
            outputs[id] = out
            results += StepResult(id, step.type, out, null)
        }
        val finding = outputs.values.filterIsInstance<Finding>().lastOrNull()
        return RunResult(w, results, finding)
    }
}

/** Settings helpers for runners. */
internal fun Map<String, Any>.long(key: String): Long = (this[key] as Number).toLong()
internal fun Map<String, Any>.int(key: String): Int = (this[key] as Number).toInt()
