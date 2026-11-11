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
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.tween
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.WindowInsets
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
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.windowInsetsPadding
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
import androidx.compose.material3.TriStateCheckbox
import androidx.compose.material3.CheckboxDefaults
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.key
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.zIndex
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
import com.projectrevivesolutions.layertime.controller.diag.EventsExporter
import com.projectrevivesolutions.layertime.controller.diag.Exporter
import com.projectrevivesolutions.layertime.controller.hunt.HuntSession
import com.projectrevivesolutions.layertime.controller.link.LinkClient
import com.projectrevivesolutions.layertime.controller.mission.EvidenceHold
import com.projectrevivesolutions.layertime.controller.mission.MissionPrefs
import com.projectrevivesolutions.layertime.controller.mission.SensorSelection
import com.projectrevivesolutions.layertime.controller.link.LinkConstants
import com.projectrevivesolutions.layertime.controller.link.LinkMeasure
import com.projectrevivesolutions.layertime.controller.link.ConnNotice
import com.projectrevivesolutions.layertime.controller.link.CardClear
import com.projectrevivesolutions.layertime.controller.link.HunterRoster
import com.projectrevivesolutions.layertime.controller.link.HunterScanner
import com.projectrevivesolutions.layertime.controller.DeviceKind
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.TextButton
import com.projectrevivesolutions.layertime.controller.BuildConfig
import com.projectrevivesolutions.layertime.controller.model.ConnState
import com.projectrevivesolutions.layertime.controller.model.EventRow
import com.projectrevivesolutions.layertime.controller.model.LogEntry
import com.projectrevivesolutions.layertime.controller.service.LinkForegroundService
import com.projectrevivesolutions.layertime.controller.state.ControllerRepository
import com.projectrevivesolutions.layertime.controller.ui.theme.LayerTimeControllerTheme
import com.projectrevivesolutions.layertime.controller.ui.theme.LtColors
import com.projectrevivesolutions.layertime.controller.ui.theme.LocalSpacecraft
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
        enableEdgeToEdge()
        ControllerRepository.deviceReport = DeviceReportBuilder.build(applicationContext)
        // The shipped workflow (deauth triage) loads once; the Builder shows it and TRIAGE runs it.
        com.projectrevivesolutions.layertime.controller.flow.TriageSession.load(applicationContext)
        intent?.getStringExtra(EXTRA_PAGE)?.let { NavRequest.page.value = it }
        setContent {
            LayerTimeControllerTheme {
                Surface(Modifier.fillMaxSize(), color = LtColors.Bg) {
                    Box(Modifier.windowInsetsPadding(WindowInsets.safeDrawing)) { ControllerScreen() }
                }
            }
        }
    }

    override fun onResume() { super.onResume(); com.projectrevivesolutions.layertime.controller.service.AppVisibility.inFront = true }
    override fun onPause() { com.projectrevivesolutions.layertime.controller.service.AppVisibility.inFront = false; super.onPause() }

    /** A detection alert was tapped while the controller was already open. */
    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        intent.getStringExtra(EXTRA_PAGE)?.let { NavRequest.page.value = it }
    }

    companion object {
        /** Console page to open, set by a detection alert ("attacks"). */
        const val EXTRA_PAGE = "com.projectrevivesolutions.layertime.controller.PAGE"

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

/** A page the console should open, from a tapped detection alert; consumed once. */
internal object NavRequest { val page = kotlinx.coroutines.flow.MutableStateFlow<String?>(null) }

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
    fun read(ctx: Context): Float = ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE).getFloat(KEY, defaultMult(ctx)).coerceIn(MIN_MULT, MAX_MULT)
    /**
     * Until the user presses A+ or A-, the text size depends on the device (build 71). The 12 sp
     * minimum came from the watch field test and stays 1.0x there; a phone is held closer and is
     * sharper, so the same sizes looked oversized and it starts at 0.85x. The watch test is the
     * watch layout's (WatchFormFactor), checked orientation-free on the screen's long and short sides.
     */
    fun defaultMult(ctx: Context): Float {
        val c = ctx.resources.configuration
        val long = maxOf(c.screenWidthDp, c.screenHeightDp).toFloat()
        val short = minOf(c.screenWidthDp, c.screenHeightDp).toFloat()
        return if (WatchFormFactor.matches(long, short)) 1.0f else 0.85f
    }
    fun write(ctx: Context, mult: Float) { ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit().putFloat(KEY, mult.coerceIn(MIN_MULT, MAX_MULT)).apply() }
    fun readFont(ctx: Context): Int = ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE).getInt("font_set", 1)
    fun writeFont(ctx: Context, i: Int) { ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit().putInt("font_set", i).apply() }
}
private val panelShape = CutCornerShape(topStart = 10.dp, bottomEnd = 10.dp)
private val btnShape = CutCornerShape(topStart = 6.dp, bottomEnd = 6.dp)
private val dataShape = RoundedCornerShape(16.dp)

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
    val linkStale by repo.linkStale.collectAsState()
    val reconnecting by repo.reconnecting.collectAsState()
    val connDetail by repo.connDetail.collectAsState()
    val huntOpen by HuntSession.open.collectAsState()
    // HUNTER ACTIONS (2026-11-06).
    val roster by HunterRoster.hunters.collectAsState()
    val rosterScan by HunterRoster.scan.collectAsState()
    val cardClear by CardClear.state.collectAsState()
    var confirmClear by remember { mutableStateOf<HunterRoster.Hunter?>(null) }
    // The Link client is not observable, so which Hunter it holds is read each second.
    var heldAddress by remember { mutableStateOf<String?>(null) }
    var heldName by remember { mutableStateOf<String?>(null) }
    var heldNoSdLog by remember { mutableStateOf<Boolean?>(null) }
    LaunchedEffect(Unit) {
        while (true) {
            val c = LinkForegroundService.active
            heldAddress = c?.currentAddress; heldName = c?.currentName; heldNoSdLog = c?.lastNoSdLog
            kotlinx.coroutines.delay(1000)
        }
    }
    // What to do once Bluetooth permission is granted (Scan, or Connect on a row).
    var afterPermission by remember { mutableStateOf<(() -> Unit)?>(null) }

    var showLog by remember { mutableStateOf(false) }
    val navigation=remember {PageNavigation()}
    var page by navigation
    val backDispatcher=androidx.activity.compose.LocalOnBackPressedDispatcherOwner.current?.onBackPressedDispatcher
    fun goBack(){if(DeviceKind.isLokmatWatch)navigation.back() else backDispatcher?.onBackPressed()}
    androidx.activity.compose.BackHandler(enabled=!DeviceKind.isLokmatWatch){navigation.back()}
    var selectedEvent by remember { mutableStateOf<EventRow?>(null) }
    val sensorsOn = remember { mutableStateMapOf<Int, Boolean>().apply { MissionPrefs.readSensors(context).forEach { put(it, true) } } }
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
        val then = afterPermission
        afterPermission = null
        if (granted) (then ?: { LinkForegroundService.start(context, target = null) })()
        else repo.log(LogEntry.Category.ERROR, "permissions denied; cannot scan or connect")
    }

    var locationNotice by remember { mutableStateOf<String?>(null) }
    fun connect(address: String? = null, name: String? = null) {
        if (LinkClient.locationSwitchBlocksScanning(context)) {
            locationNotice = LinkClient.LOCATION_OFF_MESSAGE
            repo.log(LogEntry.Category.ERROR, LinkClient.LOCATION_OFF_MESSAGE)
            return
        }
        locationNotice = null
        val missing = MainActivity.requiredPermissions().any {
            ContextCompat.checkSelfPermission(context, it) != PackageManager.PERMISSION_GRANTED
        }
        val go = { LinkForegroundService.start(context, target = null, address = address, name = name) }
        if (missing) { afterPermission = go; permLauncher.launch(MainActivity.requiredPermissions()) }
        else go()
    }

    // HUNTER ACTIONS: SCAN, and Clear Card once the operator says ERASE.
    fun scanHunters() {
        val missing = MainActivity.requiredPermissions().any {
            ContextCompat.checkSelfPermission(context, it) != PackageManager.PERMISSION_GRANTED
        }
        // The Hunter this phone holds, whether the handshake has finished or not: it is not
        // advertising, so the scan cannot hear it (2026-11-06 capture: "held Hunter none").
        val held = LinkForegroundService.active?.takeIf { conn != ConnState.IDLE }?.let { c ->
            c.currentAddress?.let { a ->
                HunterScanner.Connected(a, c.currentName ?: "Hunter", when (c.lastNoSdLog) {
                    true -> HunterRoster.Card.NO; false -> HunterRoster.Card.YES; null -> HunterRoster.Card.UNKNOWN })
            }
        }
        val go = { HunterScanner.scan(context, held) }
        if (missing) { afterPermission = go; permLauncher.launch(MainActivity.requiredPermissions()) }
        else go()
    }
    fun eraseCard(h: HunterRoster.Hunter) {
        val c = LinkForegroundService.active
        if (c != null && c.connectedAddress == h.address) { c.clearCard(h.address); return }
        // Not connected to it: connect, and the clear goes after HELLO.
        LinkClient.pendingClearAddress = h.address
        CardClear.set(CardClear.State.Connecting(h.name))
        connect(h.address, h.name)
    }
    confirmClear?.let { h ->
        androidx.compose.ui.window.Dialog(onDismissRequest={confirmClear=null}) {
            OpsConnectionHousing(Modifier.fillMaxWidth()) {
                Column(Modifier.fillMaxWidth().padding(18.dp).verticalScroll(rememberScrollState()),verticalArrangement=Arrangement.spacedBy(12.dp)) {
                    FrameBackControl(onBack={confirmClear=null})
                    Text("Clear Card",color=LtColors.Cyan,fontSize=18.sp)
                    Text(CardClear.confirmText(h.name),color=LtColors.Ivory,fontSize=15.sp)
                    SciFiPrimary("Erase",true){confirmClear=null;eraseCard(h)}
                    FrameBackControl(bottom=true,onBack={confirmClear=null})
                }
            }
        }
    }

    val active = conn == ConnState.CONNECTED
    // MISSION's note: what the Hunter is doing with the sensor list (SetSensors, 2026-11-04).
    val sensorsApplied by repo.sensorsApplied.collectAsState()
    val missionSelection = sensorsOn.filterValues { it }.keys.toSet()
    val missionLink = when {
        !active -> MissionLink.OFFLINE
        ((server?.capabilities ?: 0) and LinkConstants.Capability.SENSOR_SET) == 0 -> MissionLink.OLD_FIRMWARE
        sensorsApplied == SensorSelection.mask(missionSelection) -> MissionLink.APPLIED
        else -> MissionLink.SENDING
    }
    // SELECT SENSORS on OPS is a shortcut into MISSION and reads back from it (Michael,
    // 2026-11-04): ALL SENSORS checks every box, ONLY DEAUTH leaves Deauth alone, and a
    // hand-edited list lights neither. START's Recon target follows the list: the Wi-Fi-only
    // deauth lock when the list is Deauth alone, the full sweep (with the list applied on
    // the Hunter) otherwise.
    // Quick Start's All Sensors: every sensor but the trackers (2026-11-06).
    val everySensor = SensorSelection.DEFAULT_SENSORS
    val onlyDeauth = setOf(LinkConstants.ReconTarget.DEAUTH)
    val target = if (missionSelection == onlyDeauth) LinkConstants.ReconTarget.DEAUTH else LinkConstants.ReconTarget.ALL
    val scanPick = when (missionSelection) { everySensor -> LinkConstants.ReconTarget.ALL; onlyDeauth -> LinkConstants.ReconTarget.DEAUTH; else -> -1 }
    val scanning = status?.monitoring == true
    // QUICK START's badge: a hunt first, then stopped, then which page defined the running list.
    val scanMode = when {
        huntOpen -> ScanMode.HUNTING
        !active || !scanning -> ScanMode.NOT_SCANNING
        scanPick == -1 -> ScanMode.ON_MISSION
        else -> ScanMode.QUICKSTART
    }
    // Saves the list and sends it; the one path for both MISSION layouts.
    val setMission: (Set<Int>) -> Unit = { selection ->
        sensorsOn.clear()
        selection.forEach { sensorsOn[it] = true }
        MissionPrefs.writeSensors(context, selection)
        LinkForegroundService.active?.sensorsChanged()
    }
    // A QUICK START pick while scanning stops the scan: the operator presses START to run the new list.
    // MISSION's evidence hold (2026-11-04): saved here, sent to the Hunter that takes it.
    var evidenceHold by remember { mutableIntStateOf(MissionPrefs.readEvidenceHold(context)) }
    val evidenceHoldApplied by repo.evidenceHoldApplied.collectAsState()
    val evidenceLink = when {
        !active -> EvidenceHold.Link.OFFLINE
        ((server?.capabilities ?: 0) and LinkConstants.Capability.EVIDENCE_HOLD) == 0 -> EvidenceHold.Link.OLD_FIRMWARE
        evidenceHoldApplied == evidenceHold -> EvidenceHold.Link.APPLIED
        else -> EvidenceHold.Link.SENDING
    }
    val setEvidenceHold: (Int) -> Unit = { sec ->
        evidenceHold = EvidenceHold.normalize(sec)
        MissionPrefs.writeEvidenceHold(context, evidenceHold)
        LinkForegroundService.active?.evidenceHoldChanged()
    }
    val quickPick: (Set<Int>) -> Unit = { selection ->
        if (scanning) LinkForegroundService.active?.reconStop()
        setMission(selection)
    }
    val connecting = conn in listOf(ConnState.SCANNING, ConnState.CONNECTING, ConnState.DISCOVERING, ConnState.HANDSHAKING)
    val shownHunters = HunterRoster.withHeld(roster, if (conn == ConnState.IDLE) null else heldAddress, heldName, if (conn == ConnState.IDLE) null else heldNoSdLog)
    val scanBusy = rosterScan is HunterRoster.Scan.Listening || rosterScan is HunterRoster.Scan.Checking
    val clearBusy = cardClear is CardClear.State.Connecting || cardClear is CardClear.State.Clearing || cardClear is CardClear.State.Verifying
    // Amber when the clear failed or the card still holds something afterwards.
    val clearProblem = cardClear is CardClear.State.Failed ||
        (cardClear as? CardClear.State.Done)?.let { !(it.complete && CardClear.clean(it.remaining)) } == true
    val wands = if (active) 1 else 0

    // On the sliding layout the face is parked off screen while the operator works
    // in the panel. Its inputs are frozen then, so it is not redrawn every second.
    var faceParked by remember { mutableStateOf(false) }
    var frozen by remember { mutableStateOf<FaceInputs?>(null) }
    // Ring data: ambient temperature only from a real sensor; sunrise/sunset from location.
    var ambientC by remember { mutableStateOf<Float?>(null) }
    val hasTempSensor = remember { RingData.hasAmbientTemperature(context) }
    // A sensor that sends nothing within 10 s is treated as absent (the LOKMAT watch lists an
    // MTK ambient sensor that has never produced a reading).
    var tempSilent by remember { mutableStateOf(false) }
    var satUsed by remember { mutableIntStateOf(0) }
    var satSeen by remember { mutableIntStateOf(0) }
    var location by remember { mutableStateOf(RingData.lastKnown(context)) }
    var locPerm by remember { mutableStateOf(RingData.hasLocationPermission(context)) }
    // Location is asked for when the app opens, not only at SCAN & CONNECT, because the
    // sunrise and sunset ring needs it whether or not a wand is connected.
    val locLauncher = rememberLauncherForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) {
        locPerm = RingData.hasLocationPermission(context)
    }
    LaunchedEffect(Unit) {
        if (!locPerm) locLauncher.launch(arrayOf(Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_COARSE_LOCATION))
    }
    DisposableEffect(Unit) {
        val stop = RingData.watchAmbientTemperature(context, onSilent = { tempSilent = true }) { ambientC = it; tempSilent = false }
        onDispose { stop?.invoke() }
    }
    // Keep after a GPS fix: try for 3 minutes, rest 5, until one arrives; then refresh every
    // 30 minutes. The LOKMAT watch has no network location, so GPS is the only source.
    // Until the first fix there is no saved position, so the receiver keeps searching (a cold
    // start with no assisted GPS, as on the LOKMAT watch without a SIM, can take many minutes).
    // After that, a 3-minute refresh every 30 minutes.
    LaunchedEffect(locPerm) {
        if (!locPerm) return@LaunchedEffect
        while (true) {
            val fix = RingData.awaitFix(context, if (location == null) 60 * 60_000L else 180_000L)
            if (fix != null) location = fix
            delay(if (location != null) 30 * 60_000L else 60_000L)
        }
    }
    DisposableEffect(locPerm) {
        val stop = if (locPerm) RingData.watchSatellites(context) { u, n -> satUsed = u; satSeen = n } else null
        onDispose { stop?.invoke() }
    }
    val fahrenheit = remember { RingData.useFahrenheit() }
    // N/A only when the device has no sensor; "--" while a sensor has not reported yet.
    val tempValue = ambientC?.let { c -> "%.0f°".format(if (fahrenheit) c * 9f / 5f + 32f else c) }
        ?: if (hasTempSensor && !tempSilent) "--" else null
    val tempUnit = if (ambientC != null) (if (fahrenheit) "°F" else "°C") else null
    // Position for the SUN ring: a GPS fix or a grid the operator entered, whichever is newer.
    // A fresh GPS fix takes over from an older manual entry; entering a grid after the last
    // fix means the operator knows better where they are now.
    var manual by remember { mutableStateOf(RingData.manualPosition(context)) }
    val useManual = manual != null && (location == null || manual!!.time > location!!.time)
    val posLat = if (useManual) manual!!.lat else location?.latitude
    val posLon = if (useManual) manual!!.lon else location?.longitude
    val sun = remember(nowMs / 60_000L, posLat, posLon) {
        if (posLat != null && posLon != null) SunTimes.next(posLat, posLon, nowMs, java.util.TimeZone.getDefault()) else null
    }
    val sunLabel = when (sun?.first) {
        true -> "SUNRISE"; false -> "SUNSET"
        null -> if (posLat != null) "SUN" else if (!locPerm) "NO LOC" else "NO FIX"
    }
    val sunValue = sun?.let { SimpleDateFormat(if (is24) "H:mm" else "h:mm", Locale.US).format(Date(it.second)) } ?: "--"
    val sunAmPm = if (sun != null && !is24) SimpleDateFormat("a", Locale.US).format(Date(sun.second)) else null
    // A tag on top of the ring: MANUAL whenever the position was entered by hand, so it is never
    // mistaken for GPS; with no position yet, the receiver working (satellites used of seen).
    val sunTag = when {
        sun != null && useManual -> "MANUAL"
        posLat == null && locPerm -> "SAT $satUsed/$satSeen"
        else -> null
    }

    val live = FaceInputs(conn, status, btOn, battery, hourText, minText, dateText,
        tempValue, tempUnit, sunLabel, sunValue, sunAmPm, sunTag, linkStale, reconnecting)
    val shown = if (faceParked) (frozen ?: live) else live
    // Plain holder, not state: the sliding layout installs its park action here each
    // composition without triggering another recomposition.
    val faceActions = remember { FaceActions() }
    // A tapped detection alert opens DETECTED ATTACKS and slides the face out of the way.
    val navPage by NavRequest.page.collectAsState()
    LaunchedEffect(navPage) {
        navPage?.let { page = it; NavRequest.page.value = null; faceActions.afterButton() }
    }

    // sliding: the rolling watch layout, where the caller draws the black half, the
    // light, and the cast shadow as separate layers that do not turn with the face.
    val face: @Composable (Modifier, Boolean) -> Unit = { m, sliding ->
        FaceView(shown.conn, shown.status, shown.btOn, shown.battery, shown.hour, shown.min, shown.date, linkStale = shown.linkStale, reconnecting = shown.reconnecting,
            tempValue = shown.tempValue, tempUnit = shown.tempUnit,
            sunLabel = shown.sunLabel, sunValue = shown.sunValue, sunAmPm = shown.sunAmPm, sunTag = shown.sunTag,
            onCounter = { page = "attacks"; faceActions.afterButton() }, onRecon = { page = "config"; faceActions.afterButton() },
            onOutsideCircle = { faceActions.outsideCircle() }, modifier = m, drawLeftBlack = !sliding, drawLighting = !sliding,
            refined = !sliding)
    }

    var textMult by remember { mutableStateOf(PanelTextScale.read(context)) }
    LaunchedEffect(Unit) { ConsoleFonts.index = PanelTextScale.readFont(context) }

    // onHull: the watch layout, where the console's frame is part of the salvaged hull drawn
    // behind it (HullRelief.kt), so the pane draws no frame or surface of its own.
    val dataPane: @Composable (Modifier, PaddingValues, Boolean) -> Unit = { paneMod, outerPad, onHull ->
        BoxWithConstraints(paneMod.padding(outerPad)) {
            val autoScale = PanelTextScale.auto(maxWidth.value, maxHeight.value)
            val fontScale = autoScale * textMult * (if (onHull) 1f else LocalDensity.current.fontScale)
            val base = LocalDensity.current
            CompositionLocalProvider(LocalDensity provides Density(base.density, fontScale), LocalSpacecraft provides !onHull,
                LocalSystemScreen provides (!onHull && page == "system")) {
            ProvideTextStyle(TextStyle(fontFamily = if (onHull) ConsoleFonts.body else FontFamily.SansSerif)) {
            Box(
                Modifier.fillMaxSize().clip(RoundedCornerShape(20.dp))
                    .padding(if(onHull) CONSOLE_BAND else 0.dp)
            ) {
                SectionWindow(framed=!onHull,onBack={goBack()}) {
                when (page) {
                    "config" -> {
                        if (!onHull) {
                            MissionScreen(selected = missionSelection, link = missionLink, onSelectionChange = setMission,
                                evidenceHold = evidenceHold, evidenceLink = evidenceLink, onEvidenceHoldChange = setEvidenceHold)
                        } else {
                        LazyColumn(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                            item {
                                Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically,
                                    horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                                    SciFiOutline("‹ BACK", enabled = true) { page = "main" }
                                    Text("MISSION CONFIGURATION", color = LtColors.ConsoleEdge, fontWeight = FontWeight.Bold,
                                        fontSize = 16.sp, letterSpacing = 2.sp, style = glowText())
                                }
                            }
                            item { Text("Check a sensor to let the Hunter report it. A preset checks its whole group.", color = LtColors.Ivory, fontSize = 12.sp) }
                            item {
                                val (title, text) = missionNote(missionLink, missionSelection)
                                Text("$title. $text", color = if (missionLink == MissionLink.APPLIED && missionSelection.isNotEmpty()) LtColors.Green else LtColors.Amber,
                                    fontSize = 12.sp)
                            }
                            item { Text("PRESETS", color = LtColors.ConsoleEdge, fontWeight = FontWeight.Bold, fontSize = 13.sp, letterSpacing = 2.sp) }
                            items(SensorSelection.GROUPS) { g ->
                                val st = SensorSelection.presetState(g.preset, missionSelection)
                                val lit = st != SensorSelection.PresetState.NONE
                                Row(
                                    Modifier.fillMaxWidth().padding(horizontal = 2.dp, vertical = 2.dp)
                                        .hudPanel(edge = if (lit) LtColors.Green else LtColors.ConsoleEdge, radius = 16.dp, elevation = if (lit) 4.dp else 1.dp, halo = st == SensorSelection.PresetState.ALL)
                                        .clip(ConsoleShape).padding(horizontal = 19.dp, vertical = 8.dp),
                                    verticalAlignment = Alignment.CenterVertically,
                                ) {
                                    TriStateCheckbox(
                                        state = when (st) {
                                            SensorSelection.PresetState.ALL -> androidx.compose.ui.state.ToggleableState.On
                                            SensorSelection.PresetState.PARTIAL -> androidx.compose.ui.state.ToggleableState.Indeterminate
                                            SensorSelection.PresetState.NONE -> androidx.compose.ui.state.ToggleableState.Off
                                        },
                                        onClick = { setMission(SensorSelection.togglePreset(g.preset, missionSelection)) },
                                        colors = CheckboxDefaults.colors(
                                            checkedColor = LtColors.Green, uncheckedColor = LtColors.Grey, checkmarkColor = LtColors.Bg,
                                        ),
                                    )
                                    Text(g.label, color = if (lit) LtColors.Green else LtColors.Ivory,
                                        fontWeight = FontWeight.Bold, fontSize = 14.sp, letterSpacing = 1.sp)
                                }
                            }
                            item { Text("SENSORS", color = LtColors.ConsoleEdge, fontWeight = FontWeight.Bold, fontSize = 13.sp, letterSpacing = 2.sp) }
                            items(SensorSelection.ALL_SENSORS) { id ->
                                val on = sensorsOn[id] == true
                                Row(
                                    Modifier.fillMaxWidth().padding(horizontal = 2.dp, vertical = 2.dp)
                                        .hudPanel(edge = if (on) LtColors.Green else LtColors.ConsoleEdge, radius = 16.dp, elevation = if (on) 4.dp else 1.dp, halo = on)
                                        .clip(ConsoleShape).padding(horizontal = 19.dp, vertical = 8.dp),
                                    verticalAlignment = Alignment.CenterVertically,
                                ) {
                                    Checkbox(
                                        checked = on, onCheckedChange = {
                                            setMission(if (it) missionSelection + id else missionSelection - id)
                                        },
                                        colors = CheckboxDefaults.colors(
                                            checkedColor = LtColors.Green, uncheckedColor = LtColors.Grey, checkmarkColor = LtColors.Bg,
                                        ),
                                    )
                                    Text(LinkConstants.ReconTarget.name(id),
                                        color = if (on) LtColors.Green else LtColors.Ivory,
                                        fontWeight = FontWeight.Bold, fontSize = 14.sp, letterSpacing = 1.sp)
                                }
                            }
                            // The evidence hold (2026-11-04), the same setting as the phone page's EVIDENCE HOLD.
                            item { Text("EVIDENCE HOLD", color = LtColors.ConsoleEdge, fontWeight = FontWeight.Bold, fontSize = 13.sp, letterSpacing = 2.sp) }
                            item {
                                Text("${EvidenceHold.meaning(evidenceHold)} ${EvidenceHold.status(evidenceLink)}",
                                    color = if (evidenceLink == EvidenceHold.Link.APPLIED) LtColors.Green else LtColors.Amber, fontSize = 12.sp)
                            }
                            items(EvidenceHold.PRESETS) { sec ->
                                val picked = sec == evidenceHold
                                Row(
                                    Modifier.fillMaxWidth().padding(horizontal = 2.dp, vertical = 2.dp)
                                        .hudPanel(edge = if (picked) LtColors.Green else LtColors.ConsoleEdge, radius = 16.dp, elevation = if (picked) 4.dp else 1.dp, halo = picked)
                                        .clip(ConsoleShape).padding(horizontal = 19.dp, vertical = 8.dp),
                                    verticalAlignment = Alignment.CenterVertically,
                                ) {
                                    androidx.compose.material3.RadioButton(
                                        selected = picked, onClick = { setEvidenceHold(sec) },
                                        colors = androidx.compose.material3.RadioButtonDefaults.colors(
                                            selectedColor = LtColors.Green, unselectedColor = LtColors.Grey,
                                        ),
                                    )
                                    Text(EvidenceHold.label(sec), color = if (picked) LtColors.Green else LtColors.Ivory,
                                        fontWeight = FontWeight.Bold, fontSize = 14.sp, letterSpacing = 1.sp)
                                }
                            }
                        }
                        }
                    }

                    "attacks" -> {
                        if (!onHull) {
                            EventsScreen(events, emptyEventsMessage(active, status),
                                onImport = { page = "capture" },
                                onOpen = { selectedEvent = it; page = "detail" },
                                onImportCard = { page = "card" })
                        } else {
                        LazyColumn(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                            item {
                                Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically,
                                    horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                                    SciFiOutline("‹ BACK", enabled = true) { page = "main" }
                                    Text("DETECTED ATTACKS  //  ${events.size}", color = LtColors.ConsoleEdge,
                                        fontWeight = FontWeight.Bold, fontSize = 16.sp, letterSpacing = 2.sp, style = glowText())
                                }
                            }
                            item {
                                SciFiOutline("DECODE A CAPTURE FILE", enabled = true, large = true) { page = "capture" }
                            }
                            if (events.isEmpty()) {
                                item { Text(emptyEventsMessage(active, status), color = LtColors.Grey, fontSize = 14.sp) }
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
                                            fontWeight = FontWeight.Bold, fontSize = 16.sp, letterSpacing = 1.sp)
                                        Text("${LinkConstants.SourceKind.name(e.sourceKind)} · ${LinkConstants.Band.name(e.band)} · ch ${e.channel} · ${e.rssi} dBm · x${e.count}   → TAP FOR DETAIL",
                                            color = LtColors.Ivory, fontSize = 12.sp, fontFamily = ConsoleFonts.mono)
                                    }
                                }
                            }
                        }
                        }
                    }

                    "builder" -> BuilderScreen(onWireshark = { page = "wireshark" }, onTimeline = { page = "timeline" })

                    "capture" -> CaptureOnlyScreen(onBack = { goBack() }, onOpenWireshark = { page = "wireshark" })

                    "wireshark" -> WiresharkScreen { goBack() }
                    "card" -> HunterCardScreen(onBack = { goBack() }, onOpenWireshark = { page = "wireshark" })

                    "timeline" -> AttackTimelineScreen(onBack = { goBack() }, onOpenWireshark = { page="wireshark" })

                    "detail" -> {
                        // The live row, so QUICK LOOK follows updates while it is open.
                        val ev = selectedEvent?.let { sel -> events.firstOrNull { it.eventId == sel.eventId && it.sessionId == sel.sessionId } ?: sel }
                        if (ev != null) EventDetailScreen(ev, deviceLabel, conn, onBack = { goBack() }, onOpenWireshark = { page = "wireshark" })
                        else LazyColumn(Modifier.fillMaxSize()) { item { Text("No detection selected.", color = LtColors.Grey) } }
                    }

                    // TOOLS: the analytics home. Build 77 starts it with capture decode and the
                    // measurement panel; the live Wireshark window and the rest arrive here next,
                    // as windows, per claude/layerscope_analytics_design.md.
                    "tools" -> {
                        if (!onHull) {
                            ToolsScreen(
                                onWireshark = { page = "wireshark" }, onTimeline = { page = "timeline" },
                            )
                        } else {
                        LazyColumn(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                            item {
                                SciFiPanel("ANALYTICS TOOLS") {
                                    Text("Analysis you run in the field, in real time. Each tool opens as its own window.",
                                        color = LtColors.Ivory, fontSize = 12.sp)
                                    SciFiPrimary("OPEN WIRESHARK", enabled = true, large = true) { page = "wireshark" }
                                    SciFiPrimary("ATTACK TIMELINE", enabled = true, large = true) { page = "timeline" }
                                    Text("Wireshark decodes a capture; the timeline reads the same frames per transmitter. " +
                                        "Both feed on live frames once Link 0.2 lands. A transmitter dossier comes next.",
                                        color = LtColors.Amber, fontSize = 12.sp)
                                }
                            }
                            if (BuildConfig.DEBUG) item { LinkMeasurePanel(active) }
                        }
                        }
                    }

                    // SYSTEM: position, diagnostics, display, and the log. The panels that used
                    // to sit at the bottom of OPS moved here in build 78.
                    "system" -> {
                        LazyColumn(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                            item {
                                PositionPanel(
                                    gpsTime = location?.time, manual = manual, useManual = useManual,
                                    posLat = posLat, posLon = posLon,
                                    onSet = { lat, lon -> manual = RingData.saveManual(context, lat, lon) },
                                    onClear = { RingData.clearManual(context); manual = null },
                                )
                            }
                            item { LayerTimeButtonPanel(nowMs) }
                            // Moved here from HUNTER ACTIONS (Michael, 2026-11-06). Only the LOKMAT
                            // watch uses it; on every other device it is greyed out and has no effect.
                            item {
                                val applies = DeviceKind.isLokmatWatch
                                var swipeOff by remember { mutableStateOf(LinkForegroundService.swipeDisconnects(context)) }
                                SciFiPanel("LINK") {
                                    FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                                        Text("Swipe away disconnects", color = if (applies) LtColors.Ivory else LtColors.Grey, fontSize = 12.sp,
                                            letterSpacing = 1.sp, modifier = Modifier.align(Alignment.CenterVertically))
                                        val flip = { swipeOff = !swipeOff; LinkForegroundService.setSwipeDisconnects(context, swipeOff) }
                                        if (applies && swipeOff) SciFiPrimary("ON", enabled = true) { flip() }
                                        else SciFiOutline("OFF", enabled = applies) { flip() }
                                    }
                                    Text(if (applies) "When this is on, swiping LayerScope away from the recent apps disconnects the Hunter."
                                        else "This setting applies only to the LOKMAT watch.",
                                        color = LtColors.Grey, fontSize = 12.sp)
                                }
                            }
                            if (BuildConfig.DEBUG && !onHull) item { LinkMeasurePanel(active) }
                            item {
                                SciFiPanel("SCREEN-OFF RECEPTION TEST") {
                                    Text("Start, turn the screen off for a while, turn it on, then stop.", color = LtColors.Ivory, fontSize = 12.sp)
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
                                SciFiPanel("DISPLAY") {
                                    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                                        Text("TEXT SIZE  %.1fx".format(fontScale), color = LtColors.Grey, fontSize = 12.sp, letterSpacing = 1.sp)
                                        SciFiOutline("A-", enabled = textMult > PanelTextScale.MIN_MULT + 0.01f) {
                                            textMult = (textMult - 0.1f).coerceAtLeast(PanelTextScale.MIN_MULT); PanelTextScale.write(context, textMult)
                                        }
                                        SciFiOutline("A+", enabled = textMult < PanelTextScale.MAX_MULT - 0.01f) {
                                            textMult = (textMult + 0.1f).coerceAtMost(PanelTextScale.MAX_MULT); PanelTextScale.write(context, textMult)
                                        }
                                    }
                                    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                                        Text("FONT", color = LtColors.Grey, fontSize = 12.sp, letterSpacing = 1.sp)
                                        SciFiOutline(ConsoleFonts.current.name, enabled = true) {
                                            ConsoleFonts.next(); PanelTextScale.writeFont(context, ConsoleFonts.index)
                                        }
                                    }
                                }
                            }
                            item {
                                SciFiPanel("EXPORT") {
                                    Text("Writes a diagnostic bundle you can share.", color = LtColors.Ivory, fontSize = 12.sp)
                                    SciFiPrimary("EXPORT DIAGNOSTICS", enabled = true, large = true) {
                                        val file = Exporter.writeBundle(context)
                                        if (file != null) {
                                            context.startActivity(Intent.createChooser(Exporter.shareIntent(context, file), "Share diagnostics"))
                                        }
                                    }
                                    Text("Writes the detections as CSV and JSON, filtered to cut the tracker-beacon flood. Saved to the app folder and offered to share.", color = LtColors.Ivory, fontSize = 12.sp)
                                    SciFiPrimary("EXPORT EVENTS", enabled = true, large = true) {
                                        val w = EventsExporter.writeSnapshot(context)
                                        if (w != null) {
                                            context.startActivity(Intent.createChooser(EventsExporter.shareIntent(context, w.shareCsv), "Share events"))
                                        }
                                    }
                                }
                            }
                            item {
                                SciFiOutline(if (showLog) "HIDE LOG  //  ${log.size}" else "SHOW LOG  //  ${log.size}", enabled = true) { showLog = !showLog }
                            }
                            if (showLog) {
                                items(log.asReversed().take(200)) { l ->
                                    Text("${logFmt.format(Date(l.epochMillis))} [${l.category}] ${l.message}",
                                        color = LtColors.Grey, fontSize = 12.sp, fontFamily = ConsoleFonts.mono)
                                }
                            }
                        }
                    }

                    else -> {
                        LazyColumn(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(if (onHull) 8.dp else 0.dp)) {
                            if (onHull) item {
                                SciFiPanel("FIELD STATUS", statusColor = if (active && !linkStale) LtColors.Green else LtColors.Red) {
                                    StatusRow("Hunter", if (active) "Online" else "Offline", if (active) LtColors.Green else LtColors.Red)
                                    if (active && deviceLabel.isNotBlank()) StatusRow("Device", deviceLabel.uppercase(), LtColors.Cyan)
                                    StatusRow("Active sensor", activeSensorLabel(huntOpen, status), LtColors.Cyan)
                                    HudReadout("DETECTIONS", "${events.size}", LtColors.Amber)
                                    events.maxByOrNull { it.eventId }?.let {
                                        StatusRow("Last alert", "${it.detectorName} detected", LtColors.Amber)
                                    }
                                }
                            }
                            if (onHull) item { HudStatusStrip(active, connecting, events.size, linkStale) }
                            item {
                                if (!onHull) {
                                    OpsHunterActionsPanel(
                                        hunters = shownHunters, scanning = scanBusy, scanSentence = HunterRoster.sentence(rosterScan),
                                        currentAddress = if (conn == ConnState.IDLE) null else heldAddress,
                                        online = active, connecting = connecting,
                                        metadata = server?.takeIf { active }?.let { "Server ${it.serverMajor}.${it.serverMinor}   |   Session ${it.sessionId}   |   Max frame ${it.maxFrame} B" },
                                        notice = locationNotice?.let { ConnNotice.Notice(it, ConnNotice.Tone.PROBLEM) }
                                            ?: ConnNotice.of(conn, connDetail, reconnecting),
                                        clearSentence = CardClear.sentence(cardClear), clearFailed = clearProblem,
                                        clearBusy = clearBusy,
                                        onScan = { scanHunters() }, onConnect = { h -> connect(h.address, h.name) },
                                        onDisconnect = { LinkForegroundService.stop(context) }, onClearCard = { h -> confirmClear = h },
                                    )
                                } else {
                                // HUNTER ACTIONS (Michael, 2026-11-06): SCAN, then a row per Hunter.
                                SciFiPanel("HUNTER ACTIONS", statusColor = if (active) LtColors.Green else LtColors.Red, statusCount = shownHunters.size) {
                                    if (active && server != null) {
                                        Text("Server 0.${server!!.serverMinor}   |   Session ${server!!.sessionId}   |   Max frames ${server!!.maxFrame}",
                                            color = LtColors.Ivory, fontSize = 12.sp, fontFamily = ConsoleFonts.mono)
                                    }
                                    SciFiPrimary(if (scanBusy) "Scanning…" else "Scan", enabled = !scanBusy, large = true) { scanHunters() }
                                    HunterRoster.sentence(rosterScan)?.let { Text(it, color = LtColors.Cyan, fontSize = 12.sp) }
                                    // Why the link is where it is, and what to tap next (ConnNotice).
                                    val notice = locationNotice?.let { ConnNotice.Notice(it, ConnNotice.Tone.PROBLEM) }
                                        ?: ConnNotice.of(conn, connDetail, reconnecting)
                                    notice?.let {
                                        Text(it.text, color = if (it.tone == ConnNotice.Tone.PROBLEM) LtColors.Red else LtColors.Cyan,
                                            fontSize = 12.sp)
                                    }
                                    CardClear.sentence(cardClear)?.let {
                                        Text(it, color = if (clearProblem) LtColors.Red else LtColors.Cyan, fontSize = 12.sp)
                                    }
                                    if (shownHunters.isEmpty() && !scanBusy) Text("Tap Scan to find the Hunters in range.", color = LtColors.Grey, fontSize = 12.sp)
                                    val held = if (conn == ConnState.IDLE) null else heldAddress
                                    shownHunters.forEach { h ->
                                        val mine = h.address == held
                                        Text(h.name.uppercase() + when {
                                                mine && active -> "  //  CONNECTED"; mine && connecting -> "  //  LINKING"
                                                h.card == HunterRoster.Card.YES -> "  //  SD CARD"; h.card == HunterRoster.Card.NO -> "  //  NO SD CARD"; else -> "" },
                                            color = if (mine && active) LtColors.Green else LtColors.Cyan, fontSize = 13.sp,
                                            fontFamily = ConsoleFonts.mono, letterSpacing = 1.sp)
                                        FlowRow(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                                            SciFiPrimary("Connect", enabled = !mine && !scanBusy, large = true) { connect(h.address, h.name) }
                                            SciFiOutline("Disconnect", enabled = mine && conn != ConnState.IDLE, large = true) {
                                                LinkForegroundService.stop(context)
                                            }
                                            if (h.card == HunterRoster.Card.YES)
                                                SciFiOutline("Clear Card", enabled = !scanBusy && !clearBusy, large = true) { confirmClear = h }
                                        }
                                    }
                                }
                                }
                            }
                            item {
                                if (!onHull) {
                                    OpsDetectionPanel(
                                        hunters = wands, connected = active, running = scanning, mode = scanMode,
                                        allSelected = scanPick == LinkConstants.ReconTarget.ALL,
                                        deauthSelected = scanPick == LinkConstants.ReconTarget.DEAUTH,
                                        onAll = { quickPick(everySensor) },
                                        onDeauth = { quickPick(onlyDeauth) },
                                        onStart = { LinkForegroundService.active?.reconStart(target) },
                                        onStop = { LinkForegroundService.active?.reconStop() },
                                        onClear = { LinkForegroundService.active?.clearEvents() },
                                        onHunt = {
                                            HuntSession.begin(android.os.SystemClock.elapsedRealtime())
                                            LinkForegroundService.active?.huntStart()
                                        },
                                    )
                                } else {
                                SciFiPanel("QUICK START", statusColor = if (wands > 0) LtColors.Green else LtColors.Cyan, statusCount = wands) {
                                    val (modeText, _) = scanModeLabel(scanMode)
                                    Text(modeText, color = if (scanMode == ScanMode.NOT_SCANNING) LtColors.Red else if (scanMode == ScanMode.HUNTING) LtColors.Amber else LtColors.Green,
                                        fontWeight = FontWeight.Bold, fontSize = 12.sp, letterSpacing = 1.sp)
                                    // Wraps onto a second line on narrow panels so CLEAR is never cut off.
                                    FlowRow(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                                        TargetButton("All Sensors", LinkConstants.ReconTarget.ALL, scanPick) { quickPick(everySensor) }
                                        TargetButton("Only Deauth", LinkConstants.ReconTarget.DEAUTH, scanPick) { quickPick(onlyDeauth) }
                                        // The lit button shows the Hunter's state: START while stopped, STOP while scanning.
                                        val running = status?.monitoring == true
                                        if (running) SciFiOutline("Start", enabled = active) { LinkForegroundService.active?.reconStart(target) }
                                        else SciFiPrimary("Start", enabled = active) { LinkForegroundService.active?.reconStart(target) }
                                        if (running) SciFiPrimary("Stop", enabled = active) { LinkForegroundService.active?.reconStop() }
                                        else SciFiOutline("Stop", enabled = active) { LinkForegroundService.active?.reconStop() }
                                        SciFiOutline("Clear", enabled = active) { LinkForegroundService.active?.clearEvents() }
                                    }
                                    // HUNT v1: the fox hunt. The whole screen becomes the hunt screen.
                                    SciFiPrimary("Hunt Vixen", enabled = active, large = true) {
                                        HuntSession.begin(android.os.SystemClock.elapsedRealtime())
                                        LinkForegroundService.active?.huntStart()
                                    }
                                }
                                }
                            }
                            item {
                                if (!onHull) {
                                    OpsLatestDetection(
                                        detector = events.maxByOrNull { it.lastUpdatedMillis }?.detectorName,
                                        count = events.size,
                                    )
                                } else {
                                events.maxByOrNull { it.eventId }?.let { latest ->
                                    Box(Modifier.fillMaxWidth().padding(horizontal = 2.dp, vertical = 3.dp)
                                        .hudPanel(edge = LtColors.Amber, radius = 16.dp, elevation = 4.dp)
                                        .clip(ConsoleShape).padding(horizontal = 18.dp, vertical = 12.dp)) {
                                        FlowRow(horizontalArrangement = Arrangement.spacedBy(10.dp), verticalArrangement = Arrangement.spacedBy(2.dp)) {
                                            Text("Latest detection", color = LtColors.Amber, fontWeight = FontWeight.Bold,
                                                fontSize = 13.sp, letterSpacing = 1.sp)
                                            Text("${latest.detectorName}  /  ${events.size} detections", color = LtColors.Ivory,
                                                fontSize = 12.sp, fontFamily = ConsoleFonts.mono)
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
            }
            }
        }
    }

    BoxWithConstraints(Modifier.fillMaxSize()) {
        if (huntOpen) {
            // HUNT v1 takes the whole screen until STOP HUNT.
            SectionWindow(framed=!DeviceKind.isLokmatWatch,onBack={goBack()}) {
            HuntScreen(
                onStop = { LinkForegroundService.active?.huntStop(); HuntSession.end() },
                onAgain = {
                    HuntSession.begin(android.os.SystemClock.elapsedRealtime())
                    LinkForegroundService.active?.huntStart()
                },
            )
            }
            return@BoxWithConstraints
        }
        // The watch layout of the design standard (claude/android_watch_design_standard.md).
        val mode = LayoutPlan.mode(maxWidth.value, maxHeight.value)
        val sliding = mode == LayoutMode.WATCH
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
                scope.launch { slide.animateTo(-travelPx, tween(ROLL_MS)) }
            }
            fun unpark() {
                if (!faceParked) return
                sounds.slideBack()
                scope.launch { slide.animateTo(0f, tween(ROLL_MS)); faceParked = false; frozen = null }
            }
            faceActions.afterButton = { park() }
            faceActions.outsideCircle = { park() }
            // The LayerTime button (the watch's green side button, which sends Back): inside the
            // app it slides the face in or out. Pages have their own on-screen BACK buttons.
            androidx.activity.compose.BackHandler { if (faceParked) unpark() else park() }

            // The panel is a fixed layer that never moves: laid out once across the screen,
            // starting right of the parked sliver. The face is a true circle on top of it, so
            // the panel shows through the corners of the face's square, and sliding the face
            // away uncovers what it was hiding. The slide is translation only; nothing is
            // re-measured.
            // The salvaged hull: computed once per screen size off the main thread, then cached.
            val density = LocalDensity.current
            val hullSpec = with(density) {
                HullSpec(
                    w = maxWidth.roundToPx(), h = maxHeight.roundToPx(), dp = density.density,
                    consoleL = (sliver + 4.dp).toPx(), consoleT = 8.dp.toPx(),
                    consoleR = maxWidth.toPx() - 8.dp.toPx(), consoleB = maxHeight.toPx() - 8.dp.toPx(),
                    band = CONSOLE_BAND.toPx(), faceR = faceSize.toPx() / 2f, homeCx = faceSize.toPx() / 2f, travel = travelPx,
                )
            }
            val hull by androidx.compose.runtime.produceState<HullImages?>(null, hullSpec) {
                value = kotlinx.coroutines.withContext(kotlinx.coroutines.Dispatchers.Default) { HullRelief.render(hullSpec) }
            }
            Canvas(Modifier.fillMaxSize()) { hull?.let { drawImage(it.hull) } ?: drawRect(LtColors.Bg) }
            dataPane(Modifier.fillMaxSize(), PaddingValues(start = sliver + 4.dp, top = 8.dp, end = 8.dp, bottom = 8.dp), true)
            // the cover glass over the panels: cracks, glow tubes, and the straps riveted through it
            Canvas(Modifier.fillMaxSize()) { hull?.let { drawImage(it.overlay) } }
            if (!faceParked) {
                // A tap anywhere in the visible strip only slides the face out; it does not
                // also press whatever control is under the finger.
                Box(Modifier.fillMaxSize().pointerInput(Unit) { detectTapGestures { park() } })
            }
            // The face rolls off to the left: it slides and turns together, the turn equal to the
            // distance travelled over the radius (rolling without slipping), so it rotates
            // counter-clockwise as it goes left. Behind it is the hull plate, not black, so the
            // look does not depend on an AMOLED screen; the face is transparent outside the
            // circle, so the hull and the panel show through the corners.
            val radiusPx = with(LocalDensity.current) { faceSize.toPx() } / 2f
            // The face's shadow on the console: slides with the face, never turns (FaceLight.kt).
            FaceShadowLayer(Modifier.size(faceSize).graphicsLayer { translationX = slide.value })
            Box(Modifier.size(faceSize).graphicsLayer {
                translationX = slide.value
                rotationZ = Math.toDegrees((slide.value / radiusPx).toDouble()).toFloat()
            }) {
                face(Modifier.fillMaxSize(), true)
            }
            // The light: slides with the face but does not turn, so the face rolls under it.
            FaceLightLayer(Modifier.size(faceSize).graphicsLayer { translationX = slide.value })
            if (faceParked) {
                // The visible sliver at the left edge is the handle that rolls the face back.
                Box(Modifier.fillMaxHeight().width(sliver).pointerInput(Unit) { detectTapGestures { unpark() } })
            }
        } else {
            // Phone and tablet: the adaptive shell (LayoutPlan). A bottom bar on a compact
            // phone, a left rail on a wide screen; the five destinations switch the console.
            // The face shows on OPS and rolls up into a header strip on the other tabs. The
            // watch keeps its own sliding layout above, untouched.
            val dest = destForPage(page)
            val rail = LayoutPlan.usesRail(mode)
            val body: @Composable (Modifier) -> Unit = { m ->
                Column(m) {
                    if (dest == Destination.OPS) {
                        BoxWithConstraints(Modifier.fillMaxWidth().padding(start = 10.dp, end = 10.dp, top = 8.dp)) {
                            val diameter = (maxWidth * .55f).coerceAtMost(300.dp)
                            val panelStart = diameter * .86f
                            Box(Modifier.fillMaxWidth()) {
                                // Offset the instruments as one assembly; the foreground watch stays fixed.
                                Column(Modifier.fillMaxWidth().padding(top = 10.dp)) {
                                FieldStatusPanel(
                                    modifier = Modifier.padding(start = panelStart, top = 8.dp).fillMaxWidth(),
                                    minHeight = diameter * .80f,
                                    online = active,
                                    stale = linkStale,
                                    connecting = connecting,
                                    device = deviceLabel.substringBefore(" / "),
                                    sensor = activeSensorLabel(huntOpen, status),
                                    detections = events.size,
                                    latest = status?.lastAlertEventId?.let { alertId ->
                                        events.firstOrNull { it.eventId == alertId || alertId in it.mergedEventIds }?.detectorName
                                    },
                                )
                                    OpsLinkStrip(active, connecting, linkStale, events.size)
                                }
                                OpsWatchAssembly(Modifier.size(diameter * 1.10f).zIndex(1f)) { face(it, false) }
                            }
                        }
                    } else {
                        NavHeaderStrip(dest.label, active, events.size, linkStale)
                    }
                    dataPane(Modifier.fillMaxWidth().weight(1f),
                        PaddingValues(start = 10.dp, top = 0.dp, end = if (rail || page == "main" || page == "system" || page == "config" || page == "tools" || page == "attacks" || page == "builder") 10.dp else 24.dp, bottom = 10.dp), false)
                }
            }
            if (rail) {
                Row(Modifier.fillMaxSize()) {
                    NeonNavRail(dest) { d -> page = pageFor(d) }
                    body(Modifier.weight(1f).fillMaxHeight())
                }
            } else {
                Column(Modifier.fillMaxSize()) {
                    body(Modifier.fillMaxWidth().weight(1f))
                    NeonNavBar(dest) { d -> page = pageFor(d) }
                }
            }
        }
    }
}

