package com.projectrevivesolutions.layertime.controller

import com.projectrevivesolutions.layertime.controller.ai.*
import com.projectrevivesolutions.layertime.controller.flow.*
import kotlinx.coroutines.*
import org.junit.Assert.*
import org.junit.Test

class AiToolkitTest {
    private val ap = "02:00:00:00:00:01"
    private val target = "02:00:00:00:00:02"
    private fun frames(): List<AnalyticFrame> = (1..500).map { i ->
        AnalyticFrame(i, i * 1_000_000_000L, when(i) {128 -> AnalyticFrame.DEAUTH;129,130 -> AnalyticFrame.DATA;else -> AnalyticFrame.BEACON},
            ap, if(i == 130) "02:00:00:00:00:03" else target, ap, eapol = i == 129 || i == 130)
    }
    private fun evidence() = CaptureEvidence.index("capture-test", frames())
    private fun proposal(workflow: Workflow = BuilderPractice.workflow()) = FlowJson.write(linkedMapOf(
        "schemaVersion" to 1L, "workflow" to FlowJson.parse(WorkflowLoader.write(workflow))))
    private fun decoded(json: String = proposal()) = AnalyticAuthoring.decode(json, "Find repeated disconnects followed by network access messages.", "test-model", "1")
    private fun manifest() = LocalModelManifest("test-model", "1", "a".repeat(64), 1,
        setOf(AiTask.CREATE_ANALYTIC, AiTask.REVIEW_CAPTURE), setOf(ModelCapability.STRUCTURED_OUTPUT), 4096, true)
    private fun provider(block: suspend (LocalModelRequest) -> LocalModelResponse) = object : LocalModelProvider {
        override fun availability() = ModelAvailability(ModelState.READY, manifest())
        override suspend fun countInputTokens(request: LocalModelRequest) = 128
        override suspend fun generate(request: LocalModelRequest) = block(request)
    }
    private fun issue(ids: List<Any>) = FlowJson.write(mapOf("schemaVersion" to 1L, "issues" to listOf(
        mapOf("headline" to "Review reconnection behavior", "explanation" to "Disconnect followed by EAPOL.", "frameIds" to ids))))

