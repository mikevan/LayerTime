package com.projectrevivesolutions.layertime.controller.service

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.os.Build
import android.util.Log
import com.projectrevivesolutions.layertime.controller.R
import com.projectrevivesolutions.layertime.controller.link.LinkConstants
import com.projectrevivesolutions.layertime.controller.model.EventRow
import com.projectrevivesolutions.layertime.controller.ui.MainActivity

/**
 * Detection alerts for the always-on controller: a high-priority notification with
 * vibration for every new detection, raised by the link service, so it reaches the
 * operator whatever is on screen (the face, the console, another app, or a dark
 * screen). Tapping it opens the controller on DETECTED ATTACKS.
 *
 * "New" means an event id above the highest already alerted for that LayerWand
 * session, remembered across restarts so a reconnect or an app restart does not
 * alert the same detections again. More than three new at once collapse into one
 * summary alert. While the LayerWand reports sleep mode, alerts are silent, as on
 * the Garmin face.
 */
class DetectionAlerts(private val ctx: Context) {
    private val prefs = ctx.getSharedPreferences("layertime_alerts", Context.MODE_PRIVATE)
    private val nm = ctx.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager

    init {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            nm.createNotificationChannel(NotificationChannel(CHANNEL_ID, "Detection alerts", NotificationManager.IMPORTANCE_HIGH).apply {
                description = "A new detection reported by the LayerWand."
                enableVibration(true)
                vibrationPattern = VIBRATION
            })
            nm.createNotificationChannel(NotificationChannel(CHANNEL_QUIET, "Detection alerts in sleep mode", NotificationManager.IMPORTANCE_LOW).apply {
                description = "New detections while the LayerWand is in sleep mode; no sound or vibration."
                enableVibration(false)
            })
        }
    }

    /** Events alerted while their session was unknown (-1); kept in memory only. */
    private val unknownSessionAlerted = HashSet<Long>()

    /** Called with the full event list whenever it changes; alerts only what is new. */
    fun onEvents(events: List<EventRow>, sleepMode: Boolean) {
        if (events.isEmpty()) return
        val session = events.first().sessionId
        if (session == -1) {
            // Should not happen now that the session comes from HELLO_ACK, but never let an
            // unknown session suppress or poison alerts: alert, remember in memory only.
            val fresh = events.filter { it.eventId !in unknownSessionAlerted }.sortedBy { it.eventId }
            if (fresh.isEmpty()) return
            unknownSessionAlerted.addAll(fresh.map { it.eventId })
            alert(fresh, sleepMode)
            Log.i(TAG, "alerted ${fresh.size} new detection(s) with the session not yet known")
            return
        }
        val lastSession = prefs.getInt(KEY_SESSION, Int.MIN_VALUE)
        var lastId = if (session == lastSession) prefs.getLong(KEY_EVENT, -1L) else -1L
        // Event ids only grow within a session; if every id is below the last one alerted, the
        // LayerWand restarted and happened to reuse the session number, so start over.
        if (lastId >= 0 && events.maxOf { it.eventId } < lastId) lastId = -1L
        val fresh = events.filter { it.sessionId == session && it.eventId > lastId }.sortedBy { it.eventId }
        if (fresh.isEmpty()) return
        prefs.edit().putInt(KEY_SESSION, session).putLong(KEY_EVENT, fresh.last().eventId).apply()
        alert(fresh, sleepMode)
        Log.i(TAG, "alerted ${fresh.size} new detection(s) in session $session, last event ${fresh.last().eventId}, sleep=$sleepMode")
    }

    private fun alert(fresh: List<EventRow>, sleepMode: Boolean) {
        if (fresh.size > 3) {
            val latest = fresh.last()
            post(SUMMARY_ID, "${fresh.size} new detections.", "Latest: ${describe(latest)}", sleepMode)
        } else {
            for (e in fresh) post(NOTIF_BASE + (e.eventId % 1000).toInt(), "${e.detectorName} detected.", describe(e), sleepMode)
        }
    }

    private fun describe(e: EventRow): String =
        "Event ${e.eventId}, ${LinkConstants.Band.name(e.band)} channel ${e.channel}, ${e.rssi} dBm, " +
            "${LinkConstants.Confidence.name(e.confidence).lowercase()} confidence."

    private fun post(id: Int, title: String, text: String, quiet: Boolean) {
        val open = PendingIntent.getActivity(
            ctx, 0,
            Intent(ctx, MainActivity::class.java)
                .putExtra(MainActivity.EXTRA_PAGE, "attacks")
                .addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP or Intent.FLAG_ACTIVITY_CLEAR_TOP),
            PendingIntent.FLAG_UPDATE_CURRENT or (if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) PendingIntent.FLAG_IMMUTABLE else 0),
        )
        val b = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
            Notification.Builder(ctx, if (quiet) CHANNEL_QUIET else CHANNEL_ID)
        else @Suppress("DEPRECATION") Notification.Builder(ctx).apply {
            setPriority(if (quiet) Notification.PRIORITY_LOW else Notification.PRIORITY_HIGH)
            if (!quiet) setVibrate(VIBRATION)
        }
        b.setContentTitle(title).setContentText(text).setSmallIcon(R.drawable.ic_launcher_foreground)
            .setCategory(Notification.CATEGORY_ALARM).setContentIntent(open).setAutoCancel(true)
            .setStyle(Notification.BigTextStyle().bigText(text))
        runCatching { nm.notify(id, b.build()) }.onFailure { Log.i(TAG, "alert not posted: ${it.message}") }
    }

    companion object {
        private const val TAG = "LT_ALERTS"
        const val CHANNEL_ID = "layertime_detections"
        const val CHANNEL_QUIET = "layertime_detections_quiet"
        private const val KEY_SESSION = "alerted_session"
        private const val KEY_EVENT = "alerted_event"
        private const val NOTIF_BASE = 2000
        private const val SUMMARY_ID = 1999
        private val VIBRATION = longArrayOf(0, 400, 150, 400)
    }
}
