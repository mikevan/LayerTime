package com.projectrevivesolutions.layertime.controller.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.gestures.calculateCentroid
import androidx.compose.foundation.gestures.calculatePan
import androidx.compose.foundation.gestures.calculateZoom
import androidx.compose.foundation.gestures.detectDragGesturesAfterLongPress
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Add
import androidx.compose.material.icons.outlined.CenterFocusStrong
import androidx.compose.material.icons.outlined.FitScreen
import androidx.compose.material.icons.outlined.Layers
import androidx.compose.material.icons.outlined.Remove
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.translate
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.graphics.vector.rememberVectorPainter
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.input.pointer.positionChange
import androidx.compose.ui.layout.boundsInRoot
import androidx.compose.ui.layout.onGloballyPositioned
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.ui.layout.positionInRoot
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.IntSize
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.projectrevivesolutions.layertime.controller.flow.ConnectResult
import com.projectrevivesolutions.layertime.controller.flow.DataType
import com.projectrevivesolutions.layertime.controller.flow.Draft
import com.projectrevivesolutions.layertime.controller.flow.DraftEdits
import com.projectrevivesolutions.layertime.controller.flow.Feature
import com.projectrevivesolutions.layertime.controller.flow.Finding
import com.projectrevivesolutions.layertime.controller.flow.FlowLayout
import com.projectrevivesolutions.layertime.controller.flow.ScenarioCheck
import com.projectrevivesolutions.layertime.controller.flow.StepRegistry
import com.projectrevivesolutions.layertime.controller.flow.StepSpec
import com.projectrevivesolutions.layertime.controller.flow.TriageSession
import com.projectrevivesolutions.layertime.controller.flow.WorkflowStore
import kotlin.math.ceil
import kotlin.math.floor
import kotlin.math.roundToInt

private val BuilderInk=Color(0xFFE6EFF4)
private val BuilderCyan=Color(0xFF71E9FA)
private val BuilderMuted=Color(0xFF9BB1BF)
private val BuilderAmber=Color(0xFFFFD044)
private val BuilderGreen=Color(0xFF91E833)
private val BuilderType=TextStyle(fontFamily=FontFamily.SansSerif,fontSize=13.sp,lineHeight=17.sp)

private fun wireColor(t:DataType)=when(t) {DataType.EVENT->Color(0xFF9BB1BF);DataType.FRAMES->BuilderCyan;DataType.FEATURE->BuilderAmber;DataType.FINDING->BuilderGreen}

private enum class Grab {PAN,MOVE,WIRE,PINCH}

/**
 * The Builder (2026-11-04 viewer; editing 2026-11-06). Its first goal, in Michael's words:
 * be able to recreate the deauth process in the Builder.
 *
 * - The workspace shows one flow as tiles and wires. A tile is the step's icon with its
 *   name below; a step that is a Tools page tool uses that tool's icon, a step that only
 *   passes field values uses the block shape. A placed tile looks the same as in the palette.
 * - Shipped flows are read-only. Copy makes your own; New starts empty. On your own flow:
 *   hold a palette tile and drag it in; drag a tile to move it; drag from a tile's right
 *   dot to another tile's left dot to connect them. Pinch to zoom, drag empty ground to pan.
 * - Save keeps a new version. Check runs the flow on the seven deauth test scenarios.
 * - Below, the list of flows: accepted ones can be opened or used for TRIAGE.
 */
