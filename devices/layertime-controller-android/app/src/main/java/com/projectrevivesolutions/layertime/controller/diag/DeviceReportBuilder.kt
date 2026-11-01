package com.projectrevivesolutions.layertime.controller.diag

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import com.projectrevivesolutions.layertime.controller.model.DeviceReport

/**
 * Builds the device + BLE capability report the diagnostic milestone requires:
 * device model, Android version / API level, and BLE capability. Every value is
 * read from the platform, never assumed.
 */
object DeviceReportBuilder {

    @SuppressLint("MissingPermission") // only reads adapter flags; no scan/connect here
    fun build(context: Context): DeviceReport {
        val pm = context.packageManager
        val bleSupported = pm.hasSystemFeature(PackageManager.FEATURE_BLUETOOTH_LE)

        val adapter: BluetoothAdapter? =
            (context.getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager)?.adapter

        val bluetoothOn = adapter?.isEnabled == true
        val le2m = adapter?.isLe2MPhySupported == true
        val leCoded = adapter?.isLeCodedPhySupported == true
        val leExt = adapter?.isLeExtendedAdvertisingSupported == true
        val multiAdv = adapter?.isMultipleAdvertisementSupported == true

        val pkg = runCatching {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU)
                pm.getPackageInfo(context.packageName, PackageManager.PackageInfoFlags.of(0))
            else
                @Suppress("DEPRECATION") pm.getPackageInfo(context.packageName, 0)
        }.getOrNull()
        val vName = pkg?.versionName ?: "?"
        val vCode = pkg?.let {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) it.longVersionCode
            else @Suppress("DEPRECATION") it.versionCode.toLong()
        } ?: 0L

        return DeviceReport(
            manufacturer = Build.MANUFACTURER ?: "?",
            model = Build.MODEL ?: "?",
            device = Build.DEVICE ?: "?",
            product = Build.PRODUCT ?: "?",
            androidRelease = Build.VERSION.RELEASE ?: "?",
            sdkInt = Build.VERSION.SDK_INT,
            bleSupported = bleSupported,
            bluetoothOn = bluetoothOn,
            le2mPhy = le2m,
            leCodedPhy = leCoded,
            leExtendedAdvertising = leExt,
            multipleAdvertisement = multiAdv,
            maxConnectedAudioNote = "Android exposes no direct max-GATT-client count; " +
                "simultaneous-Hunter capacity is proven empirically, not queried.",
            appVersionName = vName,
            appVersionCode = vCode,
        )
    }
}
