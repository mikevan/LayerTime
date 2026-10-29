package com.projectrevivesolutions.layertime.controller.ui

import android.Manifest
import android.annotation.SuppressLint
import android.content.Context
import android.content.pm.PackageManager
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.location.Location
import android.location.LocationListener
import android.location.LocationManager
import android.os.Bundle
import android.os.Looper
import android.util.Log
import androidx.core.content.ContextCompat
import java.util.Locale
import kotlin.coroutines.resume
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withTimeoutOrNull

/**
 * Sources for the face's two rings. Temperature comes only from a real ambient
 * temperature sensor; with none, the ring says N/A rather than showing battery
 * or CPU heat, and "--" while a sensor has not reported yet. Sunrise and sunset
 * come from a GPS (or network) fix and [SunTimes]; the last fix is saved so a
 * restart keeps it, and with none yet the ring reads NO FIX.
 */
object RingData {
    private const val TAG = "LT_RINGS"

    /** True when the app may read location (Android 11 and older grant it with BLE). */
    fun hasLocationPermission(ctx: Context): Boolean =
        ContextCompat.checkSelfPermission(ctx, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED ||
            ContextCompat.checkSelfPermission(ctx, Manifest.permission.ACCESS_COARSE_LOCATION) == PackageManager.PERMISSION_GRANTED

    private const val PREFS = "layertime_ui"

    /** The last fix this app saw, saved so a restart does not start from nothing (the watch has no network location). */
    fun savedFix(ctx: Context): Location? {
        val p = ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        if (!p.contains("fix_lat")) return null
        return Location("saved").apply {
            latitude = p.getFloat("fix_lat", 0f).toDouble()
            longitude = p.getFloat("fix_lon", 0f).toDouble()
            time = p.getLong("fix_time", 0L)
        }
    }

    private fun saveFix(ctx: Context, l: Location) {
        ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit()
            .putFloat("fix_lat", l.latitude.toFloat()).putFloat("fix_lon", l.longitude.toFloat())
            .putLong("fix_time", l.time).apply()
    }

    /** A position the operator entered by hand, with when it was entered. */
    data class Manual(val lat: Double, val lon: Double, val time: Long)

    fun manualPosition(ctx: Context): Manual? {
        val p = ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        if (!p.contains("man_lat")) return null
        return Manual(java.lang.Double.longBitsToDouble(p.getLong("man_lat", 0L)),
            java.lang.Double.longBitsToDouble(p.getLong("man_lon", 0L)), p.getLong("man_time", 0L))
    }

    fun saveManual(ctx: Context, lat: Double, lon: Double): Manual {
        val m = Manual(lat, lon, System.currentTimeMillis())
        ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit()
            .putLong("man_lat", java.lang.Double.doubleToRawLongBits(lat))
            .putLong("man_lon", java.lang.Double.doubleToRawLongBits(lon))
            .putLong("man_time", m.time).apply()
        Log.i(TAG, "manual position set")
        return m
    }

    fun clearManual(ctx: Context) {
        ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit()
            .remove("man_lat").remove("man_lon").remove("man_time").apply()
        Log.i(TAG, "manual position cleared")
    }

    /** The newest of the system's last known locations and this app's saved fix. */
    @SuppressLint("MissingPermission")
    fun lastKnown(ctx: Context): Location? {
        val system = if (!hasLocationPermission(ctx)) emptyList() else {
            val lm = ctx.getSystemService(Context.LOCATION_SERVICE) as? LocationManager
            listOf(LocationManager.GPS_PROVIDER, LocationManager.NETWORK_PROVIDER, LocationManager.PASSIVE_PROVIDER)
                .mapNotNull { p -> runCatching { lm?.getLastKnownLocation(p) }.getOrNull() }
        }
        return (system + listOfNotNull(savedFix(ctx))).maxByOrNull { it.time }
    }

    /**
     * Waits up to [timeoutMs] for one fix from GPS (and from network location when it is
     * enabled), saves it, and returns it; null on timeout, with no permission, or with
     * every provider off. Listeners are removed on return or cancellation.
     */
    @SuppressLint("MissingPermission")
    suspend fun awaitFix(ctx: Context, timeoutMs: Long): Location? {
        if (!hasLocationPermission(ctx)) { Log.i(TAG, "no location permission; sunrise unavailable"); return null }
        val lm = ctx.getSystemService(Context.LOCATION_SERVICE) as? LocationManager ?: return null
        val providers = listOf(LocationManager.GPS_PROVIDER, LocationManager.NETWORK_PROVIDER)
            .filter { runCatching { lm.isProviderEnabled(it) }.getOrDefault(false) }
        if (providers.isEmpty()) { Log.i(TAG, "no location provider enabled; sunrise unavailable"); return null }
        Log.i(TAG, "requesting a fix from $providers for ${timeoutMs / 1000} s")
        val started = System.currentTimeMillis()
        var listener: LocationListener? = null
        val fix = try {
            withTimeoutOrNull(timeoutMs) {
                suspendCancellableCoroutine<Location> { cont ->
                    val l = object : LocationListener {
                        override fun onLocationChanged(loc: Location) { if (cont.isActive) cont.resume(loc) }
                        @Deprecated("API < 29 callback") override fun onStatusChanged(p: String?, s: Int, e: Bundle?) {}
                        override fun onProviderEnabled(p: String) {}
                        override fun onProviderDisabled(p: String) {}
                    }
                    listener = l
                    for (p in providers) {
                        runCatching { lm.requestLocationUpdates(p, 1_000L, 0f, l, Looper.getMainLooper()) }
                            .onFailure { Log.i(TAG, "location request on $p failed: ${it.message}") }
                    }
                }
            }
        } finally {
            listener?.let { lm.removeUpdates(it) }
        }
        val secs = (System.currentTimeMillis() - started) / 1000
        if (fix == null) Log.i(TAG, "no fix after $secs s; will retry")
        else { Log.i(TAG, "fix from ${fix.provider} after $secs s, accuracy ${fix.accuracy} m"); saveFix(ctx, fix) }
        return fix
    }


    /**
     * Registers the ambient temperature sensor if the device has one, and logs what it
     * sends: the first readings, one a minute after that, and a line if nothing has
     * arrived 10 seconds after registering. Returns an unregister action, or null with
     * no sensor.
     */
    fun watchAmbientTemperature(ctx: Context, onSilent: () -> Unit = {}, onCelsius: (Float) -> Unit): (() -> Unit)? {
        val sm = ctx.getSystemService(Context.SENSOR_SERVICE) as? SensorManager ?: return null
        val sensor = sm.getDefaultSensor(Sensor.TYPE_AMBIENT_TEMPERATURE)
        if (sensor == null) { Log.i(TAG, "no ambient temperature sensor; TEMP shows N/A"); return null }
        Log.i(TAG, "ambient temperature sensor: ${sensor.name} by ${sensor.vendor}, reporting mode ${sensor.reportingMode}")
        var count = 0
        var lastLog = 0L
        val l = object : SensorEventListener {
            override fun onSensorChanged(e: SensorEvent) {
                count++
                val now = System.currentTimeMillis()
                if (count <= 3 || now - lastLog > 60_000L) { Log.i(TAG, "ambient temperature reading ${e.values[0]} C (event $count)"); lastLog = now }
                onCelsius(e.values[0])
            }
            override fun onAccuracyChanged(s: Sensor?, a: Int) {}
        }
        sm.registerListener(l, sensor, SensorManager.SENSOR_DELAY_NORMAL)
        val h = android.os.Handler(Looper.getMainLooper())
        val check = Runnable { if (count == 0) { Log.i(TAG, "ambient temperature sensor sent no reading in 10 s; treating it as unavailable"); onSilent() } }
        h.postDelayed(check, 10_000L)
        return { h.removeCallbacks(check); sm.unregisterListener(l) }
    }

    /**
     * Reports what the satellite receiver sees: satellites used in the fix and satellites
     * in view, delivered on every status update and logged every 30 seconds with the best
     * signal strength (C/N0, dB-Hz). Only fires while a location request is active.
     * Returns an unregister action, or null with no permission.
     */
    @SuppressLint("MissingPermission")
    fun watchSatellites(ctx: Context, onStatus: (used: Int, seen: Int) -> Unit): (() -> Unit)? {
        if (!hasLocationPermission(ctx)) return null
        val lm = ctx.getSystemService(Context.LOCATION_SERVICE) as? LocationManager ?: return null
        var lastLog = 0L
        val cb = object : android.location.GnssStatus.Callback() {
            override fun onSatelliteStatusChanged(status: android.location.GnssStatus) {
                val seen = status.satelliteCount
                var used = 0; var best = 0f
                for (i in 0 until seen) {
                    if (status.usedInFix(i)) used++
                    best = maxOf(best, status.getCn0DbHz(i))
                }
                val now = System.currentTimeMillis()
                if (now - lastLog > 30_000L) { Log.i(TAG, "satellites used $used of $seen in view, best signal $best dB-Hz"); lastLog = now }
                onStatus(used, seen)
            }
            override fun onStarted() { Log.i(TAG, "satellite receiver started") }
            override fun onStopped() { Log.i(TAG, "satellite receiver stopped") }
        }
        @Suppress("DEPRECATION")
        val ok = runCatching { lm.registerGnssStatusCallback(cb, android.os.Handler(Looper.getMainLooper())) }.getOrDefault(false)
        if (!ok) { Log.i(TAG, "satellite status unavailable"); return null }
        return { lm.unregisterGnssStatusCallback(cb) }
    }

    /** True when the device has an ambient temperature sensor at all. */
    fun hasAmbientTemperature(ctx: Context): Boolean =
        (ctx.getSystemService(Context.SENSOR_SERVICE) as? SensorManager)?.getDefaultSensor(Sensor.TYPE_AMBIENT_TEMPERATURE) != null

    /** Fahrenheit for the United States and its territories, Celsius elsewhere. */
    fun useFahrenheit(locale: Locale = Locale.getDefault()): Boolean =
        locale.country in setOf("US", "LR", "MM", "PR", "GU", "VI", "AS", "MP", "UM")
}
