package com.projectrevivesolutions.layertime.controller.flow

/** Tap-to-add helpers. Only wire an unambiguous source/report; never replace a user's connection. */
object GuidedBuilder {
    fun questionAnalysis():Workflow {
        val base=BuilderPractice.workflow()
        val rules=VerdictRuleSet(emptyList(),listOf(
            VerdictRule(Cond.MetricAtLeast("target_behavior","linked_episodes",1),Verdict.WATCH,
                "A device was asked to leave the network. Messages used to set up its network access followed. Read the observations below to see which device and packets were involved.",
                "DISCONNECT REQUESTS FOLLOWED BY SETUP"),
            VerdictRule(Cond.Always,Verdict.WATCH,"The selected checks did not find that sequence in this recording. Read the observations below; missing messages do not establish safety.","NO MATCHING SEQUENCE SEEN")))
        return base.copy(name="Was a device told to disconnect?",steps=base.steps.map {if(it.type=="deauth_verdict")it.copy(settings=mapOf("rules" to rules.toJson())) else it})
    }
    fun newAnalysis():Draft {
        val source=Step("capture_source","capture_source")
        val report=Step("verdict","deauth_verdict",mapOf("rules" to RuleEdits.initial().toJson()))
        return DraftEdits.open(Workflow("My analytic",1,listOf(source,report),emptyList()),null,false)
    }
    fun add(draft:Draft,type:String):Pair<Draft,String>? {
        val own=if(draft.editable)draft else DraftEdits.copyOf(draft)
        val added=DraftEdits.add(own,type,220f,own.workflow.steps.size*110f) ?: return null
        return attach(added.first,added.second) to added.second
    }
    fun attach(draft:Draft,id:String):Draft {
        var d=draft
        val spec=d.workflow.step(id)?.let {StepRegistry.spec(it.type)} ?: return d
        val sources=d.workflow.steps.filter {StepRegistry.spec(it.type)?.output==DataType.FRAMES}
        val reports=d.workflow.steps.filter {it.type=="deauth_verdict"}
        if(spec.inputs.any {it.name=="frames"} && sources.size==1 && d.workflow.edgesInto(id).none {it.toPort=="frames"})
            (DraftEdits.connect(d,sources.single().id,id,"frames") as? ConnectResult.Done)?.let {d=it.draft}
        if(spec.output==DataType.FEATURE && reports.size==1)
            (DraftEdits.connect(d,id,reports.single().id,"features") as? ConnectResult.Done)?.let {d=it.draft}
        return d
    }
}
