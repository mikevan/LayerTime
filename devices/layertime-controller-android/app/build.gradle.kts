import java.io.FileInputStream
import java.util.Properties

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.compose")
}

// Release signing. keystore.properties is gitignored and holds the keystore path and passwords.
// Without it (a machine that does not have the key), release builds are left unsigned instead of failing.
val keystorePropsFile = rootProject.file("keystore.properties")
val keystoreProps = Properties().apply { if (keystorePropsFile.exists()) FileInputStream(keystorePropsFile).use { load(it) } }

android {
    namespace = "com.projectrevivesolutions.layertime.controller"
    compileSdk = 36
    // NDK r28+ links 16 KB-aligned by default. https://developer.android.com/guide/practices/page-sizes
    ndkVersion = "30.0.16248370"

    defaultConfig {
        applicationId = "com.projectrevivesolutions.layerscope"
        minSdk = 26          // Android 8.0. Covers the TCL K24 and most Android-watch OSes.
        targetSdk = 36
        versionCode = 89
        versionName = "0.1.88" // build 89: Link 0.2 evidence (slice 2) - pull a Wi-Fi detection's frames from the Hunter (GET_FRAMES/GET_FRAME_BYTES, MTU 247), import them, run the triage; protected-network flood now LIKELY ATTACK with a note
        // build 88: analytic pipeline slice 1 - workflow wiring files (deauth triage shipped), engine, seven checks + verdict, frame fabricator fixtures, read-only Builder viewer, TRIAGE on event detail
        // build 87: MISSION wired to the Hunter (SetSensors allow-list), presets check their sensors, sensors grouped, EAPOL listed; OPS "Select Sensors" (All Sensors / Only Deauth) is a shortcut into MISSION
        // build 86: watch to cool steel, teal ground, toward the OPS comp; six destinations (OPS, EVENTS, TOOLS, BUILDER, MISSION, SYSTEM)
        // build 83: first release-signed build (PRS upload key)
        // build 82: GET_TEXT through the link FIFO so source MACs stop starving under an event flood; target bound at send time; empty replies logged
        // build 79: Wireshark window in TOOLS - import, display filter, packet list, protocol tree and hex
        // build 78: OPS thinned to face + connection + detection; panels moved to SYSTEM, measure to TOOLS
        // build 77: adaptive shell - five destinations, bottom bar (compact) / left rail (wide), face rolls to a header off OPS
        // build 76: phone face 220->190, landscape console inset to clear the edge handle
        // build 75: phone layout - fixed watch-size face pinned upper-left (portrait) / far-left (landscape), data console extended to fill the rest
        // build 74: detections grouped one row per MAC address, one alert per MAC; LayerTime-Sensors 1.1.2 AirTag grading
        // build 73: target API 36 (Play Store requirement, min 2026-08-31) on AGP 8.11 / Gradle 8.13; edge-to-edge handled with safeDrawing insets

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"

        // One ABI first: the TCL K24 is arm64-v8a. Add more after phone results.
        ndk { abiFilters += "arm64-v8a" }
    }

    flavorDimensions += "distribution"
    productFlavors {
        // full: the sideload and watch build. Keeps the accessibility service (the watch hardware button).
        create("full") { dimension = "distribution" }
        // play: the Google Play build. src/play/AndroidManifest.xml removes the accessibility service so the
        // app is not subject to Google Play accessibility review. Same applicationId and signing key as full,
        // so the two are update-compatible.
        create("play") { dimension = "distribution" }
    }

    signingConfigs {
        create("release") {
            if (keystorePropsFile.exists()) {
                storeFile = file(keystoreProps.getProperty("storeFile"))
                storePassword = keystoreProps.getProperty("storePassword")
                keyAlias = keystoreProps.getProperty("keyAlias")
                keyPassword = keystoreProps.getProperty("keyPassword")
            }
        }
    }

    buildTypes {
        debug {
            // Diagnostic APK: no minification so logs and class names stay readable.
            isMinifyEnabled = false
            applicationIdSuffix = ".diag"
            versionNameSuffix = "-diag"
        }
        release {
            isMinifyEnabled = false
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
            if (keystorePropsFile.exists()) signingConfig = signingConfigs.getByName("release")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions {
        jvmTarget = "17"
    }
    buildFeatures {
        compose = true
        buildConfig = true
    }
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
    packaging {
        resources.excludes += "/META-INF/{AL2.0,LGPL2.1}"
        // Store native libs uncompressed and 16 KB zip-aligned (AGP 8.5.1+), so Android maps them straight
        // from the APK instead of unpacking a second copy. Google's preferred 16 KB setup:
        // https://developer.android.com/guide/practices/page-sizes
        jniLibs.useLegacyPackaging = false
    }
}

dependencies {
    val composeBom = platform("androidx.compose:compose-bom:2024.09.03")
    implementation(composeBom)

    implementation("androidx.core:core-ktx:1.13.1")
    implementation("androidx.activity:activity-compose:1.9.2")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.8.6")
    implementation("androidx.lifecycle:lifecycle-viewmodel-compose:2.8.6")
    implementation("androidx.lifecycle:lifecycle-service:2.8.6")

    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.ui:ui-graphics")
    implementation("androidx.compose.ui:ui-tooling-preview")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.compose.material:material-icons-extended")

    debugImplementation("androidx.compose.ui:ui-tooling")

    testImplementation("junit:junit:4.13.2")
}
