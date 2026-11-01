package com.projectrevivesolutions.layertime.controller.wireshark

import android.content.pm.ApplicationInfo
import android.os.Build
import android.os.Bundle
import android.os.Debug
import android.os.Process
import android.util.Log
import android.view.WindowManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.produceState
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.projectrevivesolutions.layertime.controller.BuildConfig
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.withContext
import java.io.File
import java.util.zip.ZipFile
import java.util.concurrent.atomic.AtomicInteger

private const val TAG = "LTWS_DIAG"

/** Intent extras for the stability / memory investigation. */
const val EXTRA_DECODES = "ltws_decodes"     // int: iterations after the report (0 = skip)
const val EXTRA_MODE = "ltws_mode"           // "decode" (default) or "control"
const val EXTRA_GC = "ltws_gc"               // int 0/1: request GC before each sample (diagnostic aid)
const val EXTRA_WARMUP = "ltws_warmup"       // int: iterations before the baseline sample (default 4000)

private object DiagProcess {
    val pid: Int = Process.myPid()
    val activityCreates = AtomicInteger()
    val reports = AtomicInteger()
    @Volatile var baseline: Mem? = null
}

/** Per-category memory, in KB. Android summary stats (API 23+) plus native heap and RSS. */
private data class Mem(
    val javaHeap: Long, val nativeHeap: Long, val code: Long, val stack: Long,
    val graphics: Long, val privateOther: Long, val system: Long,
    val totalPss: Long, val nativeHeapAlloc: Long, val rss: Long
) {
    operator fun minus(o: Mem) = Mem(
        javaHeap - o.javaHeap, nativeHeap - o.nativeHeap, code - o.code, stack - o.stack,
        graphics - o.graphics, privateOther - o.privateOther, system - o.system,
        totalPss - o.totalPss, nativeHeapAlloc - o.nativeHeapAlloc, rss - o.rss)
    fun oneLine() = "PSS %,d (java %,d, native %,d, code %,d, stack %,d, gfx %,d, other %,d, sys %,d) KB; nHeapAlloc %,d KB; RSS %,d KB"
        .format(totalPss, javaHeap, nativeHeap, code, stack, graphics, privateOther, system, nativeHeapAlloc, rss)
    fun delta() = "PSS %+,d (java %+,d, native %+,d, code %+,d, gfx %+,d, other %+,d) KB; nHeapAlloc %+,d KB; RSS %+,d KB"
        .format(totalPss, javaHeap, nativeHeap, code, graphics, privateOther, nativeHeapAlloc, rss)
}

private fun stat(mi: Debug.MemoryInfo, key: String): Long =
    runCatching { mi.getMemoryStat(key)?.toLong() ?: 0L }.getOrDefault(0L)

private fun sampleMem(): Mem {
    val mi = Debug.MemoryInfo(); Debug.getMemoryInfo(mi)
    val rss = runCatching {
        File("/proc/self/status").readLines().first { it.startsWith("VmRSS:") }.filter { it.isDigit() }.toLong()
    }.getOrDefault(-1L)
    return Mem(
        stat(mi, "summary.java-heap"), stat(mi, "summary.native-heap"), stat(mi, "summary.code"),
        stat(mi, "summary.stack"), stat(mi, "summary.graphics"), stat(mi, "summary.private-other"),
        stat(mi, "summary.system"), stat(mi, "summary.total-pss"),
        Debug.getNativeHeapAllocatedSize() / 1024, rss)
}

/** Ask ART to collect. A hint, not a guarantee: used only to tell retained memory from uncollected garbage. */
private fun requestGc() {
    Runtime.getRuntime().gc(); System.runFinalization(); Runtime.getRuntime().gc()
    runCatching { Thread.sleep(60) }
}

