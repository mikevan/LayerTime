package com.projectrevivesolutions.layertime.controller.ui

import androidx.compose.foundation.border
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.projectrevivesolutions.layertime.controller.flow.*
import com.projectrevivesolutions.layertime.controller.ui.theme.LtColors

private val ruleText=TextStyle(fontSize=13.sp,color=LtColors.Ivory)

/** Touch authoring is a view of the same portable rules, not a second execution engine. */
@Composable
internal fun ReportRuleEditor(draft:Draft,step:Step,onChange:(Draft)->Unit) {
    val raw=(step.settings["rules"] as? String).orEmpty()
    val checks=draft.workflow.edgesInto(step.id).filter {it.toPort=="features"}
        .mapNotNull {draft.workflow.step(it.from)}.distinctBy {it.id}
    val parsed=remember(raw) {if(raw.isBlank())null else runCatching {VerdictRuleSet.parse(raw)}.getOrNull()}
    var advanced by remember(step.id) {mutableStateOf(false)}
    androidx.activity.compose.BackHandler(enabled=advanced){advanced=false}
    val editable=draft.editable
    fun save(rules:VerdictRuleSet)=onChange(DraftEdits.setSetting(draft,step.id,"rules",rules.toJson()))
    Column(Modifier.fillMaxWidth(),verticalArrangement=Arrangement.spacedBy(8.dp)) {
        Text("REPORT RULES",color=LtColors.Cyan,fontSize=14.sp)
        Text("Rules run from top to bottom. The first matching rule supplies the heading and explanation. Missing evidence remains visible in the report.",style=ruleText)
        if(raw.isBlank()) {
            Text("Using the built-in deauth example. Start your own rules to define a different report.",style=ruleText)
            SciFiOutline("START MY OWN RULES",editable){save(RuleEdits.initial())}
        } else if(parsed==null) {
            Text("These rules need correction. Open Advanced text to fix them, or start a new rule set.",color=LtColors.Amber,fontSize=13.sp)
            SciFiOutline("START NEW RULE SET",editable){save(RuleEdits.initial())}
        } else {
            Text("Checks to count",color=LtColors.Cyan,fontSize=13.sp)
            if(checks.isEmpty())Text("Connect a Wireshark or Timeline block to this report first.",style=ruleText)
            for(check in checks) {
                SciFiOutline((if(check.id in parsed.counted) "[x] " else "[ ] ")+checkTitle(check),editable) {
                    save(RuleEdits.count(parsed,check.id,check.id !in parsed.counted))
                }
            }
            parsed.rules.forEachIndexed { index,rule ->
                key(index) {
                    Column(Modifier.fillMaxWidth().border(1.dp,LtColors.Cyan.copy(alpha=.4f),RoundedCornerShape(8.dp)).padding(10.dp),verticalArrangement=Arrangement.spacedBy(7.dp)) {
                        Text("RULE ${index+1}",color=LtColors.Amber,fontSize=14.sp)
                        ConditionEditor(rule.cond,checks,editable,0) { condition -> save(RuleEdits.replace(parsed,index,rule.copy(cond=condition))) }
                        RuleChoices("Posture preset",Verdict.values().map {it to it.label},rule.verdict,editable) {v ->
                            save(RuleEdits.replace(parsed,index,rule.copy(verdict=v,label=v.label)))
                        }
                        OutlinedTextField(value=rule.label ?: rule.verdict.label,onValueChange={save(RuleEdits.replace(parsed,index,rule.copy(label=it)))},
                            enabled=editable,label={Text("Your report heading")},modifier=Modifier.fillMaxWidth(),textStyle=ruleText)
                        OutlinedTextField(value=rule.summary,onValueChange={save(RuleEdits.replace(parsed,index,rule.copy(summary=it)))},
                            enabled=editable,label={Text("Explain the observations in your own words")},modifier=Modifier.fillMaxWidth(),textStyle=ruleText)
                        Row(Modifier.horizontalScroll(rememberScrollState()),horizontalArrangement=Arrangement.spacedBy(6.dp)) {
                            SciFiOutline("MOVE UP",editable && index>0){save(RuleEdits.move(parsed,index,-1))}
                            SciFiOutline("MOVE DOWN",editable && index<parsed.rules.lastIndex){save(RuleEdits.move(parsed,index,1))}
                            SciFiOutline("REMOVE RULE",editable){save(RuleEdits.remove(parsed,index))}
                        }
                    }
                }
            }
            SciFiOutline("ADD A RULE",editable){save(RuleEdits.add(parsed,RuleEdits.defaultCondition(checks)))}
            Text("Sentence blanks",color=LtColors.Cyan,fontSize=13.sp)
            Text("{suspicious}, {benign}, {unknown}, {counted}, {frames}, {top}, {groups}",style=ruleText)
            for(check in checks)Text("${checkTitle(check)}: {${check.id}}"+
                if(check.type=="target_behavior") "  {${check.id}.linked_episodes}  {${check.id}.max_linked_episodes_per_pair}  {${check.id}.eapol_frames}  {${check.id}.targets}" else "",style=ruleText)
        }
        SciFiOutline(if(advanced)"HIDE ADVANCED TEXT" else "ADVANCED TEXT",true){advanced=!advanced}
        if(advanced)OutlinedTextField(value=raw,onValueChange={onChange(DraftEdits.setSetting(draft,step.id,"rules",it))},
            enabled=editable,label={Text("Portable rule text")},modifier=Modifier.fillMaxWidth(),textStyle=ruleText)
    }
}

