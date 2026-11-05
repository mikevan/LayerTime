package com.projectrevivesolutions.layertime.controller.ui

/** Canvas navigation in pixels. Zoom keeps the world point under the gesture stationary. */
data class BuilderViewport(val zoom: Float = 1f, val x: Float = 0f, val y: Float = 0f) {
    fun transform(
        factor: Float, anchorX: Float, anchorY: Float, panX: Float, panY: Float,
        width: Float, height: Float, worldWidth: Float, worldHeight: Float,
    ): BuilderViewport {
        val next = (zoom * factor).coerceIn(MIN_ZOOM, MAX_ZOOM)
        val ratio = next / zoom
        return BuilderViewport(next,
            anchorX - (anchorX - x) * ratio + panX,
            anchorY - (anchorY - y) * ratio + panY,
        ).bounded(width, height, worldWidth, worldHeight)
    }

    fun bounded(width: Float, height: Float, worldWidth: Float, worldHeight: Float): BuilderViewport {
        fun axis(offset: Float, viewport: Float, world: Float): Float =
            if (world * zoom <= viewport) (viewport - world * zoom) / 2f
            else offset.coerceIn(viewport - world * zoom, 0f)
        return copy(x = axis(x, width, worldWidth), y = axis(y, height, worldHeight))
    }

    companion object {
        const val MIN_ZOOM = .025f
        const val MAX_ZOOM = 4f
        fun centered(zoom: Float, width: Float, height: Float, worldWidth: Float, worldHeight: Float): BuilderViewport {
            val scale = zoom.coerceIn(MIN_ZOOM, MAX_ZOOM)
            return BuilderViewport(scale, (width - worldWidth * scale) / 2f,
                (height - worldHeight * scale) / 2f)
        }
        fun fit(width: Float, height: Float, worldWidth: Float, worldHeight: Float): BuilderViewport =
            centered(minOf(width / worldWidth, height / worldHeight) * .92f, width, height, worldWidth, worldHeight)
    }
}