/**
 * Liveness reporting, so a long run can be told apart from a stalled one.
 * A "PHASE <name>" line at every phase change, and a "HEARTBEAT" line at least
 * every 5 s inside long loops: iteration count, the rate over the last
 * interval, elapsed time, and estimated time left. If the interval rate falls
 * below half the phase's first-interval rate, the line is flagged SLOWING.
 * The latest line is also shown live on screen.
 */
private object Heartbeat {
    val progress = MutableStateFlow("PHASE starting")
    private const val INTERVAL_NS = 5_000_000_000L
    @Volatile private var phaseStartNs = 0L
    @Volatile private var lastNs = 0L
    @Volatile private var lastI = 0
    @Volatile private var firstRate = -1.0

    fun phase(name: String) {
        val now = System.nanoTime()
        phaseStartNs = now; lastNs = now; lastI = 0; firstRate = -1.0
        emit("PHASE $name")
    }

    /** Call once per iteration; logs at most every 5 s, and always on the last iteration. */
    fun tick(phase: String, i: Int, n: Int) {
        val now = System.nanoTime()
        if (now - lastNs < INTERVAL_NS && i != n) return
        val intervalS = (now - lastNs) / 1e9
        val rate = if (intervalS > 0) (i - lastI) / intervalS else 0.0
        if (firstRate < 0 && rate > 0) firstRate = rate
        lastNs = now; lastI = i
        val elapsed = (now - phaseStartNs) / 1e9
        val avg = if (elapsed > 0) i / elapsed else 0.0
        val eta = if (avg > 0) (n - i) / avg else 0.0
        val slow = if (firstRate > 0 && rate < firstRate * 0.5)
            " SLOWING (now %.0f/s, started at %.0f/s)".format(rate, firstRate) else ""
        emit("HEARTBEAT %s %d/%d (%d%%) %.0f/s elapsed %.0f s eta %.0f s%s"
            .format(phase, i, n, (i.toLong() * 100 / maxOf(1, n)).toInt(), rate, elapsed, eta, slow))
    }

    private fun emit(line: String) { Log.i(TAG, line); progress.value = line }
}

/**
 * EXPLICITLY DIAGNOSTIC screen. Separate launcher activity, isolated from the
 * main controller UI. It decodes a fixed 802.11 deauth fixture on-device with
 * libwireshark/epan, shows the structured protocol tree and a PCAP-file decode,
 * and reports size, native-timed init, decode timing, and per-category memory
 * against a pre-load baseline. With the int extra "ltws_decodes" = N it appends
 * a stability and memory test; "ltws_mode" = control runs the same JNI round
 * trip with no dissection; "ltws_gc" = 1 requests a collection before each
 * sample (a diagnostic aid, never a guarantee); "ltws_warmup" sets how many
 * iterations run before the baseline sample. Logcat tag LTWS_DIAG; ends "END".
 *
 * It decodes a FIXTURE, not live traffic. It makes no detection or
 * countermeasure claim: decoding a frame is not a "countered" verdict.
 */
class WiresharkDiagActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val instance = DiagProcess.activityCreates.incrementAndGet()
        val cfg = StabilityConfig(
            decodes = intent.getIntExtra(EXTRA_DECODES, 0),
            control = intent.getStringExtra(EXTRA_MODE) == "control",
            gc = intent.getIntExtra(EXTRA_GC, 0) == 1,
            warmup = intent.getIntExtra(EXTRA_WARMUP, 4000)
        )
        // Keep the display on while the report runs: once the screen turns off the
        // activity leaves the foreground, and Android throttles and then freezes the
        // process (seen on the TCL T513V, Android 15, build 17).
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        val info = applicationInfo
        val cacheDir = cacheDir
        setContent { DiagScreen(info, instance, cfg, cacheDir) }
    }

    // Logged so a run log always says whether the app left the foreground during a run.
    override fun onPause() { super.onPause(); Log.i(TAG, "PHASE paused: app left the foreground") }
    override fun onResume() { super.onResume(); Log.i(TAG, "PHASE resumed: app is in the foreground") }
}

