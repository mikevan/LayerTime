package com.projectrevivesolutions.layertime.controller.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.gestures.detectTransformGestures
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Add
import androidx.compose.material.icons.outlined.CenterFocusStrong
import androidx.compose.material.icons.outlined.FitScreen
import androidx.compose.material.icons.outlined.Remove
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.runtime.collectAsState
import com.projectrevivesolutions.layertime.controller.flow.DataType
import com.projectrevivesolutions.layertime.controller.flow.Feature
import com.projectrevivesolutions.layertime.controller.flow.Finding
import com.projectrevivesolutions.layertime.controller.flow.FlowLayout
import com.projectrevivesolutions.layertime.controller.flow.StepRegistry
import com.projectrevivesolutions.layertime.controller.flow.TriageSession
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.IntSize
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.ceil
import kotlin.math.floor
import kotlin.math.roundToInt

private val BuilderInk=Color(0xFFE6EFF4)
private val BuilderCyan=Color(0xFF71E9FA)
private val BuilderMuted=Color(0xFF9BB1BF)
private val BuilderType=TextStyle(fontFamily=FontFamily.SansSerif,fontSize=11.sp,lineHeight=14.sp)

/**
 * The Builder: a large zoomable workspace that shows the loaded workflow as blocks
 * and wires, read-only (2026-11-04, slice 1). Each block is one step from the
 * wiring file; a tap shows what it does, what it needs, and its settings. After a
 * run, each block carries its result color: green benign, amber suspicious, grey
 * could-not-run, and the verdict block the verdict's color. Editing comes later,
 * on expanded-width screens, once the engine has proven the format.
 */
