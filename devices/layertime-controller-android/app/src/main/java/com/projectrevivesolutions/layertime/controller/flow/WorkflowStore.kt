package com.projectrevivesolutions.layertime.controller.flow

import android.content.Context
import java.io.File

/**
 * The flows LayerScope knows (Michael, 2026-11-06): the shipped ones in the app's assets
 * (read-only) and the operator's own, saved in the app's storage under workflows/. A flow
 * is accepted when the validator finds nothing wrong with its wiring; only an accepted
 * flow can run. Each save of the same name gets the next version number.
 */
object WorkflowStore {
    data class Entry(
        val name: String,
        val version: Int,
        val shipped: Boolean,
        val file: String,            // asset path for a shipped flow, file path for a saved one
        val problems: List<String>,  // sentences; empty means accepted
    ) {
        val accepted: Boolean get() = problems.isEmpty()
    }

    private const val SHIPPED_DIR = "workflows"
    private fun savedDir(ctx: Context) = File(ctx.filesDir, "workflows").apply { mkdirs() }

    /** A file name for a flow name: letters, digits, and dashes. */
    fun slug(name: String): String = name.lowercase().map { if (it.isLetterOrDigit()) it else '-' }.joinToString("")
        .replace(Regex("-+"), "-").trim('-').ifEmpty { "workflow" }

    fun list(ctx: Context): List<Entry> {
        DeauthSteps.register()
        val out = ArrayList<Entry>()
        val shipped = runCatching { ctx.assets.list(SHIPPED_DIR)?.toList() }.getOrNull().orEmpty().filter { it.endsWith(".json") }
        for (f in shipped.sorted()) entry(runCatching { ctx.assets.open("$SHIPPED_DIR/$f").bufferedReader().use { it.readText() } }.getOrNull(),
            "$SHIPPED_DIR/$f", true)?.let { out += it }
        for (f in (savedDir(ctx).listFiles() ?: emptyArray()).filter { it.isFile && it.name.endsWith(".json") }.sortedBy { it.name })
            entry(runCatching { f.readText() }.getOrNull(), f.path, false)?.let { out += it }
        return out
    }

    private fun entry(text: String?, file: String, shipped: Boolean): Entry? {
        if (text == null) return null
        return try {
            val w = WorkflowLoader.parse(text)
            Entry(w.name, w.version, shipped, file, DraftEdits.sentences(w))
        } catch (e: WorkflowFileError) {
            Entry(File(file).nameWithoutExtension, 0, shipped, file, listOf("The file cannot be read: ${e.message}."))
        }
    }

    /** Opens a listed flow as a draft (shipped ones read-only), or null when it cannot be read. */
    fun open(ctx: Context, e: Entry): Draft? {
        val text = runCatching { if (e.shipped) ctx.assets.open(e.file).bufferedReader().use { it.readText() } else File(e.file).readText() }.getOrNull() ?: return null
        val w = runCatching { WorkflowLoader.parse(text) }.getOrNull() ?: return null
        return DraftEdits.open(w, DraftEdits.layoutOf(text), e.shipped)
    }

    /** Saves the operator's draft as the next version of its name. Returns the saved draft (with its new version). */
    fun save(ctx: Context, d: Draft): Draft {
        val f = File(savedDir(ctx), slug(d.workflow.name) + ".json")
        val previous = runCatching { WorkflowLoader.parse(f.readText()).version }.getOrNull() ?: 0
        val saved = d.copy(workflow = d.workflow.copy(version = previous + 1))
        val tmp = File(f.path + ".part")
        tmp.writeText(DraftEdits.write(saved))
        if (!tmp.renameTo(f)) { f.delete(); tmp.renameTo(f) }
        return saved
    }
}