@Composable
fun BuilderScreen(onWireshark:()->Unit={},onTimeline:()->Unit={}) {
    val context=LocalContext.current
    val density=LocalDensity.current
    val dpPx=with(density) {1.dp.toPx()}
    val worldWidth=with(density) {10000.dp.toPx()}
    val worldHeight=with(density) {6000.dp.toPx()}
    // The flow's own coordinates (canvas dp) start at the middle of the world.
    val originX=worldWidth/2f; val originY=worldHeight/2f
    var zoom by remember {mutableFloatStateOf(1f)}
    var panX by remember {mutableFloatStateOf(0f)}
    var panY by remember {mutableFloatStateOf(0f)}
    var initialized by remember {mutableStateOf(false)}
    var viewportSize by remember {mutableStateOf(IntSize.Zero)}
    var selected by remember {mutableStateOf<String?>(null)}
    val loaded by TriageSession.workflow.collectAsState()
    val lastRun by TriageSession.run.collectAsState()
    var draft by remember {mutableStateOf<Draft?>(null)}
    LaunchedEffect(loaded) { if(draft==null) loaded?.let {draft=DraftEdits.open(it,null,shipped=true)} }
    var flows by remember {mutableStateOf(WorkflowStore.list(context))}
    var message by remember {mutableStateOf<Pair<String,Boolean>?>(null)} // text, is a problem
    var check by remember {mutableStateOf<ScenarioCheck?>(null)}
    // Dragging a palette tile onto the workspace.
    var paletteDrag by remember {mutableStateOf<String?>(null)}
    var dragRoot by remember {mutableStateOf(Offset.Zero)}
    var canvasRect by remember {mutableStateOf(Rect.Zero)}
    var screenOrigin by remember {mutableStateOf(Offset.Zero)}
    // A connection being drawn: from this step, to the finger.
    var wireFrom by remember {mutableStateOf<String?>(null)}
    var wireEnd by remember {mutableStateOf(Offset.Zero)}
    // The Wireshark check menu: the id of the tile it is choosing for, and the check awaiting Yes/No.
    var menuFor by remember {mutableStateOf<String?>(null)}
    var confirmType by remember {mutableStateOf<String?>(null)}
    var suppressTap by remember {mutableStateOf(false)}
    val textMeasurer=rememberTextMeasurer()
    val timelinePainter=rememberVectorPainter(Icons.Outlined.Layers)
    val vw=viewportSize.width.toFloat(); val vh=viewportSize.height.toFloat()

    fun current()=BuilderViewport(zoom,panX,panY)
    fun update(next:BuilderViewport) {zoom=next.zoom;panX=next.x;panY=next.y}
    fun toWorld(p:Offset)=Offset(((p.x-panX)/zoom-originX)/dpPx,((p.y-panY)/zoom-originY)/dpPx)
    fun toScreen(x:Float,y:Float)=Offset(panX+(originX+x*dpPx)*zoom,panY+(originY+y*dpPx)*zoom)
    fun nodeAt(d:Draft,p:Offset):String? {
        val w=toWorld(p)
        return d.workflow.steps.lastOrNull {s->d.at(s.id)?.let {(x,y)->w.x in x..x+FlowLayout.NODE_W && w.y in y..y+FlowLayout.NODE_H}==true}?.id
    }
    fun inputPorts(d:Draft,id:String):List<Pair<String,Offset>> {
        val spec=d.workflow.step(id)?.let {StepRegistry.spec(it.type)} ?: return emptyList()
        val (x,y)=d.at(id) ?: return emptyList()
        val n=spec.inputs.size
        return spec.inputs.mapIndexed {i,p-> p.name to toScreen(x,y+FlowLayout.PORT_Y+(i-(n-1)/2f)*18f)}
    }
    fun outputPort(d:Draft,id:String):Offset? {
        val spec=d.workflow.step(id)?.let {StepRegistry.spec(it.type)} ?: return null
        if(spec.output==null) return null
        val (x,y)=d.at(id) ?: return null
        return toScreen(x+FlowLayout.NODE_W,y+FlowLayout.PORT_Y)
    }
    val portReach=with(density) {22.dp.toPx()}
    fun outputAt(d:Draft,p:Offset)=d.workflow.steps.firstOrNull {s->outputPort(d,s.id)?.let {(it-p).getDistance()<=portReach}==true}?.id
    fun inputAt(d:Draft,p:Offset):Pair<String,String>? {
        for(s in d.workflow.steps) for((port,at) in inputPorts(d,s.id)) if((at-p).getDistance()<=portReach) return s.id to port
        // Dropped on a tile rather than its dot: its first input.
        val id=nodeAt(d,p) ?: return null
        return StepRegistry.spec(d.workflow.step(id)!!.type)?.inputs?.firstOrNull()?.let {id to it.name}
    }
    fun fit() {
        val d=draft; if(vw<=0f) return
        if(d==null || d.positions.isEmpty()) {update(BuilderViewport(1f,vw/2f-originX,vh/3f-originY).bounded(vw,vh,worldWidth,worldHeight));return}
        val xs=d.positions.values.map {it.first}; val ys=d.positions.values.map {it.second}
        val minX=xs.min(); val minY=ys.min(); val maxX=xs.max()+FlowLayout.NODE_W; val maxY=ys.max()+FlowLayout.NODE_H
        val dw=(maxX-minX)*dpPx; val dh=(maxY-minY)*dpPx
        val z=(minOf(vw/dw,vh/dh)*.85f).coerceIn(BuilderViewport.MIN_ZOOM,1.6f)
        val cx=originX+(minX+maxX)/2f*dpPx; val cy=originY+(minY+maxY)/2f*dpPx
        update(BuilderViewport(z,vw/2f-cx*z,vh/2f-cy*z).bounded(vw,vh,worldWidth,worldHeight))
    }
    fun say(text:String,problem:Boolean=false) {message=text to problem}
    fun dropFromPalette(type:String,at:Offset) {
        if(!canvasRect.contains(at)) return
        val d=draft ?: DraftEdits.blank()
        if(!d.editable) {say("This flow is read-only. Tap Copy to edit your own copy, or New to start empty.",true);return}
        val w=toWorld(at-canvasRect.topLeft)
        val added=DraftEdits.add(d,type,w.x-FlowLayout.NODE_W/2f,w.y-FlowLayout.PORT_Y) ?: return
        draft=added.first; selected=added.second; check=null
        say("Added ${StepRegistry.spec(type)?.title}. Drag from its right dot to connect it.")
    }

    Box(Modifier.fillMaxSize().onGloballyPositioned {screenOrigin=it.positionInRoot()}) {
    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()),verticalArrangement=Arrangement.spacedBy(4.dp)) {
    OpsTelemetryHousing(Modifier.fillMaxWidth().height(560.dp)) {
        Column(Modifier.fillMaxSize().padding(start=18.dp,end=18.dp,top=20.dp,bottom=20.dp),
            verticalArrangement=Arrangement.spacedBy(8.dp)) {
            Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically,horizontalArrangement=Arrangement.spacedBy(4.dp)) {
                Text("WORKSPACE",color=BuilderInk,style=BuilderType.copy(letterSpacing=.5.sp),modifier=Modifier.weight(1f))
                BuilderControl("Zoom out",Icons.Outlined.Remove,zoom>BuilderViewport.MIN_ZOOM) {
                    if(vw>0f)update(current().transform(1f/1.25f,vw/2f,vh/2f,0f,0f,vw,vh,worldWidth,worldHeight))}
                Text("${(zoom*100).roundToInt()}%",color=BuilderCyan,style=BuilderType.copy(fontSize=12.sp,lineHeight=15.sp),modifier=Modifier.widthIn(min=36.dp))
                BuilderControl("Zoom in",Icons.Outlined.Add,zoom<BuilderViewport.MAX_ZOOM) {
                    if(vw>0f)update(current().transform(1.25f,vw/2f,vh/2f,0f,0f,vw,vh,worldWidth,worldHeight))}
                BuilderControl("Fit the workflow",Icons.Outlined.FitScreen) {fit()}
                BuilderControl("Reset view to 100 percent",Icons.Outlined.CenterFocusStrong) {
                    if(vw>0f)update(current().transform(1f/zoom,vw/2f,vh/2f,0f,0f,vw,vh,worldWidth,worldHeight))}
            }
            Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),horizontalArrangement=Arrangement.spacedBy(6.dp)) {
                val d=draft
                BuilderTextButton("Copy",d!=null && d.shipped) {
                    draft=DraftEdits.copyOf(d!!); check=null; say("This is your own copy. Drag steps in from the Tools Palette below, or rewire what is here.")}
                BuilderTextButton("New",true) {draft=DraftEdits.blank(); selected=null; check=null
                    say("An empty flow. Hold a step in the Tools Palette and drag it here.")}
                BuilderTextButton("Save",d!=null && d.editable && d.workflow.steps.isNotEmpty()) {
                    val saved=WorkflowStore.save(context,d!!); draft=saved; flows=WorkflowStore.list(context)
                    say("Saved ${saved.workflow.name}, version ${saved.workflow.version}.")}
                BuilderTextButton("Check",d!=null && d.workflow.steps.isNotEmpty()) {check=DraftEdits.checkScenarios(d!!.workflow)}
            }
            val glass=RoundedCornerShape(5.dp)
            Canvas(Modifier.fillMaxWidth().weight(1f).background(Color(0xFF020D16),glass)
                .border(.6.dp,BuilderCyan.copy(alpha=.45f),glass).clip(glass)
                .onGloballyPositioned {canvasRect=it.boundsInRoot()}
                .onSizeChanged {next->
                    viewportSize=next
                    if(!initialized && next.width>0) {initialized=true}
                }
                .pointerInput(viewportSize, draft?.editable) {
                    detectTapGestures(onLongPress = { p ->
                        val d0 = draft
                        if (d0 != null && d0.editable) {
                            val id = nodeAt(d0, p)
                            val type = id?.let { d0.workflow.step(it)?.type }
                            if (id != null && (type == StepRegistry.WIRESHARK_PLACEHOLDER || type in StepRegistry.WIRESHARK_CHECKS)) {
                                selected = id; menuFor = id; suppressTap = true
                            }
                        }
                    })
                }
                .pointerInput(viewportSize) {
                    awaitEachGesture {
                        val down=awaitFirstDown(requireUnconsumed=false)
                        val start=down.position
                        val d0=draft
                        val fromPort=d0?.takeIf {it.editable}?.let {outputAt(it,start)}
                        val onNode=if(fromPort==null) d0?.let {nodeAt(it,start)} else null
                        var mode=when {fromPort!=null->Grab.WIRE; onNode!=null && d0!!.editable->Grab.MOVE; else->Grab.PAN}
                        val grab=if(mode==Grab.MOVE) d0!!.at(onNode!!)!!.let {(x,y)->toWorld(start).let {Offset(it.x-x,it.y-y)}} else Offset.Zero
                        var moved=false
                        if(mode==Grab.WIRE) {wireFrom=fromPort; wireEnd=start}
                        while(true) {
                            val ev=awaitPointerEvent()
                            val pressed=ev.changes.filter {it.pressed}
                            if(pressed.isEmpty()) break
                            if(pressed.size>=2) {
                                if(mode!=Grab.PINCH) {mode=Grab.PINCH; wireFrom=null}
                                moved=true
                                val c=ev.calculateCentroid()
                                if(vw>0f)update(current().transform(ev.calculateZoom(),c.x,c.y,ev.calculatePan().x,ev.calculatePan().y,vw,vh,worldWidth,worldHeight))
                            } else {
                                val ch=pressed.first()
                                if((ch.position-start).getDistance()>viewConfiguration.touchSlop) moved=true
                                if(moved) when(mode) {
                                    Grab.MOVE -> { val w=toWorld(ch.position); draft=DraftEdits.move(draft!!,onNode!!,w.x-grab.x,w.y-grab.y) }
                                    Grab.WIRE -> wireEnd=ch.position
                                    Grab.PAN -> { val dlt=ch.positionChange()
                                        if(vw>0f)update(current().transform(1f,0f,0f,dlt.x,dlt.y,vw,vh,worldWidth,worldHeight)) }
                                    Grab.PINCH -> {}
                                }
                            }
                            ev.changes.forEach {if(it.positionChange()!=Offset.Zero) it.consume()}
                        }
                        val d1=draft
                        if(mode==Grab.WIRE && moved && d1!=null) {
                            val from=wireFrom; wireFrom=null
                            val target=inputAt(d1,wireEnd)
                            if(from!=null && target!=null) {
                                when(val r=DraftEdits.connect(d1,from,target.first,target.second)) {
                                    is ConnectResult.Done -> {draft=r.draft; check=null
                                        say("Connected ${StepRegistry.spec(d1.workflow.step(from)!!.type)?.title} to ${StepRegistry.spec(d1.workflow.step(target.first)!!.type)?.title}.")}
                                    is ConnectResult.Refused -> say(r.sentence,true)
                                }
                            }
                        } else {
                            wireFrom=null
                            if(!moved) { if(suppressTap) suppressTap=false else selected=d1?.let {nodeAt(it,start)} }
                        }
                    }
                }) {
                if(!initialized) return@Canvas
                val origin=Offset(panX,panY)
                val board=Size(worldWidth*zoom,worldHeight*zoom)
                drawRect(Brush.verticalGradient(listOf(Color(0xFF071A27),Color(0xFF030F1A))),origin,board)
                var step=24.dp.toPx()
                while(step*zoom<12.dp.toPx())step*=2
                val pitch=step*zoom
                val firstX=ceil(-panX/pitch).toInt().coerceAtLeast(0)
                val lastX=minOf(floor((size.width-panX)/pitch).toInt(),floor(worldWidth/step).toInt())
                val firstY=ceil(-panY/pitch).toInt().coerceAtLeast(0)
                val lastY=minOf(floor((size.height-panY)/pitch).toInt(),floor(worldHeight/step).toInt())
                for(i in firstX..lastX)drawLine(BuilderCyan.copy(alpha=if(i%4==0).16f else .055f),
                    Offset(panX+i*pitch,maxOf(0f,panY)),Offset(panX+i*pitch,minOf(size.height,panY+board.height)),.5.dp.toPx())
                for(i in firstY..lastY)drawLine(BuilderCyan.copy(alpha=if(i%4==0).16f else .055f),
                    Offset(maxOf(0f,panX),panY+i*pitch),Offset(minOf(size.width,panX+board.width),panY+i*pitch),.5.dp.toPx())
                val d=draft ?: return@Canvas
                val w=d.workflow
                val wireStroke=1.6.dp.toPx()*zoom.coerceIn(.5f,1.6f)
                fun curve(a:Offset,b:Offset)=Path().apply {moveTo(a.x,a.y);cubicTo(a.x+(b.x-a.x)*.5f,a.y,a.x+(b.x-a.x)*.5f,b.y,b.x,b.y)}
                for(e in w.edges) {
                    val a=outputPort(d,e.from) ?: continue
                    val b=inputPorts(d,e.toStep).firstOrNull {it.first==e.toPort}?.second ?: continue
                    drawPath(curve(a,b),wireColor(e.carries).copy(alpha=.75f),style=Stroke(wireStroke))
                }
                wireFrom?.let {f-> outputPort(d,f)?.let {a-> drawPath(curve(a,wireEnd),Color.White.copy(alpha=.85f),style=Stroke(wireStroke))}}
                for(s in w.steps) {
                    val (x,y)=d.at(s.id) ?: continue
                    val spec=StepRegistry.spec(s.type)
                    val out=lastRun?.takeIf {it.workflow==w}?.output(s.id)
                    val status=when(out) {is Feature->leanColor(out.lean);is Finding->verdictColor(out.verdict);null->Color(0xFF5E7A88);else->BuilderCyan}
                    val tl=toScreen(x,y); val sz=Size(FlowLayout.NODE_W*dpPx*zoom,FlowLayout.NODE_H*dpPx*zoom)
                    drawTile(this,spec,tl,sz,zoom,status,s.id==selected,timelinePainter)
                    if(zoom>=.3f) {
                        val title=spec?.title ?: s.type
                        val value=when(out) {is Feature->out.value;is Finding->out.verdict.label;else->""}
                        drawText(textMeasurer,title,Offset(tl.x+4.dp.toPx()*zoom,tl.y+50.dp.toPx()*zoom),
                            TextStyle(color=BuilderInk,fontSize=with(density){(12.dp.toPx()*zoom).toSp()},lineHeight=with(density){(14.dp.toPx()*zoom).toSp()},
                                fontFamily=FontFamily.SansSerif,textAlign=TextAlign.Center),
                            maxLines=2,overflow=TextOverflow.Ellipsis,size=Size(sz.width-8.dp.toPx()*zoom,30.dp.toPx()*zoom))
                        if(value.isNotEmpty()) drawText(textMeasurer,value,Offset(tl.x+4.dp.toPx()*zoom,tl.y+79.dp.toPx()*zoom),
                            TextStyle(color=status,fontSize=with(density){(10.dp.toPx()*zoom).toSp()},fontFamily=FontFamily.SansSerif,textAlign=TextAlign.Center),
                            maxLines=1,overflow=TextOverflow.Ellipsis,size=Size(sz.width-8.dp.toPx()*zoom,15.dp.toPx()*zoom))
                    }
                    val dot=4.5.dp.toPx()*zoom.coerceIn(.6f,1.6f)
                    for((p,at) in inputPorts(d,s.id)) {
                        val t=spec?.input(p)?.type ?: continue
                        drawCircle(Color(0xFF020D16),dot*1.5f,at); drawCircle(wireColor(t),dot,at)
                    }
                    outputPort(d,s.id)?.let {at-> spec?.output?.let {t-> drawCircle(Color(0xFF020D16),dot*1.5f,at); drawCircle(wireColor(t),dot,at)}}
                }
            }
            LaunchedEffect(initialized,draft==null) { if(initialized) fit() }
            val d=draft
            Text(when {
                d==null -> "No flow is open. Tap New, or open one from the list below."
                d.shipped -> "${d.workflow.name}, version ${d.workflow.version}, shipped and read-only. Tap Copy to edit your own copy."
                else -> "${d.workflow.name}, version ${d.workflow.version}, your own. Hold a step in the Tools Palette and drag it here; drag from a right dot to a left dot to connect."
            },color=BuilderMuted,style=BuilderType.copy(fontSize=12.sp,lineHeight=15.sp))
            message?.let {(t,bad)-> Text(t,color=if(bad)BuilderAmber else BuilderCyan,style=BuilderType.copy(fontSize=12.sp,lineHeight=15.sp))}
        }
    }
    // What is wrong with the open flow, and how it did on the test scenarios.
    draft?.takeIf {it.workflow.steps.isNotEmpty()}?.let {d->
        val problems=remember(d.workflow) {DraftEdits.sentences(d.workflow)}
        OpsConnectionHousing(Modifier.fillMaxWidth()) {
            Column(Modifier.fillMaxWidth().padding(start=18.dp,end=18.dp,top=14.dp,bottom=16.dp),verticalArrangement=Arrangement.spacedBy(4.dp)) {
                Text(if(problems.isEmpty())"ACCEPTED" else "NOT ACCEPTED YET",color=if(problems.isEmpty())BuilderGreen else BuilderAmber,
                    style=BuilderType.copy(fontSize=12.sp,lineHeight=15.sp,letterSpacing=.6.sp))
                if(problems.isEmpty()) Text("The wiring holds together, so this flow can run.",color=BuilderInk,style=BuilderType)
                problems.take(6).forEach {Text(it,color=BuilderInk,style=BuilderType)}
                if(problems.size>6) Text("${problems.size-6} more problems are not shown.",color=BuilderMuted,style=BuilderType)
                check?.let {Text(it.sentence,color=if(it.failures.isEmpty())BuilderGreen else BuilderAmber,style=BuilderType)}
            }
        }
    }
    selected?.let {id-> draft?.let {d-> d.workflow.step(id)?.let {step->
        BuilderStepDetail(d,step,lastRun?.takeIf {it.workflow==d.workflow}?.output(id),
            onChange={draft=it; check=null},onRemove={draft=DraftEdits.remove(d,id); selected=null; check=null},onClose={selected=null},
            onChooseCheck={menuFor=it})
    }}}
    BuilderToolsPalette(
        onPick={spec-> say("${spec.title}: ${spec.description} Hold it and drag it onto the workspace.")},
        onUnavailable={say("ATTACK TIMELINE is not a workflow step yet, so it cannot be placed. Open it from the Tools page.",true)},
        onDragStart={type,at-> paletteDrag=type; dragRoot=at},
        onDrag={delta-> dragRoot+=delta},
        onDragEnd={ paletteDrag?.let {dropFromPalette(it,dragRoot)}; paletteDrag=null },
        onDragCancel={paletteDrag=null},
    )
    BuilderFlowList(flows,draft,
        onOpen={e-> WorkflowStore.open(context,e)?.let {draft=it; selected=null; check=null; fit()
            say("Opened ${e.name}, version ${e.version}.")} ?: say("${e.name} could not be opened.",true)},
        onUse={e-> val w=WorkflowStore.open(context,e)?.workflow
            if(w!=null && TriageSession.use(w)) say("TRIAGE now runs ${e.name}, version ${e.version}.") else say("${e.name} is not accepted, so TRIAGE cannot use it.",true)},
        onRefresh={flows=WorkflowStore.list(context)})
    Spacer(Modifier.height(16.dp))
    }
    // The Wireshark check menu and its confirm popup, above everything.
    menuFor?.let {id->
        WiresharkCheckMenu(
            current=draft?.workflow?.step(id)?.type,
            onHold={type-> confirmType=type},
            onDismiss={menuFor=null; confirmType=null})
    }
    confirmType?.let {type-> val id=menuFor
        if(id!=null) WiresharkConfirm(type,
            onYes={ draft=draft?.let {DraftEdits.assign(it,id,type)}; check=null; confirmType=null; menuFor=null
                say("This Wireshark check is now ${StepRegistry.spec(type)?.title}.") },
            onNo={confirmType=null})
    }
    // The tile under the finger while it is dragged from the palette.
    paletteDrag?.let {type-> StepRegistry.spec(type)?.let {spec->
        val half=with(density) {48.dp.toPx()}
        Box(Modifier.offset {IntOffset((dragRoot.x-screenOrigin.x-half).roundToInt(),(dragRoot.y-screenOrigin.y-half).roundToInt())}.alpha(.85f)) {
            PaletteTile(spec,enabled=true,highlight=true)
        }
    }}
    }
}

