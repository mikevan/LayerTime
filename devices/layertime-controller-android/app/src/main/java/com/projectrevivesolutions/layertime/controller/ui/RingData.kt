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

/**
 * Sources for the face's two rings. Temperature comes only from a real ambient
 * temperature sensor; with none, the ring says N/A rather than showing battery
 * or CPU heat. Sunrise and sunset come from the device's last known location
 * and [SunTimes]; with no location yet, the ring shows "--".
 */
object RingData {
    private const val TAG = "LT_RINGS"

    /** True when the app may read location (Android 11 and older grant it with BLE). */
    fun hasLocationPermission(ctx: Context): Boolean =
        ContextCompat.checkSelfPermission(ctx, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED ||
            ContextCompat.checkSelfPermission(ctx, Manifest.permission.ACCESS_COARSE_LOCATION) == PackageManager.PERMISSION_GRANTED

    @SuppressLint("MissingPermission")
    fun lastKnown(ctx: Context): Location? {
        if (!hasLocationPermission(ctx)) return null
        val lm = ctx.getSystemService(Context.LOCATION_SERVICE) as? LocationManager ?: return null
        return listOf(LocationManager.GPS_PROVIDER, LocationManager.NETWORK_PROVIDER, LocationManager.PASSIVE_PROVIDER)
            .mapNotNull { p -> runCatching { lm.getLastKnownLocation(p) }.getOrNull() }
            .maxByOrNull { it.time }
    }

    /** Ask once for a fresh fix when nothing is cached; the callback delivers it and unregisters. */
    @SuppressLint("MissingPermission")
    fun requestOneFix(ctx: Context, onFix: (Location) -> Unit) {
        if (!hasLocationPermission(ctx)) { Log.i(TAG, "no location permission; sunrise unavailable"); return }
        val lm = ctx.getSystemService(Context.LOCATION_SERVICE) as? LocationManager ?: return
        val provider = listOf(LocationManager.NETWORK_PROVIDER, LocationManager.GPS_PROVIDER)
            .firstOrNull { runCatching { lm.isProviderEnabled(it) }.getOrDefault(false) }
            ?: run { Log.i(TAG, "no location provider enabled; sunrise unavailable"); return }
        val listener = object : LocationListener {
            override fun onLocationChanged(l: Location) { lm.removeUpdates(this); Log.i(TAG, "location fix from $provider"); onFix(l) }
            @Deprecated("API < 29 callback") override fun onStatusChanged(p: String?, s: Int, e: Bundle?) {}
            override fun onProviderEnabled(p: String) {}
            override fun onProviderDisabled(p: String) {}
        }
        runCatching { lm.requestLocationUpdates(provider, 0L, 0f, listener, Looper.getMainLooper()) }
            .onFailure { Log.i(TAG, "location request failed: ${it.message}") }
    }

    /** Registers the ambient temperature sensor if the device has one. Returns an unregister action, or null with no sensor. */
    fun watchAmbientTemperature(ctx: Context, onCelsius: (Float) -> Unit): (() -> Unit)? {
        val sm = ctx.getSystemService(Context.SENSOR_SERVICE) as? SensorManager ?: return null
        val sensor = sm.getDefaultSensor(Sensor.TYPE_AMBIENT_TEMPERATURE)
        if (sensor == null) { Log.i(TAG, "no ambient temperature sensor; TEMP shows N/A"); return null }
        Log.i(TAG, "ambient temperature sensor: ${sensor.name}")
        val l = object : SensorEventListener {
            override fun onSensorChanged(e: SensorEvent) { onCelsius(e.values[0]) }
            override fun onAccuracyChanged(s: Sensor?, a: Int) {}
        }
        sm.registerListener(l, sensor, SensorManager.SENSOR_DELAY_NORMAL)
        return { sm.unregisterListener(l) }
    }

    /** Fahrenheit for the United States and its territories, Celsius elsewhere. */
    fun useFahrenheit(locale: Locale = Locale.getDefault()): Boolean =
        locale.country in setOf("US", "LR", "MM", "PR", "GU", "VI", "AS", "MP", "UM")
}