@Composable
fun BuilderScreen() {
    val density=LocalDensity.current
    val worldWidth=with(density) {10000.dp.toPx()}
    val worldHeight=with(density) {6000.dp.toPx()}
    var zoom by rememberSaveable {mutableFloatStateOf(1f)}
    var panX by rememberSaveable {mutableFloatStateOf(0f)}
    var panY by rememberSaveable {mutableFloatStateOf(0f)}
    var initialized by rememberSaveable {mutableStateOf(false)}
    var viewportSize by remember {mutableStateOf(IntSize.Zero)}
    var selected by rememberSaveable {mutableStateOf<String?>(null)}
    val workflow by TriageSession.workflow.collectAsState()
    val lastRun by TriageSession.run.collectAsState()
    val layout=remember(workflow) {workflow?.let {FlowLayout.of(it)}}
    val textMeasurer=rememberTextMeasurer()
    // The diagram sits at the center of the world, in canvas dp.
    val dpPx=with(density) {1.dp.toPx()}
    val diagramX=if(layout!=null)(worldWidth-layout.width*dpPx)/2f else 0f
    val diagramY=if(layout!=null)(worldHeight-layout.height*dpPx)/2f else 0f
    fun current()=BuilderViewport(zoom,panX,panY)
    fun update(next:BuilderViewport) {zoom=next.zoom;panX=next.x;panY=next.y}
    // Fit the diagram (not the whole world) into the viewport.
    fun fitDiagram(vw:Float,vh:Float) {
        val l=layout ?: run {update(BuilderViewport.centered(1f,vw,vh,worldWidth,worldHeight));return}
        val dw=l.width*dpPx; val dh=l.height*dpPx
        val z=(minOf(vw/dw,vh/dh)*.9f).coerceIn(BuilderViewport.MIN_ZOOM,BuilderViewport.MAX_ZOOM)
        update(BuilderViewport(z,(vw-dw*z)/2f-diagramX*z,(vh-dh*z)/2f-diagramY*z).bounded(vw,vh,worldWidth,worldHeight))
    }
    fun zoomBy(factor:Float) {
        if(viewportSize.width==0)return
        update(current().transform(factor,viewportSize.width/2f,viewportSize.height/2f,0f,0f,
            viewportSize.width.toFloat(),viewportSize.height.toFloat(),worldWidth,worldHeight))
    }
    Column(Modifier.fillMaxSize(),verticalArrangement=Arrangement.spacedBy(4.dp)) {
    OpsTelemetryHousing(Modifier.fillMaxWidth().weight(1f)) {
        Column(Modifier.fillMaxSize().padding(start=18.dp,end=18.dp,top=20.dp,bottom=24.dp),
            verticalArrangement=Arrangement.spacedBy(8.dp)) {
            Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically,horizontalArrangement=Arrangement.spacedBy(4.dp)) {
                Text("WORKSPACE",color=BuilderInk,style=BuilderType.copy(fontSize=11.sp,lineHeight=14.sp,letterSpacing=.5.sp),modifier=Modifier.weight(1f))
                BuilderControl("Zoom out",Icons.Outlined.Remove,zoom>BuilderViewport.MIN_ZOOM) {zoomBy(1f/1.25f)}
                Text("${(zoom*100).roundToInt()}%",color=BuilderCyan,style=BuilderType.copy(fontSize=10.sp,lineHeight=13.sp),
                    modifier=Modifier.widthIn(min=32.dp))
                BuilderControl("Zoom in",Icons.Outlined.Add,zoom<BuilderViewport.MAX_ZOOM) {zoomBy(1.25f)}
                BuilderControl("Fit the workflow",Icons.Outlined.FitScreen) {
                    if(viewportSize.width>0)fitDiagram(viewportSize.width.toFloat(),viewportSize.height.toFloat())
                }
                BuilderControl("Reset view to 100 percent",Icons.Outlined.CenterFocusStrong) {
                    if(viewportSize.width>0)update(BuilderViewport.centered(1f,viewportSize.width.toFloat(),viewportSize.height.toFloat(),worldWidth,worldHeight))
                }
            }
            val glass=RoundedCornerShape(5.dp)
            Canvas(Modifier.fillMaxWidth().weight(1f).background(Color(0xFF020D16),glass)
                .border(.6.dp,BuilderCyan.copy(alpha=.45f),glass).clip(glass)
                .onSizeChanged {next->
                    viewportSize=next
                    if(!initialized) {
                        fitDiagram(next.width.toFloat(),next.height.toFloat())
                        initialized=true
                    } else update(current().bounded(next.width.toFloat(),next.height.toFloat(),worldWidth,worldHeight))
                }
                .pointerInput(layout) {
                    // A tap picks the block under the finger; a tap on empty ground clears the pick.
                    detectTapGestures {pos->
                        val l=layout ?: return@detectTapGestures
                        val wx=((pos.x-panX)/zoom-diagramX)/dpPx; val wy=((pos.y-panY)/zoom-diagramY)/dpPx
                        selected=l.hit(wx,wy)?.step
                    }
                }
                .pointerInput(worldWidth,worldHeight) {
                    detectTransformGestures(panZoomLock=true) {centroid,pan,factor,_->
                        if(viewportSize.width>0)update(current().transform(factor,centroid.x,centroid.y,pan.x,pan.y,
                            viewportSize.width.toFloat(),viewportSize.height.toFloat(),worldWidth,worldHeight))
                    }
                }) {
                val origin=Offset(panX,panY)
                val board=Size(worldWidth*zoom,worldHeight*zoom)
                drawRect(Brush.verticalGradient(listOf(Color(0xFF071A27),Color(0xFF030F1A))),origin,board)
                // Adapt grid density to the zoom level; only visible lines are drawn.
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
                drawRect(BuilderCyan.copy(alpha=.5f),origin,board,style=Stroke(1.dp.toPx()))
                // The workflow: wires first, then blocks on top.
                val l=layout; val w=workflow
                if(l!=null && w!=null) {
                    fun sx(x:Float)=panX+(diagramX+x*dpPx)*zoom
                    fun sy(y:Float)=panY+(diagramY+y*dpPx)*zoom
                    for(e in w.edges) {
                        val a=l.node(e.from) ?: continue; val b=l.node(e.toStep) ?: continue
                        val color=when(e.carries) {DataType.EVENT->Color(0xFF9BB1BF);DataType.FRAMES->BuilderCyan;DataType.FEATURE->Color(0xFFFFD044);DataType.FINDING->Color(0xFF91E833)}
                        val x1=sx(a.right); val y1=sy(a.cy); val x2=sx(b.x); val y2=sy(b.cy)
                        val path=Path().apply {moveTo(x1,y1);cubicTo(x1+(x2-x1)*.5f,y1,x1+(x2-x1)*.5f,y2,x2,y2)}
                        drawPath(path,color.copy(alpha=.7f),style=Stroke(1.5.dp.toPx()*zoom.coerceIn(.5f,1.5f)))
                    }
                    for(n in l.nodes) {
                        val step=w.step(n.step) ?: continue
                        val spec=StepRegistry.spec(step.type)
                        val out=lastRun?.output(n.step)
                        val status=when(out) {
                            is Feature -> leanColor(out.lean)
                            is Finding -> verdictColor(out.verdict)
                            null -> Color(0xFF5E7A88)
                            else -> BuilderCyan
                        }
                        val tl=Offset(sx(n.x),sy(n.y)); val sz=Size(n.w*dpPx*zoom,n.h*dpPx*zoom)
                        val r=androidx.compose.ui.geometry.CornerRadius(5.dp.toPx()*zoom)
                        drawRoundRect(Brush.verticalGradient(listOf(Color(0xFF123447),Color(0xFF071C2A)),tl.y,tl.y+sz.height),tl,sz,r)
                        drawRoundRect(if(n.step==selected)Color.White else status,tl,sz,r,style=Stroke((if(n.step==selected)2f else 1f).dp.toPx()*zoom.coerceIn(.5f,2f)))
                        // the status lamp
                        drawCircle(status,4.dp.toPx()*zoom,Offset(tl.x+10.dp.toPx()*zoom,tl.y+10.dp.toPx()*zoom))
                        if(zoom>=.35f) {
                            val title=spec?.title ?: step.type
                            val value=when(out) {is Feature->out.value;is Finding->out.verdict.label;else->""}
                            val fontPx=12f*zoom
                            drawText(textMeasurer,title,Offset(tl.x+18.dp.toPx()*zoom,tl.y+5.dp.toPx()*zoom),
                                TextStyle(color=BuilderInk,fontSize=with(density){fontPx.toSp()},fontFamily=FontFamily.SansSerif),
                                maxLines=1,overflow=TextOverflow.Ellipsis,size=Size(sz.width-22.dp.toPx()*zoom,sz.height))
                            if(value.isNotEmpty()) drawText(textMeasurer,value,Offset(tl.x+10.dp.toPx()*zoom,tl.y+sz.height*.52f),
                                TextStyle(color=status,fontSize=with(density){(10.5f*zoom).toSp()},fontFamily=FontFamily.SansSerif),
                                maxLines=1,overflow=TextOverflow.Ellipsis,size=Size(sz.width-14.dp.toPx()*zoom,sz.height*.48f))
                        }
                    }
                }
            }
            Text(if(workflow==null)"No workflow is loaded." else "${workflow!!.name}, version ${workflow!!.version}. Pinch to zoom, drag to pan, tap a block.",
                color=BuilderMuted,style=BuilderType.copy(fontSize=10.sp,lineHeight=13.sp))
        }
    }
    selected?.let {id-> workflow?.step(id)?.let {step-> BuilderStepDetail(step,lastRun?.output(id)) {selected=null}}}
    BuilderToolsPalette()
    }
}