private data class StabilityConfig(val decodes: Int, val control: Boolean, val gc: Boolean, val warmup: Int)

@Composable
private fun DiagScreen(info: ApplicationInfo, instance: Int, cfg: StabilityConfig, cacheDir: File) {
    val report by produceState("The decoder is running. epan initialization can take a few seconds.") {
        value = withContext(Dispatchers.Default) { buildReport(info, instance, cfg, cacheDir) }
    }
    val progress by Heartbeat.progress.collectAsState()
    Column(
        Modifier.fillMaxSize().background(Color(0xFF0B0F14)).padding(16.dp)
            .verticalScroll(rememberScrollState())
    ) {
        Text("WIRESHARK DECODER: DIAGNOSTIC", color = Color(0xFFFFB300), fontSize = 16.sp)
        Text(progress, color = Color(0xFF39D98A), fontFamily = FontFamily.Monospace, fontSize = 12.sp)
        Text("Fixture decode only. Not live traffic. Not a countermeasure claim.",
            color = Color(0xFF7A8794), fontSize = 12.sp, modifier = Modifier.padding(bottom = 12.dp))
        Text(report, color = Color(0xFFE8F0F7), fontFamily = FontFamily.Monospace, fontSize = 13.sp)
    }
}

private fun kb(bytes: Long) = "%,d KB".format(bytes / 1024)

