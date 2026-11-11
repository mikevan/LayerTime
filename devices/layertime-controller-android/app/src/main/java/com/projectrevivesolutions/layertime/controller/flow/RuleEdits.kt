package com.projectrevivesolutions.layertime.controller.flow

/** Immutable rule edits shared by touch controls and tests. Rule order is operator-controlled. */
object RuleEdits {
    fun initial()=VerdictRuleSet(emptyList(),listOf(VerdictRule(Cond.Always,Verdict.WATCH,
        "Review the available observations and their supporting evidence.","REVIEW POSTURE")))
    fun defaultCondition(checks:List<Step>):Cond = checks.firstOrNull { it.type=="target_behavior" }?.let {
        Cond.MetricAtLeast(it.id,"linked_episodes",1)
    } ?: checks.firstOrNull()?.let {Cond.CheckLeans(it.id,Lean.SUSPICIOUS)} ?: Cond.CountLeans(Lean.SUSPICIOUS,1)
    fun add(set:VerdictRuleSet,condition:Cond):VerdictRuleSet {
        val position=set.rules.indexOfFirst { it.cond==Cond.Always }.let {if(it<0)set.rules.size else it}
        return set.copy(rules=set.rules.toMutableList().apply {add(position,VerdictRule(condition,Verdict.WATCH,
            "Describe what these observations mean.","REVIEW POSTURE"))})
    }
    fun replace(set:VerdictRuleSet,index:Int,rule:VerdictRule)=set.copy(rules=set.rules.mapIndexed { i,r -> if(i==index)rule else r })
    fun remove(set:VerdictRuleSet,index:Int)=set.copy(rules=set.rules.filterIndexed {i,_->i!=index})
    fun move(set:VerdictRuleSet,index:Int,delta:Int):VerdictRuleSet {
        val target=index+delta
        if(index !in set.rules.indices || target !in set.rules.indices)return set
        val list=set.rules.toMutableList();val rule=list.removeAt(index);list.add(target,rule)
        return set.copy(rules=list)
    }
    fun count(set:VerdictRuleSet,step:String,enabled:Boolean)=set.copy(counted=
        if(enabled)(set.counted+step).distinct() else set.counted.filter {it!=step})
}
