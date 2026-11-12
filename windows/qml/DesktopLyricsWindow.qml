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
 readonly property bool clickThrough:pref("desktopClickThrough",false)&&!editMode
 readonly property bool hasLyrics:controller.lyrics.length>0
 readonly property bool policyAllows:editMode || ((pref("desktopLyrics/fullscreenPolicy","show")!=="hide"||!foregroundFullscreen) && (pref("desktopLyrics/emptyPolicy","hide")!=="hide"||hasLyrics) && (pref("desktopLyrics/pausePolicy","show")!=="hide"||controller.playing))
 function toggleRequested(){requestedVisible=!requestedVisible}
 function showRequested(){requestedVisible=true}
 function hideRequested(){requestedVisible=false;editMode=false}
 visible:requestedVisible&&policyAllows
 width:800;height:200;minimumWidth:editMode?560:320
 minimumHeight:Math.max(100,(pref("desktopLyrics/fontSize",pref("lyricSize",36))*(1.4+(pref("lyrics/translation",true)?.75:0)+(pref("lyrics/romanization",false)?.65:0)))*(pref("desktopLyrics/mode","two")==="single"?1:2)+24+(editMode?52:0))
 x:200;y:600
 Component.onCompleted:{x=controller.setting("lyricsX",200);y=controller.setting("lyricsY",600);positionLoaded=true}
 color:"transparent";title:qsTr("Desktop lyrics")
 flags:Qt.Tool|Qt.FramelessWindowHint|Qt.NoDropShadowWindowHint|(pref("overlayTopmost",true)?Qt.WindowStaysOnTopHint:0)|(clickThrough?Qt.WindowTransparentForInput:0)
 opacity:Math.max(.1,Math.min(1,pref("desktopLyrics/opacity",1)))
 onClosing:function(event){event.accepted=false;hideRequested()}
 onXChanged:if(visible&&positionLoaded)savePosition.restart()
 onYChanged:if(visible&&positionLoaded)savePosition.restart()
 Timer{id:savePosition;interval:350;onTriggered:{window.controller.setSetting("lyricsX",window.x);window.controller.setSetting("lyricsY",window.y)}}
 Loader{anchors.fill:parent;active:window.visible&&window.pref("desktopLyrics/background",false)
  sourceComponent:Item{
   AmbientField{id:desktopAmbient;anchors.fill:parent;visible:false;primary:window.controller.ambientPrimary;secondary:window.controller.ambientSecondary;accent:window.controller.ambientAccent;deep:window.controller.ambientDeep;motion:false;active:window.visible;configuration:{window.settingsRevision;return window.controller.appearanceConfig("background")}}
   GlassMaterial{anchors.fill:parent;sourceItem:desktopAmbient;active:window.visible;configuration:{window.settingsRevision;return window.controller.appearanceConfig("desktop")}
    glassEnabled:window.pref("appearance/style","liquid")==="liquid";motion:window.pref("motion",true);dark:true;fallbackColor:"#99222a35"}
  }
 }
 Rectangle {anchors.fill:parent;visible:window.editMode;color:"#db1b2029";radius:14;border.width:1;border.color:"#90aebfc6"}
 component EditButton:Button {
  id:button;implicitHeight:36;leftPadding:12;rightPadding:12;hoverEnabled:true;Accessible.name:text
  background:Rectangle{radius:8;color:button.down?"#465362":button.hovered?"#35414d":"#29333e";border.width:button.visualFocus?2:0;border.color:"#b8d5df"}
  contentItem:Text{text:button.text;color:"#eef2f5";font.family:"Microsoft YaHei UI";font.pixelSize:13;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter}
 }
 HoverHandler{id:hover;enabled:!window.clickThrough;onHoveredChanged:if(hovered){hoverExit.stop();window.editMode=true}else hoverExit.restart()}
 Timer{id:hoverExit;interval:1200;onTriggered:if(!hover.hovered)window.editMode=false}
 MouseArea {
  anchors.fill:parent;enabled:!window.clickThrough;acceptedButtons:Qt.LeftButton|Qt.RightButton
  onPressed:function(mouse){if(mouse.button===Qt.RightButton)window.editMode=true;else window.startSystemMove()}
  onDoubleClicked:window.editMode=!window.editMode
 }
 LyricView {
  anchors.fill:parent;anchors.leftMargin:20;anchors.rightMargin:20;anchors.topMargin:window.editMode?52:12;anchors.bottomMargin:12
  controller:window.controller;compact:true;
  scrollDuration:window.pref("lyrics/scrollDuration",420);easingCurve:window.pref("lyrics/easing","outCubic");lineSpacing:window.pref("lyrics/lineSpacing",.8);activeScale:window.pref("lyrics/activeScale",1.04);inactiveOpacity:window.pref("lyrics/inactiveOpacity",.58);highlightMode:window.pref("lyrics/highlight","word");compactLines:window.pref("desktopLyrics/mode","two")==="single"?1:2
  fontFamily:window.pref("desktopLyrics/fontFamily","Microsoft YaHei UI");fontSize:window.pref("desktopLyrics/fontSize",window.pref("lyricSize",36))
  textColor:window.pref("desktopLyrics/color","#fff6e9");inactiveColor:window.pref("desktopLyrics/inactiveColor","#b3fff6e9")
  outlineWidth:window.pref("desktopLyrics/outlineWidth",1);outlineColor:window.pref("desktopLyrics/outlineColor","#99000000");shadow:window.pref("desktopLyrics/shadow",true)
  showTranslation:window.pref("lyrics/translation",true);showRomanization:window.pref("lyrics/romanization",false);motion:window.pref("motion",true)
 }
 Text {anchors.centerIn:parent;visible:window.editMode&&!window.hasLyrics;text:qsTr("No lyrics");font.pixelSize:24;color:"#ccd4dc"}
 RowLayout {
  visible:window.editMode;anchors.top:parent.top;anchors.topMargin:8;anchors.left:parent.left;anchors.right:parent.right;anchors.margins:10;spacing:8
  Label{text:qsTr("Desktop lyrics");color:"#eef2f5";Layout.fillWidth:true}
  SpinBox {from:18;to:96;value:window.pref("desktopLyrics/fontSize",36);editable:true;Accessible.name:qsTr("Font size");onValueModified:window.controller.setSetting("desktopLyrics/fontSize",value)}
  EditButton {text:qsTr("Lock lyrics");onClicked:{window.controller.setSetting("desktopClickThrough",true);window.editMode=false}}
  EditButton {text:qsTr("Done");onClicked:window.editMode=false}
  EditButton {text:qsTr("Close");onClicked:window.hideRequested()}
 }
 MouseArea {visible:window.editMode;anchors.right:parent.right;anchors.bottom:parent.bottom;width:20;height:20;cursorShape:Qt.SizeFDiagCursor;onPressed:window.startSystemResize(Qt.RightEdge|Qt.BottomEdge)
  Text{anchors.centerIn:parent;text:"◢";color:"#b0c0ca"}
 }
 Shortcut{sequence:"Escape";enabled:window.visible&&window.editMode;onActivated:window.editMode=false}
 Shortcut{sequence:"Ctrl+L";enabled:window.visible&&!window.clickThrough;onActivated:window.controller.setSetting("desktopClickThrough",!window.pref("desktopClickThrough",false))}
}
