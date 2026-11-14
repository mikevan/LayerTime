# Embedded AI workflow evaluation

Portable analysis should be available on equipment people can carry and control. This experiment evaluates a framework choice within that wider purpose; it does not qualify an AI interpretation or demonstrate an end-to-end warning.

This isolated Kotlin/JVM project evaluates Koog 1.3.0 without changing the Android application's Kotlin 2.0.21 toolchain. The published framework uses Kotlin 2.3.10 dependencies.

Run with the repository's existing Gradle wrapper:

```powershell
devices/layertime-controller-android/gradlew.bat -p tools/ai-toolkit-evaluation test --console=plain
```

The test executes an actual Koog graph with local inspection, interpretation-fixture, and authoring-fixture nodes over 500 integers. It checks ordering and the final artifact. Its model executor rejects all calls; the test proves local graph execution only, not model intelligence or mobile compatibility. There is no AI service or server involved.

Next gate: compile and run the same graph on Android, measure dependency size, exercise cancellation and checkpoints, and connect the product-owned local-model contract. Only then choose how Koog will participate in the application. Preserve the existing graph format and model APIs independently of the framework.
