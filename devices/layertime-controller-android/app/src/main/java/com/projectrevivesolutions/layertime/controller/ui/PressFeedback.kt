package com.projectrevivesolutions.layertime.controller.ui

import android.content.Context
import android.os.VibrationEffect
import android.os.Vibrator
import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.tween
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawWithContent
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch

/**
 * Press feedback for every button (Michael, 2026-10-30: "I need some sort of feedback
 * when I hit a button to let me know I hit it"). Two signals, both only when the press
 * registers, so a disabled button stays silent and dark:
 *  - the button flashes: a bright overlay while the finger is down, then a short fade
 *    after the tap, so even a quick tap shows;
 *  - one very short buzz, [BUZZ_MS], straight from the Vibrator. Android's own touch
 *    haptics depend on a system setting that may be off on the watch, so they are not used.
 */
object PressFeedback {
    const val BUZZ_MS = 20L
    const val FLASH_MS = 220

    fun buzz(context: Context) {
        runCatching {
            val v = context.getSystemService(Vibrator::class.java) ?: return
            if (v.hasVibrator()) v.vibrate(VibrationEffect.createOneShot(BUZZ_MS, VibrationEffect.DEFAULT_AMPLITUDE))
        }
    }
}

/** One button's feedback: call [fire] from its click handler. */
class PressFlash internal constructor(private val context: Context, private val scope: CoroutineScope) {
    internal val level = Animatable(0f)
    fun fire() {
        PressFeedback.buzz(context)
        scope.launch {
            level.snapTo(1f)
            level.animateTo(0f, tween(PressFeedback.FLASH_MS))
        }
    }
}

@Composable
fun rememberPressFlash(): PressFlash {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    return remember { PressFlash(context.applicationContext, scope) }
}

/**
 * Draws the flash over a pill-shaped control: full while [pressed], then fading after
 * [PressFlash.fire]. Drawing only, so nothing is re-measured and no other control moves.
 */
fun Modifier.pressFlash(flash: PressFlash, pressed: Boolean, color: Color = Color.White): Modifier =
    drawWithContent {
        drawContent()
        val a = maxOf(flash.level.value, if (pressed) 1f else 0f)
        if (a > 0f) drawRoundRect(color.copy(alpha = 0.45f * a), cornerRadius = CornerRadius(size.height / 2f))
    }
