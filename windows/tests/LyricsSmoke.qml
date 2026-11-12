pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import "../qml" as UI
ApplicationWindow {
 id:root;width:1080;height:760;visible:true;color:"#152027";title:"DroidMediaLink lyrics lifecycle TEST"
 UI.LyricView {anchors.fill:parent;anchors.margins:40;controller:fixture;fontSize:32}
 UI.DesktopLyricsWindow {id:desktop;controller:fixture;requestedVisible:true;x:160;y:180}
 Timer {interval:120;running:true;repeat:true;property int iterations:0;onTriggered:{iterations++;desktop.requestedVisible=iterations%3!==0;desktop.editMode=iterations%7===0;if(iterations%10===0)fixture.reload();if(iterations>=300){stop();desktop.requestedVisible=true;capture.start()}}}
 Timer{id:capture;interval:300;onTriggered:fixture.finish(root,desktop)}
}
