package com.projectrevivesolutions.layertime.controller.ui

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioTrack
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.os.VibrationEffect
import android.os.Vibrator
import androidx.activity.compose.BackHandler
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.ContextCompat
import com.projectrevivesolutions.layertime.controller.hunt.AccelStepDetector
import com.projectrevivesolutions.layertime.controller.hunt.HuntEngine
import com.projectrevivesolutions.layertime.controller.hunt.HuntPhase
import com.projectrevivesolutions.layertime.controller.hunt.HuntSession
import com.projectrevivesolutions.layertime.controller.hunt.HuntView
import com.projectrevivesolutions.layertime.controller.hunt.SignalBand
import com.projectrevivesolutions.layertime.controller.hunt.Trend
import com.projectrevivesolutions.layertime.controller.ui.theme.ConsoleFonts
import com.projectrevivesolutions.layertime.controller.ui.theme.LtColors
import kotlinx.coroutines.delay
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.sin
import kotlin.math.sqrt

/**
 * HUNT v1: the full-screen hunt (claude/hunt_v1_layervixen_plan.md). Made for kids:
 * one dot that goes red, yellow, then green, a tone that speeds up, and FOUND.
 *
 *  - Radar: you at the center, and the step trail curling out behind you. Each
 *    step is a dot colored by the signal where you took it. The watch has no
 *    compass, so the trail is the order of your steps, not a map.
 *  - Upper right: the status dot and the signal in dBm.
 *  - Down the side: signal, 1 s average, best, CLOSING or OPENING, last heard,
 *    steps, and time.
 *  - FOUND: the whole screen flashes green.
 *
 * The side button (Back) does nothing here, so a bumped button cannot end a
 * hunt; STOP HUNT does.
 */
@Composable
fun HuntScreen(onStop: () -> Unit, onAgain: () -> Unit) {
    val context = LocalContext.current
    val problem by HuntSession.problem.collectAsState()
    var nowMs by remember { mutableLongStateOf(SystemClock.elapsedRealtime()) }
    LaunchedEffect(Unit) { while (true) { nowMs = SystemClock.elapsedRealtime(); delay(200) } }
    val view = remember(nowMs) { HuntSession.view(nowMs) }

    BackHandler { /* only STOP HUNT ends a hunt */ }
    val hostView = LocalView.current
    DisposableEffect(Unit) { hostView.keepScreenOn = true; onDispose { hostView.keepScreenOn = false } }

    StepSource(context)
    HuntTone(context)

    BoxWithConstraints(Modifier.fillMaxSize().background(LtColors.Bg)) {
        val wide = maxWidth > maxHeight * 1.1f
        val radarSide = maxHeight - 12.dp // read here: maxHeight is not reachable inside Row's scope
        if (wide) {
            Row(Modifier.fillMaxSize().padding(6.dp)) {
                Radar(view, Modifier.fillMaxHeight().width(radarSide))
                Spacer(Modifier.width(6.dp))
                SidePanel(view, problem, onStop, onAgain, Modifier.fillMaxHeight().weight(1f))
            }
        } else {
            Column(Modifier.fillMaxSize().padding(6.dp)) {
                Radar(view, Modifier.fillMaxWidth().weight(1f))
                SidePanel(view, problem, onStop, onAgain, Modifier.fillMaxWidth())
            }
        }
        StatusDot(view, Modifier.align(Alignment.TopEnd).padding(8.dp))
        if (view.phase == HuntPhase.FOUND) FoundFlash(onStop, onAgain)
    }
}

private fun bandColor(b: SignalBand): Color = when (b) {
    SignalBand.GREEN -> LtColors.Green
    SignalBand.YELLOW -> Color(0xFFFFE040)
    SignalBand.RED, SignalBand.NONE -> LtColors.Red
}

private fun phaseLine(v: HuntView): String = when (v.phase) {
    HuntPhase.STARTING -> "STARTING"
    HuntPhase.SEARCHING -> "SEARCHING"
    HuntPhase.TRACKING -> when (v.band) {
        SignalBand.GREEN -> "STRONG"
        SignalBand.YELLOW -> "GETTING WARMER"
        else -> "FAINT"
    }
    HuntPhase.LOST -> "TARGET LOST"
    HuntPhase.FOUND -> "FOUND"
}