/** The console's content inset; on the watch layout it is also the width of the hull's raised frame band. */
private val CONSOLE_BAND = 24.dp

/** Fixed face diameter on a phone (both orientations), sized to match the watch's compact face
 *  instead of filling a large fraction of the screen. A phone gives the data console the rest. */
private val FACE_PHONE = 190.dp

/** Roll time in ms; matches the 1 s of real gravel in the roll sounds so the thud lands as the face stops. */
private const val ROLL_MS = 980

private class FaceActions { var afterButton: () -> Unit = {}; var outsideCircle: () -> Unit = {} }

/** Everything the face draws from. Captured when the face is parked so it stops redrawing. */
private data class FaceInputs(
    val conn: ConnState, val status: com.projectrevivesolutions.layertime.controller.link.StatusSnapshot?,
    val btOn: Boolean, val battery: Int, val hour: String, val min: String, val date: String,
    val tempValue: String?, val tempUnit: String?, val sunLabel: String, val sunValue: String, val sunAmPm: String?,
    val sunTag: String?, val linkStale: Boolean, val reconnecting: Boolean,
)

/**
 * What "Active sensor" shows (2026-11-04): the operator's selection, not the
 * sweep's current phase. Status.active is which part of an ALL sweep the radio is
 * on this instant (DEAUTH on the Wi-Fi part, FLOCK on the Bluetooth burst), which
 * read as the wrong answer after ALL SENSORS and START. Status.selected is what
 * was started. Not scanning reads STOPPED; a hunt reads VIXEN.
 */
