package com.projectrevivesolutions.layertime.controller.ui

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import kotlin.properties.ReadWriteProperty
import kotlin.reflect.KProperty

/** One return path for tabs and nested tool windows. Root deliberately stays in the app. */
internal data class PageHistory(val current:String="main",val previous:List<String> = emptyList()) {
    fun open(page:String)=if(page==current)this else PageHistory(page,(previous+current).takeLast(100))
    fun back()=if(previous.isEmpty())PageHistory() else PageHistory(previous.last(),previous.dropLast(1))
}
internal class PageNavigation:ReadWriteProperty<Any?,String> {
    private var state by mutableStateOf(PageHistory())
    override fun getValue(thisRef:Any?,property:KProperty<*>)=state.current
    override fun setValue(thisRef:Any?,property:KProperty<*>,value:String){state=state.open(value)}
    fun back(){state=state.back()}
}
