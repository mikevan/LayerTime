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
 * One alert per row, and a row is one MAC address (build 74): the same device seen
 * again, even as a new Hunter record, does not alert again. A Low row that rises to
 * Medium or High alerts then. [AlertSelector] decides; what it has alerted is
 * remembered across restarts so a reconnect or an app restart does not alert the
 * same detections again. More than three new at once collapse into one summary
 * alert. While the Hunter reports sleep mode, alerts are silent, as on
 * the Garmin face.
 *
 * Low-confidence detections never raise an alert. LayerTime-Sensors defines Low as
 * "worth logging, not worth an alert": since its 1.1.0, every Flock prefix that
 * belongs to a module maker (Liteon, Silicon Labs, Espressif, and others found in
 * ordinary laptops and gadgets) is Low. They still arrive, show in DETECTED ATTACKS,
 * and count as seen; they just do not buzz the wrist. See [AlertPolicy].
 */
class DetectionAlerts(private val ctx: Context) {
    private val prefs = ctx.getSharedPreferences("layertime_alerts", Context.MODE_PRIVATE)
    private val nm = ctx.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager

    init {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            nm.createNotificationChannel(NotificationChannel(CHANNEL_ID, "Detection alerts", NotificationManager.IMPORTANCE_HIGH).apply {
                description = "A new detection reported by the Hunter."
                enableVibration(true)
                vibrationPattern = VIBRATION
            })
            nm.createNotificationChannel(NotificationChannel(CHANNEL_QUIET, "Detection alerts in sleep mode", NotificationManager.IMPORTANCE_LOW).apply {
                description = "New detections while the Hunter is in sleep mode; no sound or vibration."
                enableVibration(false)
            })
        }
    }

    /** The stored alert state, cached after the first read. */
    private var saved: AlertSelector.State? = null

    /** Alert state while the session is unknown (-1); kept in memory only. */
    private var unknownSessionState = AlertSelector.State(session = -1)

    /** Called with the full event list whenever it changes; alerts only what is new. */
    fun onEvents(events: List<EventRow>, sleepMode: Boolean) {
        if (events.isEmpty()) return
        val session = events.first().sessionId
        if (session == -1) {
            // Should not happen now that the session comes from HELLO_ACK, but never let an
            // unknown session suppress or poison alerts: alert, remember in memory only.
            val r = AlertSelector.select(events, -1, unknownSessionState)
            unknownSessionState = r.state
            if (r.fresh.isEmpty()) return
            alert(r.fresh, sleepMode)
            Log.i(TAG, "alerted ${r.fresh.size} new detection(s) with the session not yet known")
            return
        }
        // Read from storage once; written back only when something changed, since this
        // runs on every update the Hunter sends.
        val prior = saved ?: loadState().also { saved = it }
        val r = AlertSelector.select(events, session, prior)
        if (r.state != prior) { saveState(r.state); saved = r.state }
        if (r.fresh.isEmpty()) return
        alert(r.fresh, sleepMode)
        Log.i(TAG, "alerted ${r.fresh.size} new detection(s) in session $session, rows ${r.fresh.map { it.eventId }}, sleep=$sleepMode")
    }

    private fun loadState(): AlertSelector.State {
        val session = prefs.getInt(KEY_SESSION, Int.MIN_VALUE)
        val ids = prefs.getString(KEY_ALERTED, "").orEmpty().split(',').mapNotNull { it.toLongOrNull() }.toSet()
        if (!prefs.contains(KEY_ALERTED)) {
            // First run after build 73, which remembered only the highest event id alerted.
            val legacy = prefs.getLong(KEY_LEGACY_EVENT, -1L)
            return AlertSelector.State(session, legacy, emptySet(), legacy)
        }
        return AlertSelector.State(session, prefs.getLong(KEY_MAX_ID, -1L), ids, prefs.getLong(KEY_THROUGH, -1L))
    }

    private fun saveState(s: AlertSelector.State) {
        prefs.edit().putInt(KEY_SESSION, s.session).putLong(KEY_MAX_ID, s.maxId)
            .putLong(KEY_THROUGH, s.alertedThrough)
            .putString(KEY_ALERTED, s.alerted.sorted().joinToString(",")).apply()
    }

    private fun alert(fresh: List<EventRow>, sleepMode: Boolean) {
        val worth = fresh   // AlertSelector already left out Low rows
        if (worth.size > 3) {
            val latest = worth.last()
            post(SUMMARY_ID, "${worth.size} new detections.", "Latest: ${describe(latest)}", sleepMode)
        } else {
            for (e in worth) post(NOTIF_BASE + (e.eventId % 1000).toInt(), "${e.detectorName} detected.", describe(e), sleepMode)
        }
    }

    private fun describe(e: EventRow): String =
        "Event ${e.eventId}" + (e.sourceId?.let { ", $it" } ?: "") +
            ", ${LinkConstants.Band.name(e.band)} channel ${e.channel}, ${e.rssi} dBm, " +
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
        // Build 74 keys. The build 73 key "alerted_event" is read once, on upgrade.
        private const val KEY_LEGACY_EVENT = "alerted_event"
        private const val KEY_THROUGH = "alerted_through"
        private const val KEY_MAX_ID = "alerted_max_row"
        private const val KEY_ALERTED = "alerted_rows"
        private const val NOTIF_BASE = 2000
        private const val SUMMARY_ID = 1999
        private val VIBRATION = longArrayOf(0, 400, 150, 400)
    }
}
