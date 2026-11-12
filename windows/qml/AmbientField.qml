import QtQuick

// Analytic full-resolution colour field. No stretched cover texture or blur pass.
Item {
    id: field
    property color primary: "#6176b4"
    property color secondary: "#926b91"
    property color accent: "#b89a67"
    property color deep: "#172237"
    property bool light: false
    property bool motion: true
    property bool active: true
    property int transitionDuration: 900
    property var configuration: ({})
    function setting(key, fallback) { const value=configuration[key];return value===undefined?fallback:value }
    // Captured backdrops may be visible:false while their owning window is live.
    // The host controls active from that window, independently of source visibility.
    readonly property bool animating: active && motion && setting("backgroundDynamics",true) && setting("backgroundFlow",.35)>0
    property real phase: 0
    property color primaryColor: primary
    property color secondaryColor: secondary
    property color accentColor: accent
    property color baseColor: deep
    Behavior on primaryColor { ColorAnimation {duration:field.motion&&field.active?field.transitionDuration:0;easing.type:Easing.InOutCubic} }
    Behavior on secondaryColor { ColorAnimation {duration:field.motion&&field.active?field.transitionDuration:0;easing.type:Easing.InOutCubic} }
    Behavior on accentColor { ColorAnimation {duration:field.motion&&field.active?field.transitionDuration:0;easing.type:Easing.InOutCubic} }
    Behavior on baseColor { ColorAnimation {duration:field.motion&&field.active?field.transitionDuration:0;easing.type:Easing.InOutCubic} }
    Timer {
        interval: field.setting("quality",1)===0?66:field.setting("quality",1)===2?16:33
        running: field.animating
        repeat: true
        onTriggered: field.phase=(field.phase+interval*.001*field.setting("backgroundFlow",.35))%6283.185307
    }
    ShaderEffect {
        anchors.fill: parent
        fragmentShader: "qrc:/qt/qml/DroidMediaLink/shaders/ambient.frag.qsb"
        property color primaryColor: field.primaryColor
        property color secondaryColor: field.secondaryColor
        property color accentColor: field.accentColor
        property color deepColor: field.baseColor
        property vector4d tuning: Qt.vector4d(field.setting("backgroundStrength",1),field.setting("backgroundBrightness",1),field.setting("backgroundSaturation",1.08),field.setting("backgroundVibrance",.24))
        property vector4d atmosphere: Qt.vector4d(field.setting("backgroundOpacity",1),field.setting("backgroundDiffusion",.85),field.setting("backgroundDither",.7),field.setting("backgroundShadowProtection",.35))
        property vector4d geometry: Qt.vector4d(width,height,Screen.devicePixelRatio,field.light?1:0)
        property real phase: field.phase
    }
}
