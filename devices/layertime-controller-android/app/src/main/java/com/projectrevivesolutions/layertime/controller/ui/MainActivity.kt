package com.projectrevivesolutions.layertime.controller.ui

import android.Manifest
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.os.BatteryManager
import android.os.Build
import android.os.Bundle
import android.text.format.DateFormat
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.tween
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.CutCornerShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.Checkbox
import androidx.compose.material3.CheckboxDefaults
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.DisposableEffect
import androidx.compose.ui.unit.Density
import androidx.compose.runtime.rememberCoroutineScope
import kotlinx.coroutines.launch
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.ContextCompat
import com.projectrevivesolutions.layertime.controller.diag.DeviceReportBuilder
import com.projectrevivesolutions.layertime.controller.diag.Exporter
import com.projectrevivesolutions.layertime.controller.link.LinkClient
import com.projectrevivesolutions.layertime.controller.link.LinkConstants
import com.projectrevivesolutions.layertime.controller.model.ConnState
import com.projectrevivesolutions.layertime.controller.model.EventRow
import com.projectrevivesolutions.layertime.controller.model.LogEntry
import com.projectrevivesolutions.layertime.controller.service.LinkForegroundService
import com.projectrevivesolutions.layertime.controller.state.ControllerRepository
import com.projectrevivesolutions.layertime.controller.ui.theme.LayerTimeControllerTheme
import com.projectrevivesolutions.layertime.controller.ui.theme.LtColors
import com.projectrevivesolutions.layertime.controller.ui.theme.ConsoleShape
import com.projectrevivesolutions.layertime.controller.ui.theme.ConsoleButtonShape
import com.projectrevivesolutions.layertime.controller.ui.theme.consoleFrame
import com.projectrevivesolutions.layertime.controller.ui.theme.haloOnly
import com.projectrevivesolutions.layertime.controller.ui.theme.ConsoleFonts
import com.projectrevivesolutions.layertime.controller.ui.theme.railBrush
import com.projectrevivesolutions.layertime.controller.ui.theme.domeBrush
import androidx.compose.material3.ProvideTextStyle
import androidx.compose.ui.text.TextStyle
import com.projectrevivesolutions.layertime.controller.ui.theme.glowText
import kotlinx.coroutines.delay
import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Spacer
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.draw.drawWithContent
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.Shadow
import androidx.compose.ui.graphics.lerp
import androidx.compose.ui.unit.Dp
import com.projectrevivesolutions.layertime.controller.ui.theme.PillShape
import com.projectrevivesolutions.layertime.controller.ui.theme.hudPanel
import com.projectrevivesolutions.layertime.controller.ui.theme.hudRail
import com.projectrevivesolutions.layertime.controller.ui.theme.glassPill
import com.projectrevivesolutions.layertime.controller.ui.theme.pillShine
import com.projectrevivesolutions.layertime.controller.ui.theme.readoutWell
import com.projectrevivesolutions.layertime.controller.ui.theme.drawCornerBrackets
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        ControllerRepository.deviceReport = DeviceReportBuilder.build(applicationContext)
        setContent {
            LayerTimeControllerTheme {
                Surface(Modifier.fillMaxSize(), color = LtColors.Bg) { ControllerScreen() }
            }
        }
    }

    companion object {
        fun requiredPermissions(): Array<String> =
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S)
                buildList {
                    add(Manifest.permission.BLUETOOTH_SCAN); add(Manifest.permission.BLUETOOTH_CONNECT)
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) add(Manifest.permission.POST_NOTIFICATIONS)
                }.toTypedArray()
            else arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
    }
}

private val logFmt = SimpleDateFormat("HH:mm:ss", Locale.US)

/**
 * Text size for the data panel. Android sizes text by the density a device
 * declares; the LOKMAT watch declares 160 dpi for both the logical and the
 * physical density, so its true (much finer) pitch is unknown to the system
 * and 11 sp text comes out small. The automatic part therefore comes from the
 * panel's own size: the scale that fits about 24 body lines in its height and
 * about 60 characters across its width, whichever is smaller, between 1.0 and
 * 1.8 (the watch panel, 596 x 464 dp, gives 1.36). On top of that the operator adjusts with TEXT A- / A+, and the
 * choice is remembered. Phones in portrait get 1.0 from the automatic part.
 */
