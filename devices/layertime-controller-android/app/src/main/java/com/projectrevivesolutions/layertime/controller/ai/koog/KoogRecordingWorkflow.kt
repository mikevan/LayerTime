package com.projectrevivesolutions.layertime.controller.ai.koog

import ai.koog.agents.core.agent.GraphAIAgent
import ai.koog.agents.core.agent.config.AIAgentConfig
import ai.koog.agents.core.dsl.builder.node
import ai.koog.agents.core.dsl.builder.strategy
import ai.koog.prompt.Prompt
import ai.koog.prompt.llm.LLMProvider
import ai.koog.prompt.llm.LLModel
import com.projectrevivesolutions.layertime.controller.ai.*
import kotlinx.coroutines.currentCoroutineContext
import kotlinx.coroutines.ensureActive

data class ToolkitReview(val baseline:RecordingReview,val assessment:AiOutcome<List<AiIssue>>,
                         val reviewedPages:Int,val totalPages:Int)

/** A real embedded Koog graph mixes deterministic atoms and bounded local model tasks. */
class KoogRecordingWorkflow(private val provider:LocalModelProvider,private val pageSize:Int=16) {
    private data class State(val evidence:CaptureEvidence,val baseline:RecordingReview,
                             val assessment:AiOutcome<List<AiIssue>> = AiOutcome.Unavailable("AI review has not run."),
                             val pages:Int=0,val total:Int=0)

    suspend fun review(evidence:CaptureEvidence,prompt:String,
                       onBaseline:suspend (RecordingReview)->Unit = {}):ToolkitReview {
        require(prompt.isNotBlank() && prompt.length<=8000)
        val coordinator=AiCoordinator(provider)
        val graph=strategy<CaptureEvidence,ToolkitReview>("recording-review-v2") {
            val observe by node<CaptureEvidence,State> {input->
                val baseline=CaptureReviewer.review(input)
                onBaseline(baseline)
                State(input,baseline)
            }
            val assess by node<State,State> {state->
                val available=coordinator.availability()
                val manifest=available.manifest
                if(available.state!=ModelState.READY || manifest?.blueTeamTrained!=true ||
                    manifest.tasks.contains(AiTask.REVIEW_GROUNDED_CAPTURE).not()) {
                    state.copy(assessment=AiOutcome.Unavailable(available.reason ?: "No compatible grounded-review model is available."))
                } else {
                    val pages=state.evidence.pages(pageSize)
                    val issues=arrayListOf<AiIssue>()
                    var reviewed=0
                    var failed:AiOutcome<List<AiIssue>>?=null
                    for(page in pages) {
                        currentCoroutineContext().ensureActive()
                        val request = try { GroundedReviewTasks.request(prompt,state.evidence,page) }
                        catch(ex:IllegalArgumentException) {
                            failed=AiOutcome.Invalid("The recording facts could not be validated. The original observations remain available.");break
                        } catch(ex:ArithmeticException) {
                            failed=AiOutcome.Invalid("The recording timing could not be validated. The original observations remain available.");break
                        }
                        when(val result=coordinator.execute(request) {
                            GroundedReviewTasks.decode(it,state.evidence,page)
                        }) {
                            is AiOutcome.Completed->{issues+=result.value;reviewed++}
                            is AiOutcome.Unavailable->{failed=result;break}
                            is AiOutcome.Invalid->{failed=result;break}
                            AiOutcome.TimedOut->{failed=AiOutcome.TimedOut;break}
                        }
                    }
                    val outcome=failed ?: AiOutcome.Completed(issues.distinctBy {it.headline to it.frameIds},manifest.id,manifest.version)
                    state.copy(assessment=outcome,pages=reviewed,total=pages.size)
                }
            }
            val report by node<State,ToolkitReview> {state->ToolkitReview(state.baseline,state.assessment,state.pages,state.total)}
            edge(nodeStart forwardTo observe)
            edge(observe forwardTo assess)
            edge(assess forwardTo report)
            edge(report forwardTo nodeFinish)
        }
        val manifest=provider.availability().manifest
        val executor=KoogLocalPromptExecutor(provider,AiTask.REVIEW_GROUNDED_CAPTURE,"{\"type\":\"object\"}")
        val config=AIAgentConfig(Prompt.build("recording-review") {system("Review local blue-team evidence.")},
            LLModel(LLMProvider("layertime","LayerTime bundled model"),manifest?.id ?: "not-installed"),10)
        val agent=GraphAIAgent(executor,config,graph)
        return try {agent.run(evidence)} finally {agent.close()}
    }
}