/**
 * One tile, as the workspace draws it: the step's icon on top (the Tools page icon for a
 * tool step, the block shape for a step that passes field values), the name below.
 */
private fun drawTile(ds:DrawScope,spec:StepSpec?,tl:Offset,sz:Size,zoom:Float,status:Color,selected:Boolean,
                     timeline:androidx.compose.ui.graphics.painter.Painter) = with(ds) {
    val r=CornerRadius(6.dp.toPx()*zoom)
    drawRoundRect(Color(0xFF0B2433).copy(alpha=.55f),tl,sz,r)
    drawRoundRect(if(selected)Color.White else status.copy(alpha=.7f),tl,sz,r,style=Stroke((if(selected)2f else .8f).dp.toPx()*zoom.coerceIn(.5f,2f)))
    val icon=36.dp.toPx()*zoom
    val at=Offset(tl.x+(sz.width-icon)/2f,tl.y+6.dp.toPx()*zoom)
    when(LayerTool.of(spec?.tool)) {
        LayerTool.WIRESHARK -> drawWiresharkFin(at,icon,BuilderCyan,1.4.dp.toPx()*zoom.coerceIn(.5f,2f))
        LayerTool.ATTACK_TIMELINE -> translate(at.x,at.y) {with(timeline) {draw(Size(icon,icon),colorFilter=androidx.compose.ui.graphics.ColorFilter.tint(BuilderCyan))}}
        null -> drawBlockShape(at,icon,status,zoom)
    }
}

