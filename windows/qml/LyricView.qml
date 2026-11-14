pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls

Item {
 id:view
 required property var controller
 property int settingsRevision:0
 property string settingsPrefix:compact?"desktopLyrics":"lyrics"
 readonly property bool desktop:settingsPrefix==="desktopLyrics"
 property var configuration:({})
 function option(key,fallback){var revision=settingsRevision;if(configuration&&configuration[key]!==undefined)return configuration[key];return controller&&typeof controller.setting==="function"?controller.setting(settingsPrefix+"/"+key,fallback):fallback}
 function colorOption(key,fallback){var value=option(key,"auto");return value==="auto"?fallback:value}
 signal seekRequested(real position)
 property color textColor:"#fff8f5"
 property color inactiveColor:"#bdc5d5"
 property string fontFamily:option("fontFamily",type.displayFamily)
 property int fontSize:option("fontSize",desktop?36:28)
 property int scrollDuration:option("scrollDuration",420)
 property string easingCurve:option("easing","outCubic")
 property real focusPosition:option("focus",desktop?.5:.45)
 property real lineSpacing:option("lineSpacing",desktop?.22:.65)
 property real activeScale:option("activeScale",1.10)
 property real inactiveOpacity:option("futureOpacity",desktop?.42:.5)
 property string highlightMode:option("highlight","word")
 property bool motion:true
 property bool clockEnabled:true
 property bool interactive:true
 property real originalFocus:0
 readonly property rect primaryRect:{view.width;view.height;timeline.contentY;timeline.count;const row=timeline.itemAtIndex(line);return row?row.primaryGeometry():Qt.rect(0,height*.45,width,fontSize*1.4)}
 property bool centerCurrentText:false
 property bool flushAlignment:false
 property bool alignmentAnimation:true
 property real alignmentPosition:alignment==="right"?1:alignment==="center"?.5:0
 Behavior on alignmentPosition{enabled:view.alignmentAnimation&&view.active&&view.motion;NumberAnimation{duration:220;easing.type:Easing.OutCubic}}
 property bool compact:false
 property int compactLines:2
 property bool showTranslation:option("translation",true)
 property bool showRomanization:option("romanization",false)
 property real outlineWidth:option("outlineWidth",0)
 property color outlineColor:option("outlineColor","#80000000")
 property bool shadow:option("shadow",false)
 property bool centered:compact
 readonly property string alignment:option("alignment",centered?"center":"left")
 readonly property string longLine:option("longLine",desktop?"shrink":"wrap")
 readonly property real reserveScale:Math.max(1,activeScale,option("inactiveScale",1))
 readonly property int fontWeight:option("fontWeight",desktop?450:500)
 readonly property int activeWeight:option("activeWeight",desktop?500:600)
 readonly property real inkPadding:Math.ceil(Math.max(5,outlineWidth+2,shadow?option("shadowBlur",6)+Math.abs(option("shadowOffset",2)):0,option("glow",false)||option("wordGlow",false)?option("glowRadius",8):0))
 readonly property int rendering:option("rendering","curve")==="native"?Text.NativeRendering:option("rendering","curve")==="qt"?Text.QtRendering:Text.CurveRendering
 readonly property var rows:controller?controller.lyrics:[]
 readonly property int line:controller?controller.currentLine:-1
 readonly property bool untimed:rows.length>0&&rows[0].untimed===true
 readonly property bool active:visible&&Window.window&&Window.window.visible&&Window.window.visibility!==Window.Minimized
 readonly property bool edgeLayer:active&&!compact&&(originalFocus>0||(option("fadeStrength",1)>0&&(option("fadeTop",70)>0||option("fadeBottom",70)>0)))&&GraphicsInfo.api!==GraphicsInfo.Software
 readonly property real topFade:Math.min(option("fadeTop",70),height*.28)
 readonly property real bottomFade:Math.min(option("fadeBottom",70),height*.28)
 readonly property var timelinePosition:active&&controller?controller.lyricPosition:0
 readonly property var position:clockEnabled?timelinePosition:0
 property string clockSurface:"lyrics-"+Math.random().toString(36).slice(2)
 readonly property font layoutFace:Qt.font({family:fontFamily,pixelSize:fontSize,weight:Math.max(fontWeight,activeWeight),letterSpacing:option("letterSpacing",0),contextFontMerging:true,preferTypoLineMetrics:true,hintingPreference:Font.PreferNoHinting})
 TypographyTokens{id:type}
 function align(value){return value==="right"?Text.AlignRight:value==="center"?Text.AlignHCenter:Text.AlignLeft}
 function plainChunks(row){const text=row.text||"";const parts=text.match(/[\u2e80-\u9fff\u3040-\u30ff]|[^\s\u2e80-\u9fff\u3040-\u30ff]+\s*|\s+/g)||[];return parts.map(function(part){return {text:part,time:row.time||0,end:row.end||0}})}
 function primaryTokens(){const row=timeline.itemAtIndex(line);return row?row.originalTokens():[]}
 function syncClock(){if(controller&&typeof controller.setLyricsVisible==="function")controller.setLyricsVisible(clockSurface,active&&clockEnabled)}
 function recover(){timeline.userScrolling=false;timeline.cancelFlick();timeline.follow()}
 onClockEnabledChanged:syncClock()
 onActiveChanged:{syncClock();if(!active){scrollMotion.stop();timeline.positioned=false}else Qt.callLater(timeline.follow)}
 onLineChanged:Qt.callLater(timeline.follow)
 onWidthChanged:Qt.callLater(timeline.follow)
 Component.onCompleted:syncClock()
 Component.onDestruction:if(controller&&typeof controller.setLyricsVisible==="function")controller.setLyricsVisible(clockSurface,false)
 clip:true

 component Glyph:Item {
  id:glyph;objectName:"lyricGlyph"
  required property string value
  required property real progress
  required property real limit
  required property font face
  property font metricsFace:face
  required property color inkColor
  property bool highlight:false
  property real lineShift:0
  transform:Translate{x:glyph.lineShift}
  TextMetrics{id:measure;font:glyph.metricsFace;text:glyph.value}
  TextMetrics{id:visibleMeasure;font:glyph.metricsFace;text:glyph.value.replace(/\s+$/,"")}
  readonly property real visibleAdvance:visibleMeasure.advanceWidth
  width:Math.max(1,Math.min(limit,measure.advanceWidth))
  height:Math.max(view.fontSize*view.option("lineHeight",1.25),layoutSizer.implicitHeight)
  Text{id:layoutSizer;visible:false;width:glyph.width;text:glyph.value;textFormat:Text.PlainText;font:glyph.metricsFace;wrapMode:Text.NoWrap;lineHeight:view.option("lineHeight",1.25)}
  Text {id:base;visible:!glyph.highlight;width:glyph.width;text:glyph.value;textFormat:Text.PlainText;font:glyph.face;wrapMode:Text.NoWrap;elide:view.longLine==="elide"?Text.ElideRight:Text.ElideNone;color:glyph.inkColor;renderType:view.rendering;renderTypeQuality:104;lineHeight:view.option("lineHeight",1.25)}
  readonly property real fill:view.option("wordFill","continuous")==="step"?(progress>=1?1:0):Math.max(0,Math.min(1,progress))
  readonly property real soft:fill>0&&fill<1?Math.min(fill,.3,view.option("wordSoftness",.04)):0
  Item {
   visible:glyph.highlight;width:Math.max(0,glyph.width*(glyph.fill-glyph.soft));height:glyph.height;clip:true
   Text {width:glyph.width;text:glyph.value;textFormat:Text.PlainText;font:glyph.face;wrapMode:Text.NoWrap;elide:view.longLine==="elide"?Text.ElideRight:Text.ElideNone;color:glyph.inkColor;renderType:view.rendering;renderTypeQuality:104;lineHeight:view.option("lineHeight",1.25)}
  }
  Repeater {model:glyph.highlight&&glyph.soft>0?6:0
   Item {required property int index
    x:Math.max(0,glyph.width*(glyph.fill-glyph.soft))+index*glyph.width*glyph.soft/6
    width:glyph.width*glyph.soft/6;height:glyph.height;clip:true;opacity:1-(index+.5)/6
    Text {x:-parent.x;width:glyph.width;text:glyph.value;textFormat:Text.PlainText;font:glyph.face;wrapMode:Text.NoWrap;color:glyph.inkColor;renderType:view.rendering;renderTypeQuality:104;lineHeight:view.option("lineHeight",1.25)}
   }
  }
 }
 component Line:Item {
  id:rowItem;objectName:"lyricRow-"+index
  required property var modelData
  required property int index
  readonly property var row:modelData
  readonly property bool selected:index===view.line
  readonly property bool past:index<view.line
  function originalTokens(){
   let prefix="",result=[]
   for(let i=0;i<words.children.length;i++){
    const glyph=words.children[i]
    if(glyph.objectName!=="lyricGlyph")continue
    const token=rowItem.row.words&&rowItem.row.words[result.length]
    const rect=glyph.mapToItem(view,Qt.rect(0,0,glyph.width,glyph.height))
    result.push({text:glyph.value,prefix:prefix,rect:rect,scale:rect.width/Math.max(1,glyph.width),family:glyph.face.family,time:token?token.time:0,end:token?token.end:0,timed:rowItem.wordTimed})
    prefix+=glyph.value
   }
   return result
  }
  property var wrappedLineEdges:({})
  function measureLineEdges(){
   const edges={}
   for(let i=0;i<words.children.length;i++){const glyph=words.children[i];if(glyph.objectName!=="lyricGlyph"||glyph.visibleAdvance<=0)continue;const key=String(Math.round(glyph.y));edges[key]=Math.max(edges[key]||0,glyph.x+glyph.visibleAdvance)}
   wrappedLineEdges=edges
  }
  function primaryGeometry(){return primary.mapToItem(view,Qt.rect(0,0,primary.width,primary.height))}
  readonly property real primaryCenterOffset:primary.height/2
  // Delegate measurement can finish after the initial follow request. Re-anchor
  // only on layout changes, never on each media-clock tick.
  onYChanged:if(selected)Qt.callLater(timeline.follow)
  onHeightChanged:if(selected)Qt.callLater(timeline.follow)
  readonly property real layoutWidth:Math.max(1,(Math.min(timeline.width,view.option("maxWidth",view.desktop?1800:1000))/view.reserveScale)-view.inkPadding*2-8)
  readonly property font face:Qt.font({family:type.resolvedFamily(rowItem.row.text||"",view.fontFamily),pixelSize:view.fontSize,weight:selected?view.activeWeight:view.fontWeight,letterSpacing:view.option("letterSpacing",0),wordSpacing:!view.desktop&&!rowItem.selected?-view.fontSize*.075:0,contextFontMerging:true,preferTypoLineMetrics:true,hintingPreference:Font.PreferNoHinting})
  readonly property var chunks:view.longLine==="elide"?[{text:row.text||"",time:row.time||0,end:row.end||0}]:row.words&&row.words.length?row.words:view.plainChunks(row)
  readonly property bool wordTimed:view.longLine!=="elide"&&!!row.words&&row.words.length>0
  readonly property font measureFace:Qt.font({family:type.resolvedFamily(rowItem.row.text||"",view.fontFamily),pixelSize:view.fontSize,weight:Math.max(view.fontWeight,view.activeWeight),letterSpacing:view.option("letterSpacing",0),wordSpacing:!view.desktop&&!rowItem.selected?-view.fontSize*.075:0,contextFontMerging:true,preferTypoLineMetrics:true,hintingPreference:Font.PreferNoHinting})
  TextMetrics{id:fullMeasure;font:rowItem.measureFace;text:rowItem.row.text||""}
  readonly property real shrink:view.longLine==="shrink"?Math.max(view.option("minimumScale",.65),Math.min(1,layoutWidth/Math.max(1,flowWidth))):1
  readonly property real naturalWidth:Math.max(1,fullMeasure.advanceWidth)
  readonly property real flowWidth:view.longLine==="wrap"?Math.min(layoutWidth,naturalWidth):naturalWidth
  readonly property real scrollViewportWidth:Math.max(1,Math.min(rowItem.width,view.option("maxWidth",view.desktop?1800:1000)))
  readonly property bool scrolling:view.longLine==="scroll"&&rowItem.naturalWidth*rowItem.visualScale>rowItem.scrollViewportWidth
  readonly property real scrollStart:row.words&&row.words.length?row.words[0].time:row.time||0
  readonly property real scrollEnd:row.words&&row.words.length?row.words[row.words.length-1].end:row.end||scrollStart
  readonly property real scrollFraction:scrollEnd>scrollStart?Math.max(0,Math.min(1,(view.timelinePosition-scrollStart)/(scrollEnd-scrollStart))):0
  readonly property real scrollDistance:Math.max(0,(primary.width-view.inkPadding*2)*primary.scale-rowItem.scrollViewportWidth)
  readonly property real scrollOffset:scrolling?scrollDistance*scrollFraction:0
  readonly property real sourceWidth:view.longLine==="elide"?Math.min(layoutWidth,naturalWidth):flowWidth
  readonly property color baseColor:selected?view.colorOption("unsungColor",view.inactiveColor):view.untimed?view.textColor:past?view.colorOption("pastColor",view.inactiveColor):view.colorOption("futureColor",view.inactiveColor)
  readonly property real stateOpacity:selected||view.untimed?view.option("activeOpacity",1):view.line<0?.78:Math.min(1,(past?view.option("pastOpacity",view.desktop?.5:.58):view.inactiveOpacity)+view.option("nearbyEmphasis",.12)/Math.max(1,Math.abs(index-view.line)))
  readonly property real visualScale:(selected?view.activeScale:view.option("inactiveScale",1))*shrink
  readonly property real centerY:y-timeline.contentY+height/2
  readonly property real fade:view.compact||view.edgeLayer?1:Math.pow(Math.max(0,Math.min(1,view.topFade>0?centerY/view.topFade:1,view.bottomFade>0?(timeline.height-centerY)/view.bottomFade:1)),view.option("fadeStrength",1))
  width:timeline.width
  height:body.height+view.option("groupSpacing",10)
  opacity:(view.compact&&(index<view.line||index>=view.line+view.compactLines)?0:stateOpacity)*fade*(selected?1:1-view.originalFocus)
  Behavior on opacity {enabled:view.active&&view.motion;NumberAnimation{duration:view.option("highlightDuration",180)}}
  Column {
   id:body;width:rowItem.layoutWidth*view.reserveScale+view.inkPadding*2
   x:view.flushAlignment?(rowItem.width-width)*view.alignmentPosition+view.inkPadding*primary.scale*(2*view.alignmentPosition-1):view.alignment==="center"?(rowItem.width-width)/2:view.alignment==="right"?rowItem.width-width:4
   spacing:0
   Item {
    x:rowItem.scrolling?(rowItem.width-rowItem.scrollViewportWidth)*view.alignmentPosition-body.x:0;width:rowItem.scrolling?rowItem.scrollViewportWidth:parent.width;height:primary.height*view.reserveScale*rowItem.shrink;clip:rowItem.scrolling
    layer.enabled:view.active&&rowItem.scrolling&&rowItem.selected&&GraphicsInfo.api!==GraphicsInfo.Software
    layer.smooth:true
    layer.effect:ShaderEffect{
     property var source
     property vector4d fading:Qt.vector4d(0,0,0,1)
     property vector4d focusWindow:Qt.vector4d(rowItem.scrollOffset>.5?18/Math.max(1,rowItem.scrollViewportWidth):0,rowItem.scrollDistance-rowItem.scrollOffset>.5?18/Math.max(1,rowItem.scrollViewportWidth):0,.001,0)
     fragmentShader:"qrc:/qt/qml/DroidMediaLink/shaders/lyricfocus.frag.qsb"
    }
    LyricInk {
     id:primary
     width:(view.longLine==="scroll"?Math.max(1,words.childrenRect.width):rowItem.sourceWidth)+view.inkPadding*2;height:words.height+view.inkPadding*2
     x:rowItem.scrolling?-rowItem.scrollOffset-view.inkPadding*scale:view.flushAlignment?(parent.width-width*scale)*view.alignmentPosition:view.alignment==="center"?(parent.width-width)/2:view.alignment==="right"?parent.width-width:0
     scale:rowItem.visualScale;transformOrigin:view.flushAlignment||rowItem.scrolling?Item.TopLeft:view.alignment==="center"?Item.Center:view.alignment==="right"?Item.Right:Item.Left
     outlineWidth:rowItem.selected&&width<=8192?view.outlineWidth:0;outlineColor:view.outlineColor
     shadow:rowItem.selected&&view.shadow&&width<=8192;shadowBlur:view.option("shadowBlur",6);shadowOffset:view.option("shadowOffset",2);shadowColor:view.option("shadowColor","#80000000")
     glow:rowItem.selected&&view.option("glow",false)&&width<=8192;glowRadius:view.option("glowRadius",8);glowOpacity:view.option("glowOpacity",.2);glowColor:view.option("glowColor","#d4e7ff")
     Behavior on scale {enabled:view.active&&view.motion&&view.option("scaleAnimation",true);NumberAnimation{duration:view.option("scaleDuration",280);easing.type:Easing.OutCubic}}
     Flow {
      id:words;objectName:"lyricWords";onPositioningComplete:rowItem.measureLineEdges();x:view.inkPadding;y:view.inkPadding;width:view.longLine==="scroll"?100000:rowItem.sourceWidth
      Repeater {model:rowItem.chunks
       Glyph {required property var modelData;lineShift:view.longLine==="wrap"?(rowItem.sourceWidth-(rowItem.wrappedLineEdges[String(Math.round(y))]||rowItem.sourceWidth))*view.alignmentPosition:0;value:modelData.text||"";limit:rowItem.sourceWidth;face:rowItem.face;metricsFace:rowItem.measureFace;progress:0;inkColor:rowItem.baseColor}
      }
     }
     LyricInk {
      x:0;y:0;width:primary.width;height:primary.height;visible:rowItem.selected&&!view.untimed
      glow:view.option("wordGlow",false)&&width<=8192;glowRadius:view.option("glowRadius",8);glowOpacity:view.option("glowOpacity",.2);glowColor:view.option("glowColor","#d4e7ff")
      Flow {x:view.inkPadding;y:view.inkPadding;width:view.longLine==="scroll"?100000:rowItem.sourceWidth
       Repeater {model:rowItem.chunks
        Glyph {required property var modelData;lineShift:view.longLine==="wrap"?(rowItem.sourceWidth-(rowItem.wrappedLineEdges[String(Math.round(y))]||rowItem.sourceWidth))*view.alignmentPosition:0;value:modelData.text||"";limit:rowItem.sourceWidth;face:rowItem.face;metricsFace:rowItem.measureFace;highlight:true;inkColor:view.colorOption("sungColor",view.colorOption("color",view.textColor))
         progress:!rowItem||!view?0:!rowItem.wordTimed||view.highlightMode==="line"?1:Math.max(0,Math.min(1,(view.position-modelData.time)/Math.max(1,modelData.end-modelData.time)))
         Behavior on progress {enabled:view.motion&&view.active;NumberAnimation{duration:view.option("wordDuration",70);easing.type:Easing.Linear}}
        }
       }
      }
     }
    }
   }
   Text {
    visible:view.showTranslation&&(rowItem.row.translation||"").length>0
    width:view.flushAlignment?Math.min(implicitWidth,parent.width):parent.width;topPadding:view.option("translationSpacing",6)
    x:view.flushAlignment?(rowItem.width-width)*(view.option("translationAlignment","inherit")==="inherit"?view.alignmentPosition:horizontalAlignment===Text.AlignRight?1:horizontalAlignment===Text.AlignHCenter?.5:0)-body.x:horizontalAlignment===Text.AlignLeft&&view.alignment==="left"?view.inkPadding*primary.scale:horizontalAlignment===Text.AlignRight&&view.alignment==="right"?-view.inkPadding*primary.scale:0
    text:rowItem.row.translation||"";textFormat:Text.PlainText;wrapMode:Text.Wrap
    horizontalAlignment:view.align(view.option("translationAlignment","inherit")==="inherit"?view.alignment:view.option("translationAlignment","inherit"))
    color:view.colorOption("translationColor",view.textColor);opacity:view.option("translationOpacity",.78)*(1-view.originalFocus)
    font.family:type.resolvedFamily(text,view.option("translationFont",type.uiFamily));font.pixelSize:Math.round(view.fontSize*view.option("translationSize",.60));font.weight:view.option("translationWeight",400);font.contextFontMerging:true
    lineHeight:view.option("translationLineHeight",1.2);renderType:view.rendering;renderTypeQuality:104
   }
   Text {
    visible:view.showRomanization&&(rowItem.row.romanization||"").length>0
    width:view.flushAlignment?Math.min(implicitWidth,parent.width):parent.width;topPadding:view.option("romanizationSpacing",6)
    x:view.flushAlignment?(rowItem.width-width)*(view.option("romanizationAlignment","inherit")==="inherit"?view.alignmentPosition:horizontalAlignment===Text.AlignRight?1:horizontalAlignment===Text.AlignHCenter?.5:0)-body.x:horizontalAlignment===Text.AlignLeft&&view.alignment==="left"?view.inkPadding*primary.scale:horizontalAlignment===Text.AlignRight&&view.alignment==="right"?-view.inkPadding*primary.scale:0
    text:rowItem.row.romanization||"";textFormat:Text.PlainText;wrapMode:Text.Wrap
    horizontalAlignment:view.align(view.option("romanizationAlignment","inherit")==="inherit"?view.alignment:view.option("romanizationAlignment","inherit"))
    color:view.colorOption("romanizationColor",view.textColor);opacity:view.option("romanizationOpacity",.62)*(1-view.originalFocus)
    font.family:type.resolvedFamily(text,view.option("romanizationFont",type.uiFamily));font.pixelSize:Math.round(view.fontSize*view.option("romanizationSize",.52));font.weight:view.option("romanizationWeight",400);font.contextFontMerging:true
    lineHeight:view.option("romanizationLineHeight",1.2);renderType:view.rendering;renderTypeQuality:104
   }
  }
  TapHandler {enabled:view.interactive&&!view.compact&&!view.untimed;gesturePolicy:TapHandler.DragThreshold;onTapped:{if(!timeline.moving&&!timeline.dragging){view.seekRequested(rowItem.row.time);view.recover()}}}
 }
 ListView {
  id:timeline;objectName:"lyricTimeline";anchors.fill:parent
  model:view.active?view.rows:null;spacing:Math.round(view.fontSize*view.lineSpacing);cacheBuffer:Math.max(0,height*.5)
  layer.enabled:view.edgeLayer
  layer.smooth:true
  layer.effect:ShaderEffect {
   property var source
   property vector4d fading:Qt.vector4d(view.topFade/Math.max(1,view.height),view.bottomFade/Math.max(1,view.height),view.option("fadeStrength",1),0)
   property vector4d focusWindow:Qt.vector4d(view.primaryRect.y/Math.max(1,view.height),(view.primaryRect.y+view.primaryRect.height)/Math.max(1,view.height),2/Math.max(1,view.height),view.originalFocus)
   fragmentShader:"qrc:/qt/qml/DroidMediaLink/shaders/lyricfocus.frag.qsb"
  }
  currentIndex:-1;highlight:null;highlightRangeMode:ListView.NoHighlightRange
  property bool userScrolling:false
  property bool positioned:false
  topMargin:height;bottomMargin:height
  boundsBehavior:view.option("bounds","bounce")==="stop"?Flickable.StopAtBounds:Flickable.DragAndOvershootBounds
  maximumFlickVelocity:view.option("inertia",2500);flickDeceleration:view.option("deceleration",1800)
  rebound:Transition{NumberAnimation{properties:"x,y";duration:view.option("bounceDuration",240);easing.type:Easing.OutCubic}}
  interactive:view.interactive&&!view.compact;flickableDirection:Flickable.VerticalFlick
  function manual(){scrollMotion.stop();userScrolling=true;resumeFollow.stop()}
  function follow(){
   const i=view.line
   if(i<0||i>=count||userScrolling||!view.active||!view.option("autoScroll",true))return
   const row=itemAtIndex(i)
   if(!row){positionViewAtIndex(i,ListView.Center);Qt.callLater(follow);return}
   const focus=view.centerCurrentText?.5:view.compact?(view.compactLines===1?.5:.18):view.focusPosition
   const target=row.y-height*focus+(view.centerCurrentText?row.primaryCenterOffset:!view.compact||view.compactLines===1?row.height/2:0)
   if(Math.abs(target-contentY)<.5)return
   scrollMotion.stop()
   if(view.motion&&positioned){scrollMotion.from=contentY;scrollMotion.to=target;scrollMotion.duration=view.scrollDuration;scrollMotion.start()}else contentY=target
   positioned=true
  }
  onMovementStarted:manual()
  onMovementEnded:resumeFollow.restart()
  onCountChanged:{currentIndex=-1;Qt.callLater(follow)}
  onHeightChanged:Qt.callLater(follow)
  delegate:Line{}
  NumberAnimation{id:scrollMotion;target:timeline;property:"contentY";duration:view.scrollDuration;easing.type:view.easingCurve==="linear"?Easing.Linear:view.easingCurve==="inOutCubic"?Easing.InOutCubic:view.easingCurve==="outQuint"?Easing.OutQuint:Easing.OutCubic}
  WheelHandler {enabled:view.interactive&&!view.compact;target:null;acceptedDevices:PointerDevice.Mouse
   onWheel:function(event){timeline.manual();timeline.cancelFlick();const delta=(event.pixelDelta.y||event.angleDelta.y/120*view.fontSize*2)*view.option("wheelSensitivity",1.4);const target=Math.max(-timeline.topMargin,Math.min(timeline.contentHeight-timeline.height+timeline.bottomMargin,timeline.contentY-delta));scrollMotion.from=timeline.contentY;scrollMotion.to=target;scrollMotion.duration=Math.min(200,view.scrollDuration);scrollMotion.start();resumeFollow.restart();event.accepted=true}
  }
  Timer{id:resumeFollow;interval:view.option("recoveryDelay",2400);onTriggered:view.recover()}
  ScrollBar.vertical:ScrollBar{visible:view.interactive&&!view.compact;policy:ScrollBar.AsNeeded;implicitWidth:4;background:Item{}
 contentItem:Rectangle{radius:2;color:view.inactiveColor;opacity:.25}}
 }
}