internal fun activeSensorLabel(huntOpen: Boolean, status: com.projectrevivesolutions.layertime.controller.link.StatusSnapshot?): String = when {
    huntOpen -> "VIXEN"
    status == null || !status.monitoring -> "STOPPED"
    else -> LinkConstants.ReconTarget.name(status.selected)
}

/** Empty-list text that matches the real state instead of always saying "connect a Hunter". */
internal fun emptyEventsMessage(connected: Boolean, status: com.projectrevivesolutions.layertime.controller.link.StatusSnapshot?): String = when {
    !connected -> "No detections yet. Connect a Hunter and start detection."
    status?.monitoring == true -> "No detections yet. Detection is running."
    else -> "No detections yet. Tap START to begin detection."
}

/**
 * LINK MEASURE: the watch side of Link 0.2 Phase 0 (claude/link_0_2_phase0_plan.md,
 * M5). A PING burst for the round trip that limits HUNT polling, and BULK runs for
 * throughput at 20 bytes and at a larger MTU. BULK needs the Hunter measurement
 * firmware; a release Hunter answers UnknownOp, and the panel says so.
 */
@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun LinkMeasurePanel(connected: Boolean) {
    val st by LinkMeasure.state.collectAsState()
    val idle = connected && st.running == null
    val client = LinkForegroundService.active
    SciFiPanel("LINK MEASURE  //  TEST") {
        Text("Link 0.2 Phase 0. Run in order: PING, BULK at 20 bytes, MTU 247, then BULK at 244 bytes. " +
            "BULK needs the Hunter measurement firmware. The larger MTU lasts until this connection drops.",
            color = LtColors.Ivory, fontSize = 12.sp)
        HudReadout("MTU", "${st.mtu}", if (st.mtu > 23) LtColors.Green else LtColors.Cyan)
        if (!connected) Text("Connect to a Hunter first.", color = LtColors.Grey, fontSize = 12.sp)
        st.running?.let { Text(it, color = LtColors.Green, fontSize = 12.sp, fontFamily = ConsoleFonts.mono) }
        FlowRow(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            SciFiOutline("PING x100", enabled = idle) { client?.measurePingBurst(100) }
            SciFiOutline("BULK 200 x 20", enabled = idle) { client?.measureBulk(200, 20) }
            SciFiOutline("MTU 247", enabled = idle && st.mtu <= 23) { client?.measureRequestMtu() }
            SciFiOutline("BULK 200 x 244", enabled = idle) { client?.measureBulk(200, 244) }
            SciFiOutline("CLEAR", enabled = st.lines.isNotEmpty() && st.running == null) { LinkMeasure.clear() }
        }
        st.lines.forEach { Text(it, color = LtColors.Ivory, fontSize = 12.sp, fontFamily = ConsoleFonts.mono) }
    }
}