/** The block shape (the Builder's original step block, small): a panel with its status lamp. */
private fun DrawScope.drawBlockShape(at:Offset,side:Float,status:Color,zoom:Float) {
    val h=side*.66f; val top=Offset(at.x,at.y+(side-h)/2f)
    val r=CornerRadius(4.dp.toPx()*zoom)
    drawRoundRect(Brush.verticalGradient(listOf(Color(0xFF123447),Color(0xFF071C2A)),top.y,top.y+h),top,Size(side,h),r)
    drawRoundRect(status,top,Size(side,h),r,style=Stroke(.9.dp.toPx()*zoom.coerceIn(.5f,2f)))
    drawCircle(status,side*.08f,Offset(top.x+side*.17f,top.y+h*.3f))
    drawLine(BuilderMuted.copy(alpha=.7f),Offset(top.x+side*.32f,top.y+h*.3f),Offset(top.x+side*.82f,top.y+h*.3f),side*.05f)
    drawLine(BuilderMuted.copy(alpha=.45f),Offset(top.x+side*.17f,top.y+h*.66f),Offset(top.x+side*.7f,top.y+h*.66f),side*.05f)
}

/** A palette tile: the icon, and below it the name in smaller text. Placed tiles look the same. */
@Composable
private fun PaletteTile(spec:StepSpec?,enabled:Boolean,highlight:Boolean=false,tool:LayerTool?=null,title:String?=null) {
    val shape=RoundedCornerShape(6.dp)
    val tint=if(enabled)BuilderCyan else BuilderMuted
    Column(Modifier.width(96.dp).background(Brush.verticalGradient(listOf(Color(0xFF123447),Color(0xFF071C2A))),shape)
        .border(if(highlight)1.4.dp else .8.dp,if(highlight)Color.White else tint.copy(alpha=if(enabled).6f else .3f),shape)
        .padding(horizontal=6.dp,vertical=8.dp).alpha(if(enabled)1f else .4f),
        horizontalAlignment=Alignment.CenterHorizontally,verticalArrangement=Arrangement.spacedBy(5.dp)) {
        val t=tool ?: LayerTool.of(spec?.tool)
        if(t!=null) ToolIcon(t,36.dp,tint)
        else Canvas(Modifier.size(36.dp)) {drawBlockShape(Offset.Zero,size.minDimension,BuilderCyan,1f)}
        Text(title ?: spec?.title ?: "",color=if(enabled)BuilderInk else BuilderMuted,
            style=BuilderType.copy(fontSize=11.sp,lineHeight=13.sp,textAlign=TextAlign.Center),maxLines=2,overflow=TextOverflow.Ellipsis)
    }
}