object PanelTextScale {
    private const val PREFS = "layertime_ui"
    private const val KEY = "text_mult"
    const val MIN_MULT = 0.8f
    const val MAX_MULT = 1.6f
    /** The smaller of a height-based and a width-based scale, so a tall narrow phone panel stays at 1.0. */
    fun auto(panelWidthDp: Float, panelHeightDp: Float): Float =
        minOf(panelHeightDp / 340f, panelWidthDp / 420f).coerceIn(1.0f, 1.8f)
    fun read(ctx: Context): Float = ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE).getFloat(KEY, 1.0f).coerceIn(MIN_MULT, MAX_MULT)
    fun write(ctx: Context, mult: Float) { ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit().putFloat(KEY, mult.coerceIn(MIN_MULT, MAX_MULT)).apply() }
    fun readFont(ctx: Context): Int = ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE).getInt("font_set", 1)
    fun writeFont(ctx: Context, i: Int) { ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit().putInt("font_set", i).apply() }
}
private val panelShape = CutCornerShape(topStart = 10.dp, bottomEnd = 10.dp)
private val btnShape = CutCornerShape(topStart = 6.dp, bottomEnd = 6.dp)
private val dataShape = RoundedCornerShape(16.dp)
private val SENSOR_IDS = (2..16).toList()

private fun batteryPct(ctx: Context): Int {
    val i = ctx.registerReceiver(null, IntentFilter(Intent.ACTION_BATTERY_CHANGED)) ?: return -1
    val level = i.getIntExtra(BatteryManager.EXTRA_LEVEL, -1)
    val scale = i.getIntExtra(BatteryManager.EXTRA_SCALE, -1)
    return if (level >= 0 && scale > 0) level * 100 / scale else -1
}

private fun bluetoothOn(ctx: Context): Boolean {
    val mgr = ctx.getSystemService(Context.BLUETOOTH_SERVICE) as? android.bluetooth.BluetoothManager
    return mgr?.adapter?.isEnabled == true
}