@Composable
private fun SciFiPanel(
    title: String,
    statusColor: Color? = null,
    statusCount: Int? = null,
    content: @Composable () -> Unit,
) {
    if (LocalSystemScreen.current) {
        SystemModule(title, status = statusColor) { content() }
        return
    }
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
    if (LocalSpacecraft.current) {
        Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                if (statusColor != null) Box(Modifier.size(6.dp).background(statusColor, CircleShape))
                Text(title, color = LtColors.Ivory, fontSize = 13.sp, letterSpacing = 1.sp,
                    fontWeight = FontWeight.Medium, fontFamily = FontFamily.SansSerif, modifier = Modifier.weight(1f))
                statusCount?.let { Text("$it", color = statusColor ?: edge, fontSize = 12.sp) }
            }
            Box(Modifier.fillMaxWidth().height(1.dp).background(edge.copy(alpha = .18f)))
        }
        return
    }
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
                Text("$statusCount", color = statusColor, fontWeight = FontWeight.Bold, fontSize = 16.sp, fontFamily = ConsoleFonts.mono,
                    style = TextStyle(shadow = Shadow(statusColor, Offset.Zero, 10f)))
            }
            Text(title, color = edge, fontWeight = FontWeight.Bold, fontSize = 14.sp, letterSpacing = 2.sp, style = glowText(),
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
/** The console page that a destination opens. */
private fun pageFor(d: Destination): String = when (d) {
    Destination.OPS -> "main"
    Destination.EVENTS -> "attacks"
    Destination.TOOLS -> "tools"
    Destination.BUILDER -> "builder"
    Destination.MISSION -> "config"
    Destination.SYSTEM -> "system"
}

/** Which destination a page belongs to, so the nav highlights the right tab (and sub-pages
 *  like capture and detail stay under EVENTS). */
private fun destForPage(page: String): Destination = when (page) {
    "attacks", "capture", "detail", "card" -> Destination.EVENTS
    "tools", "wireshark", "timeline" -> Destination.TOOLS
    "builder" -> Destination.BUILDER
    "config" -> Destination.MISSION
    "system" -> Destination.SYSTEM
    else -> Destination.OPS
}

/** One navigation tab: a glass pill with a lit rim, cyan when selected (matches HudKey). */
@Composable
private fun NavTab(modifier: Modifier, label: String, selected: Boolean, onClick: () -> Unit) {
    val edge = if (selected) LtColors.Cyan else LtColors.ConsoleEdge
    val flash = rememberPressFlash()
    Box(
        modifier.padding(3.dp).height(48.dp)
            .background(if (selected) Color(0xFF12394C) else Color(0xFF091323), RoundedCornerShape(8.dp))
            .border(1.dp, edge.copy(alpha = if (selected) .85f else .18f), RoundedCornerShape(8.dp))
            .pressFlash(flash, false, edge)
            .clip(RoundedCornerShape(8.dp))
            .clickable { flash.fire(); onClick() },
        contentAlignment = Alignment.Center,
    ) {
        Text(label, color = edge, fontWeight = FontWeight.Bold, fontSize = 10.sp, letterSpacing = 0.sp,
            fontFamily = FontFamily.SansSerif, maxLines = 1)
    }
}

/** The compact phone's bottom navigation bar: the five destinations, evenly across the width. */
@Composable
private fun NeonNavBar(current: Destination, onSelect: (Destination) -> Unit) {
    OpsNavigationBar(current, onSelect)
}

/** The wide screen's left navigation rail. */
@Composable
private fun NeonNavRail(current: Destination, onSelect: (Destination) -> Unit) {
    Column(Modifier.fillMaxHeight().width(86.dp).padding(horizontal = 6.dp, vertical = 10.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp)) {
        for (d in Destination.values()) NavTab(Modifier.fillMaxWidth(), d.label, d == current) { onSelect(d) }
    }
}

/** The slim header on every tab but OPS: a link light, the tab name, and the event count. */
@Composable
private fun NavHeaderStrip(title: String, active: Boolean, events: Int, stale: Boolean) {
    Row(Modifier.fillMaxWidth().padding(horizontal = 12.dp, vertical = 8.dp),
        verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
        HudLed(if (active && !stale) LtColors.Green else LtColors.Red, 10.dp)
        Text(title, color = LtColors.ConsoleEdge, fontWeight = FontWeight.Bold, fontSize = 16.sp, letterSpacing = 2.sp)
        Spacer(Modifier.weight(1f))
        Text("EVT $events", color = LtColors.Amber, fontWeight = FontWeight.Bold, fontSize = 12.sp, fontFamily = ConsoleFonts.mono)
    }
}

@Composable
private fun HudStatusStrip(active: Boolean, connecting: Boolean, events: Int, stale: Boolean = false) {
    val edge = LtColors.ConsoleEdge
    val t = rememberInfiniteTransition(label = "scan")
    val p by t.animateFloat(0f, 1f, infiniteRepeatable(tween(2600, easing = LinearEasing)), label = "scanP")
    val linkText = when { active && stale -> "LINK STALE"; active -> "LINK ONLINE"; connecting -> "LINKING"; else -> "LINK OFFLINE" }
    val linkColor = when { active && stale -> LtColors.Amber; active -> LtColors.Green; connecting -> LtColors.Amber; else -> LtColors.Red }
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
        Text("LT ◆ CONSOLE", color = edge, fontWeight = FontWeight.Bold, fontSize = 12.sp, letterSpacing = 2.sp,
            style = glowText(), modifier = Modifier.weight(1f))
        HudLed(linkColor, 8.dp)
        Text(linkText, color = linkColor, fontWeight = FontWeight.Bold, fontSize = 12.sp, letterSpacing = 1.sp,
            fontFamily = ConsoleFonts.heading, style = TextStyle(shadow = Shadow(linkColor, Offset.Zero, 10f)))
        Text("EVT $events", color = LtColors.Amber, fontWeight = FontWeight.Bold, fontSize = 12.sp, fontFamily = ConsoleFonts.mono)
    }
}