/**
 * The Tools Palette: every step the deauth triage is made of, each as its icon with the name
 * below. Tap one to read what it does; hold it and drag it onto the workspace to place it.
 * ATTACK TIMELINE is shown greyed out: it is on the Tools page but is not a workflow step yet.
 */
@Composable
private fun BuilderToolsPalette(onPick:(StepSpec)->Unit,onUnavailable:()->Unit,
                                onDragStart:(String,Offset)->Unit,onDrag:(Offset)->Unit,onDragEnd:()->Unit,onDragCancel:()->Unit) {
    OpsConnectionHousing(Modifier.fillMaxWidth()) {
        Column(Modifier.fillMaxWidth().padding(start=18.dp,end=18.dp,top=17.dp,bottom=18.dp),verticalArrangement=Arrangement.spacedBy(7.dp)) {
            Text("TOOLS PALETTE",color=BuilderInk,style=BuilderType.copy(fontSize=12.sp,lineHeight=15.sp,letterSpacing=.6.sp))
            Text("Tap a step to read what it does. Hold it and drag it onto the workspace to place it.",
                color=BuilderMuted,style=BuilderType.copy(fontSize=12.sp,lineHeight=15.sp))
            val shape=RoundedCornerShape(5.dp)
            Row(Modifier.fillMaxWidth().background(Brush.verticalGradient(listOf(Color(0xFF092331),Color(0xFF030F19))),shape)
                .border(.6.dp,BuilderCyan.copy(alpha=.3f),shape).clip(shape).horizontalScroll(rememberScrollState()).padding(6.dp),
                horizontalArrangement=Arrangement.spacedBy(8.dp)) {
                for(type in StepRegistry.PALETTE) {
                    val spec=StepRegistry.spec(type)!!
                    var origin by remember {mutableStateOf(Offset.Zero)}
                    Box(Modifier.onGloballyPositioned {origin=it.positionInRoot()}
                        .pointerInput(spec.type) {
                            detectDragGesturesAfterLongPress(
                                onDragStart={off-> onDragStart(spec.type,origin+off)},
                                onDrag={ch,amount-> ch.consume(); onDrag(amount)},
                                onDragEnd={onDragEnd()},onDragCancel={onDragCancel()})
                        }
                        .clickable(role=Role.Button,onClickLabel="What ${spec.title} does") {onPick(spec)}) {
                        PaletteTile(spec,enabled=true)
                    }
                }
                Box(Modifier.clickable(role=Role.Button,onClickLabel="Attack timeline is not available") {onUnavailable()}) {
                    PaletteTile(null,enabled=false,tool=LayerTool.ATTACK_TIMELINE,title=LayerTool.ATTACK_TIMELINE.title)
                }
            }
        }
    }
}

