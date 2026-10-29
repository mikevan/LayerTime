package com.projectrevivesolutions.layertime.controller.wireshark

import android.content.Context
import android.net.Uri
import android.provider.OpenableColumns
import android.util.Log
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import java.io.File

/**
 * Imports a capture file the user selects and decodes every frame with
 * Wireshark (libwireshark/epan on the phone). One import runs at a time, on
 * its own thread, and survives leaving and reopening the event-detail screen.
 *
 * Progress is visible two ways: [state] drives the on-screen progress line,
 * and logcat tag LTWS_PCAP carries PHASE lines, a HEARTBEAT at least every
 * 5 s, and a final "PCAP DONE", "PCAP CANCELLED", or "PCAP FAILED" line.
 *
 * The file is copied into the app's cache first, because Wireshark reads
 * files by path and the system picker hands out content URIs.
 *
 * An imported capture is evidence the user chose. It is not linked to any
 * Link event, and decoding it makes no detection or "countered" claim.
 */
object PcapImport {
    const val TAG = "LTWS_PCAP"

    /** Frames whose full protocol tree is kept for display. Every frame is still decoded and counted. */
    const val KEEP_LIMIT = 500

    sealed interface State {
        data object Idle : State
        data class Running(val fileName: String, val phase: String, val frames: Int, val percent: Int?,
                           val ratePerSecond: Double, val elapsedSeconds: Double, val slowing: Boolean) : State
        data class Done(val fileName: String, val fileBytes: Long, val fileType: String, val wiresharkVersion: String,
                        val result: CaptureDecodeLoop.Result) : State
        data class Failed(val fileName: String, val message: String) : State
    }

    private val _state = MutableStateFlow<State>(State.Idle)
    val state: StateFlow<State> = _state.asStateFlow()

    @Volatile private var cancelRequested = false
    @Volatile private var worker: Thread? = null

    val running: Boolean get() = worker?.isAlive == true

    fun cancel() { if (running) { cancelRequested = true; Log.i(TAG, "PCAP cancel requested") } }

    fun clear() { if (!running) _state.value = State.Idle }

    /** Start an import. Ignored while another import is running. */
    @Synchronized
    fun start(context: Context, uri: Uri) {
        Log.i(TAG, "IMPORT start requested uri=$uri running=$running")
        if (running) { Log.i(TAG, "IMPORT ignored: another import is running"); return }
        val app = context.applicationContext
        cancelRequested = false
        val name = displayName(app, uri)
        _state.value = State.Running(name, "copy", 0, null, 0.0, 0.0, false)
        worker = Thread({ runImport(app, uri, name) }, "ltws-pcap-import").apply { start() }
    }

    private fun displayName(context: Context, uri: Uri): String = runCatching {
        context.contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use { c ->
            if (c.moveToFirst()) c.getString(0) else null
        }
    }.getOrNull() ?: uri.lastPathSegment ?: "selected file"

    private fun phase(name: String, fileName: String) {
        Log.i(TAG, "PHASE $name")
        _state.value = State.Running(fileName, name, 0, null, 0.0, 0.0, false)
    }

    private fun fail(fileName: String, message: String) {
        Log.i(TAG, "PCAP FAILED file=$fileName: $message")
        _state.value = State.Failed(fileName, message)
    }

    private fun runImport(context: Context, uri: Uri, fileName: String) {
        try {
            phase("copy", fileName)
            val dir = File(context.cacheDir, "pcap_import").apply { mkdirs() }
            val local = File(dir, "import.cap")
            val bytes = context.contentResolver.openInputStream(uri)?.use { input ->
                local.outputStream().use { out -> input.copyTo(out, 64 * 1024) }
            } ?: return fail(fileName, "The file could not be opened for reading.")
            Log.i(TAG, "PCAP file=$fileName bytes=$bytes")
            if (bytes == 0L) return fail(fileName, "The file is empty.")
            if (cancelRequested) return cancelledBeforeDecode(fileName)

            phase("init", fileName)
            if (!WiresharkDecoder.ensureLoaded()) {
                val f = WiresharkDecoder.failure()
                return fail(fileName, "The Wireshark decoder did not load. ${f?.explanation() ?: ""} Loader error: ${f?.raw ?: "unknown"}")
            }
            val init = WiresharkDecoder.initialize()
            if (init == null || !init.ok) return fail(fileName, "Wireshark initialization failed (epan_init returned false).")
            val version = WiresharkDecoder.version() ?: "unknown"
            if (cancelRequested) return cancelledBeforeDecode(fileName)

            phase("decode", fileName)
            val reader = WiresharkDecoder.openPcap(local.absolutePath)
                ?: return fail(fileName, "The Wireshark decoder is not available.")
            reader.use { r ->
                r.error?.let { return fail(fileName, it) }
                Log.i(TAG, "PCAP opened type=${r.fileType} size=${r.fileSize} wireshark=$version")
                var logged = 0
                val result = CaptureDecodeLoop.run(
                    source = r, keepLimit = KEEP_LIMIT,
                    cancelled = { cancelRequested },
                    nowNs = System::nanoTime,
                    onBeat = { b ->
                        Log.i(TAG, b.line())
                        _state.value = State.Running(fileName, "decode", b.frames, b.percentOfFile,
                            b.ratePerSecond, b.elapsedSeconds, b.slowing)
                    },
                    onFrame = { f ->
                        // The first frames are logged in full key fields so a run can be checked from logcat.
                        if (logged < 5) {
                            logged++
                            val keys = listOf("wlan.fc.type_subtype", "wlan.sa", "wlan.da", "wlan.bssid",
                                "wlan.seq", "wlan.fixed.reason_code", "wlan.ssid", "wlan.ds.current_channel")
                            val kv = keys.mapNotNull { k -> f.first(k)?.let { "$k=${it.display}@${it.start}+${it.length}" } }
                            Log.i(TAG, "FRAME ${f.number} len=${f.capturedLength} encap=${f.encapName} fields=${f.fields.size}" +
                                (if (f.malformed) " MALFORMED" else "") + " " + kv.joinToString(" "))
                        }
                    }
                )
                val tail = "frames=${result.totalFrames} kept=${result.kept.size} malformed=${result.malformedFrames} " +
                    "elapsed=" + "%.1f".format(result.elapsedSeconds) + " s type=${r.fileType} file=$fileName"
                when {
                    result.cancelled -> Log.i(TAG, "PCAP CANCELLED $tail")
                    result.readError != null -> Log.i(TAG, "PCAP DONE WITH READ ERROR $tail error=${result.readError}")
                    else -> Log.i(TAG, "PCAP DONE $tail")
                }
                _state.value = State.Done(fileName, bytes, r.fileType, version, result)
            }
        } catch (t: Throwable) {
            Log.e(TAG, "PCAP FAILED file=$fileName", t)
            _state.value = State.Failed(fileName, "Import stopped with an error: ${t.javaClass.simpleName}: ${t.message}")
        }
    }

    private fun cancelledBeforeDecode(fileName: String) {
        Log.i(TAG, "PCAP CANCELLED frames=0 file=$fileName")
        _state.value = State.Failed(fileName, "The import was cancelled before decoding started.")
    }
}