/** A label and a value in a recessed, lit readout well. */
@Composable
private fun HudReadout(label: String, value: String, color: Color) {
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(7.dp)) {
        Text(label, color = LtColors.Grey, fontSize = 12.sp, letterSpacing = 1.5.sp, fontFamily = ConsoleFonts.heading)
        Box(Modifier.readoutWell(color).padding(horizontal = 10.dp, vertical = 1.dp)) {
            Text(value, color = color, fontWeight = FontWeight.Bold, fontSize = 16.sp, fontFamily = ConsoleFonts.mono,
                style = TextStyle(shadow = Shadow(color, Offset.Zero, 12f)))
        }
    }
}

/** A label on the left and a value on the right, for a status panel row. */
@Composable
private fun StatusRow(label: String, value: String, color: Color) {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        Text(label, color = LtColors.Grey, fontSize = 12.sp, letterSpacing = 1.sp, fontFamily = ConsoleFonts.heading)
        Spacer(Modifier.weight(1f))
        Text(value, color = color, fontWeight = FontWeight.Bold, fontSize = 13.sp, fontFamily = ConsoleFonts.body)
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
    if (LocalSpacecraft.current) {
        SpacecraftKey(text, enabled, true, onClick)
        return
    }
    val g = LtColors.Green
    val look = if (enabled)
        Modifier.shadow(if (large) 8.dp else 5.dp, PillShape, clip = false, ambientColor = g.copy(alpha = 0.7f), spotColor = g)
            .haloOnly(g, radius = buttonHeight(large) / 2)
            .background(domeBrush(g), PillShape)
            .border(2.dp, railBrush(g), PillShape)
            .pillShine()
    else Modifier.glassPill(g, false).border(1.5.dp, g.copy(alpha = 0.28f), PillShape)
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    val flash = rememberPressFlash()
    Button(
        onClick = { flash.fire(); onClick() }, enabled = enabled, shape = PillShape, contentPadding = if (large) btnPadLarge else btnPadPill,
        modifier = look.height(buttonHeight(large)).pressFlash(flash, pressed && enabled),
        interactionSource = source,
        elevation = ButtonDefaults.buttonElevation(0.dp, 0.dp, 0.dp, 0.dp, 0.dp),
        colors = ButtonDefaults.buttonColors(
            containerColor = Color.Transparent, contentColor = LtColors.Bg,
            disabledContainerColor = Color.Transparent, disabledContentColor = LtColors.Grey,
        ),
    ) { Text(text, fontWeight = FontWeight.Bold, fontSize = if (large) 13.sp else 11.sp, letterSpacing = 1.sp, fontFamily = ConsoleFonts.heading,
        maxLines = 1, softWrap = false) }
}

