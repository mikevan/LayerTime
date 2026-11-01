package com.projectrevivesolutions.layertime.controller.service

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.net.Uri
import android.os.Build
import androidx.lifecycle.LifecycleService
import androidx.lifecycle.lifecycleScope
import com.projectrevivesolutions.layertime.controller.R
import com.projectrevivesolutions.layertime.controller.wireshark.PcapImport
import kotlinx.coroutines.launch

/**
 * Foreground service that keeps a capture import alive while the app is not on
 * screen. Without it the import thread is throttled and then frozen as soon as
 * the activity leaves the foreground (Android 15 on the TCL T513V froze it at
 * about 16,000 frames; the LOKMAT watch's power manager can kill background
 * apps outright). The decoding itself stays in [PcapImport]; this service only
 * holds the foreground status and shows progress with a Cancel action.
 *
 * It stops itself as soon as the import ends, is cancelled, or fails.
 */
class PcapImportService : LifecycleService() {

    override fun onCreate() {
        super.onCreate()
        createChannel()
        lifecycleScope.launch {
            PcapImport.state.collect { st ->
                when (st) {
                    is PcapImport.State.Running -> notify(buildNotification(st))
                    is PcapImport.State.Done, is PcapImport.State.Failed, PcapImport.State.Idle -> {
                        if (started) { stopForegroundCompat(); stopSelf() }
                    }
                }
            }
        }
    }

    private var started = false

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        super.onStartCommand(intent, flags, startId)
        android.util.Log.i(PcapImport.TAG, "SERVICE onStartCommand action=${intent?.action} data=${intent?.data}")
        when (intent?.action) {
            ACTION_IMPORT -> {
                val uri = intent.data ?: return START_NOT_STICKY.also { stopSelf() }
                val initial = PcapImport.State.Running("selected file", "copy", 0, null, 0.0, 0.0, false)
                val n = buildNotification(initial)
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                    startForeground(NOTIF_ID, n, ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC)
                } else {
                    startForeground(NOTIF_ID, n)
                }
                started = true
                PcapImport.start(applicationContext, uri)
            }
            ACTION_CANCEL -> PcapImport.cancel()
        }
        return START_NOT_STICKY
    }

    private fun notify(n: Notification) {
        if (!started) return
        (getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager).notify(NOTIF_ID, n)
    }

    private fun buildNotification(st: PcapImport.State.Running): Notification {
        val text = when (st.phase) {
            "copy" -> "Copying ${st.fileName}."
            "init" -> "Loading Wireshark."
            else -> "Decoding ${st.fileName}: " + "%,d frames".format(st.frames) +
                (st.percent?.let { ", $it% of the file" } ?: "") + "."
        }
        val cancel = PendingIntent.getService(
            this, 0, Intent(this, PcapImportService::class.java).setAction(ACTION_CANCEL),
            PendingIntent.FLAG_UPDATE_CURRENT or (if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) PendingIntent.FLAG_IMMUTABLE else 0)
        )
        val builder = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
            Notification.Builder(this, CHANNEL_ID) else @Suppress("DEPRECATION") Notification.Builder(this)
        builder.setContentTitle("LayerScope capture import")
            .setContentText(text)
            .setSmallIcon(R.drawable.ic_launcher_foreground)
            .setOngoing(true)
            .setOnlyAlertOnce(true)
        if (st.percent != null && st.phase == "decode") builder.setProgress(100, st.percent, false)
        else builder.setProgress(0, 0, true)
        @Suppress("DEPRECATION")
        builder.addAction(Notification.Action.Builder(null as android.graphics.drawable.Icon?, "Cancel", cancel).build())
        return builder.build()
    }

    private fun createChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val nm = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
            nm.createNotificationChannel(NotificationChannel(CHANNEL_ID, "Capture import", NotificationManager.IMPORTANCE_LOW)
                .apply { description = "Progress of a capture file being decoded with Wireshark." })
        }
    }

    private fun stopForegroundCompat() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) stopForeground(STOP_FOREGROUND_REMOVE)
        else @Suppress("DEPRECATION") stopForeground(true)
    }

    companion object {
        const val CHANNEL_ID = "layertime_pcap_import"
        const val NOTIF_ID = 1002
        const val ACTION_IMPORT = "com.projectrevivesolutions.layertime.controller.PCAP_IMPORT"
        const val ACTION_CANCEL = "com.projectrevivesolutions.layertime.controller.PCAP_CANCEL"

        /** Start an import of [uri] under the foreground service. */
        fun importFile(context: Context, uri: Uri) {
            android.util.Log.i(PcapImport.TAG, "SERVICE start requested for $uri")
            val i = Intent(context, PcapImportService::class.java).setAction(ACTION_IMPORT).setData(uri)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) context.startForegroundService(i)
            else context.startService(i)
        }
    }
}