/** Empty, horizontally scrollable tool shelf; shares the navigation buttons' row height. */
@Composable
private fun BuilderToolsPalette() {
    val scroll=rememberScrollState()
    OpsConnectionHousing(Modifier.fillMaxWidth()) {
        Column(Modifier.fillMaxWidth().padding(start=18.dp,end=18.dp,top=17.dp,bottom=18.dp),
            verticalArrangement=Arrangement.spacedBy(7.dp)) {
            Text("TOOLS PALETTE",color=BuilderInk,style=BuilderType.copy(fontSize=12.sp,lineHeight=15.sp,letterSpacing=.6.sp))
            val shape=RoundedCornerShape(5.dp)
            BoxWithConstraints(Modifier.fillMaxWidth().height(NavigationInstrumentHeight)
                .background(Brush.verticalGradient(listOf(Color(0xFF092331),Color(0xFF030F19))),shape)
                .border(.6.dp,BuilderCyan.copy(alpha=.3f),shape).clip(shape)) {
                // Reserve three viewport widths without inventing tool icons or actions.
                val contentWidth=maxWidth*3f
                Row(Modifier.fillMaxSize().horizontalScroll(scroll)
                    .semantics {contentDescription="Tools Palette, empty tool tray"}) {
                    Spacer(Modifier.width(contentWidth).fillMaxHeight())
                }
            }
            Canvas(Modifier.fillMaxWidth().height(2.dp)) {
                drawRoundRect(BuilderCyan.copy(alpha=.1f),cornerRadius=androidx.compose.ui.geometry.CornerRadius(size.height/2))
                val total=size.width+scroll.maxValue.toFloat()
                val thumb=(size.width*size.width/total.coerceAtLeast(size.width)).coerceAtLeast(12.dp.toPx())
                val fraction=scroll.value.toFloat()/scroll.maxValue.coerceAtLeast(1)
                drawRoundRect(BuilderCyan.copy(alpha=.55f),Offset((size.width-thumb)*fraction,0f),Size(thumb,size.height),
                    cornerRadius=androidx.compose.ui.geometry.CornerRadius(size.height/2))
            }
        }
    }
}

