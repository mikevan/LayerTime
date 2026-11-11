import ai.koog.agents.core.agent.GraphAIAgent
import ai.koog.agents.core.agent.config.AIAgentConfig
import ai.koog.agents.core.dsl.builder.strategy
import ai.koog.agents.core.dsl.builder.node
import ai.koog.agents.core.tools.ToolDescriptor
import ai.koog.prompt.Prompt
import ai.koog.prompt.dsl.ModerationResult
import ai.koog.prompt.executor.model.PromptExecutor
import ai.koog.prompt.llm.LLMProvider
import ai.koog.prompt.llm.LLModel
import ai.koog.prompt.message.Message
import ai.koog.prompt.streaming.StreamFrame
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.runBlocking
import org.junit.Assert.*
import org.junit.Test

/** Framework test only. There are no weights and no claims about model quality in this fixture. */
class KoogOfflineGraphTest {
    private val offlineExecutor = object : PromptExecutor() {
        override suspend fun execute(prompt: Prompt, model: LLModel, tools: List<ToolDescriptor>): Message.Assistant =
            error("This orchestration test must not invoke an external model.")
        override fun executeStreaming(prompt: Prompt, model: LLModel, tools: List<ToolDescriptor>): Flow<StreamFrame> =
            error("No streaming service exists in this test.")
        override suspend fun moderate(prompt: Prompt, model: LLModel): ModerationResult = error("No external service.")
        override fun close() {}
    }

    @Test fun graphRunsLocalAtomsAndProducesRepeatableRecipe() = runBlocking {
        val calls = arrayListOf<String>()
        val graph = strategy<List<Int>, String>("capture-to-analytic") {
            val inspect by node<List<Int>, List<Int>> { frames ->
                calls += "inspect:${frames.size}"
                frames.filter { it == 128 || it == 129 }
            }
            val interpret by node<List<Int>, String> { ids ->
                calls += "interpret"
                "evidence=" + ids.joinToString(",")
            }
            val author by node<String, String> { evidence ->
                calls += "author"
                "{\"analytic\":\"local-test\",\"$evidence\":true}"
            }
            edge(nodeStart forwardTo inspect)
            edge(inspect forwardTo interpret)
            edge(interpret forwardTo author)
            edge(author forwardTo nodeFinish)
        }
        val config = AIAgentConfig(
            prompt = Prompt.build("offline-test") { system("Use local recording tools only.") },
            model = LLModel(LLMProvider("layertime", "LayerTime bundled model"), "test-no-weights"),
            maxAgentIterations = 10,
        )
        val agent = GraphAIAgent(offlineExecutor, config, graph)
        try {
            val result = agent.run((1..500).toList())
            assertTrue(result.contains("evidence=128,129"))
            assertEquals(listOf("inspect:500", "interpret", "author"), calls)
        } finally { agent.close() }
    }
}