private fun leanTitle(lean:Lean)=when(lean) {Lean.BENIGN->"Ordinary-looking";Lean.SUSPICIOUS->"Suspicious";Lean.UNKNOWN->"Not enough evidence"}

private fun checkTitle(step:Step)="${StepRegistry.spec(step.type)?.title ?: step.type} (${step.id})"

@Composable
private fun <T> RuleChoices(title:String,options:List<Pair<T,String>>,selected:T,enabled:Boolean,onChoose:(T)->Unit) {
    Text(title,color=LtColors.Cyan,fontSize=12.sp)
    Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),horizontalArrangement=Arrangement.spacedBy(5.dp)) {
        for((value,label) in options)SciFiOutline((if(value==selected)"[x] " else "")+label,enabled){onChoose(value)}
    }
}

@Composable
private fun WholeNumber(label:String,value:Long,enabled:Boolean,onValue:(Long)->Unit) {
    var text by remember(value){mutableStateOf(value.toString())}
    OutlinedTextField(value=text,onValueChange={next ->text=next;next.toLongOrNull()?.takeIf {it in 0..Int.MAX_VALUE.toLong()}?.let(onValue)},
        enabled=enabled,isError=text.toLongOrNull()?.let {it !in 0..Int.MAX_VALUE.toLong()} ?: true,
        label={Text(label)},singleLine=true,modifier=Modifier.fillMaxWidth(),textStyle=ruleText,keyboardOptions=KeyboardOptions(keyboardType=KeyboardType.Number))
    Text("Applied threshold: $value",color=LtColors.Grey,fontSize=12.sp)
}

