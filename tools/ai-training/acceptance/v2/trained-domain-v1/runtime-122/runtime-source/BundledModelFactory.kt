package com.projectrevivesolutions.layertime.controller.ai.runtime

import android.content.Context
import com.projectrevivesolutions.layertime.controller.BuildConfig
import com.projectrevivesolutions.layertime.controller.ai.*
import com.projectrevivesolutions.layertime.controller.flow.FlowJson
import java.io.File
import java.security.MessageDigest

/** The APK signature authenticates the bundled manifest. Its hash authenticates the copied weights. */
object BundledModelFactory {
    private var productionModel:LocalModelProvider?=null
    @Synchronized fun production(context: Context): LocalModelProvider = productionModel
        ?: read(context.applicationContext,"ai/production",false).also {productionModel=it}
    fun development(context: Context): LocalModelProvider {
        check(BuildConfig.DEBUG) {"Development models are unavailable in release builds."}
        return read(context.applicationContext,"ai/development",true)
    }

    private fun read(context: Context,assetDir:String,development:Boolean):LocalModelProvider {
        val raw=runCatching {context.assets.open("$assetDir/package.json").bufferedReader().use {it.readText()}}.getOrNull()
            ?: return UninstalledLocalModel
        return runCatching {
            if(BuildConfig.LT_ARM_DOTPROD_EXPERIMENT) {
                val features=runCatching {File("/proc/cpuinfo").readText()}.getOrDefault("")
                require(features.contains("asimddp") && features.contains("asimdhp")) {
                    "This experimental inference build requires ARM dot-product and FP16 CPU support."
                }
            }
            require(raw.length<=32000)
            val data=FlowJson.parse(raw) as? Map<*,*> ?: error("Invalid model manifest.")
            val promptFormat=data["promptFormat"] as? String ?: error("Missing prompt format.")
            require(data["schemaVersion"]==1L && LocalPromptFormat.supports(promptFormat))
            val filename=data["weightsFile"] as? String ?: error("Missing weights filename.")
            require(filename.matches(Regex("[a-zA-Z0-9_-]+\\.gguf")))
            val id=data["modelId"] as? String ?: error("Missing model identity.")
            val version=data["modelVersion"] as? String ?: error("Missing model version.")
            val hash=data["sha256"] as? String ?: error("Missing model hash.")
            require(id.length in 1..120 && version.length in 1..120 && hash.matches(Regex("[a-f0-9]{64}")))
            val trained=data["blueTeamTrained"]==true && data["releaseEligible"]==true
            require(development || trained)
            val tokens=(data["contextTokens"] as? Long)?.takeIf {it in 256..16384}?.toInt() ?: error("Invalid context budget.")
            val tasks=(data["tasks"] as? List<*>)?.map {AiTask.valueOf(it as String)}?.toSet() ?: error("Missing model tasks.")
            val manifest=LocalModelManifest(id,version,hash,1,tasks,setOf(ModelCapability.STRUCTURED_OUTPUT),tokens,trained)
            val grammar=context.assets.open("ai/json.gbnf").use {it.readBytes()}
            NativeLlamaProvider(manifest,modelFile={
                val dir=File(context.filesDir,"ai-models/$hash").apply {mkdirs()}
                val destination=File(dir,filename)
                fun digest(file:File):String {
                    val sha=MessageDigest.getInstance("SHA-256")
                    file.inputStream().use {input->val buffer=ByteArray(1024*1024);while(true){val n=input.read(buffer);if(n<0)break;sha.update(buffer,0,n)}}
                    return sha.digest().joinToString(""){"%02x".format(it)}
                }
                if(!destination.isFile || digest(destination)!=hash) {
                    val pending=File(dir,"weights.pending")
                    context.assets.open("$assetDir/$filename").use {input->pending.outputStream().use {input.copyTo(it,1024*1024)}}
                    check(digest(pending)==hash) {"Bundled model integrity check failed."}
                    check(pending.renameTo(destination)) {"Could not install the bundled model."}
                }
                destination
            },grammar=grammar,developmentOnly=development,promptFormat=promptFormat)
        }.getOrElse {object:LocalModelProvider {
            override fun availability()=ModelAvailability(ModelState.INCOMPATIBLE,reason="The bundled model package is incompatible or incomplete.")
            override suspend fun countInputTokens(request:LocalModelRequest):Int=error("Model package unavailable.")
            override suspend fun generate(request:LocalModelRequest):LocalModelResponse=error("Model package unavailable.")
        }}
    }
}