/** The flows LayerScope knows: accepted ones first, then the ones that are not accepted yet. */
@Composable
private fun BuilderFlowList(flows:List<WorkflowStore.Entry>,open:Draft?,onOpen:(WorkflowStore.Entry)->Unit,
                            onUse:(WorkflowStore.Entry)->Unit,onRefresh:()->Unit) {
    OpsConnectionHousing(Modifier.fillMaxWidth()) {
        Column(Modifier.fillMaxWidth().padding(start=18.dp,end=18.dp,top=16.dp,bottom=18.dp),verticalArrangement=Arrangement.spacedBy(6.dp)) {
            Row(verticalAlignment=Alignment.CenterVertically) {
                Text("ACCEPTED FLOWS",color=BuilderInk,style=BuilderType.copy(fontSize=12.sp,lineHeight=15.sp,letterSpacing=.6.sp),modifier=Modifier.weight(1f))
                BuilderTextButton("Refresh",true) {onRefresh()}
            }
            val accepted=flows.filter {it.accepted}
            if(accepted.isEmpty()) Text("No flow is accepted yet.",color=BuilderMuted,style=BuilderType)
            accepted.forEach {e-> BuilderFlowRow(e,open,onOpen,onUse)}
            val pending=flows.filter {!it.accepted}
            if(pending.isNotEmpty()) {
                Text("NOT ACCEPTED YET",color=BuilderAmber,style=BuilderType.copy(fontSize=12.sp,lineHeight=15.sp,letterSpacing=.6.sp))
                pending.forEach {e-> BuilderFlowRow(e,open,onOpen,onUse)}
            }
        }
    }
}

