package com.projectrevivesolutions.layertime.controller.flow

import java.io.File
import java.io.FileOutputStream
import java.nio.file.Files
import java.nio.file.StandardCopyOption

/** Replace only after the complete UTF-8 document is written and synced.
 * If atomic replacement is unsupported, fail with the previous document intact.
 */
internal class WorkflowFileWriter(
    private val replace: (File, File) -> Unit = { from, to ->
        Files.move(from.toPath(), to.toPath(), StandardCopyOption.ATOMIC_MOVE,
            StandardCopyOption.REPLACE_EXISTING)
    },
) {
    fun write(target: File, text: String) {
        val temporary = File.createTempFile("workflow-", ".part", target.absoluteFile.parentFile)
        try {
            FileOutputStream(temporary).use { stream ->
                stream.write(text.toByteArray(Charsets.UTF_8))
                stream.fd.sync()
            }
            replace(temporary, target)
        } finally {
            temporary.delete()
        }
    }
}
