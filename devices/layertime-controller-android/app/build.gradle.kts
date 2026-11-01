plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.compose")
}

android {
    namespace = "com.projectrevivesolutions.layertime.controller"
    compileSdk = 34
    // NDK r28+ links 16 KB-aligned by default. https://developer.android.com/guide/practices/page-sizes
    ndkVersion = "30.0.16248370"

    defaultConfig {
        applicationId = "com.projectrevivesolutions.layerscope"
        minSdk = 26          // Android 8.0. Covers the TCL K24 and most Android-watch OSes.
        targetSdk = 34
        versionCode = 72
        versionName = "0.1.71" // build 72: 16 KB page alignment for libltws and all 14 Wireshark libraries (clears the Android 15+ compatibility warning); NDK r30; native libs stored uncompressed

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"

        // One ABI first: the TCL K24 is arm64-v8a. Add more after phone results.
        ndk { abiFilters += "arm64-v8a" }
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