@Composable
private fun BuilderFlowRow(e:WorkflowStore.Entry,open:Draft?,onOpen:(WorkflowStore.Entry)->Unit,onUse:(WorkflowStore.Entry)->Unit) {
    val shape=RoundedCornerShape(5.dp)
    val isOpen=open!=null && open.workflow.name==e.name && open.shipped==e.shipped
    Column(Modifier.fillMaxWidth().background(Color(0xFF0B2433).copy(alpha=.6f),shape)
        .border(.6.dp,(if(isOpen)Color.White else BuilderCyan).copy(alpha=.4f),shape).padding(10.dp),verticalArrangement=Arrangement.spacedBy(4.dp)) {
        Text(e.name,color=BuilderInk,style=BuilderType.copy(fontSize=14.sp,lineHeight=18.sp))
        Text("Version ${e.version}, ${if(e.shipped)"shipped and read-only" else "your own"}.",color=BuilderMuted,style=BuilderType.copy(fontSize=12.sp,lineHeight=15.sp))
        if(!e.accepted) Text(e.problems.first(),color=BuilderAmber,style=BuilderType.copy(fontSize=12.sp,lineHeight=15.sp))
        Row(horizontalArrangement=Arrangement.spacedBy(6.dp)) {
            BuilderTextButton("Open",true) {onOpen(e)}
            if(e.accepted) BuilderTextButton("Use for triage",true) {onUse(e)}
        }
    }
}

@Composable
private fun BuilderTextButton(label:String,enabled:Boolean,onClick:()->Unit) {
    val flash=rememberPressFlash()
    val shape=RoundedCornerShape(5.dp)
    Box(Modifier.heightIn(min=32.dp).background(Color(0xFF0E2939),shape)
        .border(.7.dp,BuilderCyan.copy(alpha=if(enabled).6f else .15f),shape).clip(shape)
        .clickable(enabled=enabled,role=Role.Button) {flash.fire();onClick()}
        .pressFlash(flash,false,BuilderCyan).padding(horizontal=12.dp,vertical=6.dp),contentAlignment=Alignment.Center) {
        Text(label,color=if(enabled)BuilderInk else BuilderMuted.copy(alpha=.4f),style=BuilderType.copy(fontSize=13.sp,lineHeight=16.sp))
    }
}

@Composable
private fun BuilderControl(label:String,icon:ImageVector,enabled:Boolean=true,onClick:()->Unit) {
    val flash=rememberPressFlash()
    val shape=RoundedCornerShape(5.dp)
    Box(Modifier.size(32.dp).background(Color(0xFF0E2939),shape)
        .border(.6.dp,BuilderCyan.copy(alpha=if(enabled).55f else .15f),shape).clip(shape)
        .clickable(enabled=enabled,role=Role.Button,onClickLabel=label) {flash.fire();onClick()}
        .pressFlash(flash,false,BuilderCyan),contentAlignment=Alignment.Center) {
        Icon(icon,contentDescription=label,tint=if(enabled)BuilderInk else BuilderMuted.copy(alpha=.35f),modifier=Modifier.size(18.dp))
    }
}

/** A tapped tile: what it does, what feeds it, its settings, and on your own flow Disconnect and Remove. */
@Composable
private fun BuilderStepDetail(d:Draft,step:com.projectrevivesolutions.layertime.controller.flow.Step,output:Any?,
                              onChange:(Draft)->Unit,onRemove:()->Unit,onClose:()->Unit,onChooseCheck:(String)->Unit) {
    val spec=StepRegistry.spec(step.type)
    OpsConnectionHousing(Modifier.fillMaxWidth()) {
        Column(Modifier.fillMaxWidth().padding(start=18.dp,end=18.dp,top=16.dp,bottom=18.dp),verticalArrangement=Arrangement.spacedBy(6.dp)) {
            Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically) {
                Text((spec?.title ?: step.type).uppercase(),color=BuilderInk,style=BuilderType.copy(fontSize=13.sp,lineHeight=16.sp,letterSpacing=.6.sp),modifier=Modifier.weight(1f))
                BuilderTextButton("Close",true) {onClose()}
            }
            if(spec==null) { Text("This app has no step type \"${step.type}\".",color=Color(0xFFFF5A4E),style=BuilderType); return@Column }
            Text(spec.description,color=BuilderInk,style=BuilderType)
            if(d.editable && (step.type==StepRegistry.WIRESHARK_PLACEHOLDER || step.type in StepRegistry.WIRESHARK_CHECKS))
                BuilderTextButton(if(step.type==StepRegistry.WIRESHARK_PLACEHOLDER) "Choose which check this is" else "Change which check this is",true){onChooseCheck(step.id)}
            for(p in spec.inputs) {
                val feeds=d.workflow.edgesInto(step.id).filter {it.toPort==p.name}
                if(feeds.isEmpty()) Text("Nothing feeds ${p.name} yet. It takes ${p.type.wire}.",color=BuilderAmber,style=BuilderType.copy(fontSize=12.sp,lineHeight=15.sp))
                for(e in feeds) Row(verticalAlignment=Alignment.CenterVertically,horizontalArrangement=Arrangement.spacedBy(8.dp)) {
                    val fromTitle=d.workflow.step(e.from)?.let {StepRegistry.spec(it.type)?.title} ?: e.from
                    Text("$fromTitle feeds ${p.name}.",color=BuilderMuted,style=BuilderType.copy(fontSize=12.sp,lineHeight=15.sp),modifier=Modifier.weight(1f))
                    if(d.editable) BuilderTextButton("Disconnect",true) {onChange(DraftEdits.disconnect(d,e))}
                }
            }
            Text("Produces: ${spec.output?.wire ?: "nothing"}.",color=BuilderMuted,style=BuilderType.copy(fontSize=12.sp,lineHeight=15.sp))
            for(st in spec.settings) {
                val v=((step.settings[st.key] ?: st.default) as? Number)?.toLong() ?: 0L
                val inc=maxOf(1L,((st.default as? Number)?.toLong() ?: 10L)/10L)
                Text("${st.description} Now $v.",color=BuilderMuted,style=BuilderType.copy(fontSize=12.sp,lineHeight=15.sp))
                if(d.editable) Row(horizontalArrangement=Arrangement.spacedBy(6.dp)) {
                    BuilderTextButton("Less",v-inc>=1) {onChange(DraftEdits.setSetting(d,step.id,st.key,v-inc))}
                    BuilderTextButton("More",true) {onChange(DraftEdits.setSetting(d,step.id,st.key,v+inc))}
                }
            }
            when(output) {
                is Feature -> Text("Last run: ${output.value}. ${output.reason}",color=leanColor(output.lean),style=BuilderType)
                is Finding -> Text("Last run: ${output.verdict.label}. ${output.summary}",color=verdictColor(output.verdict),style=BuilderType)
                else -> {}
            }
            if(d.editable) BuilderTextButton("Remove",true) {onRemove()}
        }
    }
}


