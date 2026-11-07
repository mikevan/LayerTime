package com.projectrevivesolutions.layertime.controller.link

import android.annotation.SuppressLint
import android.bluetooth.BluetoothManager
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.os.Handler
import android.os.Looper
import android.os.ParcelUuid
import java.util.UUID

/**
 * SCAN on HUNTER ACTIONS (Michael, 2026-11-06, option 2). Listens for Hunters by service UUID
 * for [HunterRoster.SCAN_MS] and lists every one it hears, right away. It does not connect to
 * anything: a scan is only "who is in range," which keeps it fast and reliable and leaves the
 * radio free for several Hunters at once (the groundwork for finding a signal's direction).
 *
 * The SD-card answer does NOT come from the scan. It comes from the live Status of the Hunter
 * you connect to (LinkClient.lastNoSdLog, overlaid by [HunterRoster.withHeld]), because that is
 * the one connection that reads it reliably. So the card label and Clear Card appear once a
 * Hunter is connected, not during the scan. The 2026-11-06 capture showed the old scan-time
 * probe stalling on service discovery while the real connect a second later read it in 0.6 s.
 *
 * The Hunter this phone already holds is not advertising, so it is passed in and seeded into the
 * list with its live card, and it stays shown through [HunterRoster.withHeld] while connected.
 */
@SuppressLint("MissingPermission")
object HunterScanner {
    data class Connected(val address: String, val name: String, val card: HunterRoster.Card)

    private val handler = Handler(Looper.getMainLooper())
    private var appContext: Context? = null

    /** Starts a scan unless one is running. [connected]: the Hunter this phone is connected to, if any. */
    fun scan(context: Context, connected: Connected?) { handler.post { scanNow(context, connected) } }

    /** Stops a running scan now (the operator tapped Connect): the listed Hunters stay. */
    fun cancel() {
        handler.post {
            if (!HunterRoster.scanning) return@post
            val ctx = appContext
            val scanner = (ctx?.getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager)?.adapter?.bluetoothLeScanner
            runCatching { scanner?.stopScan(scanCallback) }
            diag("scan cancelled by connect")
            HunterRoster.setScan(HunterRoster.Scan.Done(HunterRoster.hunters.value.size))
        }
    }

    private fun scanNow(context: Context, connected: Connected?) {
        if (HunterRoster.scanning) return
        val ctx = context.applicationContext
        appContext = ctx
        val adapter = (ctx.getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager)?.adapter
        if (adapter == null || !adapter.isEnabled) {
            HunterRoster.setScan(HunterRoster.Scan.Failed("Bluetooth is off. Turn it on and tap Scan again.")); return
        }
        if (LinkClient.locationSwitchBlocksScanning(ctx)) {
            HunterRoster.setScan(HunterRoster.Scan.Failed(LinkClient.LOCATION_OFF_MESSAGE)); return
        }
        // A fresh list each scan: a Hunter that was switched off drops out. The held Hunter,
        // which does not advertise while connected, is seeded with its live card so it still shows.
        HunterRoster.update { _ -> emptyList() }
        connected?.let { c -> HunterRoster.update { HunterRoster.withCard(HunterRoster.seen(it, c.address, c.name, 0), c.address, c.card) } }
        val scanner = adapter.bluetoothLeScanner
        if (scanner == null) {
            HunterRoster.setScan(HunterRoster.Scan.Failed("This phone cannot scan for Bluetooth devices right now. Turn Bluetooth off and on, then tap Scan again.")); return
        }
        val filter = ScanFilter.Builder().setServiceUuid(ParcelUuid(UUID.fromString(LinkConstants.SERVICE_UUID))).build()
        val settings = ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build()
        HunterRoster.setScan(HunterRoster.Scan.Listening(HunterRoster.hunters.value.size))
        diag("scan start; held Hunter ${connected?.name ?: "none"} ${connected?.address ?: ""} card ${connected?.card ?: "-"}")
        val started = runCatching { scanner.startScan(listOf(filter), settings, scanCallback) }.isSuccess
        if (!started) {
            HunterRoster.setScan(HunterRoster.Scan.Failed("LayerScope does not have permission to scan for Hunters. Allow Nearby devices for LayerScope in Settings.")); return
        }
        handler.postDelayed({
            runCatching { scanner.stopScan(scanCallback) }
            if (HunterRoster.scan.value is HunterRoster.Scan.Failed) return@postDelayed
            diag("scan done: " + HunterRoster.hunters.value.joinToString { "${it.name} ${it.address}" })
            HunterRoster.setScan(HunterRoster.Scan.Done(HunterRoster.hunters.value.size))
        }, HunterRoster.SCAN_MS)
    }

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val device = result.device ?: return
            val name = runCatching { device.name }.getOrNull() ?: result.scanRecord?.deviceName
            handler.post {
                if (HunterRoster.hunters.value.none { it.address == device.address }) diag("heard ${name ?: "?"} ${device.address} rssi ${result.rssi}")
                HunterRoster.update { HunterRoster.seen(it, device.address, name, result.rssi) }
                if (HunterRoster.scan.value is HunterRoster.Scan.Listening)
                    HunterRoster.setScan(HunterRoster.Scan.Listening(HunterRoster.hunters.value.size))
            }
        }

        override fun onScanFailed(errorCode: Int) {
            handler.post {
                if (errorCode != SCAN_FAILED_ALREADY_STARTED)
                    HunterRoster.setScan(HunterRoster.Scan.Failed("The Bluetooth scan failed (code $errorCode). Tap Scan again."))
            }
        }
    }

    /** Diagnostics (2026-11-06): each scan to logcat, tag LayerScope.Scan. */
    private fun diag(msg: String) { runCatching { android.util.Log.i("LayerScope.Scan", msg) } }
}
