package com.projectrevivesolutions.layertime.controller.ai.runtime

import com.projectrevivesolutions.layertime.controller.ai.*

/** Pinned to the Qwen3 template used by our training exporter. No phone-vendor model is involved. */
object LocalPromptFormat {
    const val ID = "qwen3-chatml-no-thinking-v1"
    const val INSTRUCT_2507 = "qwen3-instruct-2507-chatml-v1"
    const val COMPACT_2507 = "qwen3-instruct-2507-compact-v1"
    fun supports(id: String) = id == ID || id == INSTRUCT_2507 || id == COMPACT_2507
    fun render(request: LocalModelRequest, format: String = ID): String {
        require(supports(format)) { "Unsupported local model prompt format." }
        require(request.tools.isEmpty()) { "This runtime revision does not yet support tool-call decoding." }
        require(request.messages.none {it.role==PromptMessage.Role.TOOL}) { "Tool turns require a tool-capable model package." }
        fun data(s: String) = s.replace("<|", "< |")
        val compact = format == COMPACT_2507
        if (compact) require(request.messages.firstOrNull()?.let {
            it.role == PromptMessage.Role.SYSTEM && it.content.contains(request.outputSchema)
        } == true) { "Compact domain requests must contain their complete schema in the supplied system message." }
        return buildString {
            if (!compact) {
                append("<|im_start|>system\nReturn only a JSON object matching this schema: ")
                append(data(request.outputSchema));append("<|im_end|>\n")
            }
            request.messages.forEach { message ->
                append("<|im_start|>");append(message.role.name.lowercase());append('\n')
                append(data(message.content));append("<|im_end|>\n")
            }
            append("<|im_start|>assistant\n")
            if(format == ID)append("<think>\n\n</think>\n\n")
        }
    }
}
