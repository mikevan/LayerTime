package com.projectrevivesolutions.layertime.controller.flow

/** Reproducible offline teaching fixture. It writes a file only; it never transmits. */
object BuilderPractice {
    const val FILE_NAME="TRAINING-builder-correlation.pcap"
    const val AP="02:00:00:00:00:01"
    const val CLIENT="02:00:00:00:00:02"
    const val OTHER="02:00:00:00:00:03"
    const val HEADING="TWO LINKED EPISODES"
    const val FALLBACK="THRESHOLD NOT MET"
    const val SUMMARY="{target_behavior.episodes} disconnect episodes, {target_behavior.linked_episodes} linked episodes, {target_behavior.eapol_frames} related EAPOL frames."
    private const val T0=1700000000000000000L
    private const val SECOND=1000000000L
    fun fixture():FrameFabricator=FrameFabricator().apply {
        deauth(T0+SECOND,AP,CLIENT,AP,3,1,6,-50)
        deauth(T0+2*SECOND,AP,CLIENT,AP,3,2,6,-50)
        eapol(T0+3*SECOND,AP,CLIENT,AP,3,6,-50,msg=1)
        eapol(T0+4*SECOND,AP,OTHER,AP,4,6,-50,msg=1)
        deauth(T0+10*SECOND,AP,CLIENT,AP,3,5,6,-50)
        eapol(T0+11*SECOND,CLIENT,AP,AP,6,6,-50,msg=2)
    }
    fun workflow()=Workflow("Practice - two linked episodes",1,listOf(
        Step("capture_source","capture_source"),Step("rate","deauth_rate"),
        Step("target_behavior","target_behavior"),Step("verdict","deauth_verdict",mapOf("rules" to expectedRules().toJson()))),listOf(
        Edge("capture_source","rate","frames",DataType.FRAMES),Edge("capture_source","target_behavior","frames",DataType.FRAMES),
        Edge("rate","verdict","features",DataType.FEATURE),Edge("target_behavior","verdict","features",DataType.FEATURE)))
    fun expectedRules(threshold:Long=2)=VerdictRuleSet(emptyList(),listOf(
        VerdictRule(Cond.MetricAtLeast("target_behavior","linked_episodes",threshold),Verdict.WATCH,SUMMARY,HEADING),
        VerdictRule(Cond.Always,Verdict.WATCH,"The selected threshold was not met in this practice recording.",FALLBACK)))
}