@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun ControllerScreen() {
    val context = LocalContext.current
    val repo = ControllerRepository

    val conn by repo.conn.collectAsState()
    val deviceLabel by repo.deviceLabel.collectAsState()
    val status by repo.status.collectAsState()
    val server by repo.server.collectAsState()
    val events by repo.events.collectAsState()
    val log by repo.log.collectAsState()
    val background by repo.background.collectAsState()

    var target by remember { mutableIntStateOf(LinkConstants.ReconTarget.ALL) }
    var showLog by remember { mutableStateOf(false) }
    var page by remember { mutableStateOf("main") }
    var selectedEvent by remember { mutableStateOf<EventRow?>(null) }
    val sensorsOn = remember { mutableStateMapOf<Int, Boolean>() }
    var battery by remember { mutableIntStateOf(batteryPct(context)) }
    var btOn by remember { mutableStateOf(bluetoothOn(context)) }
    var nowMs by remember { mutableLongStateOf(System.currentTimeMillis()) }

    LaunchedEffect(Unit) {
        while (true) {
            nowMs = System.currentTimeMillis(); battery = batteryPct(context); btOn = bluetoothOn(context)
            delay(1000)
        }
    }

    val is24 = DateFormat.is24HourFormat(context)
    val hourText = remember(nowMs, is24) { SimpleDateFormat(if (is24) "HH" else "hh", Locale.US).format(Date(nowMs)) }
    val minText = remember(nowMs) { SimpleDateFormat("mm", Locale.US).format(Date(nowMs)) }
    val dateText = remember(nowMs) { SimpleDateFormat("EEE '•' MMM dd", Locale.US).format(Date(nowMs)).uppercase() }

    val permLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { result ->
        val granted = MainActivity.requiredPermissions().all {
            result[it] == true || it == Manifest.permission.POST_NOTIFICATIONS
        }
        if (granted) LinkForegroundService.start(context, target = null)
        else repo.log(LogEntry.Category.ERROR, "permissions denied; cannot scan or connect")
    }

    var locationNotice by remember { mutableStateOf<String?>(null) }
    fun connect() {
        if (LinkClient.locationSwitchBlocksScanning(context)) {
            locationNotice = LinkClient.LOCATION_OFF_MESSAGE
            repo.log(LogEntry.Category.ERROR, LinkClient.LOCATION_OFF_MESSAGE)
            return
        }
        locationNotice = null
        val missing = MainActivity.requiredPermissions().any {
            ContextCompat.checkSelfPermission(context, it) != PackageManager.PERMISSION_GRANTED
        }
        if (missing) permLauncher.launch(MainActivity.requiredPermissions())
        else LinkForegroundService.start(context, target = null)
    }

    val active = conn == ConnState.CONNECTED
    val connecting = conn in listOf(ConnState.SCANNING, ConnState.CONNECTING, ConnState.DISCOVERING, ConnState.HANDSHAKING)
    val wands = if (active) 1 else 0

    // On the sliding layout the face is parked off screen while the operator works
    // in the panel. Its inputs are frozen then, so it is not redrawn every second.
    var faceParked by remember { mutableStateOf(false) }
    var frozen by remember { mutableStateOf<FaceInputs?>(null) }
    // Ring data: ambient temperature only from a real sensor; sunrise/sunset from location.
    var ambientC by remember { mutableStateOf<Float?>(null) }
    var location by remember { mutableStateOf(RingData.lastKnown(context)) }
    DisposableEffect(Unit) {
        val stop = RingData.watchAmbientTemperature(context) { ambientC = it }
        if (location == null) RingData.requestOneFix(context) { location = it }
        onDispose { stop?.invoke() }
    }
    val fahrenheit = remember { RingData.useFahrenheit() }
    val tempValue = ambientC?.let { c -> "%.0f°".format(if (fahrenheit) c * 9f / 5f + 32f else c) }
    val tempUnit = if (ambientC != null) (if (fahrenheit) "°F" else "°C") else null
    val sun = remember(nowMs / 60_000L, location) {
        location?.let { SunTimes.next(it.latitude, it.longitude, nowMs, java.util.TimeZone.getDefault()) }
    }
    val sunLabel = when (sun?.first) { true -> "SUNRISE"; false -> "SUNSET"; null -> "SUN" }
    val sunValue = sun?.let { SimpleDateFormat(if (is24) "H:mm" else "h:mm", Locale.US).format(Date(it.second)) } ?: "--"
    val sunAmPm = sun?.let { if (is24) null else SimpleDateFormat("a", Locale.US).format(Date(it.second)) }

    val live = FaceInputs(conn, status, btOn, battery, hourText, minText, dateText,
        tempValue, tempUnit, sunLabel, sunValue, sunAmPm)
    val shown = if (faceParked) (frozen ?: live) else live
    // Plain holder, not state: the sliding layout installs its park action here each
    // composition without triggering another recomposition.
    val faceActions = remember { FaceActions() }

    val face: @Composable (Modifier) -> Unit = { m ->
        FaceView(shown.conn, shown.status, shown.btOn, shown.battery, shown.hour, shown.min, shown.date,
            tempValue = shown.tempValue, tempUnit = shown.tempUnit,
            sunLabel = shown.sunLabel, sunValue = shown.sunValue, sunAmPm = shown.sunAmPm,
            onCounter = { page = "attacks"; faceActions.afterButton() }, onRecon = { page = "config"; faceActions.afterButton() },
            onOutsideCircle = { faceActions.outsideCircle() }, modifier = m)
    }

    var textMult by remember { mutableStateOf(PanelTextScale.read(context)) }
    LaunchedEffect(Unit) { ConsoleFonts.index = PanelTextScale.readFont(context) }

    val dataPane: @Composable (Modifier, PaddingValues) -> Unit = { paneMod, outerPad ->
        BoxWithConstraints(paneMod.padding(outerPad)) {
            val autoScale = PanelTextScale.auto(maxWidth.value, maxHeight.value)
            val fontScale = autoScale * textMult
            val base = LocalDensity.current
            CompositionLocalProvider(LocalDensity provides Density(base.density, fontScale)) {
            ProvideTextStyle(TextStyle(fontFamily = ConsoleFonts.body)) {
            Box(
                Modifier.fillMaxSize()
                    .consoleFrame(surface = LtColors.ConsoleBg, radius = 20.dp, elevation = 10.dp)
                    .drawWithContent { drawContent(); drawCornerBrackets(LtColors.ConsoleEdge, 20.dp.toPx()) }
                    .clip(RoundedCornerShape(20.dp))
                    .padding(24.dp)
            ) {
                when (page) {
                    "config" -> {
                        LazyColumn(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                            item {
                                Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically,
                                    horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                                    SciFiOutline("‹ BACK", enabled = true) { page = "main" }
                                    Text("MISSION CONFIGURATION", color = LtColors.ConsoleEdge, fontWeight = FontWeight.Bold,
                                        fontSize = 15.sp, letterSpacing = 2.sp, style = glowText())
                                }
                            }
                            item { Text("Check a connector to enable its sensor for the mission.", color = LtColors.Ivory, fontSize = 11.sp) }
                            items(SENSOR_IDS) { id ->
                                val on = sensorsOn[id] == true
                                Row(
                                    Modifier.fillMaxWidth().padding(horizontal = 2.dp, vertical = 2.dp)
                                        .hudPanel(edge = if (on) LtColors.Green else LtColors.ConsoleEdge, radius = 16.dp, elevation = if (on) 4.dp else 1.dp, halo = on)
                                        .clip(ConsoleShape).padding(horizontal = 19.dp, vertical = 8.dp),
                                    verticalAlignment = Alignment.CenterVertically,
                                ) {
                                    Checkbox(
                                        checked = on, onCheckedChange = { sensorsOn[id] = it },
                                        colors = CheckboxDefaults.colors(
                                            checkedColor = LtColors.Green, uncheckedColor = LtColors.Grey, checkmarkColor = LtColors.Bg,
                                        ),
                                    )
                                    Text(LinkConstants.ReconTarget.name(id),
                                        color = if (on) LtColors.Green else LtColors.Ivory,
                                        fontWeight = FontWeight.Bold, fontSize = 13.sp, letterSpacing = 1.sp)
                                }
                            }
                        }
                    }

                    "attacks" -> {
                        LazyColumn(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                            item {
                                Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically,
                                    horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                                    SciFiOutline("‹ BACK", enabled = true) { page = "main" }
                                    Text("DETECTED ATTACKS  //  ${events.size}", color = LtColors.ConsoleEdge,
                                        fontWeight = FontWeight.Bold, fontSize = 15.sp, letterSpacing = 2.sp, style = glowText())
                                }
                            }
                            item {
                                SciFiOutline("DECODE A CAPTURE FILE", enabled = true, large = true) { page = "capture" }
                            }
                            if (events.isEmpty()) {
                                item { Text(emptyEventsMessage(active, status), color = LtColors.Grey, fontSize = 12.sp) }
                            }
                            items(events, key = { it.eventId }) { e ->
                                Box(
                                    Modifier.fillMaxWidth().padding(horizontal = 2.dp, vertical = 3.dp)
                                        .hudPanel(edge = LtColors.Amber, radius = 16.dp, elevation = 4.dp)
                                        .clip(ConsoleShape)
                                        .clickable { selectedEvent = e; page = "detail" }
                                        .padding(21.dp)
                                ) {
                                    Column {
                                        Text("#${e.eventId}  ${e.detectorName}", color = LtColors.Amber,
                                            fontWeight = FontWeight.Bold, fontSize = 14.sp, letterSpacing = 1.sp)
                                        Text("${LinkConstants.SourceKind.name(e.sourceKind)} · ${LinkConstants.Band.name(e.band)} · ch ${e.channel} · ${e.rssi} dBm · x${e.count}   → TAP FOR DETAIL",
                                            color = LtColors.Ivory, fontSize = 11.sp, fontFamily = ConsoleFonts.mono)
                                    }
                                }
                            }
                        }
                    }

                    "capture" -> CaptureOnlyScreen { page = "attacks" }

                    "detail" -> {
                        val ev = selectedEvent
                        if (ev != null) EventDetailScreen(ev, deviceLabel, conn) { page = "attacks" }
                        else LazyColumn(Modifier.fillMaxSize()) { item { Text("No detection selected.", color = LtColors.Grey) } }
                    }

                    else -> {
                        LazyColumn(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                            item { HudStatusStrip(active, connecting, events.size) }
                            item {
                                SciFiPanel("CONNECTION", statusColor = if (active) LtColors.Green else LtColors.Red, statusCount = wands) {
                                    if (active && server != null) {
                                        Text("SERVER 0.${server!!.serverMinor}  ·  SES ${server!!.sessionId}  ·  MAXF ${server!!.maxFrame}",
                                            color = LtColors.Ivory, fontSize = 10.sp, fontFamily = ConsoleFonts.mono)
                                    }
                                    if (active && deviceLabel.isNotBlank()) {
                                        Text(deviceLabel.uppercase(), color = LtColors.Cyan, fontSize = 10.sp, fontFamily = ConsoleFonts.mono)
                                    }
                                    locationNotice?.let { Text(it, color = LtColors.Red, fontSize = 11.sp) }
                                    Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                                        SciFiPrimary("SCAN & CONNECT", enabled = !active && !connecting, large = true) { connect() }
                                        SciFiOutline("DISCONNECT", enabled = active || connecting || conn == ConnState.DISCONNECTED, large = true) {
                                            LinkForegroundService.stop(context)
                                        }
                                    }
                                }
                            }
                            item {
                                SciFiPanel("DETECTION  //  ONE WAND") {
                                    HudReadout("CONNECTED WANDS", "$wands", if (wands > 0) LtColors.Green else LtColors.Cyan)
                                    // Wraps onto a second line on narrow panels so CLEAR is never cut off.
                                    FlowRow(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                                        TargetButton("ALL", LinkConstants.ReconTarget.ALL, target) { target = it }
                                        TargetButton("DEAUTH", LinkConstants.ReconTarget.DEAUTH, target) { target = it }
                                        SciFiPrimary("START", enabled = active) { LinkForegroundService.active?.reconStart(target) }
                                        SciFiOutline("STOP", enabled = active) { LinkForegroundService.active?.reconStop() }
                                        SciFiOutline("CLEAR", enabled = active) { LinkForegroundService.active?.clearEvents() }
                                    }
                                }
                            }
                            item {
                                SciFiPanel("SCREEN-OFF RECEPTION TEST") {
                                    Text("Start, turn the screen off for a while, turn it on, then stop.", color = LtColors.Ivory, fontSize = 11.sp)
                                    background?.let { bt ->
                                        FlowRow(horizontalArrangement = Arrangement.spacedBy(10.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                                            HudReadout("RECEIVED", "${bt.eventsReceivedDuring}", LtColors.Amber)
                                            HudReadout("DISCONNECTS", "${bt.disconnectsDuring}", LtColors.Amber)
                                            HudReadout("TEST", if (bt.running) "RUNNING" else "ENDED", if (bt.running) LtColors.Green else LtColors.Grey)
                                        }
                                    }
                                    Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                                        SciFiPrimary("START TEST", enabled = active && background?.running != true, large = true) {
                                            repo.startBackgroundTest("screen-off reception")
                                        }
                                        SciFiOutline("STOP TEST", enabled = background?.running == true, large = true) { repo.stopBackgroundTest() }
                                    }
                                }
                            }
                            item {
                                SciFiPanel("EXPORT") {
                                    Text("Writes a diagnostic bundle you can share.", color = LtColors.Ivory, fontSize = 11.sp)
                                    SciFiPrimary("EXPORT DIAGNOSTICS", enabled = true, large = true) {
                                        val file = Exporter.writeBundle(context)
                                        if (file != null) {
                                            context.startActivity(Intent.createChooser(Exporter.shareIntent(context, file), "Share diagnostics"))
                                        }
                                    }
                                }
                            }
                            item {
                                Text("DETECTION EVENTS  //  ${events.size}", color = LtColors.ConsoleEdge,
                                    fontWeight = FontWeight.Bold, fontSize = 13.sp, letterSpacing = 2.sp, style = glowText())
                            }
                            item {
                                Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                                    Text("TEXT SIZE  %.1fx".format(fontScale), color = LtColors.Grey, fontSize = 11.sp, letterSpacing = 1.sp)
                                    SciFiOutline("A-", enabled = textMult > PanelTextScale.MIN_MULT + 0.01f) {
                                        textMult = (textMult - 0.1f).coerceAtLeast(PanelTextScale.MIN_MULT); PanelTextScale.write(context, textMult)
                                    }
                                    SciFiOutline("A+", enabled = textMult < PanelTextScale.MAX_MULT - 0.01f) {
                                        textMult = (textMult + 0.1f).coerceAtMost(PanelTextScale.MAX_MULT); PanelTextScale.write(context, textMult)
                                    }
                                }
                            }
                            item {
                                Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                                    Text("FONT", color = LtColors.Grey, fontSize = 11.sp, letterSpacing = 1.sp)
                                    SciFiOutline(ConsoleFonts.current.name, enabled = true) {
                                        ConsoleFonts.next(); PanelTextScale.writeFont(context, ConsoleFonts.index)
                                    }
                                }
                            }
                            if (events.isEmpty()) {
                                item { Text(emptyEventsMessage(active, status), color = LtColors.Grey, fontSize = 12.sp) }
                            }
                            items(events, key = { it.eventId }) { e -> EventRowCard(e) }
                            item {
                                SciFiOutline(if (showLog) "HIDE LOG  //  ${log.size}" else "SHOW LOG  //  ${log.size}", enabled = true) {
                                    showLog = !showLog
                                }
                            }
                            if (showLog) {
                                items(log.asReversed().take(200)) { l ->
                                    Text("${logFmt.format(Date(l.epochMillis))} [${l.category}] ${l.message}",
                                        color = LtColors.Grey, fontSize = 10.sp, fontFamily = ConsoleFonts.mono)
                                }
                            }
                        }
                    }
                }
            }
            }
            }
        }
    }

    BoxWithConstraints(Modifier.fillMaxSize()) {
        val sliding = maxWidth >= maxHeight && maxWidth < maxHeight * 1.6f
        if (sliding) {
            // Short, wide, near-square screens (the LOKMAT watch): the face is a full-height
            // square on the left, over the panel. It slides off to the left when the operator
            // taps the panel or a face button, leaving a sliver as the handle to bring it back.
            val faceSize = maxHeight
            val sliver = 40.dp
            val travelPx = with(LocalDensity.current) { (faceSize - sliver).toPx() }
            val slide = remember { Animatable(0f) }
            val scope = rememberCoroutineScope()
            val sounds = remember { FaceSounds(context) }
            DisposableEffect(Unit) { onDispose { sounds.release() } }
            fun park() {
                if (faceParked) return
                frozen = live; faceParked = true
                sounds.slideOut()
                scope.launch { slide.animateTo(-travelPx, tween(260)) }
            }
            fun unpark() {
                if (!faceParked) return
                sounds.slideBack()
                scope.launch { slide.animateTo(0f, tween(260)); faceParked = false; frozen = null }
            }
            faceActions.afterButton = { park() }
            faceActions.outsideCircle = { park() }

            // The panel is a fixed layer that never moves: laid out once across the screen,
            // starting right of the parked sliver. The face is a true circle on top of it, so
            // the panel shows through the corners of the face's square, and sliding the face
            // away uncovers what it was hiding. The slide is translation only; nothing is
            // re-measured.
            dataPane(Modifier.fillMaxSize(), PaddingValues(start = sliver + 4.dp, top = 8.dp, end = 8.dp, bottom = 8.dp))
            if (!faceParked) {
                // A tap anywhere in the visible strip only slides the face out; it does not
                // also press whatever control is under the finger.
                Box(Modifier.fillMaxSize().pointerInput(Unit) { detectTapGestures { park() } })
            }
            // Not clipped: the face draws its own black left half and is transparent outside
            // the circle on the right, so the panel shows through the right-hand corners.
            Box(Modifier.size(faceSize).graphicsLayer { translationX = slide.value }) {
                face(Modifier.fillMaxSize())
                if (faceParked) {
                    // The sliver is the handle; it never reaches the face buttons underneath.
                    Box(Modifier.fillMaxHeight().width(sliver).offset(x = faceSize - sliver)
                        .pointerInput(Unit) { detectTapGestures { unpark() } })
                }
            }
        } else if (maxWidth >= maxHeight) {
            Row(Modifier.fillMaxSize()) {
                face(Modifier.weight(0.42f).fillMaxHeight())
                Box(Modifier.width(1.dp).fillMaxHeight().background(LtColors.Dim))
                dataPane(Modifier.weight(0.58f).fillMaxSize(), PaddingValues(10.dp))
            }
        } else {
            Column(Modifier.fillMaxSize()) {
                face(Modifier.fillMaxWidth().weight(0.46f))
                Box(Modifier.fillMaxWidth().height(1.dp).background(LtColors.Dim))
                dataPane(Modifier.fillMaxWidth().weight(0.54f), PaddingValues(10.dp))
            }
        }
    }
}

