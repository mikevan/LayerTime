package com.projectrevivesolutions.layertime.controller.flow

/**
 * Editing a workflow in the Builder (Michael, 2026-11-06). The goal of this first editor:
 * rebuild the deauth triage by hand from the palette and get the same verdicts.
 *
 * A Draft is a workflow plus where each block sits on the workspace (canvas dp). Every
 * edit returns a new Draft; nothing here touches Android, so it is host-tested. Shipped
 * workflows are read-only: [copyOf] makes the operator's own copy, which is what gets
 * edited and saved. Positions travel in the saved file under "layout", which the loader
 * and validator ignore, so a traded file still opens anywhere.
 */
data class Draft(
    val workflow: Workflow,
    val positions: Map<String, Pair<Float, Float>>,
    val shipped: Boolean,
    val authoring: AnalyticOrigin? = null,
) {
    val editable: Boolean get() = !shipped
    fun at(id: String): Pair<Float, Float>? = positions[id]
}

/** The answer to a connect: the new draft, or why it was refused, in a sentence. */
sealed interface ConnectResult {
    data class Done(val draft: Draft) : ConnectResult
    data class Refused(val sentence: String) : ConnectResult
}

/** How a workflow did on the deauth test scenarios. */
data class ScenarioCheck(val passed: Int, val total: Int, val failures: List<String>) {
    val sentence: String get() = when {
        total == 0 -> "There are no test scenarios to check against."
        failures.isEmpty() -> "Passes all $total test scenarios."
        else -> "Passes $passed of $total test scenarios. " + failures.joinToString(" ")
    }
}

object DraftEdits {
    /** An empty workflow of the operator's own. */
    fun blank(name: String = "My workflow"): Draft = Draft(Workflow(name, 1, emptyList(), emptyList()), emptyMap(), shipped = false)

    /** A shipped or loaded workflow as a draft, with its saved layout or the automatic one. */
    fun open(w: Workflow, layout: Map<String, Pair<Float, Float>>?, shipped: Boolean): Draft {
        val auto = FlowLayout.of(w).nodes.associate { it.step to (it.x to it.y) }
        val pos = w.steps.associate { s -> s.id to (layout?.get(s.id) ?: auto[s.id] ?: (0f to 0f)) }
        return Draft(w, pos, shipped)
    }

    /** The operator's own copy of a workflow: editable, named "<name> copy", version 1. */
    fun copyOf(d: Draft): Draft = d.copy(workflow = d.workflow.copy(name = d.workflow.name + " copy", version = 1), shipped = false)

    /** A short, unique, one-word id for a new step of [type]: "rate", "rate2", ... */
    fun newId(w: Workflow, type: String): String {
        val base = when (type) {
            "wireshark_check" -> "check"; "event_source" -> "event"; "deauth_target" -> "target"; "deauth_rate" -> "rate"
            "deauth_reason" -> "reason"; "seq_integrity" -> "seq"; "signal_match" -> "signal"; "aftermath" -> "after"
            "mgmt_protection" -> "mfp"; "deauth_verdict" -> "verdict"
            else -> type.filter { it.isLetterOrDigit() || it == '_' }.ifEmpty { "step" }
        }
        if (w.step(base) == null) return base
        var n = 2
        while (w.step("$base$n") != null) n++
        return "$base$n"
    }

    /** Adds a step of [type] at (x, y), with its declared defaults as settings. */
    fun add(d: Draft, type: String, x: Float, y: Float, registry: StepRegistry = StepRegistry): Pair<Draft, String>? {
        if (!d.editable) return null
        val spec = registry.spec(type) ?: return null
        val id = newId(d.workflow, type)
        val settings = spec.settings.associate { it.key to it.default }
        val w = d.workflow.copy(steps = d.workflow.steps + Step(id, type, settings))
        return d.copy(workflow = w, positions = d.positions + (id to (x to y))) to id
    }

    /**
     * Sets a step's type to [newType] (how the Wireshark menu turns a placeholder into a chosen
     * check). The id and position stay, settings reset to the new type's defaults, and any edge
     * the new type's ports or output no longer accept is dropped so the flow stays honest.
     */
    fun assign(d: Draft, id: String, newType: String, registry: StepRegistry = StepRegistry): Draft {
        if (!d.editable) return d
        val spec = registry.spec(newType) ?: return d
        val w = d.workflow
        val step = w.step(id) ?: return d
        val settings = spec.settings.associate { it.key to it.default }
        val edges = w.edges.filter { e ->
            when (id) {
                e.from -> spec.output != null && e.carries == spec.output
                e.toStep -> spec.input(e.toPort)?.type == e.carries
                else -> true
            }
        }
        return d.copy(workflow = w.copy(steps = w.steps.map { if (it.id == id) it.copy(type = newType, settings = settings) else it }, edges = edges))
    }

    fun move(d: Draft, id: String, x: Float, y: Float): Draft =
        if (!d.editable || d.workflow.step(id) == null) d else d.copy(positions = d.positions + (id to (x to y)))