/** Outlined cyan pill: lit glass with a tube rim and glowing text. Disabled, dim glass with a faint rim. */
@Composable
fun SciFiOutline(text: String, enabled: Boolean, large: Boolean = false, onClick: () -> Unit) {
    if (LocalSpacecraft.current) {
        SpacecraftKey(text, enabled, false, onClick)
        return
    }
    val e = LtColors.ConsoleEdge
    val glow = if (enabled) Modifier.shadow(if (large) 6.dp else 4.dp, PillShape, clip = false,
        ambientColor = e.copy(alpha = 0.7f), spotColor = e).haloOnly(e, radius = buttonHeight(large) / 2) else Modifier
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    val flash = rememberPressFlash()
    OutlinedButton(
        onClick = { flash.fire(); onClick() }, enabled = enabled, shape = PillShape, contentPadding = if (large) btnPadLarge else btnPadPill,
        modifier = glow.glassPill(e, enabled).height(buttonHeight(large)).pressFlash(flash, pressed && enabled, e),
        interactionSource = source,
        border = if (enabled) BorderStroke(2.5.dp, railBrush(e)) else BorderStroke(1.5.dp, e.copy(alpha = 0.28f)),
        colors = ButtonDefaults.outlinedButtonColors(containerColor = Color.Transparent, contentColor = e, disabledContentColor = LtColors.Grey),
    ) {
        // A button label never wraps mid-word (build 70).
        Text(text, fontSize = if (large) 13.sp else 11.sp, letterSpacing = 1.sp, fontFamily = ConsoleFonts.heading,
            maxLines = 1, softWrap = false,
            style = if (enabled) TextStyle(shadow = Shadow(e, Offset.Zero, 9f)) else TextStyle.Default)
    }
}

