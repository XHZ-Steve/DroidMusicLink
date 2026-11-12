import QtQuick
import QtQuick.Controls
import QtQuick.Effects

Item {
    id: cover
    signal activated()
    property string actionText:qsTr("Switch layout")
    property color ambientColor:"#527a9c"
    property url source
    property bool motion: true
    property int transitionDuration:340
    property int front: 0
    property url pending
    property real blend:1
    readonly property bool ready:a.status===Image.Ready||b.status===Image.Ready
    function present(index) {
        if(front===index)return
        fade.stop();front=index;blend=motion?0:1
        if(motion)fade.start()
    }
    function load() {
        if(!source.toString().length) {a.source="";b.source="";return}
        var current=front===0?a:b
        if(current.source.toString()===source.toString())return
        pending=source
        var next=front===0?b:a
        if(next.source===source && next.status===Image.Ready)present(front===0?1:0)
        else next.source=source
    }
    onSourceChanged:load()
    // Keep the outgoing image opaque underneath; opposing alpha animations
    // would expose the background and darken the cover halfway through a fade.
    Item{id:artworkContent;anchors.fill:parent;visible:false
    Image {id:a;anchors.fill:parent;asynchronous:true;retainWhileLoading:true;fillMode:Image.PreserveAspectFit;visible:cover.front===0||cover.blend<1;z:cover.front===0?1:0;opacity:cover.front===0?cover.blend:1
        onStatusChanged:if(status===Image.Ready && source===cover.pending)cover.present(0)
    }
    Image {id:b;anchors.fill:parent;asynchronous:true;retainWhileLoading:true;fillMode:Image.PreserveAspectFit;visible:cover.front===1||cover.blend<1;z:cover.front===1?1:0;opacity:cover.front===1?cover.blend:1
        onStatusChanged:if(status===Image.Ready && source===cover.pending)cover.present(1)
    }
    }
    MultiEffect {anchors.fill:parent;source:artworkContent;visible:cover.ready;shadowEnabled:true;shadowColor:Qt.rgba(cover.ambientColor.r*.32,cover.ambientColor.g*.32,cover.ambientColor.b*.32,.34);shadowBlur:.8;shadowVerticalOffset:12;shadowHorizontalOffset:0;shadowScale:1.025;autoPaddingEnabled:true;blurMax:32}
    Text{anchors.centerIn:parent;visible:!cover.ready;text:"♪";font.family:"Segoe UI";font.pixelSize:Math.max(40,Math.min(80,cover.width*.2));color:cover.ambientColor;opacity:.5}
    Button{objectName:"coverLayoutSwitch";anchors.fill:parent;text:cover.actionText;Accessible.name:text;hoverEnabled:true;background:Rectangle{color:"transparent";radius:4;border.width:parent.visualFocus?2:0;border.color:cover.ambientColor}contentItem:Item{}
 onClicked:cover.activated();ToolTip.visible:hovered;ToolTip.delay:900;ToolTip.text:text}
    NumberAnimation{id:fade;target:cover;property:"blend";from:0;to:1;duration:cover.transitionDuration;easing.type:Easing.InOutCubic}
    Component.onCompleted:load()
}
