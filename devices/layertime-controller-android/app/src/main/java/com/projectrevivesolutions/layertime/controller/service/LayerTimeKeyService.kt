package com.projectrevivesolutions.layertime.controller.service

import android.accessibilityservice.AccessibilityService
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.os.Handler
import android.os.Looper
import android.provider.Settings
import android.util.Log
import android.view.KeyEvent
import android.view.accessibility.AccessibilityEvent
import com.projectrevivesolutions.layertime.controller.ui.MainActivity

/**
 * The LayerTime button. On the LOKMAT watch the green side button sends Back (KEY_BACK,
 * measured with getevent on 2026-10-29); the red one sends Power, which Android never
 * gives to apps. This accessibility service watches only the Back key:
 *
 * - With LayerTime in front, it lets the key through; the app slides the face in or out.
 * - Anywhere else, a short press opens LayerTime, and holding it for [HOLD_MS] gives the
 *   normal Back, so other apps keep their Back key.
 *
 * It reads no screen content: the only events it asks for are window changes, used to
 * know which app is in front.
 */
/** Set by MainActivity's onResume and onPause; read by [LayerTimeKeyService] in the same process. */
object AppVisibility { @Volatile var inFront = false }

class LayerTimeKeyService : AccessibilityService() {
    private val handler = Handler(Looper.getMainLooper())
    private var foreground: String? = null
    private var downAt = 0L
    private var holdFired = false
    private var holdTimer: Runnable? = null

    override fun onServiceConnected() { Log.i(TAG, "LayerTime button active") }

    override fun onDestroy() { Log.i(TAG, "LayerTime button service stopped"); super.onDestroy() }

    override fun onAccessibilityEvent(event: AccessibilityEvent) {
        if (event.eventType == AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED) {
            event.packageName?.toString()?.let { foreground = it }
        }
    }

    override fun onInterrupt() {}

    override fun onKeyEvent(event: KeyEvent): Boolean {
        if (event.keyCode != KeyEvent.KEYCODE_BACK) return false
        // Whether LayerTime is in front comes from its own activity lifecycle (same process),
        // not from window events: build 47 relied on window events and the short press did
        // nothing outside the app on the LOKMAT watch.
        val inFront = AppVisibility.inFront
        if (event.action == KeyEvent.ACTION_DOWN && event.repeatCount == 0)
            Log.i(TAG, "green button down; LayerTime in front=$inFront, last window=$foreground")
        if (inFront) return false                             // LayerTime handles its own button
        when (event.action) {
            KeyEvent.ACTION_DOWN -> if (event.repeatCount == 0) {
                downAt = event.eventTime; holdFired = false
                holdTimer = Runnable {
                    holdFired = true
                    performGlobalAction(GLOBAL_ACTION_BACK)
                    Log.i(TAG, "held: Back")
                }.also { handler.postDelayed(it, HOLD_MS) }
            }
            KeyEvent.ACTION_UP -> {
                holdTimer?.let { handler.removeCallbacks(it) }; holdTimer = null
                if (!holdFired) openLayerTime()
            }
        }
        return true
    }

    private fun openLayerTime() {
        val i = Intent(this, MainActivity::class.java)
            .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_REORDER_TO_FRONT or Intent.FLAG_ACTIVITY_SINGLE_TOP)
        runCatching { startActivity(i) }
            .onSuccess { Log.i(TAG, "short press: opening LayerTime from $foreground") }
            .onFailure { Log.i(TAG, "could not open LayerTime: ${it.message}") }
    }

    companion object {
        private const val TAG = "LT_BUTTON"
        const val HOLD_MS = 500L

        /** True when the user has turned this service on in Accessibility settings. */
        fun isEnabled(ctx: Context): Boolean {
            val me = ComponentName(ctx, LayerTimeKeyService::class.java).flattenToString()
            val list = Settings.Secure.getString(ctx.contentResolver, Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES) ?: return false
            return list.split(':').any { it.equals(me, ignoreCase = true) }
        }
    }
}
