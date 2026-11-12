pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls

Item {
 id:view
 required property var controller
 property color textColor:"#f2f4f8"
 property color inactiveColor:"#8093a2b1"
 property string fontFamily:"Microsoft YaHei UI"
 property int fontSize:32
 property int scrollDuration:420
 property string easingCurve:"outCubic"
 property real focusPosition:.45
 property real lineSpacing:.8
 property real activeScale:1.04
 property real inactiveOpacity:.58
 property string highlightMode:"word"
 property bool motion:true
 property bool compact:false
 property int compactLines:2
 property bool showTranslation:true
 property bool showRomanization:false
 property real outlineWidth:0
 property color outlineColor:"#99000000"
 property bool shadow:false
 property bool centered:compact
 readonly property var rows:controller ? controller.lyrics : []
 readonly property int line:controller ? controller.currentLine : -1
 readonly property bool untimed:rows.length>0 && rows[0].untimed===true
 readonly property bool active:visible && Window.window && Window.window.visible && Window.window.visibility!==Window.Minimized
 property string clockSurface:"lyrics-"+Math.random().toString(36).slice(2)
 function syncClock(){if(controller&&typeof controller.setLyricsVisible==="function")controller.setLyricsVisible(clockSurface,active)}
 onActiveChanged:{syncClock();if(!active)timeline.positioned=false;else Qt.callLater(timeline.follow)}
 onLineChanged:Qt.callLater(timeline.follow)
 Component.onCompleted:syncClock()
 Component.onDestruction:if(controller&&typeof controller.setLyricsVisible==="function")controller.setLyricsVisible(clockSurface,false)
 clip:true

 component Glyph:Item {
  id:glyph
  required property string value
  required property real progress
  required property real availableWidth
  property font face
  TextMetrics{id:measure;font:glyph.face;text:glyph.value}
  width:Math.max(1,Math.min(availableWidth,Math.max(1,measure.advanceWidth)))
  height:base.implicitHeight
  Repeater {
   model:view.outlineWidth>0?8:0
   Text {
    required property int index
    x:Math.cos(index*Math.PI/4)*view.outlineWidth;y:Math.sin(index*Math.PI/4)*view.outlineWidth
    width:glyph.width;text:glyph.value;textFormat:Text.PlainText;font:glyph.face;wrapMode:Text.Wrap
    color:view.outlineColor;renderType:Text.NativeRendering
   }
  }
  Text {visible:view.shadow;x:1;y:2;width:glyph.width;text:glyph.value;textFormat:Text.PlainText;font:glyph.face;wrapMode:Text.Wrap;color:"#80000000";renderType:Text.NativeRendering}
  Text {id:base;width:glyph.width;text:glyph.value;textFormat:Text.PlainText;font:glyph.face;wrapMode:Text.Wrap;color:view.inactiveColor;renderType:Text.NativeRendering}
  Item {
   width:glyph.width*Math.max(0,Math.min(1,glyph.progress));height:glyph.height;clip:true
   Text {width:glyph.width;text:glyph.value;textFormat:Text.PlainText;font:glyph.face;wrapMode:Text.Wrap;color:view.textColor;renderType:Text.NativeRendering}
  }
 }
 component Line:Column {
  id:lyricLine
  required property var row
  required property bool selected
  property int rowIndex:-1
  required property real availableWidth
  width:Math.max(1,availableWidth);spacing:6
  opacity:view.compact&&(rowIndex<view.line||rowIndex>=view.line+view.compactLines)?0:selected || view.untimed?1:view.inactiveOpacity
  scale:selected?view.activeScale:1
  transformOrigin:Item.Left
  Behavior on scale {enabled:view.active&&view.motion;NumberAnimation{duration:240}}
  Behavior on opacity {enabled:view.active&&view.motion;NumberAnimation{duration:240}}
  property font face:Qt.font({family:view.fontFamily,pixelSize:view.fontSize,weight:Font.DemiBold})
  readonly property var chunks:row.words&&row.words.length?row.words:[{text:row.text||"",time:row.time||0,end:row.end||0}]
  readonly property bool wordTimed:row.words&&row.words.length>0
  TextMetrics{id:lineMeasure;font:lyricLine.face;text:lyricLine.row.text||""}
  Flow {
   width:Math.max(1,Math.min(lyricLine.width,Math.max(1,lineMeasure.advanceWidth)));anchors.horizontalCenter:view.centered?parent.horizontalCenter:undefined
   spacing:0
   Repeater {
    model:lyricLine.chunks
    Glyph {
     required property var modelData
     value:modelData.text||"";face:lyricLine.face;availableWidth:lyricLine.availableWidth
     progress:!lyricLine.selected?(view.untimed?1:0):!lyricLine.wordTimed||view.highlightMode==="line"?1:Math.max(0,Math.min(1,((view.active?view.controller.lyricPosition:0)-modelData.time)/Math.max(1,modelData.end-modelData.time)))
    }
   }
  }
  Text {
   visible:view.showTranslation && (lyricLine.row.translation||"").length>0;width:parent.width
   text:lyricLine.row.translation||"";textFormat:Text.PlainText;wrapMode:Text.Wrap
   horizontalAlignment:view.centered?Text.AlignHCenter:Text.AlignLeft
   color:view.textColor;opacity:.76;font.family:view.fontFamily;font.pixelSize:Math.round(view.fontSize*.58)
   style:view.outlineWidth>0?Text.Outline:Text.Normal;styleColor:view.outlineColor
  }
  Text {
   visible:view.showRomanization && (lyricLine.row.romanization||"").length>0;width:parent.width
   text:lyricLine.row.romanization||"";textFormat:Text.PlainText;wrapMode:Text.Wrap
   horizontalAlignment:view.centered?Text.AlignHCenter:Text.AlignLeft
   color:view.textColor;opacity:.65;font.family:view.fontFamily;font.pixelSize:Math.round(view.fontSize*.5)
  }
 }
 ListView {
  id:timeline;anchors.fill:parent
  model:view.active?view.rows:null;spacing:Math.round(view.fontSize*(view.compact?.22:view.lineSpacing));cacheBuffer:Math.max(0,height)
  currentIndex:view.untimed?-1:view.line
  property bool userScrolling:false
  property bool positioned:false
  topMargin:height;bottomMargin:height
  boundsBehavior:Flickable.StopAtBounds
  interactive:!view.compact
  function follow(){
   const targetIndex=view.untimed?-1:view.line
   if(targetIndex<0||targetIndex>=count||userScrolling||!view.active)return
   currentIndex=targetIndex
   const row=itemAtIndex(targetIndex)
   if(!row){positionViewAtIndex(currentIndex,ListView.Center);Qt.callLater(follow);return}
   const focus=view.compact?(view.compactLines===1?.5:.15):view.focusPosition
   const target=row.y-height*focus+(view.compact&&view.compactLines===1?row.height*.5:0)
   scrollAnimation.stop()
   if(view.motion&&positioned){scrollAnimation.from=contentY;scrollAnimation.to=target;scrollAnimation.start()}
   else contentY=target
   positioned=true
  }
  onMovementStarted:{scrollAnimation.stop();userScrolling=true;resumeFollow.stop()}
  onMovementEnded:resumeFollow.restart()

  onCountChanged:Qt.callLater(follow)
  onHeightChanged:Qt.callLater(follow)
  NumberAnimation{id:scrollAnimation;target:timeline;property:"contentY";duration:view.scrollDuration;easing.type:view.easingCurve==="linear"?Easing.Linear:view.easingCurve==="inOutCubic"?Easing.InOutCubic:Easing.OutCubic}
  delegate:Line {required property var modelData;required property int index;row:modelData;rowIndex:index;availableWidth:Math.max(1,timeline.width/view.activeScale-8);selected:index===view.line}
  Timer {id:resumeFollow;interval:2400;onTriggered:{timeline.userScrolling=false;timeline.follow()}}
  ScrollBar.vertical:ScrollBar{id:scroll;visible:!view.compact;policy:ScrollBar.AsNeeded;implicitWidth:4;background:Item{} contentItem:Rectangle{implicitWidth:4;radius:2;color:view.inactiveColor;opacity:scroll.active?.6:.18}}
 }
}