private class FaceActions { var afterButton: () -> Unit = {}; var outsideCircle: () -> Unit = {} }

/** Everything the face draws from. Captured when the face is parked so it stops redrawing. */
private data class FaceInputs(
    val conn: ConnState, val status: com.projectrevivesolutions.layertime.controller.link.StatusSnapshot?,
    val btOn: Boolean, val battery: Int, val hour: String, val min: String, val date: String,
    val tempValue: String?, val tempUnit: String?, val sunLabel: String, val sunValue: String, val sunAmPm: String?,
)

/** Empty-list text that matches the real state instead of always saying "connect a wand". */
internal fun emptyEventsMessage(connected: Boolean, status: com.projectrevivesolutions.layertime.controller.link.StatusSnapshot?): String = when {
    !connected -> "No detections yet. Connect a wand and start detection."
    status?.monitoring == true -> "No detections yet. Detection is running."
    else -> "No detections yet. Tap START to begin detection."
}

@Composable
private fun SciFiPanel(
    title: String,
    statusColor: Color? = null,
    statusCount: Int? = null,
    content: @Composable () -> Unit,
) {
    Box(Modifier.fillMaxWidth().padding(horizontal = 2.dp, vertical = 3.dp).hudPanel()) {
        Column(Modifier.padding(start = 21.dp, end = 21.dp, top = 19.dp, bottom = 21.dp), verticalArrangement = Arrangement.spacedBy(7.dp)) {
            HudHeader(title, statusColor, statusCount)
            content()
        }
    }
}

