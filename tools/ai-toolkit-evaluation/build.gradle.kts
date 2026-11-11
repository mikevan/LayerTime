plugins { kotlin("jvm") version "2.3.10" }
kotlin { jvmToolchain(17) }
dependencies {
    implementation("ai.koog:agents-core:1.3.0")
    testImplementation("junit:junit:4.13.2")
}
tasks.test { testLogging { events("passed", "failed", "skipped") } }