private fun buildReport(info: ApplicationInfo, instance: Int, cfg: StabilityConfig, cacheDir: File): String {
    val reportNo = DiagProcess.reports.incrementAndGet()
    if (DiagProcess.baseline == null && !WiresharkDecoder.loadedByFirstCaller) DiagProcess.baseline = sampleMem()
    val sb = StringBuilder()
    sb.append("build: ").append(BuildConfig.VERSION_NAME).append(" (code ").append(BuildConfig.VERSION_CODE).append(")\n")
    sb.append("device: ").append(Build.MODEL).append(", Android ").append(Build.VERSION.RELEASE)
        .append(" (API ").append(Build.VERSION.SDK_INT).append("), ABIs ").append(Build.SUPPORTED_ABIS.joinToString(",")).append('\n')
    sb.append("process: pid ").append(DiagProcess.pid).append(", report #").append(reportNo)
        .append(" in this process, activity instance #").append(instance).append("\n\n")

    val apk = File(info.sourceDir).length()
    val libs = File(info.nativeLibraryDir).listFiles()?.filter { it.name.endsWith(".so") }?.sortedBy { it.name } ?: emptyList()
    if (libs.isNotEmpty()) {
        // Extracted (compressed-in-APK) packaging: the libraries are a second copy on disk.
        val libBytes = libs.sumOf { it.length() }
        sb.append("APK on device: ").append(kb(apk)).append("; native libs ").append(libs.size)
            .append(" files, ").append(kb(libBytes)).append(" extracted; installed total ").append(kb(apk + libBytes)).append("\n")
    } else {
        // Uncompressed packaging (build 72+): Android maps the libraries straight from the APK, nothing is extracted.
        val inApk = runCatching {
            ZipFile(info.sourceDir).use { z ->
                z.entries().asSequence().filter { it.name.startsWith("lib/") && it.name.endsWith(".so") }
                    .map { it.size to (it.method == java.util.zip.ZipEntry.STORED) }.toList()
            }
        }.getOrDefault(emptyList())
        sb.append("APK on device: ").append(kb(apk)).append("; native libs ").append(inApk.size)
            .append(" files, ").append(kb(inApk.sumOf { it.first })).append(" inside the APK (")
            .append(inApk.count { it.second }).append(" stored uncompressed, not extracted); installed total ").append(kb(apk)).append("\n")
    }
    DiagProcess.baseline?.let { sb.append("baseline before libwireshark load: ").append(it.oneLine()).append('\n') }

    Heartbeat.phase("load")
    if (!WiresharkDecoder.ensureLoaded()) {
        val f = WiresharkDecoder.failure()!!
        sb.append("libwireshark: NOT LOADED\nfailure: ").append(f.kind).append('\n')
        f.library?.let { sb.append("missing library: ").append(it).append('\n') }
        f.neededBy?.let { sb.append("needed by: ").append(it).append('\n') }
        f.symbol?.let { sb.append("symbol: ").append(it).append('\n') }
        sb.append(f.explanation()).append("\nloader error (verbatim):\n").append(f.raw).append('\n')
        return finish(sb)
    }
    sb.append("System.loadLibrary(ltws + dependencies): %.1f ms".format(WiresharkDecoder.loadMillis))
        .append(if (reportNo == 1) " (this report)\n" else " (measured by report #1; not reloaded)\n")

    Heartbeat.phase("init")
    val initResult = WiresharkDecoder.initialize()!!
    if (initResult.performedNow) {
        sb.append("native init: PERFORMED IN THIS REPORT, epan_init returned ").append(if (initResult.ok) "true" else "FALSE").append('\n')
        sb.append("  epan_init: %.1f ms; total init: %.1f ms\n".format(initResult.epanInitMillis, initResult.totalInitMillis))
    } else {
        sb.append("native init: ALREADY DONE EARLIER IN THIS PROCESS; not re-run (init call #").append(initResult.callsInProcess).append(")\n")
        sb.append("  first initialization: epan_init %.1f ms, total %.1f ms, result %s\n"
            .format(initResult.epanInitMillis, initResult.totalInitMillis, if (initResult.ok) "OK" else "FAILED"))
    }
    if (!initResult.ok) { sb.append("epan_init returned false; no decode attempted.\n"); return finish(sb) }
    sb.append("libwireshark/epan version: ").append(WiresharkDecoder.version()).append('\n')
    sb.append("memory after init: ").append(sampleMem().oneLine()).append("\n\n")

    val fixture = WiresharkDecoder.DEAUTH_FIXTURE
    Heartbeat.phase("decode")
    val decoded = WiresharkDecoder.decode(fixture)
    if (decoded == null) { sb.append("decode returned null although init reported success.\n"); return finish(sb) }
    sb.append(decoded)
    val bad = WiresharkDecoder.mismatchesAgainstReference(decoded)
    sb.append("fields matching TShark 4.4.19: ").append(6 - bad.size).append(" of 6")
        .append(if (bad.isEmpty()) "\n" else ", mismatched: ${bad.joinToString("; ")}\n")
    sb.append("reference: ").append(WiresharkDecoder.REFERENCE_FILE).append('\n')
    val warm = LongArray(20) { val t = System.nanoTime(); WiresharkDecoder.decode(fixture); (System.nanoTime() - t) / 1000 }.sorted()
    sb.append("decode time, warm (20 runs): median ").append(warm[10]).append(" us, min ").append(warm.first())
        .append(" us, max ").append(warm.last()).append(" us\n\n")

    // structured tree (the event-detail data source)
    Heartbeat.phase("tree")
    val tree = WiresharkDecoder.decodeTree(fixture)
    if (tree != null) {
        sb.append("STRUCTURED TREE: ").append(tree.size).append(" fields (name, value, byte offset, length)\n")
        val keys = setOf("wlan.fc.type_subtype", "wlan.sa", "wlan.da", "wlan.bssid", "wlan.seq", "wlan.fixed.reason_code")
        tree.filter { it.name in keys }.forEach {
            sb.append("  ").append(it.name).append(" = ").append(it.display)
                .append(" @ offset ").append(it.start).append(" len ").append(it.length).append('\n')
        }
        sb.append("  (full tree available; ").append(tree.size).append(" nodes)\n\n")
    }

    // PCAP decode, self-contained: write a pcap of the fixture to cache and decode it
    runCatching {
        Heartbeat.phase("pcap")
        val pcap = File(cacheDir, "fixture.pcap")
        pcap.writeBytes(WiresharkDecoder.pcapBytes(fixture))
        val summary = WiresharkDecoder.decodePcap(pcap.absolutePath, 0)
        sb.append("PCAP IMPORT: decoded ").append(pcap.name).append(" (").append(pcap.length()).append(" bytes)\n")
        summary?.lineSequence()?.forEach { if (it.isNotBlank()) sb.append("  ").append(it).append('\n') }
        sb.append('\n')
    }.onFailure { sb.append("PCAP import error: ").append(it.message).append("\n\n") }

    // Frame-by-frame reader (the capture-import path) on the five-frame test capture.
    runCatching {
        Heartbeat.phase("pcap-reader")
        val file = File(cacheDir, "lt_test_5frames.pcap")
        file.writeBytes(WiresharkDecoder.pcapBytes(WiresharkDecoder.TEST_FRAMES))
        sb.append("PCAP READER (capture-import path): ").append(file.name).append(" (").append(file.length()).append(" bytes)\n")
        val reader = WiresharkDecoder.openPcap(file.absolutePath)
        if (reader == null) sb.append("  reader unavailable\n")
        else reader.use { r ->
            if (r.error != null) sb.append("  open error: ").append(r.error).append('\n')
            else {
                sb.append("  file type ").append(r.fileType).append(", size ").append(r.fileSize).append(" bytes\n")
                var frames = 0
                var first: WiresharkDecoder.PcapFrame? = null
                while (true) {
                    val f = r.next() ?: break
                    frames++
                    if (first == null) first = f
                    sb.append("  #").append(f.number).append(' ').append(f.title)
                        .append(if (f.addresses.isNotBlank()) "  ${f.addresses}" else "")
                        .append("  ").append(f.capturedLength).append(" bytes, ").append(f.fields.size).append(" fields")
                        .append(if (f.malformed) ", MALFORMED" else "").append('\n')
                    f.first("wlan.ssid")?.let { sb.append("     wlan.ssid = ").append(it.display).append(" @ offset ").append(it.start).append(" len ").append(it.length).append('\n') }
                    f.first("wlan.fixed.reason_code")?.let { sb.append("     wlan.fixed.reason_code = ").append(it.display).append(" @ offset ").append(it.start).append('\n') }
                }
                r.error?.let { sb.append("  read error: ").append(it).append('\n') }
                val fixtureCheck = first?.let { f ->
                    val dump = WiresharkDecoder.TSHARK_4_4_19_EXPECTED.joinToString("\n") { (k, _) -> "$k : ${f.first(k)?.display ?: "<absent>"}" }
                    WiresharkDecoder.mismatchesAgainstReference(dump)
                }
                sb.append("  frames read: ").append(frames).append(" of ").append(WiresharkDecoder.TEST_FRAMES.size)
                    .append("; frame 1 fields matching TShark 4.4.19: ")
                    .append(if (fixtureCheck == null) "n/a" else "${6 - fixtureCheck.size} of 6").append("\n\n")
            }
        }
    }.onFailure { sb.append("PCAP reader error: ").append(it.javaClass.simpleName).append(": ").append(it.message).append("\n\n") }

    sb.append("memory after fixture work: ").append(sampleMem().oneLine()).append('\n')

    if (cfg.decodes > 0) runStability(sb, fixture, decoded, cfg)
    return finish(sb)
}