@Composable
private fun BuilderControl(label:String,icon:ImageVector,enabled:Boolean=true,onClick:()->Unit) {
    val flash=rememberPressFlash()
    val shape=RoundedCornerShape(5.dp)
    Box(Modifier.size(28.dp).background(Color(0xFF0E2939),shape)
        .border(.6.dp,BuilderCyan.copy(alpha=if(enabled).55f else .15f),shape).clip(shape)
        .clickable(enabled=enabled,role=Role.Button,onClickLabel=label) {flash.fire();onClick()}
        .pressFlash(flash,false,BuilderCyan),contentAlignment=Alignment.Center) {
        Icon(icon,contentDescription=label,tint=if(enabled)BuilderInk else BuilderMuted.copy(alpha=.35f),modifier=Modifier.size(16.dp))
    }
}

/** What a tapped block is: title, description, inputs, settings, and its last result. Read-only. */
@Composable
private fun BuilderStepDetail(step:com.projectrevivesolutions.layertime.controller.flow.Step,output:Any?,onClose:()->Unit) {
    val spec=StepRegistry.spec(step.type)
    OpsConnectionHousing(Modifier.fillMaxWidth()) {
        Column(Modifier.fillMaxWidth().padding(start=18.dp,end=18.dp,top=16.dp,bottom=18.dp),verticalArrangement=Arrangement.spacedBy(5.dp)) {
            Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically) {
                Text((spec?.title ?: step.type).uppercase(),color=BuilderInk,style=BuilderType.copy(fontSize=12.sp,lineHeight=15.sp,letterSpacing=.6.sp),modifier=Modifier.weight(1f))
                BuilderControl("Close",Icons.Outlined.Remove) {onClose()}
            }
            if(spec!=null) {
                Text(spec.description,color=BuilderInk,style=BuilderType)
                val needs=spec.inputs.joinToString {"${it.name} (${it.type.wire}${if(it.many)", several" else ""})"}
                Text("Needs: ${if(needs.isEmpty())"nothing" else needs}. Produces: ${spec.output?.wire ?: "nothing"}.",color=BuilderMuted,style=BuilderType.copy(fontSize=10.sp,lineHeight=13.sp))
                for(st in spec.settings) Text("${st.key} = ${step.settings[st.key] ?: st.default}. ${st.description}",color=BuilderMuted,style=BuilderType.copy(fontSize=10.sp,lineHeight=13.sp))
            } else Text("This app has no step type \"${step.type}\".",color=Color(0xFFFF5A4E),style=BuilderType)
            when(output) {
                is Feature -> Text("Last run: ${output.value}. ${output.reason}",color=leanColor(output.lean),style=BuilderType)
                is Finding -> Text("Last run: ${output.verdict.label}. ${output.summary}",color=verdictColor(output.verdict),style=BuilderType)
                null -> Text("Not run yet. Run a triage from an event's detail page.",color=BuilderMuted,style=BuilderType.copy(fontSize=10.sp,lineHeight=13.sp))
                else -> {}
            }
        }
    }
}