@Composable
private fun ConditionEditor(condition:Cond,checks:List<Step>,enabled:Boolean,depth:Int,onChange:(Cond)->Unit) {
    val seed=RuleEdits.defaultCondition(checks)
    if(depth>=6) {Text("This condition is deeply nested. Use Advanced text to edit it.",style=ruleText);return}
    val kind=when(condition) {is Cond.MetricAtLeast->"metric";is Cond.CheckLeans->"check";is Cond.ValueContains->"value";is Cond.CountLeans->"count";is Cond.GroupAtLeast->"frames";is Cond.All->"all";is Cond.Any->"any";is Cond.Not->"not";Cond.Always->"always"}
    RuleChoices("When",listOf("check" to "Check result","metric" to "Timeline count","value" to "Check text","count" to "Count checks","frames" to "Count matching frames","all" to "All","any" to "Any","not" to "Not","always" to "Always"),kind,enabled) {pick ->
        if(pick==kind) return@RuleChoices
        val first=checks.firstOrNull()?.id ?: "check"
        val timeline=checks.firstOrNull {it.type=="target_behavior"}?.id ?: "timeline"
        onChange(when(pick) {
            "check"->Cond.CheckLeans(first,Lean.SUSPICIOUS)
            "metric"->Cond.MetricAtLeast(timeline,"linked_episodes",1)
            "value"->Cond.ValueContains(first,"")
            "count"->Cond.CountLeans(Lean.SUSPICIOUS,1)
            "frames"->Cond.GroupAtLeast(1)
            "all"->if(condition is Cond.Any) Cond.All(condition.of) else Cond.All(listOf(if(condition==Cond.Always)seed else condition))
            "any"->if(condition is Cond.All) Cond.Any(condition.of) else Cond.Any(listOf(if(condition==Cond.Always)seed else condition))
            "not"->Cond.Not(if(condition==Cond.Always)seed else condition)
            else->Cond.Always
        })
    }
    when(condition) {
        is Cond.CheckLeans -> {
            RuleChoices("Check",checks.map {it.id to checkTitle(it)},condition.step,enabled){onChange(condition.copy(step=it))}
            RuleChoices("Result",Lean.values().map {it to leanTitle(it)},condition.lean,enabled){onChange(condition.copy(lean=it))}
        }
        is Cond.MetricAtLeast -> {
            RuleChoices("Timeline",checks.filter {it.type=="target_behavior"}.map {it.id to checkTitle(it)},condition.step,enabled){onChange(condition.copy(step=it))}
            RuleChoices("Count",listOf("linked_episodes" to "Linked episodes across recording","max_linked_episodes_per_pair" to "Most linked episodes for one device/AP pair","episodes" to "Disconnect episodes","eapol_frames" to "EAPOL frames","disconnect_frames" to "Disconnect frames","targets" to "Clients"),condition.metric,enabled){onChange(condition.copy(metric=it))}
            WholeNumber("At least",condition.atLeast,enabled){onChange(condition.copy(atLeast=it))}
        }
        is Cond.ValueContains -> {
            RuleChoices("Check",checks.map {it.id to checkTitle(it)},condition.step,enabled){onChange(condition.copy(step=it))}
            OutlinedTextField(value=condition.text,onValueChange={onChange(condition.copy(text=it))},enabled=enabled,label={Text("Contains this text")},modifier=Modifier.fillMaxWidth(),textStyle=ruleText)
        }
        is Cond.CountLeans -> {
            RuleChoices("Count checks whose result is",Lean.values().map {it to leanTitle(it)},condition.lean,enabled){onChange(condition.copy(lean=it))}
            WholeNumber("At least this many selected checks",condition.atLeast.toLong(),enabled){onChange(condition.copy(atLeast=it.toInt()))}
        }
        is Cond.GroupAtLeast -> {
            Text("Requires frames wired into this report. Groups use packet type, meaning, source, destination and network; they do not count distinct attacks.",style=ruleText)
            WholeNumber("At least this many frames in one group",condition.atLeast.toLong(),enabled){onChange(condition.copy(atLeast=it.toInt()))}
        }
        is Cond.Not -> Column(Modifier.padding(start=12.dp),verticalArrangement=Arrangement.spacedBy(5.dp)) {
            Text("This condition must not match. Missing evidence remains unknown.",style=ruleText)
            ConditionEditor(condition.of,checks,enabled,depth+1){onChange(Cond.Not(it))}
        }
        is Cond.All, is Cond.Any -> {
            val children=if(condition is Cond.All)condition.of else (condition as Cond.Any).of
            fun replace(list:List<Cond>)=onChange(if(condition is Cond.All)Cond.All(list) else Cond.Any(list))
            children.forEachIndexed { index,child ->
                Column(Modifier.padding(start=12.dp).border(1.dp,LtColors.Cyan.copy(alpha=.2f)).padding(8.dp),verticalArrangement=Arrangement.spacedBy(5.dp)) {
                    ConditionEditor(child,checks,enabled,depth+1){next ->replace(children.mapIndexed {i,c->if(i==index)next else c})}
                    SciFiOutline("REMOVE CONDITION",enabled && children.size>1){replace(children.filterIndexed {i,_->i!=index})}
                }
            }
            SciFiOutline("ADD CONDITION",enabled){replace(children+seed)}
        }
        Cond.Always -> Text("Matches whenever execution reaches this rule. Put this fallback last.",style=ruleText)
    }
}
