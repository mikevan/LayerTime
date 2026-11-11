package com.projectrevivesolutions.layertime.controller.flow

import java.io.InputStream

/** A traded file opens as an editable draft; it is never selected for execution on import. */
object WorkflowExchange {
    const val MAX_BYTES=1024*1024
    fun read(input:InputStream):Draft {
        val bytes=ByteArray(MAX_BYTES+1)
        var size=0
        while(size<bytes.size) {
            val n=input.read(bytes,size,bytes.size-size)
            if(n<0) break
            if(n==0) { val one=input.read();if(one<0) break;bytes[size++]=one.toByte() } else size+=n
        }
        require(size<=MAX_BYTES) { "Workflow exceeds the 1 MB import limit." }
        val text=bytes.copyOf(size).toString(Charsets.UTF_8)
        val workflow=WorkflowLoader.parse(text)
        require(workflow.steps.size<=1000 && workflow.edges.size<=5000) { "Workflow exceeds the block or connection limit." }
        return DraftEdits.open(workflow,DraftEdits.layoutOf(text),shipped=false).copy(authoring=AnalyticOrigin.fromJson(text))
    }
}
