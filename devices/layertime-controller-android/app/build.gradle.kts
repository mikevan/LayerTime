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
        // Build 111: Android-compatible summary placeholders, on-device practice regression.
        // Build 110: visual report rules and a reproducible, labeled Builder practice capture.
        versionCode = 122
        buildConfigField("boolean", "LT_ARM_DOTPROD_EXPERIMENT", "false")
        versionName = "0.1.121" // build 109: reverted all of my build 102-108 Bluetooth connect changes. The connect and the scan-time card probe are back to the build 100 code that connected cleanly; removing the probe and then piling on retries and autoConnect was the whole problem (autoConnect leaked persistent background connects that wedged the phone stack). HUNTER ACTIONS and the link behave as they did at build 100. The Builder work (Wireshark check menu, rule-based Verdict, folded Detection) is unchanged and kept. // build 108: HUNTER ACTIONS - connect primes like the old card probe. The first cold GATT connection to a freshly advertising Hunter often does not bind on this phone (Android sits in W4_CONN_ST); the probe used to absorb that. The connect now makes a direct connectGatt (autoConnect off) and, if it does not report connected in 6 s, fully closes and tries again up to four times, each with a matching close so no background GATT client is ever left registered. This undoes the autoConnect in builds 104 to 106 that leaked persistent background connects (a dozen zombie client_if camped on the Hunter) and the build 107 over-correction to a single connect with no priming. // build 107: HUNTER ACTIONS - one clean connect, no retry churn. Each connectGatt registers an Android Bluetooth GATT client, and the build 104 to 106 retry loop leaked them until the stack could not bind any connection (the 2026-11-06 both-sides capture showed a dozen stale client_if, 44 through 128, all chasing the Hunter while the radio link formed every time). The connect is now a single direct connectGatt from a fresh adapter device by address; on a stall it fully closes and the normal reconnect rescans, so only one GATT client is ever open at a time. // build 106: HUNTER ACTIONS - the connect builds a fresh device from the Bluetooth adapter by address (adapter.getRemoteDevice), not the scan result's device object, and connects directly with autoConnect off. The scan-bound device object was why Android kept bringing the radio link up but never reporting the GATT connection; the old card probe, which connected by address this way, always got through. It still settles 400 ms after the scan and retries up to four times, closing and reopening. // build 105: HUNTER ACTIONS - the connect gets past the Android GATT no-callback race. It settles 400 ms after the scan stops before connecting, the first try is a quick direct connect, and tries 2 to 4 use autoConnect (slower but it connects when the stack next sees the Hunter), each with a longer window. The 2026-11-06 both-sides capture showed four direct tries all getting no GATT callback while the Hunter stayed advertising with zero connects. // build 104: HUNTER ACTIONS - Connect is live the moment a Hunter is listed (it no longer waits for the scan to finish), and tapping it ends the scan. Connect also retries the GATT connection itself: if Android brings the radio link up but never reports the GATT connection to the app (the 2026-11-06 capture showed the LE link up in 62 ms with no GATT callback after, because the removed scan-time probe used to warm the stack), it closes and reopens the connection up to four times before giving up. // build 103: HUNTER ACTIONS - Connect survives a cold service discovery. On a first connection Android's service discovery can be slow or never answer; the connect now settles briefly, gives discovery its own longer window, and on a miss clears the service cache and tries again on the same connection up to four times, instead of dropping the link and rescanning (which also reset the Hunter's advertising). The link health timeout starts after the handshake, so a slow discovery no longer trips a false link-lost. Needed once build 102 removed the scan-time probe that had been warming Android's service cache. // build 102: HUNTER ACTIONS - a scan only lists the Hunters it hears, right away, and never connects during a scan. The SD-card label and Clear Card come from the Hunter you connect to, read from its live link, which is the one connection that reads the card reliably; they appear once it is connected, not during the scan. The old scan-time card probe stalled on Android service discovery (the 2026-11-06 capture showed it timing out twice while the real connect read it in 0.6 s) and is gone. // build 101: Builder - every check is a Wireshark check you pick from one tile. Drop the Wireshark tile, long-press it, and hold a check to choose it (a popup confirms); the tile then shows that check's name. Detection now gathers the frames itself, so the separate Frames step is gone. The Verdict runs rules you write: each rule sets a verdict and a sentence when its test is met, read top to bottom, first match wins; a test can count checks by how they lean, count identical frames (same type, meaning, source, and destination), or combine those with all, any, and not; the sentence fills blanks like {suspicious}, {counted}, and {top}. The shipped deauth triage is those rules written out, and it still calls all seven test scenarios the same way. // build 100: one time on upgrade, the four trackers are unchecked in a saved Mission list (the 2026-11-06 capture showed SetSensors 0x5FFE0, trackers on). build 99: every step of bringing the link up has a 4 s deadline (connect 10 s) and a missed one reconnects naming the step; late callbacks from dropped connections are ignored; the Scan card check retries once; the held Hunter always has its row in HUNTER ACTIONS. Found in the 2026-11-06 capture: service discovery and a notification setup that never answered. build 98: diagnostics: the app log and every Scan card check also go to logcat (tags LayerScope, LayerScope.Scan). build 97: Clear Card erases the Hunter's SD card completely, then lists the card again and says what it holds; nothing on the phone is touched. build 96: trackers (AirTag, Tile, SmartTag, Google Tag) off by default and under Quick Start All Sensors, after a field test where their flood hid every other sensor. build 95: Builder editing (Copy, New, drag steps in from the Tools Palette, move, connect right dot to left dot, Disconnect, Remove, settings, Save with versions, Check against the 7 deauth scenarios, the list of accepted flows with Use for triage); palette and placed tiles are the icon with the name below; ATTACK TIMELINE greyed out. build 94: Builder Tools Palette holds the Tools page tools with their icons (a tap opens the tool); a tool step (Frames behind it, Wireshark) shows the tool icon; workspace text sized in dp (it drew in raw pixels, a third of its size on a dense phone) and larger. build 93: HUNTER ACTIONS replaces CONNECTION (Scan, a row per Hunter with Connect, Disconnect, and Clear Card for Hunters with an SD card); Swipe away disconnects moved to SYSTEM, LOKMAT watch only; OPS Clear also empties the phone's list; PULL FROM THE HUNTER only when the Hunter reports an SD card; Hunters named from the chip in base 32, Hunter-C5-BSV72; hunter_NNNN.log card names. build 92: PULL FROM THE HUNTER (the SD card over the Link, CARD_LIST/CARD_READ, CRC-checked) on HUNTER CARD; Hunters named Hunter-C5-001. build 91: IMPORT HUNTER CARD on EVENTS (the Hunter SD log with repeat and evidence lines, saved frames open in Wireshark). build 90: MISSION evidence hold (SetEvidenceHold, Off/5/10/20/30 s; the Hunter re-holds on a sustained deauth flood); Wireshark window explains every frame type above its card. build 89: Link 0.2 evidence (slice 2) - pull a Wi-Fi detection's frames from the Hunter (GET_FRAMES/GET_FRAME_BYTES, MTU 247), import them, run the triage; protected-network flood now LIKELY ATTACK with a note
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

        testInstrumentationRunner = "com.projectrevivesolutions.layertime.controller.BuilderSmokeInstrumentation"

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
            // Explicit experimental target, verified on the Flip6; release stays portable.
            buildConfigField("boolean", "LT_ARM_DOTPROD_EXPERIMENT", "true")
            externalNativeBuild { cmake { arguments += "-DGGML_CPU_ARM_ARCH=armv8.2-a+dotprod+fp16" } }
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
    buildFeatures {
        compose = true
        buildConfig = true
    }
    // Stream the bundled experimental model without compressing a multi-GB asset.
    androidResources { noCompress += "gguf" }
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

kotlin { compilerOptions { jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17) } }

// Explicit offline audit files must invalidate test caching when their contents change.
tasks.withType<org.gradle.api.tasks.testing.Test>().configureEach {
    listOf("LAYERTIME_INTENT_EXAMPLES", "LAYERTIME_INTENT_RESPONSES", "LAYERTIME_QUANTITY_EXAMPLES", "LAYERTIME_QUANTITY_RESPONSES").forEach { key ->
        val auditPath = providers.environmentVariable(key).orNull
        inputs.property(key, auditPath ?: "")
        if (!auditPath.isNullOrBlank()) inputs.file(auditPath)
    }
    listOf("LAYERTIME_INTENT_TEMPLATE", "LAYERTIME_INTENT_PRACTICE_EXPORT", "LAYERTIME_QUANTITY_TEMPLATE").forEach { key ->
        val exportPath = providers.environmentVariable(key).orNull
        inputs.property(key, exportPath ?: "")
        if (!exportPath.isNullOrBlank()) outputs.upToDateWhen { false }
    }
}

dependencies {
    // Embedded orchestration only. Local inference uses our own provider, not a cloud client.
    implementation("ai.koog:agents-core:1.3.0")
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