/** Recessed glass key for phones/tablets; callbacks and watch keys remain unchanged. */
@Composable
private fun SpacecraftKey(text: String, enabled: Boolean, primary: Boolean, onClick: () -> Unit) {
    val edge = LtColors.ConsoleEdge
    val shape = RoundedCornerShape(7.dp)
    Button(onClick = onClick, enabled = enabled, shape = shape,
        modifier = Modifier.height(48.dp)
            .background(Brush.verticalGradient(if (enabled && primary)
                listOf(Color(0xFF15546C), Color(0xFF092332))
                else listOf(Color(0xFF142636), Color(0xFF08121D))), shape)
            .border(1.dp, edge.copy(alpha = if (!enabled) .12f else if (primary) .85f else .35f), shape),
        contentPadding = PaddingValues(horizontal = 14.dp, vertical = 8.dp),
        colors = ButtonDefaults.buttonColors(containerColor = Color.Transparent,
            contentColor = LtColors.Ivory, disabledContainerColor = Color.Transparent,
            disabledContentColor = Color(0xFF6F8292)),
        elevation = ButtonDefaults.buttonElevation(0.dp)) {
        Text(text, fontFamily = FontFamily.SansSerif, fontSize = 12.sp,
            fontWeight = FontWeight.Medium, maxLines = 1, softWrap = false)
    }
}

