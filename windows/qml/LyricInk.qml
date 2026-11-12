import QtQuick
Item {
 id:ink
 property real outlineWidth:0
 property color outlineColor:"#80000000"
 property bool shadow:false
 property real shadowBlur:6
 property real shadowOffset:2
 property color shadowColor:"#80000000"
 property bool glow:false
 property real glowRadius:8
 property real glowOpacity:.2
 property color glowColor:"#d4e7ff"
 layer.enabled:visible&&(outlineWidth>0||shadow||glow)
 layer.smooth:true
 layer.effect:ShaderEffect {
  property var source
  property vector2d texel:Qt.vector2d(1/Math.max(1,ink.width),1/Math.max(1,ink.height))
  property real stroke:ink.outlineWidth
  property vector4d strokeColor:Qt.vector4d(ink.outlineColor.r,ink.outlineColor.g,ink.outlineColor.b,ink.outlineColor.a)
  property real shade:ink.shadow?ink.shadowBlur:0
  property real shadeOffset:ink.shadow?ink.shadowOffset:0
  property vector4d shadeColor:Qt.vector4d(ink.shadowColor.r,ink.shadowColor.g,ink.shadowColor.b,ink.shadow?ink.shadowColor.a:0)
  property real halo:ink.glow?ink.glowRadius:0
  property vector4d haloColor:Qt.vector4d(ink.glowColor.r,ink.glowColor.g,ink.glowColor.b,ink.glow?ink.glowOpacity:0)
  fragmentShader:"qrc:/qt/qml/DroidMediaLink/shaders/lyricink.frag.qsb"
 }
}