@Composable
private fun StatusDot(v: HuntView, modifier: Modifier) {
    val c = bandColor(v.band)
    Column(modifier, horizontalAlignment = Alignment.CenterHorizontally) {
        Canvas(Modifier.size(30.dp)) {
            drawCircle(c.copy(alpha = 0.30f), radius = size.minDimension / 2f)
            drawCircle(c, radius = size.minDimension / 2.8f)
        }
        Text(v.smoothedDbm?.let { "$it dBm" } ?: "--", color = c, fontSize = 11.sp, fontFamily = ConsoleFonts.mono)
    }
}

@Composable
private fun Radar(v: HuntView, modifier: Modifier) {
    val sweep by rememberInfiniteTransition(label = "sweep").animateFloat(
        0f, 360f, infiniteRepeatable(tween(3000, easing = LinearEasing)), label = "sweepAngle")
    val ring = Color(0xFF1E7A3A)
    Box(modifier, contentAlignment = Alignment.Center) {
        Canvas(Modifier.fillMaxSize()) {
            val c = Offset(size.width / 2f, size.height / 2f)
            val r = size.minDimension / 2f - 2.dp.toPx()
            drawCircle(Color(0xFF031208), radius = r, center = c)
            // Ten rings. The ring the signal puts you on is lit bright in the signal's color,
            // the rings outside it (already closed through) glow dimmer, and the rings inside
            // it (still to go) stay dark. Closer to the Vixen, the lit ring moves inward;
            // ring 1, the center ring, is FOUND strength.
            val rings = HuntEngine.RINGS
            val now = v.ring
            val lit = bandColor(v.band)
            for (k in 1..rings) {
                val rr = r * k / rings
                when {
                    now == null || k < now -> drawCircle(ring, radius = rr, center = c, style = Stroke(1.dp.toPx()))
                    k == now -> {
                        drawCircle(lit.copy(alpha = 0.25f), radius = rr, center = c, style = Stroke(8.dp.toPx()))
                        drawCircle(lit, radius = rr, center = c, style = Stroke(3.dp.toPx()))
                    }
                    else -> drawCircle(lit.copy(alpha = 0.45f), radius = rr, center = c, style = Stroke(1.5.dp.toPx()))
                }
            }
            drawLine(ring, Offset(c.x - r, c.y), Offset(c.x + r, c.y), 1.dp.toPx())
            drawLine(ring, Offset(c.x, c.y - r), Offset(c.x, c.y + r), 1.dp.toPx())
            // The sweep: a faint line with a fading wake behind it, for the radar look only.
            for (i in 0 until 24) {
                val a = Math.toRadians((sweep - i * 2.5f).toDouble())
                drawLine(LtColors.Green.copy(alpha = 0.30f * (1f - i / 24f)), c,
                    Offset(c.x + (r * cos(a)).toFloat(), c.y + (r * sin(a)).toFloat()), 2.dp.toPx())
            }
            // The step trail: newest at the center, older steps curling outward along a spiral
            // with even spacing (radius and angle both grow with the square root of the age).
            val trail = v.trail
            val shown = minOf(trail.size, VISIBLE_STEPS)
            var prev: Offset? = null
            for (age in 0 until shown) {
                val band = trail[trail.size - 1 - age]
                val f = sqrt((age + 1).toFloat() / VISIBLE_STEPS)
                val rr = r * 0.92f * f
                val th = SPIRAL_TURNS * 2 * PI * f
                val p = Offset(c.x + (rr * cos(th)).toFloat(), c.y + (rr * sin(th)).toFloat())
                val alpha = 1f - 0.6f * age / VISIBLE_STEPS
                prev?.let { drawLine(bandColor(band).copy(alpha = alpha * 0.6f), it, p, 2.dp.toPx()) }
                drawCircle(bandColor(band).copy(alpha = alpha), radius = 3.dp.toPx(), center = p)
                prev = p
            }
            // You.
            drawCircle(LtColors.Ivory, radius = 5.dp.toPx(), center = c)
            drawCircle(bandColor(v.band), radius = 9.dp.toPx(), center = c, style = Stroke(2.dp.toPx()))
        }
        Column(Modifier.align(Alignment.TopCenter).padding(top = 6.dp), horizontalAlignment = Alignment.CenterHorizontally) {
            Text(phaseLine(v), color = if (v.phase == HuntPhase.TRACKING) bandColor(v.band) else LtColors.Ivory,
                fontSize = 15.sp, fontWeight = FontWeight.Bold, fontFamily = ConsoleFonts.heading)
        }
        Text(v.ring?.let { "RING $it OF ${HuntEngine.RINGS}" } ?: "STEP TRAIL, IN ORDER", color = LtColors.Grey, fontSize = 8.sp,
            fontFamily = ConsoleFonts.mono,
            modifier = Modifier.align(Alignment.BottomCenter).padding(bottom = 4.dp))
    }
}