/**
 * Panel title: a lit tab that fades out to the right, a pulsing status light,
 * the glowing title, slash accents at the far end, and a tick-marked rail under it.
 */
@Composable
private fun HudHeader(title: String, statusColor: Color?, statusCount: Int?) {
    val edge = LtColors.ConsoleEdge
    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
        Row(
            Modifier.fillMaxWidth()
                .drawBehind {
                    val slant = size.height * 0.6f
                    val w = size.width * 0.82f
                    val tab = Path().apply {
                        moveTo(0f, 0f); lineTo(w, 0f); lineTo(w - slant, size.height); lineTo(0f, size.height); close()
                    }
                    drawPath(tab, Brush.horizontalGradient(listOf(edge.copy(alpha = 0.30f), edge.copy(alpha = 0.10f), Color.Transparent), endX = w))
                    // tube-shaded accent bar at the left end of the tab
                    drawRect(Brush.horizontalGradient(listOf(lerp(edge, Color.Black, 0.5f), lerp(edge, Color.White, 0.5f), lerp(edge, Color.Black, 0.5f)),
                        endX = 4.dp.toPx()), size = Size(4.dp.toPx(), size.height))
                }
                .padding(start = 10.dp, end = 4.dp, top = 3.dp, bottom = 3.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(7.dp),
        ) {
            HudLed(statusColor ?: LtColors.Amber, if (statusColor != null) 11.dp else 8.dp)
            if (statusCount != null && statusColor != null) {
                Text("$statusCount", color = statusColor, fontWeight = FontWeight.Bold, fontSize = 14.sp, fontFamily = ConsoleFonts.mono,
                    style = TextStyle(shadow = Shadow(statusColor, Offset.Zero, 10f)))
            }
            Text(title, color = edge, fontWeight = FontWeight.Bold, fontSize = 12.sp, letterSpacing = 2.sp, style = glowText(),
                modifier = Modifier.weight(1f))
            SlashAccent(edge)
        }
        Box(Modifier.fillMaxWidth().height(3.dp).hudRail(edge))
    }
}

