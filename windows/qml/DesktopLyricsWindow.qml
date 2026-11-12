pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Window {
 id:window;objectName:"desktop";transientParent:null
 required property var controller
 property int settingsRevision:0
 function pref(key,fallback){var revision=settingsRevision;return controller.setting(key,fallback)}
 property bool foregroundFullscreen:false
 property bool requestedVisible:false
 property bool editMode:false
 property bool positionLoaded:false
 property bool hoverLatched:false
 readonly property bool controlsVisible:editMode||hoverLatched
 readonly property bool clickThrough:pref("desktopClickThrough",false)&&!editMode
 readonly property bool hasLyrics:controller.lyrics.length>0
 readonly property bool policyAllows:editMode||((pref("desktopLyrics/fullscreenPolicy","show")!=="hide"||!foregroundFullscreen)&&(pref("desktopLyrics/emptyPolicy","hide")!=="hide"||hasLyrics)&&(pref("desktopLyrics/pausePolicy","show")!=="hide"||controller.playing))
 function toggleRequested(){requestedVisible=!requestedVisible}
 function showRequested(){requestedVisible=true;Qt.callLater(clampPosition)}
 function hideRequested(){requestedVisible=false;editMode=false;hoverLatched=false}
 function editRequested(){requestedVisible=true;editMode=true;Qt.callLater(function(){window.clampPosition();window.raise();window.requestActivate()})}
 function clampPosition(){
  if(!visible)return
  const screens=Qt.application.screens;if(!screens||screens.length===0)return
  var best=screens[0],distance=1e20,cx=x+width/2,cy=y+height/2
  for(var i=0;i<screens.length;i++){const s=screens[i];const dx=Math.max(s.virtualX-cx,0,cx-s.virtualX-s.width),dy=Math.max(s.virtualY-cy,0,cy-s.virtualY-s.height);const d=dx*dx+dy*dy;if(d<distance){distance=d;best=s}}
  width=Math.min(width,Math.max(minimumWidth,best.width-24));height=Math.min(height,Math.max(minimumHeight,best.height-64))
  x=Math.max(best.virtualX+8,Math.min(best.virtualX+best.width-width-8,x));y=Math.max(best.virtualY+8,Math.min(best.virtualY+best.height-height-48,y))
 }
 visible:requestedVisible&&policyAllows;width:800;height:260;minimumWidth:280;minimumHeight:100;x:200;y:600
 Component.onCompleted:{x=controller.setting("lyricsX",200);y=controller.setting("lyricsY",600);width=controller.setting("desktopLyrics/windowWidth",800);height=controller.setting("desktopLyrics/windowHeight",260);positionLoaded=true;Qt.callLater(clampPosition)}
 color:"transparent";title:qsTr("Desktop lyrics")
 flags:Qt.Tool|Qt.FramelessWindowHint|Qt.NoDropShadowWindowHint|(pref("overlayTopmost",true)?Qt.WindowStaysOnTopHint:0)|(clickThrough?Qt.WindowTransparentForInput:0)
 opacity:Math.max(.1,Math.min(1,pref("desktopLyrics/opacity",1)))
 onClosing:function(event){event.accepted=false;hideRequested()}
 onVisibleChanged:{if(visible)Qt.callLater(clampPosition);else{hoverLatched=false;hoverExit.stop()}}
 onScreenChanged:if(positionLoaded)Qt.callLater(clampPosition)
 onXChanged:if(visible&&positionLoaded)saveGeometry.restart()
 onYChanged:if(visible&&positionLoaded)saveGeometry.restart()
 onWidthChanged:if(visible&&positionLoaded)saveGeometry.restart()
 onHeightChanged:if(visible&&positionLoaded)saveGeometry.restart()
 Timer{id:saveGeometry;interval:350;onTriggered:{window.controller.setSetting("lyricsX",window.x);window.controller.setSetting("lyricsY",window.y);window.controller.setSetting("desktopLyrics/windowWidth",window.width);window.controller.setSetting("desktopLyrics/windowHeight",window.height)}}
 Loader{anchors.fill:parent;active:window.visible&&window.pref("desktopLyrics/background",false)
  sourceComponent:Item{
   AmbientField{id:ambient;anchors.fill:parent;visible:false;primary:window.controller.ambientPrimary;secondary:window.controller.ambientSecondary;accent:window.controller.ambientAccent;deep:window.controller.ambientDeep;motion:false;active:window.visible;configuration:window.controller.appearanceConfig("background")}
   GlassMaterial{anchors.fill:parent;sourceItem:ambient;active:window.visible;configuration:window.controller.appearanceConfig("desktop");glassEnabled:window.pref("appearance/style","liquid")==="liquid";motion:window.pref("motion",true);dark:true;fallbackColor:"#66222a35"}
  }
 }
 MouseArea {anchors.fill:parent;enabled:!window.clickThrough;acceptedButtons:Qt.LeftButton|Qt.RightButton;onPressed:function(mouse){if(mouse.button===Qt.RightButton)window.editRequested();else window.startSystemMove()};onDoubleClicked:window.editMode=!window.editMode}
 // This body never changes geometry when editing controls appear.
 Loader {id:bodyLoader;objectName:"desktopLyricBody";anchors.fill:parent;anchors.margins:12;active:window.visible
  sourceComponent:LyricView {
   controller:window.controller;settingsRevision:window.settingsRevision;settingsPrefix:"desktopLyrics";compact:true
   compactLines:window.pref("desktopLyrics/mode","two")==="single"||window.height<fontSize*4.5?1:2
   textColor:"#fff8f5";inactiveColor:"#bdc5d5";centered:true;motion:window.pref("motion",true)
   onSeekRequested:function(position){window.controller.command("seek",position)}
  }
 }
 Text {anchors.centerIn:parent;visible:window.editMode&&!window.hasLyrics;text:qsTr("No lyrics");font.family:"Segoe UI Variable Text";font.pixelSize:24;color:"#ccd4dc";renderType:Text.CurveRendering}
 HoverHandler{id:hover;enabled:!window.clickThrough;onHoveredChanged:{if(hovered){hoverExit.stop();window.hoverLatched=true}else hoverExit.restart()}}
 Timer{id:hoverExit;interval:500;onTriggered:window.hoverLatched=false}
 Rectangle {anchors.fill:parent;radius:16;color:"transparent";border.width:1;border.color:"#7099afc5";opacity:window.controlsVisible?1:0;Behavior on opacity{NumberAnimation{duration:160}}}
 component EditButton:Button {
  id:button;implicitHeight:32;implicitWidth:34;leftPadding:8;rightPadding:8;hoverEnabled:true;Accessible.name:text
  background:Rectangle{radius:8;color:button.down?"#35d4e4fa":button.hovered?"#20d4e4fa":"transparent";border.width:button.visualFocus?2:0;border.color:"#b8d5df"}
  contentItem:Text{text:button.text;color:"#eef2f5";font.family:"Segoe UI Variable Text";font.pixelSize:14;font.weight:500;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter;renderType:Text.NativeRendering}
 }
 Item {id:bar;anchors.horizontalCenter:parent.horizontalCenter;y:6;width:Math.min(window.width-24,400);height:42;z:10
  visible:opacity>0;opacity:window.controlsVisible?1:0;Behavior on opacity{NumberAnimation{duration:160}}
  Rectangle{id:barBackdrop;anchors.fill:parent;visible:false;gradient:Gradient{GradientStop{position:0;color:"#344354"}GradientStop{position:1;color:"#1b2637"}}}
  GlassMaterial {anchors.fill:parent;sourceItem:barBackdrop;cornerRadius:13;dark:true;active:window.visible&&bar.visible;glassEnabled:window.pref("appearance/style","liquid")==="liquid";tintColor:"#233044";tintStrength:.25;fallbackColor:"#db253144"}
  RowLayout {anchors.fill:parent;anchors.margins:4;spacing:4
   EditButton{text:"-";Accessible.name:qsTr("Decrease font size");onClicked:window.controller.setSetting("desktopLyrics/fontSize",Math.max(16,window.pref("desktopLyrics/fontSize",36)-2))}
   Label{text:String(window.pref("desktopLyrics/fontSize",36));color:"#eef2f5";horizontalAlignment:Text.AlignHCenter;Layout.preferredWidth:28;font.pixelSize:13}
   EditButton{text:"+";Accessible.name:qsTr("Increase font size");onClicked:window.controller.setSetting("desktopLyrics/fontSize",Math.min(96,window.pref("desktopLyrics/fontSize",36)+2))}
   Item{Layout.fillWidth:true}
   EditButton{text:qsTr("Lock");onClicked:{window.controller.setSetting("desktopClickThrough",true);window.editMode=false;window.hoverLatched=false}}
   EditButton{text:qsTr("Done");onClicked:{window.editMode=false;window.hoverLatched=false}}
   EditButton{text:"×";Accessible.name:qsTr("Close lyrics");onClicked:window.hideRequested()}
  }
 }
 Repeater {model:[Qt.LeftEdge,Qt.RightEdge,Qt.TopEdge,Qt.BottomEdge,Qt.LeftEdge|Qt.TopEdge,Qt.RightEdge|Qt.TopEdge,Qt.LeftEdge|Qt.BottomEdge,Qt.RightEdge|Qt.BottomEdge]
  MouseArea {
   required property int modelData
   readonly property bool atLeft:(modelData&Qt.LeftEdge)!==0
   readonly property bool atRight:(modelData&Qt.RightEdge)!==0
   readonly property bool atTop:(modelData&Qt.TopEdge)!==0
   readonly property bool atBottom:(modelData&Qt.BottomEdge)!==0
   readonly property int grip:8
   x:atLeft?0:atRight?window.width-grip:grip;y:atTop?0:atBottom?window.height-grip:grip
   width:atLeft||atRight?grip:window.width-grip*2;height:atTop||atBottom?grip:window.height-grip*2
   enabled:window.controlsVisible&&!window.clickThrough;z:20
   cursorShape:(atLeft||atRight)&&(atTop||atBottom)?(atLeft===atTop?Qt.SizeFDiagCursor:Qt.SizeBDiagCursor):atLeft||atRight?Qt.SizeHorCursor:Qt.SizeVerCursor
   onPressed:window.startSystemResize(modelData)
  }
 }
 Shortcut{sequence:"Escape";enabled:window.visible&&window.controlsVisible;onActivated:{window.editMode=false;window.hoverLatched=false}}
 Shortcut{sequence:"Ctrl+L";enabled:window.visible&&!window.clickThrough;onActivated:{const lock=!window.pref("desktopClickThrough",false);window.controller.setSetting("desktopClickThrough",lock);window.editMode=!lock}}
}