/**
 * Warm up, take a baseline after warm-up, then N iterations sampled 10 times.
 * Growth is measured from the post-warm-up baseline, not from the first decode.
 * With cfg.gc, a collection is requested before each sample (diagnostic only);
 * the per-sample line marks whether GC was requested so normal-run behaviour is
 * never hidden.
 */
private fun runStability(sb: StringBuilder, fixture: ByteArray, expected: String, cfg: StabilityConfig) {
    val modeName = if (cfg.control) "CONTROL (JNI round trip, no dissection)" else "DECODE (full libwireshark)"
    sb.append("\nSTABILITY TEST: ").append(cfg.decodes).append(" iterations, mode ").append(modeName)
        .append(", warm-up ").append(cfg.warmup).append(", GC before each sample: ").append(if (cfg.gc) "yes (diagnostic)" else "no (normal run)").append('\n')
    fun once(): String? = if (cfg.control) WiresharkDecoder.control(fixture) else WiresharkDecoder.decode(fixture)

    Heartbeat.phase("warmup")
    for (i in 1..cfg.warmup) { once(); Heartbeat.tick("warmup", i, cfg.warmup) }
    if (cfg.gc) requestGc()
    val base = sampleMem()
    sb.append("post-warm-up baseline (after ").append(cfg.warmup).append(" iterations): ").append(base.oneLine()).append('\n')

    val n = cfg.decodes; val step = maxOf(1, n / 10)
    var mismatches = 0
    val t0 = System.nanoTime()
    var last = base
    var lastAt = 0
    val rssDrops = ArrayList<String>()
    val nativeSamples = ArrayList<Pair<Int, Long>>()   // (iteration, nHeapAlloc delta from baseline, KB)
    Heartbeat.phase("stability")
    for (i in 1..n) {
        val s = once()
        Heartbeat.tick(if (cfg.control) "control" else "stability", i, n)
        if (!cfg.control && s != expected) mismatches++
        else if (cfg.control && s == null) mismatches++
        if (i % step == 0) {
            if (cfg.gc) requestGc()
            val prev = last
            last = sampleMem()
            val d = last - base
            val line = "  %7d  %6.1f s  %s | from baseline %s".format(i, (System.nanoTime() - t0) / 1e9, last.oneLine(), d.delta())
            sb.append(line).append('\n'); Log.i(TAG, "progress$line")
            nativeSamples.add(i to d.nativeHeapAlloc)
            val step2 = last - prev
            if (-step2.rss > StabilityVerdict.RSS_DROP_REPORT_KB) {
                val obs = StabilityVerdict.rssDropLine(lastAt, i, step2.rss, step2.javaHeap, step2.nativeHeap,
                    step2.code, step2.system, step2.totalPss)
                rssDrops.add(obs); sb.append(obs).append('\n'); Log.i(TAG, obs)
            }
            lastAt = i
        }
    }
    val totalS = (System.nanoTime() - t0) / 1e9
    val d = last - base
    sb.append("mismatched outputs: ").append(mismatches).append(" of ").append(n).append('\n')
    sb.append("mean iteration: %.0f us\n".format(totalS * 1e6 / n))
    sb.append("growth from post-warm-up baseline to end (").append(n).append(" iterations").append(if (cfg.gc) ", GC-sampled" else "").append("):\n  ").append(d.delta()).append('\n')
    sb.append("native-heap change per iteration: %+.1f bytes\n".format(d.nativeHeapAlloc * 1024.0 / n))
    rssDrops.forEach { sb.append(it.trim().let { o -> "RSS observation: ${o.removePrefix("observed: ")}" }).append('\n') }
    val trend = StabilityVerdict.secondHalfTrend(nativeSamples)
    sb.append("native-heap trend over the second half of the run: ").append(trend.describe()).append('\n')
    val verdict = StabilityVerdict.verdict(cfg.control, mismatches, d.nativeHeapAlloc, n, rssDrops.size, trend)
    sb.append("STABILITY RESULT: ").append(verdict).append(".\n")
    sb.append("The threshold is a diagnostic threshold, not a leak test. The raw figures above are unchanged. ")
        .append("GC is a request, not a guarantee.\n")
}

private fun finish(sb: StringBuilder): String {
    Heartbeat.phase("done")
    val s = sb.toString()
    s.lineSequence().forEach { Log.i(TAG, it) }
    Log.i(TAG, "END")
    return s
}