/** A status light that breathes: a glassy dome with a halo that rises and falls. */
@Composable
private fun HudLed(color: Color, dia: Dp) {
    val t = rememberInfiniteTransition(label = "led")
    val a by t.animateFloat(0.35f, 1f, infiniteRepeatable(tween(900), RepeatMode.Reverse), label = "ledA")
    Canvas(Modifier.size(dia * 1.9f)) {
        val c = center; val r = dia.toPx() / 2f
        drawCircle(color.copy(alpha = 0.22f * a), r * 1.85f, c)
        drawCircle(color.copy(alpha = 0.40f * a), r * 1.35f, c)
        drawCircle(Brush.radialGradient(listOf(lerp(color, Color.White, 0.7f), color, lerp(color, Color.Black, 0.45f)),
            center = Offset(c.x - r * 0.3f, c.y - r * 0.3f), radius = r * 1.3f), r, c)
    }
}

/** Four slanted bars of falling brightness, the "////" HUD accent. */
@Composable
private fun SlashAccent(edge: Color) {
    Canvas(Modifier.size(width = 34.dp, height = 11.dp)) {
        val bw = size.width / 6f; val sl = size.height * 0.55f
        for (i in 0 until 4) {
            val x = i * bw * 1.45f
            val p = Path().apply {
                moveTo(x + sl, 0f); lineTo(x + sl + bw, 0f); lineTo(x + bw, size.height); lineTo(x, size.height); close()
            }
            drawPath(p, edge.copy(alpha = 0.9f - i * 0.2f))
        }
    }
}

