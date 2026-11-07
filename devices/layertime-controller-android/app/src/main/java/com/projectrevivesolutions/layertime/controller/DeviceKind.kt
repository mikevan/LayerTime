package com.projectrevivesolutions.layertime.controller

import android.os.Build

/**
 * Which Android device LayerScope is running on, where that changes a setting.
 *
 * SWIPE AWAY DISCONNECTS applies only to the LOKMAT APPLLP 5 MAX watch (Michael,
 * 2026-11-06): on every other device the setting is shown greyed out and has no effect.
 * The watch's About screen reads "LOKMAT APPLLP 5 MAX"; Android may report LOKMAT as the
 * maker and "APPLLP 5 MAX" as the model, or the whole string as the model, so either
 * word is enough.
 */
object DeviceKind {
    fun isLokmatWatch(manufacturer: String?, brand: String?, model: String?): Boolean =
        listOf(manufacturer, brand, model).any { v ->
            v != null && (v.contains("LOKMAT", ignoreCase = true) || v.contains("APPLLP", ignoreCase = true))
        }

    val isLokmatWatch: Boolean get() = isLokmatWatch(Build.MANUFACTURER, Build.BRAND, Build.MODEL)
}