@Composable
private fun SidePanel(v: HuntView, problem: String?, onStop: () -> Unit, onAgain: () -> Unit, modifier: Modifier) {
    Column(modifier.padding(top = 46.dp), verticalArrangement = Arrangement.spacedBy(2.dp)) {
        Metric("SIGNAL", v.latestDbm?.let { "$it dBm" } ?: "--")
        Metric("2 S AVG", v.smoothedDbm?.let { "$it dBm" } ?: "--")
        Metric("BEST", v.strongestDbm?.let { "$it dBm" } ?: "--")
        Metric("TREND", when (v.trend) { Trend.CLOSING -> "CLOSING"; Trend.OPENING -> "OPENING"; Trend.STEADY -> "STEADY"; Trend.NONE -> "--" },
            when (v.trend) { Trend.CLOSING -> LtColors.Green; Trend.OPENING -> LtColors.Red; else -> LtColors.Ivory })
        Metric("HEARD", v.secondsSinceHeard?.let { if (it == 0) "NOW" else "$it S AGO" } ?: "NOT YET")
        Metric("STEPS", "${v.steps}")
        Metric("TIME", "%d:%02d".format(v.huntSeconds / 60, v.huntSeconds % 60))
        problem?.let { Text(it, color = LtColors.Amber, fontSize = 10.sp) }
        Spacer(Modifier.weight(1f))
        SciFiOutline("STOP HUNT", enabled = true) { onStop() }
    }
}

@Composable
private fun Metric(label: String, value: String, color: Color = LtColors.Ivory) {
    Row(Modifier.fillMaxWidth()) {
        Text(label, color = LtColors.ConsoleEdge, fontSize = 10.sp, fontFamily = ConsoleFonts.mono, modifier = Modifier.weight(1f))
        Text(value, color = color, fontSize = 11.sp, fontFamily = ConsoleFonts.mono, textAlign = TextAlign.End)
    }
}

@Composable
private fun FoundFlash(onStop: () -> Unit, onAgain: () -> Unit) {
    val pulse by rememberInfiniteTransition(label = "found").animateFloat(
        0.35f, 0.95f, infiniteRepeatable(tween(350), RepeatMode.Reverse), label = "foundAlpha")
    Box(Modifier.fillMaxSize().background(LtColors.Green.copy(alpha = pulse)), contentAlignment = Alignment.Center) {
        Column(horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(10.dp)) {
            Text("FOUND!", color = LtColors.Bg, fontSize = 44.sp, fontWeight = FontWeight.Bold, fontFamily = ConsoleFonts.heading)
            Text("You found the LayerVixen.", color = LtColors.Bg, fontSize = 13.sp)
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                SciFiPrimary("HUNT AGAIN", enabled = true) { onAgain() }
                SciFiOutline("STOP HUNT", enabled = true) { onStop() }
            }
        }
    }
}

/**
 * Steps into the hunt. The accelerometer always counts them (AccelStepDetector): on the
 * LOKMAT watch the step detector sensor is listed and permitted but never delivered a step
 * (2026-10-31, STEPS stayed 0). If the step detector does report, its steps are used and
 * the accelerometer's are ignored from then on, so no step is counted twice. With neither
 * sensor, one trail point every 1.2 s.
 */