    /** Removes a step and every connection to or from it. */
    fun remove(d: Draft, id: String): Draft {
        if (!d.editable) return d
        val w = d.workflow
        return d.copy(
            workflow = w.copy(steps = w.steps.filter { it.id != id }, edges = w.edges.filter { it.from != id && it.toStep != id }),
            positions = d.positions - id,
        )
    }

    /**
     * Connects [from]'s output to [toStep]'s input [toPort]. Refused, with a sentence, when the
     * types differ, when it would loop, or when it connects a step to itself. An input that takes
     * one connection and already has one is re-pointed to the new one.
     */
    fun connect(d: Draft, from: String, toStep: String, toPort: String, registry: StepRegistry = StepRegistry): ConnectResult {
        if (!d.editable) return ConnectResult.Refused("This workflow is read-only. Tap Copy to edit your own copy.")
        val w = d.workflow
        val a = w.step(from)?.let { registry.spec(it.type) } ?: return ConnectResult.Refused("That step is gone.")
        val b = w.step(toStep)?.let { registry.spec(it.type) } ?: return ConnectResult.Refused("That step is gone.")
        if (from == toStep) return ConnectResult.Refused("A step cannot feed itself.")
        val out = a.output ?: return ConnectResult.Refused("\"${a.title}\" produces nothing to connect.")
        val port = b.input(toPort) ?: return ConnectResult.Refused("\"${b.title}\" has no input \"$toPort\".")
        if (out != port.type)
            return ConnectResult.Refused("\"${b.title}\" takes ${port.type.wire} here, but \"${a.title}\" produces ${out.wire}.")
        if (w.edges.any { it.from == from && it.toStep == toStep && it.toPort == toPort }) return ConnectResult.Done(d)
        val kept = if (port.many) w.edges else w.edges.filter { !(it.toStep == toStep && it.toPort == toPort) }
        val next = w.copy(edges = kept + Edge(from, toStep, toPort, out))
        if (WorkflowValidator.order(next) == null) return ConnectResult.Refused("That connection would loop back on itself.")
        return ConnectResult.Done(d.copy(workflow = next))
    }

    fun disconnect(d: Draft, edge: Edge): Draft =
        if (!d.editable) d else d.copy(workflow = d.workflow.copy(edges = d.workflow.edges.filter { it != edge }))

    fun setSetting(d: Draft, id: String, key: String, value: Any): Draft {
        if (!d.editable) return d
        val w = d.workflow
        return d.copy(workflow = w.copy(steps = w.steps.map { if (it.id == id) it.copy(settings = it.settings + (key to value)) else it }))
    }

    /** The validator's problems as sentences an operator can act on, naming steps by their titles. */
    fun sentences(w: Workflow, registry: StepRegistry = StepRegistry): List<String> =
        WorkflowValidator.validate(w, registry).map { p ->
            val title = p.step?.let { id -> w.step(id)?.let { registry.spec(it.type)?.title } ?: id }
            val text = p.message.trimEnd().let { if (it.endsWith(".")) it else "$it." }
            if (title != null) "$title: $text" else text
        }

    /** Runs [w] on every deauth test scenario and compares each verdict with what it was built to mean. */
    fun checkScenarios(w: Workflow, scenarios: List<Fixtures.Scenario> = Fixtures.all()): ScenarioCheck {
        DeauthSteps.register() // the runners; registering again is harmless
        if (WorkflowValidator.validate(w).isNotEmpty()) return ScenarioCheck(0, scenarios.size, listOf("Fix the problems listed first."))
        val failures = ArrayList<String>()
        for (s in scenarios) {
            val got = try { Engine.run(w, RunContext(s.event, FrameSource { s.frames })).finding?.verdict } catch (e: Exception) { null }
            if (got != s.expected) failures += "${s.name.replace('_', ' ')}: expected ${s.expected.label}, got ${got?.label ?: "no verdict"}."
        }
        return ScenarioCheck(scenarios.size - failures.size, scenarios.size, failures)
    }

    /** The saved file: the wiring plus "layout", the block positions. */
    fun write(d: Draft): String {
        val base = WorkflowLoader.write(d.workflow)
        val layout = d.workflow.steps.mapNotNull { s -> d.positions[s.id]?.let { (x, y) -> s.id to listOf(x.toDouble(), y.toDouble()) } }.toMap()
        val root = (FlowJson.parse(base) as Map<*, *>).entries.associateTo(LinkedHashMap<String, Any?>()) { (k, v) -> k.toString() to v }
        root["layout"] = layout
        d.authoring?.let { root["authoring"] = it.wire() }
        return FlowJson.write(root)
    }

    /** The layout a saved file carries, or null when it has none. */
    fun layoutOf(json: String): Map<String, Pair<Float, Float>>? {
        val root = runCatching { FlowJson.parse(json) as? Map<*, *> }.getOrNull() ?: return null
        val l = root["layout"] as? Map<*, *> ?: return null
        return l.entries.mapNotNull { (k, v) ->
            val xy = v as? List<*> ?: return@mapNotNull null
            val x = (xy.getOrNull(0) as? Number)?.toFloat(); val y = (xy.getOrNull(1) as? Number)?.toFloat()
            if (x == null || y == null) null else k.toString() to (x to y)
        }.toMap()
    }
}
