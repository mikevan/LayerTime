package com.projectrevivesolutions.layertime.controller.flow

/**
 * Where each step sits on the Builder canvas (2026-11-04): columns by depth from
 * the sources, rows in wiring order, left to right. Pure, so it is host-tested and
 * the same picture appears on every screen size. Units are canvas dp.
 */
data class NodeBox(val step: String, val column: Int, val row: Int, val x: Float, val y: Float, val w: Float, val h: Float) {
    val cx get() = x + w / 2
    val cy get() = y + h / 2
    val right get() = x + w
    fun contains(px: Float, py: Float) = px >= x && px <= x + w && py >= y && py <= y + h
}

data class FlowLayout(val nodes: List<NodeBox>, val width: Float, val height: Float) {
    fun node(step: String) = nodes.firstOrNull { it.step == step }
    fun hit(px: Float, py: Float) = nodes.firstOrNull { it.contains(px, py) }

    companion object {
        const val NODE_W = 168f
        const val NODE_H = 60f
        const val COL_GAP = 56f
        const val ROW_GAP = 22f

        fun of(w: Workflow): FlowLayout {
            val order = WorkflowValidator.order(w) ?: w.steps.map { it.id }
            // Depth = longest path from a source, so a check sits one column past the frames it reads.
            val depth = HashMap<String, Int>()
            for (id in order) depth[id] = (w.edgesInto(id).mapNotNull { depth[it.from] }.maxOrNull() ?: -1) + 1
            val columns = order.groupBy { depth[it]!! }.toSortedMap()
            val tallest = columns.values.maxOfOrNull { it.size } ?: 0
            val height = tallest * NODE_H + (tallest - 1).coerceAtLeast(0) * ROW_GAP
            val nodes = ArrayList<NodeBox>()
            for ((c, ids) in columns) {
                val colHeight = ids.size * NODE_H + (ids.size - 1) * ROW_GAP
                val top = (height - colHeight) / 2
                ids.forEachIndexed { r, id ->
                    nodes += NodeBox(id, c, r, c * (NODE_W + COL_GAP), top + r * (NODE_H + ROW_GAP), NODE_W, NODE_H)
                }
            }
            val width = columns.size * NODE_W + (columns.size - 1).coerceAtLeast(0) * COL_GAP
            return FlowLayout(nodes, width, height)
        }
    }
}
