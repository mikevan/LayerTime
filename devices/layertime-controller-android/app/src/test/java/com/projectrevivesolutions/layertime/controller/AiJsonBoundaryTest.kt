package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.flow.FlowJson
import com.projectrevivesolutions.layertime.controller.flow.FlowJsonError
import org.junit.Assert.*
import org.junit.Test

class AiJsonBoundaryTest {
    @Test fun deeplyNestedModelOutputIsRejectedWithoutStackOverflow() {
        assertThrows(FlowJsonError::class.java) {FlowJson.parse("[".repeat(10000)+"0"+"]".repeat(10000))}
    }
    @Test fun duplicateAssessmentFieldsAreRejected() {
        assertThrows(FlowJsonError::class.java) {FlowJson.parse("{\"schemaVersion\":1,\"schemaVersion\":2}")}
    }
    @Test fun invalidJsonNumbersAreRejected() {
        for(number in listOf("01","+1","1.","1e999","9223372036854775808"))
            assertThrows(FlowJsonError::class.java) {FlowJson.parse(number)}
    }
    @Test fun unescapedControlCharacterIsRejected() {
        assertThrows(FlowJsonError::class.java) {FlowJson.parse("\"raw\nline\"")}
    }
}