@Composable
private fun StepSource(context: Context) {
    val needsPermission = Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q
    var granted by remember {
        mutableStateOf(!needsPermission || ContextCompat.checkSelfPermission(context, Manifest.permission.ACTIVITY_RECOGNITION) ==
            PackageManager.PERMISSION_GRANTED)
    }
    var asked by remember { mutableStateOf(false) }
    val launcher = rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { granted = it; asked = true }
    LaunchedEffect(Unit) {
        if (!granted && needsPermission) launcher.launch(Manifest.permission.ACTIVITY_RECOGNITION) else asked = true
    }
    if (!asked) return
    val sm = context.getSystemService(Context.SENSOR_SERVICE) as SensorManager
    val accel = sm.getDefaultSensor(Sensor.TYPE_ACCELEROMETER)
    val detector = if (granted) sm.getDefaultSensor(Sensor.TYPE_STEP_DETECTOR) else null
    if (accel == null && detector == null) {
        LaunchedEffect(Unit) { while (true) { delay(1200); HuntSession.onStep(SystemClock.elapsedRealtime()) } }
        return
    }
    DisposableEffect(accel, detector) {
        val counter = AccelStepDetector()
        var hardwareSteps = false
        val listener = object : SensorEventListener {
            override fun onSensorChanged(e: SensorEvent) {
                val now = SystemClock.elapsedRealtime()
                if (e.sensor.type == Sensor.TYPE_STEP_DETECTOR) { hardwareSteps = true; HuntSession.onStep(now); return }
                if (hardwareSteps) return
                if (counter.onSample(e.values[0], e.values[1], e.values[2], e.timestamp / 1_000_000L)) HuntSession.onStep(now)
            }
            override fun onAccuracyChanged(s: Sensor?, a: Int) {}
        }
        accel?.let { sm.registerListener(listener, it, SensorManager.SENSOR_DELAY_GAME) }
        detector?.let { sm.registerListener(listener, it, SensorManager.SENSOR_DELAY_UI) }
        onDispose { sm.unregisterListener(listener) }
    }
}

/** The hunt tone: faster and higher as the signal gets stronger; a chime and a buzz on FOUND. */
@Composable
private fun HuntTone(context: Context) {
    val beeper = remember { HuntBeeper() }
    DisposableEffect(Unit) { onDispose { beeper.release() } }
    LaunchedEffect(Unit) {
        var wasFound = false
        while (true) {
            val v = HuntSession.view(SystemClock.elapsedRealtime())
            if (v.phase == HuntPhase.FOUND && !wasFound) {
                wasFound = true
                beeper.beep(1200, 90); delay(130); beeper.beep(1600, 90); delay(130); beeper.beep(2000, 220)
                vibrate(context, 400)
                continue
            }
            if (v.phase != HuntPhase.FOUND) wasFound = false
            val tone = HuntSession.toneFor(v)
            if (tone == null) { delay(200); continue }
            beeper.beep(tone.freqHz, 45)
            delay(tone.intervalMs)
        }
    }
}

private fun vibrate(context: Context, ms: Long) {
    val vib = context.getSystemService(Context.VIBRATOR_SERVICE) as? Vibrator ?: return
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) vib.vibrate(VibrationEffect.createOneShot(ms, VibrationEffect.DEFAULT_AMPLITUDE))
    else @Suppress("DEPRECATION") vib.vibrate(ms)
}

/** Short sine beeps on the sonification stream, like the face sounds; the media volume sets the level. */
private class HuntBeeper {
    private val rate = 22050
    private val main = Handler(Looper.getMainLooper())
    private val attrs = AudioAttributes.Builder()
        .setUsage(AudioAttributes.USAGE_ASSISTANCE_SONIFICATION)
        .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION).build()
    private val live = ArrayList<AudioTrack>()

    fun beep(freqHz: Int, ms: Int) {
        val n = rate * ms / 1000
        val ramp = rate * 5 / 1000
        val pcm = ShortArray(n) { i ->
            val env = minOf(1f, i / ramp.toFloat(), (n - i) / ramp.toFloat())
            (sin(2 * PI * freqHz * i / rate) * env * 0.7 * Short.MAX_VALUE).toInt().toShort()
        }
        val track = try {
            AudioTrack.Builder().setAudioAttributes(attrs)
                .setAudioFormat(AudioFormat.Builder().setEncoding(AudioFormat.ENCODING_PCM_16BIT).setSampleRate(rate)
                    .setChannelMask(AudioFormat.CHANNEL_OUT_MONO).build())
                .setTransferMode(AudioTrack.MODE_STATIC).setBufferSizeInBytes(n * 2).build()
        } catch (e: Exception) { return }
        track.write(pcm, 0, n)
        track.play()
        synchronized(live) { live.add(track) }
        main.postDelayed({ synchronized(live) { live.remove(track) }; track.release() }, ms + 150L)
    }

    fun release() {
        main.removeCallbacksAndMessages(null)
        synchronized(live) { live.forEach { it.release() }; live.clear() }
    }
}

/** How many steps the radar shows, and how many turns the trail's spiral makes. */
private const val VISIBLE_STEPS = 150
private const val SPIRAL_TURNS = 3.0
