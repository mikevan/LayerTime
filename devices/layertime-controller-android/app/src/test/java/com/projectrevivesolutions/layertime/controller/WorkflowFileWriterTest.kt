package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.flow.WorkflowFileWriter
import java.io.File
import java.io.IOException
import org.junit.Assert.*
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder

class WorkflowFileWriterTest {
    @get:Rule val directory = TemporaryFolder()

    @Test fun successfulSaveReplacesWholeDocumentAndCleansTemporaryFile() {
        val target = directory.newFile("workflow.json")
        target.writeText("old")
        WorkflowFileWriter().write(target, "{\"name\":\"Café\"}")
        assertEquals("{\"name\":\"Café\"}", target.readText())
        assertEquals(listOf("workflow.json"), directory.root.list()!!.toList())
    }

    @Test fun failedReplacementKeepsPreviousVersionAndReportsFailure() {
        val target = directory.newFile("workflow.json")
        target.writeText("previous accepted workflow")
        val writer = WorkflowFileWriter { _, _ -> throw IOException("disk refused replacement") }
        assertThrows(IOException::class.java) { writer.write(target, "new draft") }
        assertEquals("previous accepted workflow", target.readText())
        assertEquals(listOf("workflow.json"), directory.root.list()!!.toList())
    }

    @Test fun firstSaveFailureDoesNotPublishPartialWorkflow() {
        val target = File(directory.root, "new.json")
        val writer = WorkflowFileWriter { _, _ -> throw IOException("no space") }
        assertThrows(IOException::class.java) { writer.write(target, "draft") }
        assertFalse(target.exists())
        assertTrue(directory.root.list()!!.isEmpty())
    }
}