    @Test fun fiveHundredFramesArePagedWithoutDroppingOrDuplicatingAny() {
        val e = evidence()
        assertEquals(listOf(128,128,128,116), e.pages().map { it.frames.size })
        assertEquals((1..500).toList(), e.pages().flatMap { it.frames }.map { it.index })
        assertEquals(500, CaptureReviewer.review(e).reviewedFrames)
    }
    @Test fun crossPageSequenceRetainsRelatedEapolAndExcludesOtherTarget() {
        val e = evidence()
        val report = CaptureReviewer.review(e)
        assertEquals(listOf(128,129), report.issues.single().frameIds)
        assertEquals(ReviewLevel.SIGNIFICANT, report.issues.single().level)
        val payload = FlowJson.parse(AiEvidenceWire.page(e, e.pages().first())) as Map<*, *>
        val related = (payload["relatedEpisodes"] as List<*>).single() as Map<*, *>
        assertEquals(listOf(129L), related["eapolFrames"])
    }
    @Test fun malformedOrUntimedDisconnectsDoNotEstablishSequence() {
        val fs = frames().map { if(it.index == 128) it.copy(timestampValid=false) else it }
        val e = CaptureEvidence.index("partial", fs)
        assertTrue(CaptureReviewer.review(e).issues.isEmpty())
        assertTrue(e.coverage.limitations.any { it.contains("no valid timestamp") })
    }
    @Test fun emptyRecordingDoesNotClaimSafety() {
        assertEquals("There are no frames to review.", CaptureReviewer.review(CaptureEvidence.index("empty", emptyList())).headline)
    }
    @Test fun duplicateFrameIdsAreRejected() {
        assertThrows(IllegalArgumentException::class.java) { CaptureEvidence.index("bad", frames()+frames().first()) }
    }
    @Test fun nativeZeroBasedReferencesArePreserved() {
        val frames=BuilderPractice.fixture().built.mapIndexed {i,f->f.copy(index=i)}
        val review=CaptureReviewer.review(CaptureEvidence.index("native",frames))
        assertEquals(listOf(0,1,2),review.issues.first().frameIds)
    }
    @Test fun mismatchedCoverageIsRejected() {
        assertThrows(IllegalArgumentException::class.java) { CaptureEvidence.index("one", frames(), EvidenceCoverage("two",500,500)) }
    }
    @Test fun partialCoverageRemainsVisible() {
        val review = CaptureReviewer.review(CaptureEvidence.index("limited", frames(), EvidenceCoverage("limited",600,500,cancelled=true)))
        assertTrue(review.coverage.limitations.any { it.contains("500 of 600") })
        assertTrue(review.coverage.cancelled)
    }
    @Test fun aiReferencesMayCrossPageOnlyThroughSuppliedEpisodes() {
        val e = evidence()
        assertEquals(listOf(128,129), AiReviewTasks.decode(issue(listOf(128L,129L)),e,e.pages().first()).single().frameIds)
        assertThrows(IllegalArgumentException::class.java) { AiReviewTasks.decode(issue(listOf(500L)),e,e.pages().first()) }
        assertThrows(IllegalArgumentException::class.java) { AiReviewTasks.decode(issue(listOf(128.5)),e,e.pages().first()) }
    }
    @Test fun modelCannotReturnEvidenceFreeWarning() {
        val e = evidence()
        assertThrows(IllegalArgumentException::class.java) { AiReviewTasks.decode(issue(emptyList()),e,e.pages().first()) }
    }
    @Test fun evidencePageCannotBeReusedAcrossRecordingIdentities() {
        val original=evidence()
        val other=CaptureEvidence.index("another-capture",frames())
        assertThrows(IllegalArgumentException::class.java) {AiEvidenceWire.page(original,other.pages().first())}
        val forged=original.pages().first().copy(frames=listOf(frames().first().copy(index=999)))
        assertThrows(IllegalArgumentException::class.java) {AiReviewTasks.decode(issue(listOf(999L)),original,forged)}
    }
    @Test fun modelProposalBecomesRunnableEditableDraft() {
        val d = decoded()
        assertTrue(d.editable)
        assertTrue(WorkflowValidator.validate(d.workflow).isEmpty())
        DeauthSteps.register()
        val fs = BuilderPractice.fixture().built
        val finding = Engine.run(d.workflow,RunContext(DetectionEvent("DEAUTH",ap,null,null,fs.first().timeNanos),FrameSource {fs})).finding
        assertEquals(BuilderPractice.HEADING,finding?.assessmentLabel)
    }
    @Test fun authoredPromptAndModelSurviveSaveExchangeAndCopy() {
        val d = decoded()
        val text = DraftEdits.write(d)
        val loaded = WorkflowExchange.read(text.byteInputStream())
        assertEquals(d.authoring,loaded.authoring)
        assertEquals(d.authoring,DraftEdits.copyOf(loaded).authoring)
    }
    @Test fun unknownAtomCannotEnterBuilder() {
        val w = BuilderPractice.workflow().let { it.copy(steps=it.steps.map {s->if(s.type=="target_behavior")s.copy(type="invented_tool") else s}) }
        assertThrows(IllegalArgumentException::class.java) { decoded(proposal(w)) }
    }
    @Test fun fractionalVersionCannotBeTruncatedToValidVersion() {
        assertThrows(IllegalArgumentException::class.java) { decoded(proposal().replace("\"version\":1", "\"version\":1.5")) }
    }
    @Test fun captureSpecificMacsCannotAccidentallyBecomeSharedDefaults() {
        val w=BuilderPractice.workflow().let {it.copy(steps=it.steps.map {s->if(s.type=="target_behavior")s.copy(settings=mapOf("target_mac" to target))else s})}
        assertThrows(IllegalArgumentException::class.java) {decoded(proposal(w))}
    }
    @Test fun disconnectedExtraBlocksCannotEnterReusableAnalytic() {
        val w=BuilderPractice.workflow().let {it.copy(steps=it.steps+Step("orphan","deauth_rate"),
            edges=it.edges+Edge("capture_source","orphan","frames",DataType.FRAMES))}
        assertThrows(IllegalArgumentException::class.java) {decoded(proposal(w))}
    }
    @Test fun promptIsSeparateFromSystemInstructions() {
        val r=AnalyticAuthoring.request("Find repeated reconnects.")
        assertEquals(PromptMessage.Role.USER,r.messages.last().role)
        assertEquals("Find repeated reconnects.",r.messages.last().content)
    }
    @Test fun reviewedPatternCanInformAuthoringWithoutEmbeddingCapturedMacs() {
        val review=CaptureReviewer.review(evidence())
        val request=AnalyticAuthoring.request("Make this reusable.",review)
        val context=request.messages.single {it.content.startsWith("REVIEWED OBSERVATIONS")}.content
        assertTrue(context.contains("disconnectCount"))
        assertFalse(context.contains(target))
        assertFalse(context.contains(ap))
        assertEquals("Make this reusable.",request.messages.last().content)
    }
    @Test fun missingProductModelReturnsUnavailableWithoutFakeInference() = runBlocking {
        val result=AiCoordinator(UninstalledLocalModel).execute(AnalyticAuthoring.request("Find disconnects.")) {it}
        assertTrue(result is AiOutcome.Unavailable)
    }
    @Test fun coordinatorRejectsWrongModelIdentity() = runBlocking {
        val p=provider { LocalModelResponse("{}","different-model","1") }
        assertTrue(AiCoordinator(p).execute(AnalyticAuthoring.request("Find disconnects.")) {it} is AiOutcome.Invalid)
    }
    @Test fun coordinatorTimesOutWithoutReturningAnAssessment() = runBlocking {
        val p=provider {delay(1000);LocalModelResponse("{}","test-model","1")}
        assertEquals(AiOutcome.TimedOut,AiCoordinator(p).execute(AnalyticAuthoring.request("Find disconnects."),20) {it})
    }
    @Test fun cancellationPropagatesToModelProvider() = runBlocking {
        var stopped=false
        val p=provider {try {awaitCancellation()} finally {stopped=true}}
        val job=launch {AiCoordinator(p).execute(AnalyticAuthoring.request("Find disconnects.")) {it}}
        yield();job.cancelAndJoin()
        assertTrue(stopped)
    }
    @Test fun modelContextOverflowDoesNotStartGeneration() = runBlocking {
        var generated=false
        val p=object:LocalModelProvider {
            override fun availability()=ModelAvailability(ModelState.READY,manifest())
            override suspend fun countInputTokens(request:LocalModelRequest)=4090
            override suspend fun generate(request:LocalModelRequest):LocalModelResponse {generated=true;error("Should not generate")}
        }
        assertTrue(AiCoordinator(p).execute(AnalyticAuthoring.request("Find disconnects.")) {it} is AiOutcome.Invalid)
        assertFalse(generated)
    }
    @Test fun frameworkCanReceiveOnlyDeclaredToolCallsWithoutDispatchingThem() = runBlocking {
        val p=object:LocalModelProvider {
            override fun availability()=ModelAvailability(ModelState.READY,manifest().copy(capabilities=ModelCapability.entries.toSet()))
            override suspend fun countInputTokens(request:LocalModelRequest)=128
            override suspend fun generate(request:LocalModelRequest)=LocalModelResponse("{}","test-model","1",
                listOf(LocalToolCall("call-1","review_capture","{}")))
        }
        val request=AnalyticAuthoring.request("Find disconnects.").copy(tools=listOf(LocalToolDefinition("review_capture","Read recording patterns","{\"type\":\"object\"}")))
        val result=AiCoordinator(p).turn(request)
        assertEquals("review_capture",(result as AiOutcome.Completed).value.toolCalls.single().name)
        assertTrue(AiCoordinator(p).turn(request.copy(tools=emptyList())) is AiOutcome.Invalid)
    }
}
