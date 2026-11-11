package com.projectrevivesolutions.layertime.controller.flow

/** Authoring context travels with the shared analytic, never with the recording itself. */
data class AnalyticOrigin(val prompt: String, val modelId: String, val modelVersion: String) {
    init {
        require(prompt.isNotBlank() && prompt.length <= 8000)
        require(modelId.isNotBlank() && modelId.length <= 120)
        require(modelVersion.isNotBlank() && modelVersion.length <= 120)
    }
    fun wire(): Map<String, Any> = linkedMapOf("schemaVersion" to 1L, "prompt" to prompt,
        "modelId" to modelId, "modelVersion" to modelVersion)

    companion object {
        fun fromJson(json: String): AnalyticOrigin? {
            val root = FlowJson.parse(json) as? Map<*, *> ?: return null
            val raw = root["authoring"] ?: return null
            val o = raw as? Map<*, *> ?: error("Invalid analytic authoring context.")
            require(o["schemaVersion"] == 1L) { "Unsupported authoring context version." }
            return AnalyticOrigin(o["prompt"] as? String ?: error("Missing authoring prompt."),
                o["modelId"] as? String ?: error("Missing authoring model."),
                o["modelVersion"] as? String ?: error("Missing authoring model version."))
        }
    }
}