@Composable
private fun TargetButton(label: String, value: Int, current: Int, onPick: (Int) -> Unit) {
    if (value == current) SciFiPrimary(label, enabled = true) { onPick(value) }
    else SciFiOutline(label, enabled = true) { onPick(value) }
}

@Composable
private fun EventRowCard(e: EventRow, onOpen: () -> Unit) {
    Box(Modifier.fillMaxWidth().padding(horizontal = 2.dp, vertical = 3.dp)
        .hudPanel(edge = LtColors.Amber, radius = 16.dp, elevation = 4.dp)
        .clip(ConsoleShape).clickable { onOpen() }) {
        Row(Modifier.fillMaxWidth().clip(ConsoleShape)) {
            Box(Modifier.width(3.dp).fillMaxHeight().background(LtColors.Amber))
            Column(Modifier.padding(19.dp)) {
                Text("#${e.eventId}  ${e.detectorName}", color = LtColors.Amber,
                    fontWeight = FontWeight.Bold, fontSize = 14.sp, letterSpacing = 1.sp)
                Text("${LinkConstants.SourceKind.name(e.sourceKind)} · ${LinkConstants.Band.name(e.band)} · " +
                    "ch ${e.channel} · ${e.rssi} dBm · x${e.count} · ${e.ageSeconds}s · " +
                    LinkConstants.Confidence.name(e.confidence), color = LtColors.Ivory, fontSize = 12.sp,
                    fontFamily = ConsoleFonts.mono)
                e.sourceId?.let { Text("id: $it", color = LtColors.Grey, fontSize = 12.sp, fontFamily = ConsoleFonts.mono) }
                e.detail?.let { Text("detail: $it", color = LtColors.Grey, fontSize = 12.sp, fontFamily = ConsoleFonts.mono) }
                if (e.mergedEventIds.size > 1) Text("Seen as ${e.mergedEventIds.size} Hunter records.", color = LtColors.Grey,
                    fontSize = 12.sp, fontFamily = ConsoleFonts.mono)
                Text("TAP FOR DETAIL", color = LtColors.Cyan, fontSize = 12.sp, letterSpacing = 1.sp)
            }
        }
    }
}

/**
 * POSITION: where the SUN ring's position comes from, the position as MGRS, and a
 * field to enter one by hand (MGRS, UTM, or latitude and longitude). The watch's GPS
 * stays the first source; a manual entry is used when it is newer than the last fix.
 */
@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun PositionPanel(
    gpsTime: Long?, manual: RingData.Manual?, useManual: Boolean, posLat: Double?, posLon: Double?,
    onSet: (Double, Double) -> Unit, onClear: () -> Unit,
) {
    if (LocalSystemScreen.current) {
        SystemPositionPanels(gpsTime, manual, useManual, posLat, posLon, onSet, onClear)
        return
    }
    var text by remember { mutableStateOf("") }
    var message by remember { mutableStateOf<Pair<String, Boolean>?>(null) }   // text, isError
    val fmt = remember { SimpleDateFormat("MMM dd HH:mm", Locale.US) }
    SciFiPanel("POSITION", statusColor = when { posLat == null -> LtColors.Red; useManual -> LtColors.Amber; else -> LtColors.Green }) {
        val source = when {
            posLat == null -> "NONE"
            useManual -> "MANUAL  ${fmt.format(Date(manual!!.time)).uppercase()}"
            else -> "GPS  ${fmt.format(Date(gpsTime ?: 0L)).uppercase()}"
        }
        HudReadout("SOURCE", source, when { posLat == null -> LtColors.Red; useManual -> LtColors.Amber; else -> LtColors.Green })
        if (posLat != null && posLon != null) {
            Text(GridPosition.toMgrs(posLat, posLon), color = LtColors.Ivory, fontSize = 14.sp, fontFamily = ConsoleFonts.mono,
                letterSpacing = 1.sp)
        }
        // Entry box: the HUD keypad below types into it. The system keyboard is not used, because
        // the LOKMAT watch's keyboard would not accept the digits after "18S UJ".
        Box(Modifier.fillMaxWidth().readoutWell().padding(horizontal = 10.dp, vertical = 6.dp)) {
            if (text.isEmpty()) Text("18S UJ 23478 06483", color = LtColors.Grey, fontFamily = ConsoleFonts.mono, fontSize = 16.sp)
            else Text(text + "_", color = LtColors.Ivory, fontFamily = ConsoleFonts.mono, fontSize = 16.sp, letterSpacing = 1.sp)
        }
        GridKeypad { key ->
            message = null
            text = when (key) {
                "DEL" -> text.dropLast(1)
                "CLEAR" -> ""
                "SPACE" -> if (text.isEmpty() || text.endsWith(" ")) text else "$text "
                else -> if (text.length < 32) text + key else text
            }
        }
        Text("Enter MGRS, UTM, or latitude and longitude with the keypad. The GPS takes over again with its next fix.",
            color = LtColors.Grey, fontSize = 12.sp)
        message?.let { (m, err) -> Text(m, color = if (err) LtColors.Red else LtColors.Green, fontSize = 12.sp) }
        FlowRow(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            SciFiPrimary("SET POSITION", enabled = text.isNotBlank()) {
                when (val r = GridPosition.parse(text)) {
                    is GridPosition.Parsed.Ok -> {
                        onSet(r.position.lat, r.position.lon)
                        message = "Position set from ${r.format}: ${GridPosition.toMgrs(r.position.lat, r.position.lon)}." to false
                        text = ""
                    }
                    is GridPosition.Parsed.Error -> message = r.message to true
                }
            }
            SciFiOutline("CLEAR MANUAL", enabled = manual != null) { onClear(); message = "Manual position cleared." to false }
        }
    }
}

/**
 * The console's own keypad for grid entry: digits, SPACE, DEL, CLEAR, minus, and a
 * decimal point, and an ABC key that swaps in the 24 grid letters (A to Z without I
 * and O, which MGRS and UTM never use). Keys scale with the panel text size.
 */
@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun GridKeypad(onKey: (String) -> Unit) {
    var letters by remember { mutableStateOf(false) }
    val digits = listOf("1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", ".")
    val alpha = "ABCDEFGHJKLMNPQRSTUVWXYZ".map { it.toString() }
    FlowRow(horizontalArrangement = Arrangement.spacedBy(5.dp), verticalArrangement = Arrangement.spacedBy(5.dp)) {
        for (k in if (letters) alpha else digits) HudKey(k, LtColors.ConsoleEdge) { onKey(k) }
        HudKey(if (letters) "123" else "ABC", LtColors.Amber, wide = true) { letters = !letters }
        HudKey("SPACE", LtColors.ConsoleEdge, wide = true) { onKey("SPACE") }
        HudKey("DEL", LtColors.Amber, wide = true) { onKey("DEL") }
        HudKey("CLEAR", LtColors.Red, wide = true) { onKey("CLEAR") }
    }
}

/** One keypad key: a small glass pill with a lit rim. */
@Composable
private fun HudKey(label: String, edge: Color, wide: Boolean = false, onClick: () -> Unit) {
    val scale = LocalDensity.current.fontScale
    val flash = rememberPressFlash()
    Box(
        Modifier.size(width = (if (wide) 70.dp else 40.dp) * scale, height = 36.dp * scale)
            .glassPill(edge, true)
            .border(2.dp, railBrush(edge), PillShape)
            .pressFlash(flash, false, edge)
            .clip(PillShape)
            .clickable { flash.fire(); onClick() },
        contentAlignment = Alignment.Center,
    ) {
        Text(label, color = edge, fontWeight = FontWeight.Bold, fontSize = 14.sp, fontFamily = ConsoleFonts.heading,
            style = TextStyle(shadow = Shadow(edge, Offset.Zero, 8f)))
    }
}

/**
 * LAYERTIME BUTTON: whether the green side button is set up as the LayerTime button
 * (an accessibility service the operator turns on once), and a shortcut to the
 * Accessibility settings to turn it on.
 */
@Composable
private fun LayerTimeButtonPanel(nowMs: Long) {
    if (LocalSystemScreen.current) {
        SystemSideButtonPanel(nowMs)
        return
    }
    val context = LocalContext.current
    val on = remember(nowMs / 3_000L) { com.projectrevivesolutions.layertime.controller.service.LayerTimeKeyService.isEnabled(context) }
    SciFiPanel("LAYERSCOPE BUTTON", statusColor = if (on) LtColors.Green else LtColors.Amber) {
        HudReadout("GREEN BUTTON", if (on) "ACTIVE" else "OFF", if (on) LtColors.Green else LtColors.Amber)
        Text(
            if (on) "A short press opens LayerScope from anywhere. Hold it for Back in other apps. In LayerScope it slides the face in or out."
            else "Turn on LayerScope button in Accessibility settings so the green side button opens LayerScope from anywhere.",
            color = LtColors.Ivory, fontSize = 12.sp)
        if (!on) SciFiOutline("OPEN ACCESSIBILITY SETTINGS", enabled = true) {
            runCatching { context.startActivity(Intent(android.provider.Settings.ACTION_ACCESSIBILITY_SETTINGS).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)) }
        }
    }
}
