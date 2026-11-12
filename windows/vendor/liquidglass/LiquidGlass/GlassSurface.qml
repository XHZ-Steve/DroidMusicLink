// Apache-2.0. A rounded piece of Liquid Glass with children on top. It refracts `backdrop` live, every
// frame: a ShaderEffectSource renders the part of the backdrop under the glass, and GlassEffect runs the
// production material over it. Put it over the backdrop (siblings in the same parent, say), never inside it.
import QtQuick
import QtQml.Models
import "material.js" as Material

Item {
    id: root

    /** The item the glass refracts: the page, a ListView, an Image. Keep the glass outside it. */
    // DroidMediaLink adapter: optional explicit backdrop coordinates and lifecycle controls.
    property rect sourceRect: Qt.rect(0, 0, 0, 0)
    property bool active: true
    property real refraction: 1
    property real distortion: 1
    property real dispersion: 0.07
    // DroidMediaLink exposes the existing upstream material uniforms, in dp.
    property real blurRadius: 2.4
    property real edgeGlow: 0.45
    property real edgeWidth: 1.33
    property real innerShadow: 0.06
    property int quality: 1
    property Item backdrop
    /** What the glass sees where the backdrop is transparent (a ListView's empty margins, say): the
        window's colour, usually. The material needs an opaque backdrop. */
    property color backdropColor: "white"
    /** Corner radius, logical pixels. */
    property real radius: 20
    /** The dark material. */
    property bool dark: false
    /** 0 to 100. */
    property real tintAmount: 50
    /** Toward the light (y up is negative); Qt.point(0, -1) is iOS 27's measured overhead light. */
    property point light: Qt.point(-0.35, -0.94)
    /** [r, g, b, a] in sRGB 0..1 replacing the tint colour and opacity, or null. */
    property var tint: null
    /** 0 to 1: glass materializes in and out rather than fading. */
    property real materialize: 1
    /** 0 to 1, the accessibility settings. */
    property real reducedTransparency: 0
    property real increasedContrast: 0
    /** Re-map the glass onto the backdrop every frame, for glass or a backdrop that moves without
        changing its geometry properties (inside an animated parent, say). A backdrop that scrolls its
        own content, like a ListView, needs none of this: the glass stays where it is over it. */
    property bool followsMovement: false
    /** Children are drawn on top of the glass. */
    default property alias content: contentItem.data
    /** The shader's status (ShaderEffect.Compiled 0, Uncompiled 1, Error 2) and its log, for diagnostics.
        An int, so hosts that cannot convert the enum (PySide) can read it. */
    readonly property int shaderStatus: effect.status
    readonly property string shaderLog: effect.log
    readonly property size captureTextureSize: backdropTexture.textureSize
    readonly property rect capturedSourceRect: backdropTexture.sourceRect
    readonly property point materialSize: effect.uSize

    // Two device pixels around the glass for the outer contour, as in every port.
    readonly property real dpr: Screen.devicePixelRatio > 0 ? Screen.devicePixelRatio : 1
    readonly property real padUnits: 2 / dpr
    readonly property int deviceWidth: Math.max(1, Math.ceil(width * dpr))
    readonly property int deviceHeight: Math.max(1, Math.ceil(height * dpr))
    readonly property int textureWidth: deviceWidth + 4
    readonly property int textureHeight: deviceHeight + 4
    // mapFromItem is an imperative mapping; it does not observe ancestor movement.
    // Watch the two geometry chains instead of forcing continuous idle frames.
    readonly property var geometryItems: {
        const items = []
        let item = root.parent
        while (item) { if (items.indexOf(item) < 0) items.push(item); item = item.parent }
        item = root.backdrop
        while (item) { if (items.indexOf(item) < 0) items.push(item); item = item.parent }
        return items
    }

    function updateSourceRect() {
        if (!backdrop) return
        const r = sourceRect.width > 0 && sourceRect.height > 0
            ? Qt.rect(sourceRect.x - padUnits, sourceRect.y - padUnits, sourceRect.width + 2 * padUnits, sourceRect.height + 2 * padUnits)
            : backdrop.mapFromItem(root, -padUnits, -padUnits, textureWidth / dpr, textureHeight / dpr)
        if (r.x !== backdropTexture.sourceRect.x || r.y !== backdropTexture.sourceRect.y ||
                r.width !== backdropTexture.sourceRect.width || r.height !== backdropTexture.sourceRect.height)
            backdropTexture.sourceRect = r
    }

    function updateUniforms() {
        if (width <= 0 || height <= 0) return
        const values = Material.uniforms({
            radius: radius, dark: dark, tintAmount: tintAmount, light: [light.x, light.y], tint: tint,
            materialize: materialize, reducedTransparency: reducedTransparency, increasedContrast: increasedContrast,
        }, deviceWidth, deviceHeight, dpr)
        values.uBase = [backdropColor.r, backdropColor.g, backdropColor.b]
        values.uRefractDepth[0] *= Math.max(0, refraction)
        values.uRefractBand[0] *= Math.max(0, distortion)
        values.uAberration = [Math.max(0, dispersion)]
        // Product tuning of the upstream optics, not a replacement shader. Keep
        // the colored scene visible instead of lifting it into a grey substrate.
        values.uBlur = [Math.max(0, blurRadius) * dpr]
        values.uWideKernel = [Math.max(0, blurRadius) * 1.25 * dpr]
        values.uFineShare = [0.82]
        // The upstream wide-tone path adds a separate kernel and axis average.
        // Low quality keeps the real lens/fine blur while skipping that pass.
        values.uWideStrip = [quality === 0 ? 0 : 1]
        values.uSoftDetail = [1]
        values.uLegibility = [0.12]
        values.uLift = [dark ? 0.012 : 0]
        values.uCounterLight = [0.28]
        values.uBevel = [Math.max(.3, edgeWidth) * dpr]
        values.uInnerShadow = [Math.max(0, innerShadow)]
        values.uSpecular = [Math.max(0, edgeGlow) * .4]
        values.uEdgeLight = [Math.max(0, edgeGlow)]
        values.uFresnel = [Math.max(0, edgeGlow) * .3555556]
        effect.setUniforms(values)
    }

    ShaderEffectSource {
        id: backdropTexture
        sourceItem: root.backdrop
        live: root.active && root.visible
        hideSource: false
        visible: false
        smooth: true
        textureSize: Qt.size(root.textureWidth, root.textureHeight)
        wrapMode: ShaderEffectSource.ClampToEdge
    }

    GlassEffect {
        id: effect
        x: -root.padUnits
        y: -root.padUnits
        width: root.textureWidth / root.dpr
        height: root.textureHeight / root.dpr
        source: backdropTexture
        field: backdropTexture   // the shape is analytic; the field is bound but unread
        uLayerSize: Qt.point(root.textureWidth, root.textureHeight)
    }

    Item {
        id: contentItem
        anchors.fill: parent
    }

    FrameAnimation {
        running: root.active && root.followsMovement && root.visible && root.backdrop !== null
        onTriggered: root.updateSourceRect()
    }
    onSourceRectChanged: updateSourceRect()
    onRefractionChanged: updateUniforms()
    onDistortionChanged: updateUniforms()
    onDispersionChanged: updateUniforms()
    onBlurRadiusChanged: updateUniforms()
    onEdgeGlowChanged: updateUniforms()
    onEdgeWidthChanged: updateUniforms()
    onInnerShadowChanged: updateUniforms()
    onQualityChanged: updateUniforms()
    onXChanged: updateSourceRect()
    onYChanged: updateSourceRect()
    Instantiator {
        model: root.geometryItems
        delegate: Connections {
            required property var modelData
            target: modelData
            ignoreUnknownSignals: true
            function onXChanged() { root.updateSourceRect() }
            function onYChanged() { root.updateSourceRect() }
            function onWidthChanged() { root.updateSourceRect() }
            function onHeightChanged() { root.updateSourceRect() }
            function onRotationChanged() { root.updateSourceRect() }
            function onScaleChanged() { root.updateSourceRect() }
        }
    }

    // Geometry, material uniforms and FBO size must reach the same render sync.
    // Deferring only the uniforms via callLater can place old-sized optical
    // coverage over a newly resized background during continuous resizing.
    onDeviceWidthChanged: { updateUniforms(); updateSourceRect() }
    onDeviceHeightChanged: { updateUniforms(); updateSourceRect() }
    onRadiusChanged: updateUniforms()
    onDarkChanged: updateUniforms()
    onTintAmountChanged: updateUniforms()
    onLightChanged: updateUniforms()
    onTintChanged: updateUniforms()
    onMaterializeChanged: updateUniforms()
    onReducedTransparencyChanged: updateUniforms()
    onIncreasedContrastChanged: updateUniforms()
    onBackdropColorChanged: updateUniforms()
    onDprChanged: { updateUniforms(); updateSourceRect() }
    onBackdropChanged: { updateSourceRect(); if (active) backdropTexture.scheduleUpdate() }
    onActiveChanged: { if (active) { updateUniforms(); updateSourceRect(); backdropTexture.scheduleUpdate() } }
    Component.onCompleted: { updateUniforms(); updateSourceRect() }
}
