package com.projectrevivesolutions.layertime.controller

import java.io.File
import org.junit.Assert.*
import org.junit.Test

/** Enforced package boundary while the connection is incrementally extracted.
 * This checks qualified references too, so an inline reference cannot bypass imports.
 */
class ArchitectureBoundaryTest {
    private fun sources(): File {
        val candidates = listOf(File("src/main/java"), File("app/src/main/java"))
        return candidates.firstOrNull { it.isDirectory }
            ?: error("Cannot locate application sources for architecture check")
    }

    @Test fun connectionAndMissionDoNotDependOnPresentationOrBuilder() {
        val root = File(sources(), "com/projectrevivesolutions/layertime/controller")
        for (area in listOf("link", "mission")) {
            for (file in File(root, area).walkTopDown().filter { it.extension == "kt" }) {
                val source = file.readText()
                for (forbidden in listOf("controller.ui.", "controller.flow.", "androidx.compose.")) {
                    assertFalse("${file.name} crosses boundary: $forbidden", source.contains(forbidden))
                }
            }
        }
    }
}