/**
 * The console's top strip: a dark well across the panel with a lit lower edge,
 * the console name, link state, and event count, and a scan glow sweeping across.
 */
@Composable
private fun HudStatusStrip(active: Boolean, connecting: Boolean, events: Int) {
    val edge = LtColors.ConsoleEdge
    val t = rememberInfiniteTransition(label = "scan")
    val p by t.animateFloat(0f, 1f, infiniteRepeatable(tween(2600, easing = LinearEasing)), label = "scanP")
    val linkText = when { active -> "LINK ONLINE"; connecting -> "LINKING"; else -> "LINK OFFLINE" }
    val linkColor = when { active -> LtColors.Green; connecting -> LtColors.Amber; else -> LtColors.Red }
    Row(
        Modifier.fillMaxWidth().padding(horizontal = 2.dp)
            .drawBehind {
                val r = 8.dp.toPx()
                drawRoundRect(Color(0xFF02070C), cornerRadius = androidx.compose.ui.geometry.CornerRadius(r))
                // sweeping scan glow
                val band = 70.dp.toPx(); val x = -band + (size.width + band) * p
                drawRect(Brush.horizontalGradient(listOf(Color.Transparent, edge.copy(alpha = 0.28f), Color.Transparent), startX = x, endX = x + band),
                    topLeft = Offset(maxOf(0f, x), 0f), size = Size(minOf(band, size.width - maxOf(0f, x)).coerceAtLeast(0f), size.height))
                drawRoundRect(edge.copy(alpha = 0.6f), cornerRadius = androidx.compose.ui.geometry.CornerRadius(r),
                    style = androidx.compose.ui.graphics.drawscope.Stroke(1.2.dp.toPx()))
                drawRect(Brush.horizontalGradient(listOf(Color.Transparent, lerp(edge, Color.White, 0.4f), Color.Transparent)),
                    topLeft = Offset(0f, size.height - 2.dp.toPx()), size = Size(size.width, 2.dp.toPx()))
            }
            .padding(horizontal = 10.dp, vertical = 5.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Text("LT ◆ CONSOLE", color = edge, fontWeight = FontWeight.Bold, fontSize = 11.sp, letterSpacing = 2.sp,
            style = glowText(), modifier = Modifier.weight(1f))
        HudLed(linkColor, 8.dp)
        Text(linkText, color = linkColor, fontWeight = FontWeight.Bold, fontSize = 11.sp, letterSpacing = 1.sp,
            fontFamily = ConsoleFonts.heading, style = TextStyle(shadow = Shadow(linkColor, Offset.Zero, 10f)))
        Text("EVT $events", color = LtColors.Amber, fontWeight = FontWeight.Bold, fontSize = 11.sp, fontFamily = ConsoleFonts.mono)
    }
}

/** A label and a value in a recessed, lit readout well. */
@Composable
private fun HudReadout(label: String, value: String, color: Color) {
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(7.dp)) {
        Text(label, color = LtColors.Grey, fontSize = 10.sp, letterSpacing = 1.5.sp, fontFamily = ConsoleFonts.heading)
        Box(Modifier.readoutWell(color).padding(horizontal = 10.dp, vertical = 1.dp)) {
            Text(value, color = color, fontWeight = FontWeight.Bold, fontSize = 15.sp, fontFamily = ConsoleFonts.mono,
                style = TextStyle(shadow = Shadow(color, Offset.Zero, 12f)))
        }
    }
}

