import QtQuick

Item {
    id: cover
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
    Image {id:a;anchors.fill:parent;asynchronous:true;retainWhileLoading:true;fillMode:Image.PreserveAspectFit;visible:cover.front===0||cover.blend<1;z:cover.front===0?1:0;opacity:cover.front===0?cover.blend:1
        onStatusChanged:if(status===Image.Ready && source===cover.pending)cover.present(0)
    }
    Image {id:b;anchors.fill:parent;asynchronous:true;retainWhileLoading:true;fillMode:Image.PreserveAspectFit;visible:cover.front===1||cover.blend<1;z:cover.front===1?1:0;opacity:cover.front===1?cover.blend:1
        onStatusChanged:if(status===Image.Ready && source===cover.pending)cover.present(1)
    }
    NumberAnimation{id:fade;target:cover;property:"blend";from:0;to:1;duration:cover.transitionDuration;easing.type:Easing.InOutCubic}
    Component.onCompleted:load()
}
