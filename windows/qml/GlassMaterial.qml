import QtQuick
import "../vendor/liquidglass/LiquidGlass" as Liquid

// Local optical surface for the playback dock, floating and tray windows.
Item {
    id: root
    property Item sourceItem
    property rect sourceRect: Qt.rect(0, 0, 0, 0)
    property bool glassEnabled: true
    property bool active: visible
    property bool motion: true
    property var configuration: ({})
    function setting(key, fallback) { const value=configuration[key];return value===undefined?fallback:value }
    property real cornerRadius: 24
    property bool dark: true
    property color backdropColor: "#181b22"
    property color tintColor: dark ? "#151b29" : "#f1f3f8"
    property real tintStrength: dark ? 0.08 : 0.18
    property color fallbackColor: dark ? "#202633" : "#f3f5fa"
    property real refraction: 1
    property real dispersion: 0.07
    property real reducedTransparency: 0
    property real increasedContrast: 0
    property bool followsMovement: false
    // Optical response is driven by real pointer interaction, never an idle timer.
    property real hoverResponse: active && motion && setting("glassMotion",true) && opticalHover.hovered ? setting("glassMotionIntensity",.35) : 0
    Behavior on hoverResponse { NumberAnimation {duration:root.motion&&root.active?160:0;easing.type:Easing.OutCubic} }
    readonly property real effectiveRadius: setting("glassRadius",cornerRadius)
    readonly property color effectiveTintSource: setting("glassTint","auto")==="auto"?tintColor:setting("glassTint","auto")
    readonly property color effectiveTint: Qt.rgba(backdropColor.r+(effectiveTintSource.r-backdropColor.r)*setting("glassTintStrength",.4),backdropColor.g+(effectiveTintSource.g-backdropColor.g)*setting("glassTintStrength",.4),backdropColor.b+(effectiveTintSource.b-backdropColor.b)*setting("glassTintStrength",.4),1)
    default property alias content: contentItem.data
    readonly property int shaderStatus: optical.shaderStatus
    readonly property string shaderLog: optical.shaderLog

    onXChanged: optical.updateSourceRect()
    onYChanged: optical.updateSourceRect()

    Rectangle {
        anchors.fill: parent
        visible: !root.glassEnabled || !root.sourceItem
        radius: root.effectiveRadius
        color: root.fallbackColor
        antialiasing: true
    }
    Liquid.GlassSurface {
        id: optical
        objectName: "opticalMaterial"
        anchors.fill: parent
        visible: root.glassEnabled && root.sourceItem !== null
        active: root.active && visible
        backdrop: root.sourceItem
        sourceRect: root.sourceRect
        backdropColor: root.backdropColor
        radius: root.effectiveRadius
        dark: root.dark
        tint: [root.effectiveTint.r, root.effectiveTint.g, root.effectiveTint.b, Math.max(0, Math.min(1, root.setting("glassOpacity",root.tintStrength)))]
        refraction: root.setting("glassRefraction",root.refraction)*(1+root.hoverResponse*.12)
        distortion: root.setting("glassDistortion",1)
        dispersion: root.setting("glassDispersion",root.dispersion)
        blurRadius: root.setting("glassBlur",2.4)
        edgeGlow: root.setting("glassEdgeGlow",.45)
        edgeWidth: root.setting("glassEdgeWidth",1.33)
        innerShadow: root.setting("glassShadow",.06)
        quality: root.setting("quality",1)
        light: Qt.point(-.35+Math.max(-.5,Math.min(.5,opticalHover.point.position.x/Math.max(root.width,1)-.5))*root.hoverResponse*.65,-.94+Math.max(-.5,Math.min(.5,opticalHover.point.position.y/Math.max(root.height,1)-.5))*root.hoverResponse*.24)
        followsMovement: root.followsMovement && root.motion && root.setting("glassMotion",true)
        reducedTransparency: root.reducedTransparency
        increasedContrast: root.increasedContrast
    }
    HoverHandler { id: opticalHover; enabled: root.active && root.glassEnabled && root.motion && root.setting("glassMotion",true) }
    Item { id: contentItem; anchors.fill: parent }
}
