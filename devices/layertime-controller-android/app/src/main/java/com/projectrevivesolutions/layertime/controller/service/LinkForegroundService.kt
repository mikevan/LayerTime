package com.projectrevivesolutions.layertime.controller.service

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.Binder
import android.os.Build
import android.os.IBinder
import androidx.lifecycle.LifecycleService
import androidx.lifecycle.lifecycleScope
import com.projectrevivesolutions.layertime.controller.R
import com.projectrevivesolutions.layertime.controller.link.LinkClient
import com.projectrevivesolutions.layertime.controller.model.ConnState
import com.projectrevivesolutions.layertime.controller.state.ControllerRepository
import kotlinx.coroutines.launch

/**
 * Foreground service that owns the BLE link so reception continues while the
 * screen is off or the app is backgrounded. The notification reflects the live
 * connection state. This is what makes the screen-off reception test meaningful.
 */
class LinkForegroundService : LifecycleService() {

    private lateinit var client: LinkClient
    private val binder = LocalBinder()

    inner class LocalBinder : Binder() {
        fun client(): LinkClient = client
    }

    override fun onCreate() {
        super.onCreate()
        client = LinkClient(applicationContext)
        active = client
        createChannel()
        lifecycleScope.launch {
            ControllerRepository.conn.collect { updateNotification(it) }
        }
    }

    override fun onDestroy() {
        if (active === client) active = null
        super.onDestroy()
    }

    override fun onBind(intent: Intent): IBinder {
        super.onBind(intent)
        return binder
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        super.onStartCommand(intent, flags, startId)
        startAsForeground(ControllerRepository.conn.value)
        when (intent?.action) {
            ACTION_START -> {
                val target = intent.getIntExtra(EXTRA_TARGET, -1).takeIf { it >= 0 }
                client.start(autoStart = target)
            }
            ACTION_STOP -> {
                client.stop()
                stopForegroundCompat()
                stopSelf()
            }
        }
        return START_STICKY
    }

    private fun startAsForeground(state: ConnState) {
        val n = buildNotification(state)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            startForeground(NOTIF_ID, n, ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE)
        } else {
            startForeground(NOTIF_ID, n)
        }
    }

    private fun updateNotification(state: ConnState) {
        val nm = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        nm.notify(NOTIF_ID, buildNotification(state))
    }

    private fun buildNotification(state: ConnState): Notification {
        val label = ControllerRepository.deviceLabel.value.ifBlank { "LayerWand" }
        val text = when (state) {
            ConnState.CONNECTED -> "Linked to $label"
            ConnState.SCANNING -> "Scanning for a LayerWand"
            ConnState.CONNECTING, ConnState.DISCOVERING, ConnState.HANDSHAKING -> "Connecting to $label"
            ConnState.DISCONNECTED -> "Reconnecting to $label"
            ConnState.FAILED -> "Link failed"
            ConnState.IDLE -> "Idle"
        }
        val builder = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
            Notification.Builder(this, CHANNEL_ID) else @Suppress("DEPRECATION") Notification.Builder(this)
        return builder
            .setContentTitle("LayerTime Controller")
            .setContentText(text)
            .setSmallIcon(R.drawable.ic_launcher_foreground)
            .setOngoing(true)
            .build()
    }

    private fun createChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val nm = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
            val ch = NotificationChannel(
                CHANNEL_ID, getString(R.string.notif_channel_name), NotificationManager.IMPORTANCE_LOW
            ).apply { description = getString(R.string.notif_channel_desc) }
            nm.createNotificationChannel(ch)
        }
    }

    private fun stopForegroundCompat() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) stopForeground(STOP_FOREGROUND_REMOVE)
        else @Suppress("DEPRECATION") stopForeground(true)
    }

    companion object {
        /** The live client while the service runs, for the UI to issue commands. */
        @Volatile var active: LinkClient? = null

        const val CHANNEL_ID = "layertime_link"
        const val NOTIF_ID = 1001
        const val ACTION_START = "com.projectrevivesolutions.layertime.controller.START"
        const val ACTION_STOP = "com.projectrevivesolutions.layertime.controller.STOP"
        const val EXTRA_TARGET = "target"

        fun start(context: Context, target: Int?) {
            val i = Intent(context, LinkForegroundService::class.java).setAction(ACTION_START)
            if (target != null) i.putExtra(EXTRA_TARGET, target)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) context.startForegroundService(i)
            else context.startService(i)
        }

        fun stop(context: Context) {
            context.startService(Intent(context, LinkForegroundService::class.java).setAction(ACTION_STOP))
        }
    }
}
