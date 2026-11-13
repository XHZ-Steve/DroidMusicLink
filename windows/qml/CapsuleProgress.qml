import QtQuick
import QtQuick.Controls
import QtQuick.Shapes

// The native Slider owns input, keyboard navigation and accessibility. The
// outline is an exact pair of quarter circles and the capsule's top tangent.
Slider {
    id: control
    property real radius:42
    property color accent:"#b7d9c6"
    property color trackColor:"#38e0e5ff"
    implicitWidth:160;implicitHeight:radius+20
    padding:0
    readonly property real r:Math.max(.1,Math.min(radius,width/2)-1.5)
    readonly property real span:Math.max(1,width-3)
    readonly property real tip:Math.max(0,Math.min(span,visualPosition*span))
    function edgeY(x){const t=Math.max(0,Math.min(span,x));return 1.5+(t<r?r-Math.sqrt(Math.max(0,r*r-(t-r)*(t-r))):t>span-r?r-Math.sqrt(Math.max(0,r*r-(t-span+r)*(t-span+r))):0)}
    function fraction(x){const t=Math.max(0,Math.min(span,x));const length=Math.PI*r+span-2*r;return (t<r?r*Math.acos((r-t)/r):t>span-r?Math.PI*r/2+span-2*r+r*Math.asin((t-span+r)/r):Math.PI*r/2+t-r)/Math.max(1,length)}
    containmentMask:QtObject{function contains(p: point): bool {return p.x>=0&&p.x<=control.width&&Math.abs(p.y-control.edgeY(p.x-1.5))<=18}}
    background:Shape {
        width:control.width;height:control.r+1.5
        // Both open ends are at the bottom of this upper-half path. Fade only
        // the rendered stroke; Slider hit testing and keyboard input stay intact.
        layer.enabled:control.visible&&GraphicsInfo.api!==GraphicsInfo.Software
        layer.smooth:true
        layer.effect:ShaderEffect {
            property variant source
            property vector4d fading:Qt.vector4d(0,.72,1,0)
            fragmentShader:"qrc:/qt/qml/DroidMediaLink/shaders/lyricfade.frag.qsb"
        }
        preferredRendererType:Shape.CurveRenderer
        ShapePath {
            fillColor:"transparent";strokeColor:control.trackColor;strokeWidth:3;capStyle:ShapePath.RoundCap
            startX:1.5;startY:control.r+1.5
            PathArc{x:control.r+1.5;y:1.5;radiusX:control.r;radiusY:control.r;direction:PathArc.Clockwise}
            PathLine{x:control.span-control.r+1.5;y:1.5}
            PathArc{x:control.span+1.5;y:control.r+1.5;radiusX:control.r;radiusY:control.r;direction:PathArc.Clockwise}
        }
        ShapePath {
            fillColor:"transparent";strokeColor:control.accent;strokeWidth:3;capStyle:ShapePath.RoundCap
            trim.end:control.fraction(control.tip)
            startX:1.5;startY:control.r+1.5
            PathArc{x:control.r+1.5;y:1.5;radiusX:control.r;radiusY:control.r;direction:PathArc.Clockwise}
            PathLine{x:control.span-control.r+1.5;y:1.5}
            PathArc{x:control.span+1.5;y:control.r+1.5;radiusX:control.r;radiusY:control.r;direction:PathArc.Clockwise}
        }
    }
    handle:Item{width:0;height:0;x:1.5+control.tip;y:control.edgeY(control.tip)
        Rectangle{anchors.centerIn:parent;width:control.pressed?12:8;height:width;radius:width/2;color:control.accent;border.width:control.visualFocus?2:0;border.color:"white"}
    }
}