/**
 * The Wireshark check menu (Michael, 2026-11-06). Long-press a placed Wireshark tile to open it.
 * Lifting the finger leaves it open. Dragging scrolls the list and does not choose. Holding on a
 * check chooses it, which raises a confirm popup. Tapping outside the box closes it with no choice.
 */
@Composable
private fun WiresharkCheckMenu(current:String?,onHold:(String)->Unit,onDismiss:()->Unit) {
    Box(Modifier.fillMaxSize().background(Color(0xCC01080F)).pointerInput(Unit){detectTapGestures{onDismiss()}},
        contentAlignment=Alignment.Center) {
        val shape=RoundedCornerShape(8.dp)
        Column(Modifier.fillMaxWidth(.9f).fillMaxHeight(.72f)
            .background(Brush.verticalGradient(listOf(Color(0xFF0C2636),Color(0xFF05121D))),shape)
            .border(1.dp,BuilderCyan.copy(alpha=.5f),shape).clip(shape)
            .pointerInput(Unit){detectTapGestures{}}
            .padding(16.dp),verticalArrangement=Arrangement.spacedBy(10.dp)) {
            Text("WIRESHARK CHECKS",color=BuilderInk,style=BuilderType.copy(fontSize=13.sp,lineHeight=16.sp,letterSpacing=.6.sp))
            Text("Hold a check to choose it. Drag to scroll. Tap outside this box to close it.",
                color=BuilderMuted,style=BuilderType.copy(fontSize=12.sp,lineHeight=15.sp))
            Column(Modifier.weight(1f).verticalScroll(rememberScrollState()),verticalArrangement=Arrangement.spacedBy(8.dp)) {
                for(type in StepRegistry.WIRESHARK_CHECKS) {
                    val spec=StepRegistry.spec(type)!!
                    val chosen=type==current
                    val rshape=RoundedCornerShape(6.dp)
                    Row(Modifier.fillMaxWidth().background(Color(0xFF0A2130),rshape)
                        .border(if(chosen)1.2.dp else .6.dp,(if(chosen)BuilderGreen else BuilderCyan).copy(alpha=.5f),rshape).clip(rshape)
                        .pointerInput(type){detectTapGestures(onLongPress={onHold(type)})}
                        .padding(horizontal=12.dp,vertical=10.dp),
                        verticalAlignment=Alignment.CenterVertically,horizontalArrangement=Arrangement.spacedBy(10.dp)) {
                        Canvas(Modifier.size(24.dp)){drawWiresharkFin(Offset.Zero,size.minDimension,BuilderCyan,1.2f*density)}
                        Column(Modifier.weight(1f)) {
                            Text(spec.title,color=BuilderInk,style=BuilderType.copy(fontSize=13.sp,lineHeight=16.sp))
                            Text(wiresharkFieldHint(type),color=BuilderMuted,style=BuilderType.copy(fontSize=11.sp,lineHeight=14.sp))
                        }
                        if(chosen) Text("IN USE",color=BuilderGreen,style=BuilderType.copy(fontSize=10.sp,lineHeight=13.sp,letterSpacing=.5.sp))
                    }
                }
            }
            BuilderTextButton("Close",true){onDismiss()}
        }
    }
}

/** The confirm popup the menu raises when a check is held: name it, say what it reads, ask Yes or No. */
@Composable
private fun WiresharkConfirm(type:String,onYes:()->Unit,onNo:()->Unit) {
    val spec=StepRegistry.spec(type)!!
    Box(Modifier.fillMaxSize().background(Color(0xCC01080F)).pointerInput(Unit){detectTapGestures{onNo()}},
        contentAlignment=Alignment.Center) {
        val shape=RoundedCornerShape(8.dp)
        Column(Modifier.fillMaxWidth(.82f).background(Brush.verticalGradient(listOf(Color(0xFF0C2636),Color(0xFF05121D))),shape)
            .border(1.dp,BuilderGreen.copy(alpha=.6f),shape).clip(shape).pointerInput(Unit){detectTapGestures{}}
            .padding(18.dp),verticalArrangement=Arrangement.spacedBy(12.dp)) {
            Text("Use \"${spec.title}\"?",color=BuilderInk,style=BuilderType.copy(fontSize=14.sp,lineHeight=18.sp))
            Text(wiresharkFieldHint(type),color=BuilderMuted,style=BuilderType.copy(fontSize=12.sp,lineHeight=15.sp))
            Row(horizontalArrangement=Arrangement.spacedBy(10.dp)) {
                BuilderTextButton("Yes, use it",true){onYes()}
                BuilderTextButton("No",true){onNo()}
            }
        }
    }
}

/** One plain line for the menu and the confirm popup: what each check reads, in Wireshark terms. */
private fun wiresharkFieldHint(type:String):String = when(type) {
    "deauth_target" -> "Who the disconnects are aimed at: broadcast or one client (wlan.da)."
    "deauth_rate" -> "How many disconnects there are, and how fast they arrive."
    "deauth_reason" -> "The reason each disconnect gives (wlan.fixed.reason_code)."
    "seq_integrity" -> "The disconnects' sequence numbers against the AP's beacons (wlan.seq)."
    "signal_match" -> "The disconnects' signal against the AP's beacons (radiotap.dbm_antsignal)."
    "aftermath" -> "What follows the disconnects: a clone, a password handshake, or the client talking on."
    "mgmt_protection" -> "Whether the network requires management frame protection (wlan.rsn.capabilities)."
    else -> StepRegistry.spec(type)?.description ?: ""
}
