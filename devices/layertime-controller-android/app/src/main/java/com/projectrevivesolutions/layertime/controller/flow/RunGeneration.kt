package com.projectrevivesolutions.layertime.controller.flow

/** Serialize invalidation and publication so a replaced analysis cannot publish stale evidence. */
internal class RunGeneration {
    private var current=0L
    @Synchronized fun begin(reset:()->Unit = {}):Long { current++;reset();return current }
    @Synchronized fun publish(token:Long,action:()->Unit):Boolean {
        if(token!=current)return false
        action();return true
    }
}
