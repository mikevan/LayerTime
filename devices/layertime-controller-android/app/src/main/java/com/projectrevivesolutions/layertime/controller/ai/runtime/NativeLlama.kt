package com.projectrevivesolutions.layertime.controller.ai.runtime

/** All engine-specific symbols stay behind LocalModelProvider. UTF-8 crosses JNI as bytes. */
internal object NativeLlama {
    val loaded: Boolean by lazy { runCatching { System.loadLibrary("ltai") }.isSuccess }
    external fun load(path: String): Long
    external fun countTokens(handle: Long, prompt: ByteArray): Int
    external fun generate(handle: Long, request: Long, prompt: ByteArray, grammar: ByteArray,
                          maxTokens: Int, contextTokens: Int, threads: Int): ByteArray
    external fun cancel(handle: Long, request: Long)
    external fun close(handle: Long)
}