/** Button geometry follows the panel's text scale; [large] is for buttons that are a panel's only controls. */
@Composable
private fun buttonHeight(large: Boolean) = (if (large) 44.dp else 32.dp) * LocalDensity.current.fontScale
private val btnPadLarge = PaddingValues(horizontal = 22.dp, vertical = 0.dp)
private val btnPadPill = PaddingValues(horizontal = 14.dp, vertical = 0.dp)

/** Filled green pill: domed face, tube-shaded rim, specular line, and glow. Disabled, it is dim glass that still reads as a button. */
@Composable
fun SciFiPrimary(text: String, enabled: Boolean, large: Boolean = false, onClick: () -> Unit) {
    val g = LtColors.Green
    val look = if (enabled)
        Modifier.shadow(if (large) 8.dp else 5.dp, PillShape, clip = false, ambientColor = g.copy(alpha = 0.7f), spotColor = g)
            .haloOnly(g, radius = buttonHeight(large) / 2)
            .background(domeBrush(g), PillShape)
            .border(2.dp, railBrush(g), PillShape)
            .pillShine()
    else Modifier.glassPill(g, false).border(1.5.dp, g.copy(alpha = 0.28f), PillShape)
    Button(
        onClick = onClick, enabled = enabled, shape = PillShape, contentPadding = if (large) btnPadLarge else btnPadPill,
        modifier = look.height(buttonHeight(large)),
        elevation = ButtonDefaults.buttonElevation(0.dp, 0.dp, 0.dp, 0.dp, 0.dp),
        colors = ButtonDefaults.buttonColors(
            containerColor = Color.Transparent, contentColor = LtColors.Bg,
            disabledContainerColor = Color.Transparent, disabledContentColor = LtColors.Grey,
        ),
    ) { Text(text, fontWeight = FontWeight.Bold, fontSize = if (large) 13.sp else 11.sp, letterSpacing = 1.sp, fontFamily = ConsoleFonts.heading) }
}

/** Outlined cyan pill: lit glass with a tube rim and glowing text. Disabled, dim glass with a faint rim. */
@Composable
fun SciFiOutline(text: String, enabled: Boolean, large: Boolean = false, onClick: () -> Unit) {
    val e = LtColors.ConsoleEdge
    val glow = if (enabled) Modifier.shadow(if (large) 6.dp else 4.dp, PillShape, clip = false,
        ambientColor = e.copy(alpha = 0.7f), spotColor = e).haloOnly(e, radius = buttonHeight(large) / 2) else Modifier
    OutlinedButton(
        onClick = onClick, enabled = enabled, shape = PillShape, contentPadding = if (large) btnPadLarge else btnPadPill,
        modifier = glow.glassPill(e, enabled).height(buttonHeight(large)),
        border = if (enabled) BorderStroke(2.5.dp, railBrush(e)) else BorderStroke(1.5.dp, e.copy(alpha = 0.28f)),
        colors = ButtonDefaults.outlinedButtonColors(containerColor = Color.Transparent, contentColor = e, disabledContentColor = LtColors.Grey),
    ) {
        Text(text, fontSize = if (large) 13.sp else 11.sp, letterSpacing = 1.sp, fontFamily = ConsoleFonts.heading,
            style = if (enabled) TextStyle(shadow = Shadow(e, Offset.Zero, 9f)) else TextStyle.Default)
    }
}

@Composable
private fun TargetButton(label: String, value: Int, current: Int, onPick: (Int) -> Unit) {
    if (value == current) SciFiPrimary(label, enabled = true) { onPick(value) }
    else SciFiOutline(label, enabled = true) { onPick(value) }
}

@Composable
private fun EventRowCard(e: EventRow) {
    Box(Modifier.fillMaxWidth().padding(horizontal = 2.dp, vertical = 3.dp)
        .hudPanel(edge = LtColors.Amber, radius = 16.dp, elevation = 4.dp)) {
        Row(Modifier.fillMaxWidth().clip(ConsoleShape)) {
            Box(Modifier.width(3.dp).fillMaxHeight().background(LtColors.Amber))
            Column(Modifier.padding(19.dp)) {
                Text("#${e.eventId}  ${e.detectorName}", color = LtColors.Amber,
                    fontWeight = FontWeight.Bold, fontSize = 13.sp, letterSpacing = 1.sp)
                Text("${LinkConstants.SourceKind.name(e.sourceKind)} · ${LinkConstants.Band.name(e.band)} · " +
                    "ch ${e.channel} · ${e.rssi} dBm · x${e.count} · ${e.ageSeconds}s · " +
                    LinkConstants.Confidence.name(e.confidence), color = LtColors.Ivory, fontSize = 11.sp,
                    fontFamily = ConsoleFonts.mono)
                e.sourceId?.let { Text("id: $it", color = LtColors.Grey, fontSize = 11.sp, fontFamily = ConsoleFonts.mono) }
                e.detail?.let { Text("detail: $it", color = LtColors.Grey, fontSize = 11.sp, fontFamily = ConsoleFonts.mono) }
            }
        }
    }
}
