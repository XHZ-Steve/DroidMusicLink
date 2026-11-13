pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: canvas
    objectName:"nowPlayingCanvas"
    required property var controller
    property int settingsRevision: 0
    property bool light: false
    property bool motion: true
    property bool lyricsEnabled: true
    property color textColor: "#f5f5fa"
    property color secondaryColor: "#bdc5d5"
    property color accent: "#b7d9c6"
    property bool ready: false
    property bool changingLayout: false
    readonly property int mode: {settingsRevision;return controller.setting("home/layout",0)}
    readonly property real artworkScale:{settingsRevision;return Math.max(.55,Math.min(1.35,controller.setting("home/artworkScale",1)))}
    readonly property real artworkShare:Math.min(.6,.46+Math.max(0,artworkScale-1)*.4)
    readonly property bool split: mode!==0 && width>780 && width/Math.max(1,height)>1.12
    readonly property bool lyricsMode: mode===2
    readonly property bool displayingLyrics: lyricsEnabled && mode!==1
    readonly property string trackIdentity:controller.title+"\u0000"+controller.artist+"\u0000"+controller.appName
    readonly property bool active: visible && Window.window && Window.window.visible && Window.window.visibility!==Window.Minimized
    signal connectRequested()
    signal matchRequested()
    signal seekRequested(real position)
    property real lyricFocus:0
    property real transitionProgress: 1
    property real textAlignment: split?1:0
    property var fromGeometry: ({})
    property var toGeometry: ({})
    property int transitionGeneration: 0
    function rectOf(item){return Qt.rect(item.x,item.y,item.width,item.height)}
    function formatMix(a,b){return a+(b-a)*(1-Math.pow(1-transitionProgress,3))}
    function protectLyricY(y){return Math.max(y,metadata.y+metadata.height+14-homeLyrics.y)}
    function mix(a,b){return a+(b-a)*transitionProgress}
    function geometry(name,target){
        if(!changingLayout)return target
        const a=fromGeometry[name],b=toGeometry[name]||a
        if(name==="cover"&&Math.abs(b.x-a.x)>1){
            // Clear the text's path before enlarging the artwork; reverse the
            // trajectory when returning. Both axes still share one clock.
            const p=transitionProgress,outward=b.x<a.x
            const travel=outward?1-Math.pow(1-p,6):Math.pow(p,6)
            const extent=b.width<a.width?1-Math.pow(1-p,3):p*p
            return Qt.rect(a.x+(b.x-a.x)*travel,a.y+(b.y-a.y)*(outward?p*p*p:p),a.width+(b.width-a.width)*extent,a.height+(b.height-a.height)*extent)
        }
        let vertical=transitionProgress
        if(name==="metadata"&&Math.abs(toGeometry.cover.x-fromGeometry.cover.x)>1)
            vertical=toGeometry.cover.x<fromGeometry.cover.x?Math.pow(vertical,3):1-Math.pow(1-vertical,3)
        let y=a.y+(b.y-a.y)*vertical
        if(name==="metadata"){
            const cover=geometry("cover",coverTarget)
            const titleX=fromGeometry.titleX+(toGeometry.titleX-fromGeometry.titleX)*horizontalProgress()
            const clearance=Math.max(0,Math.min(1,(titleX-cover.x-cover.width)/32))
            const released=clearance*clearance*(3-2*clearance)
            y+=(Math.max(y,cover.y+cover.height+14)-y)*(1-released)
        }
        return Qt.rect(mix(a.x,b.x),y,mix(a.width,b.width),mix(a.height,b.height))
    }
    function freezeGeometry(){
        const current={cover:rectOf(artwork),metadata:rectOf(metadata),lyrics:homeLyrics.opacity>.001?rectOf(homeLyrics):Qt.rect(metadata.x,Math.max(metadata.y+metadata.height+16,artwork.y+artwork.height+14),metadata.width,76),expansion:homeLyrics.expansion,primary:expandedLyrics.primaryRect,tokens:expandedLyrics.primaryTokens(),expandedSplit:homeLyrics.expandedSplit,alignment:textAlignment,titleX:metadata.x+titleLabel.x,titleHeight:titleLabel.height,artistX:metadata.x+artistLabel.x,sourceX:metadata.x+sourceLabel.x,lyricOpacity:homeLyrics.opacity}
        layoutMotion.stop();fromGeometry=current;toGeometry=current
        transitionProgress=0;changingLayout=true
    }
    function scheduleTransition(){
        const generation=++transitionGeneration
        // Layout is polished first; every surface then starts on the same clock.
        Qt.callLater(function(){
            if(generation!==canvas.transitionGeneration)return
            slots.ensurePolished()
            toGeometry={cover:coverTarget,metadata:metadataTarget,lyrics:lyricTarget,expansion:lyricsMode?1:0,expandedSplit:split,alignment:split?1:0,titleX:textTargetX(titleLabel),titleHeight:titleMeasure.implicitHeight,artistX:textTargetX(artistLabel),sourceX:textTargetX(sourceLabel),lyricOpacity:displayingLyrics&&controller.lyrics.length?1:0}
            if(motion)layoutMotion.start();else{transitionProgress=1;changingLayout=false}
        })
    }
    function textTargetX(label){return metadataTarget.x+(metadataTarget.width-Math.min(label.implicitWidth,metadataTarget.width))*(split?0:.5)}
    function horizontalProgress(){const p=transitionProgress;return Math.abs(toGeometry.cover.x-fromGeometry.cover.x)>1?(toGeometry.cover.x<fromGeometry.cover.x?1-Math.pow(1-p,6):Math.pow(p,6)):p}
    function textX(label,key){
        if(!changingLayout)return (metadata.width-label.width)*(1-textAlignment)/2
        const p=horizontalProgress()
        return fromGeometry[key]+(toGeometry[key]-fromGeometry[key])*p-metadata.x
    }
    function beginNextLayout(){freezeGeometry();controller.setSetting("home/layout",(mode+1)%3)}
    function nextLayout(){
        if(focusMotion.running)return
        if(lyricsMode&&controller.lyrics.length&&motion&&active&&!changingLayout){focusMotion.start();return}
        beginNextLayout()
    }
    NumberAnimation{id:focusMotion;target:canvas;property:"lyricFocus";from:0;to:1;duration:95;easing.type:Easing.InOutCubic;onFinished:canvas.beginNextLayout()}
    onModeChanged:if(ready){if(!active)cancelTransitionForResize();else{if(!changingLayout)freezeGeometry();scheduleTransition()}}
    onArtworkScaleChanged:cancelTransitionForResize()
    onWidthChanged:cancelTransitionForResize()
    onHeightChanged:cancelTransitionForResize()
    onActiveChanged:if(!active)cancelTransitionForResize()
    onTrackIdentityChanged:if(ready)cancelTransitionForResize()
    function cancelTransitionForResize(){if(!ready)return;++transitionGeneration;focusMotion.stop();layoutMotion.stop();lyricFocus=0;transitionProgress=1;changingLayout=false;Qt.callLater(polishLayout)}
    function polishLayout(){slots.ensurePolished()}
    Component.onCompleted:ready=true
    NumberAnimation{id:layoutMotion;target:canvas;property:"transitionProgress";from:0;to:1;duration:320;easing.type:Easing.InOutCubic;onFinished:{canvas.changingLayout=false;canvas.lyricFocus=0}}
    Binding{target:canvas;property:"textAlignment";value:canvas.changingLayout?canvas.mix(canvas.fromGeometry.alignment,canvas.toGeometry.alignment):canvas.split?1:0}
    TypographyTokens{id:typography}
    DesignTokens{id:tokens;light:canvas.light;motion:canvas.motion;accent:canvas.accent}
    GridLayout {
        id: slots;anchors.fill:parent;anchors.topMargin:canvas.split&&canvas.lyricsMode?Math.min(40,canvas.height*.07):0;anchors.bottomMargin:anchors.topMargin;columns:canvas.split?2:1;rowSpacing:canvas.split?24:14;columnSpacing:Math.max(32,Math.min(72,canvas.width*.065))
        Item {
            id: coverSlot
            Layout.row:0;Layout.column:0;Layout.rowSpan:canvas.split?2:1
            Layout.fillWidth:true;Layout.fillHeight:canvas.split||!canvas.lyricsMode
            Layout.preferredWidth:canvas.split?(slots.width-slots.columnSpacing)*canvas.artworkShare:slots.width
            Layout.preferredHeight:canvas.split?slots.height:canvas.lyricsMode?Math.min(180,slots.height*.32):Math.max(110,slots.height-metadata.implicitHeight-(canvas.displayingLyrics?74:0)-20)
            Layout.minimumHeight:90
        }
        Item {
            id: metadataSlot
            Layout.topMargin:canvas.split&&canvas.lyricsMode?28:0
            Layout.row:canvas.split?0:1;Layout.column:canvas.split?1:0
            Layout.fillWidth:true;Layout.fillHeight:canvas.split&&!canvas.lyricsMode
            Layout.preferredWidth:canvas.split?(slots.width-slots.columnSpacing)*(1-canvas.artworkShare):slots.width
            Layout.preferredHeight:metadata.implicitHeight
            Layout.minimumHeight:metadata.implicitHeight
        }
        Item {
            id: lyricSlot
            Layout.row:canvas.split?1:2;Layout.column:canvas.split?1:0
            Layout.fillWidth:true;Layout.fillHeight:canvas.lyricsMode
            Layout.preferredHeight:canvas.displayingLyrics?(canvas.lyricsMode?Math.max(100,slots.height*.42):76):0
            Layout.minimumHeight:canvas.displayingLyrics?Math.min(70,slots.height*.18):0
            visible:canvas.displayingLyrics
        }
    }
    readonly property rect coverTarget:Qt.rect(coverSlot.x+(coverSlot.width-artworkExtent)/2,coverSlot.y+(coverSlot.height-artworkExtent)/2,artworkExtent,artworkExtent)
    readonly property rect metadataTarget:Qt.rect(metadataSlot.x,split&&!lyricsMode?coverSlot.y+(coverSlot.height-metadata.implicitHeight)/2:metadataSlot.y,metadataSlot.width,metadata.implicitHeight)
    readonly property rect lyricTarget:Qt.rect(lyricSlot.x,lyricSlot.y,lyricSlot.width,Math.max(1,lyricSlot.height))
    readonly property rect coverGeometry:geometry("cover",coverTarget)
    readonly property rect metadataGeometry:geometry("metadata",metadataTarget)
    readonly property rect lyricGeometry:geometry("lyrics",lyricTarget)
    readonly property real artworkExtent:Math.max(1,Math.min(coverSlot.width,coverSlot.height,Math.min(split?(slots.width-slots.columnSpacing)*.46:coverSlot.width,coverSlot.height,mode===1?620:split?440:460)*artworkScale))
    CoverArtwork {
        id: artwork;objectName:"heroArtwork"
        width:canvas.coverGeometry.width;height:canvas.coverGeometry.height
        x:canvas.coverGeometry.x;y:canvas.coverGeometry.y
        source:canvas.controller.artwork;motion:canvas.motion&&canvas.active;transitionDuration:Math.min(240,tokens.change)
        ambientColor:canvas.controller.ambientPrimary||canvas.accent
        onActivated:canvas.nextLayout()
        actionText:qsTr("Switch layout")
    }
    Item {
        id:metadata;objectName:"trackMetadata"
        x:canvas.metadataGeometry.x;y:canvas.metadataGeometry.y
        width:canvas.metadataGeometry.width;height:canvas.metadataGeometry.height
        implicitHeight:titleMeasure.implicitHeight+(artistLabel.visible?artistLabel.implicitHeight+7:0)+(sourceLabel.visible?sourceLabel.implicitHeight+7:0)+(connectButton.visible?43:0)
        Text {
            id:titleMeasure;visible:false;width:metadataSlot.width;text:titleLabel.text
            font:titleLabel.font;wrapMode:Text.Wrap;maximumLineCount:2;renderType:Text.NativeRendering
        }
        Label {
            id:titleLabel;objectName:"trackTitle";width:Math.min(implicitWidth,metadata.width);x:canvas.textX(titleLabel,"titleX");text:canvas.controller.title||qsTr("No media");textFormat:Text.PlainText
            height:canvas.changingLayout?canvas.mix(canvas.fromGeometry.titleHeight,canvas.toGeometry.titleHeight):titleMeasure.implicitHeight
            color:canvas.textColor;font.family:typography.resolvedFamily(text,tokens.displayFamily);font.weight:500
            font.pixelSize:Math.max(26,Math.min(32,canvas.width*.04))
            renderType:Text.NativeRendering;wrapMode:Text.Wrap;maximumLineCount:2;elide:Text.ElideRight
            horizontalAlignment:Text.AlignLeft
        }
        Label {
            id:artistLabel;width:Math.min(implicitWidth,metadata.width);x:canvas.textX(artistLabel,"artistX");y:titleLabel.height+7;visible:text.length>0;text:canvas.controller.artist;textFormat:Text.PlainText;color:canvas.secondaryColor
            font.family:tokens.family;font.pixelSize:16;font.weight:400;renderType:Text.NativeRendering
            elide:Text.ElideRight;horizontalAlignment:Text.AlignLeft
        }
        Label {
            id:sourceLabel;width:Math.min(implicitWidth,metadata.width);x:canvas.textX(sourceLabel,"sourceX");y:(artistLabel.visible?artistLabel.y+artistLabel.height:titleLabel.height)+7;visible:text.length>0;text:canvas.controller.appName;textFormat:Text.PlainText;color:canvas.secondaryColor
            opacity:.74;font.family:tokens.family;font.pixelSize:12;renderType:Text.NativeRendering
            elide:Text.ElideRight;horizontalAlignment:Text.AlignLeft
        }
        Button {
            id:connectButton;visible:!canvas.controller.online&&!canvas.controller.title;x:(metadata.width-width)*(1-canvas.textAlignment)/2;y:(sourceLabel.visible?sourceLabel.y+sourceLabel.height:artistLabel.visible?artistLabel.y+artistLabel.height:titleLabel.height)+7
            text:qsTr("Connect device");Accessible.name:text;implicitHeight:36;leftPadding:16;rightPadding:16
            background:Rectangle{radius:18;color:canvas.accent;opacity:parent.down?.75:parent.hovered?.9:1;border.width:parent.visualFocus?2:0;border.color:canvas.textColor}
            contentItem:Label{text:parent.text;color:tokens.accentText;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter;font.family:tokens.family;font.weight:500}
            onClicked:canvas.connectRequested()
        }
    }
    Item {
        id:homeLyrics;objectName:"homeLyrics"
        x:canvas.lyricGeometry.x;y:canvas.lyricGeometry.y;width:canvas.lyricGeometry.width;height:canvas.lyricGeometry.height
        readonly property real expansion:canvas.changingLayout?canvas.mix(canvas.fromGeometry.expansion,canvas.toGeometry.expansion):canvas.lyricsMode?1:0
        readonly property bool expandedSplit:canvas.changingLayout&&canvas.fromGeometry.expansion>.001?canvas.fromGeometry.expandedSplit:canvas.split
        readonly property bool returning:canvas.lyricFocus>0
        readonly property real travel:canvas.changingLayout?canvas.transitionProgress:0
        readonly property real morph:returning?Math.max(0,Math.min(1,(travel-.2)/.8)):1-expansion
        readonly property real primaryX:returning&&canvas.changingLayout?canvas.mix(canvas.fromGeometry.primary.x,compactLyrics.primaryRect.x):0
        readonly property real primaryY:returning&&canvas.changingLayout?canvas.mix(canvas.fromGeometry.primary.y,compactLyrics.primaryRect.y):0
        readonly property real sizeRatio:.85/(expandedSplit?1:.9)
        opacity:canvas.changingLayout?canvas.fromGeometry.lyricOpacity+(canvas.toGeometry.lyricOpacity-canvas.fromGeometry.lyricOpacity)*(canvas.toGeometry.lyricOpacity>canvas.fromGeometry.lyricOpacity?canvas.transitionProgress*canvas.transitionProgress:1-Math.pow(1-canvas.transitionProgress,2)):canvas.displayingLyrics&&canvas.controller.lyrics.length?1:0
        visible:opacity>.001
        // Two stable text layouts share the same media timeline. Crossfading
        // weight and scaling their geometry avoids reshaping every word per frame.
        LyricView {
            id:expandedLyrics;objectName:"expandedHomeLyrics"
            width:homeLyrics.returning&&canvas.changingLayout?canvas.fromGeometry.lyrics.width:parent.width
            height:homeLyrics.returning&&canvas.changingLayout?canvas.fromGeometry.lyrics.height:parent.height
            x:homeLyrics.returning&&canvas.changingLayout?homeLyrics.primaryX-primaryRect.x*scale:0
            y:homeLyrics.returning&&canvas.changingLayout?homeLyrics.primaryY-primaryRect.y*scale:0
            controller:canvas.controller;settingsRevision:canvas.settingsRevision;settingsPrefix:"lyrics"
            compact:false;originalFocus:canvas.lyricFocus
            opacity:homeLyrics.returning&&canvas.changingLayout?0:homeLyrics.expansion;visible:opacity>.001||homeLyrics.returning
            transformOrigin:Item.TopLeft
            scale:homeLyrics.sizeRatio+(1-homeLyrics.sizeRatio)*homeLyrics.expansion
            configuration:({alignment:homeLyrics.expandedSplit?canvas.controller.setting("lyrics/alignment","left"):"center",fontSize:canvas.controller.setting("lyrics/fontSize",28)*(homeLyrics.expandedSplit?1:.9)})
            textColor:canvas.textColor;inactiveColor:canvas.secondaryColor;motion:canvas.motion&&!canvas.changingLayout
            onSeekRequested:function(position){canvas.seekRequested(position)}
        }
        LyricView {
            id:compactLyrics;objectName:"compactHomeLyrics"
            width:homeLyrics.returning&&canvas.changingLayout?canvas.toGeometry.lyrics.width:parent.width
            height:homeLyrics.returning&&canvas.changingLayout?canvas.toGeometry.lyrics.height:parent.height
            x:homeLyrics.returning&&canvas.changingLayout?homeLyrics.primaryX-primaryRect.x*scale:0
            y:homeLyrics.returning&&canvas.changingLayout?homeLyrics.primaryY-primaryRect.y*scale:0
            controller:canvas.controller;settingsRevision:canvas.settingsRevision;settingsPrefix:"lyrics"
            compact:true;compactLines:1;opacity:homeLyrics.returning?0:1-homeLyrics.expansion;visible:homeLyrics.returning||opacity>.001
            transformOrigin:Item.TopLeft
            scale:1+(1/homeLyrics.sizeRatio-1)*homeLyrics.expansion
            configuration:({fontSize:canvas.controller.setting("lyrics/fontSize",28)*.85,activeWeight:canvas.controller.setting("lyrics/activeWeight",500),longLine:"elide",alignment:"center",translation:false,romanization:false,autoScroll:true})
            textColor:canvas.textColor;inactiveColor:canvas.secondaryColor;motion:canvas.motion&&!canvas.changingLayout
            onSeekRequested:function(position){canvas.seekRequested(position)}
        }
        Item {
            id:originalMorph;objectName:"originalLyricMorph";anchors.fill:parent
            visible:homeLyrics.returning&&canvas.changingLayout
            Repeater {
                model:originalMorph.visible?canvas.fromGeometry.tokens:[]
                Text {
                    id:morphWord;required property var modelData
                    readonly property font targetFace:Qt.font({family:modelData.family,pixelSize:compactLyrics.fontSize,weight:compactLyrics.activeWeight,letterSpacing:compactLyrics.option("letterSpacing",0)})
                    TextMetrics{id:prefixMeasure;font:morphWord.targetFace;text:morphWord.modelData.prefix}
                    readonly property real targetX:compactLyrics.primaryRect.x+(compactLyrics.inkPadding+prefixMeasure.advanceWidth)*compactLyrics.activeScale
                    readonly property real targetY:compactLyrics.primaryRect.y+compactLyrics.inkPadding*compactLyrics.activeScale
                    x:modelData.rect.x+(targetX-modelData.rect.x)*(1-Math.pow(1-canvas.transitionProgress,3))
                    readonly property bool wrapped:canvas.fromGeometry.tokens.length&&modelData.rect.y>canvas.fromGeometry.tokens[0].rect.y+1
                    y:canvas.protectLyricY(canvas.mix(modelData.rect.y,targetY)+(wrapped?Math.sin(Math.PI*canvas.transitionProgress)*18:0))
                    scale:canvas.formatMix(modelData.scale,compactLyrics.fontSize*compactLyrics.activeScale/Math.max(1,expandedLyrics.fontSize));transformOrigin:Item.TopLeft
                    text:modelData.text;textFormat:Text.PlainText
                    color:expandedLyrics.colorOption("sungColor",canvas.textColor)
                    font.family:modelData.family;font.pixelSize:expandedLyrics.fontSize
                    font.weight:compactLyrics.activeWeight
                    font.letterSpacing:canvas.formatMix(expandedLyrics.option("letterSpacing",0),compactLyrics.option("letterSpacing",0))
                    renderType:Text.CurveRendering;renderTypeQuality:104;wrapMode:Text.NoWrap;lineHeight:expandedLyrics.option("lineHeight",1.25)
                    // Blend two cached weights instead of reshaping every word
                    // for a new intermediate QFont on every animation frame.
                    Text{text:morphWord.text;textFormat:Text.PlainText;font.family:morphWord.font.family;font.pixelSize:morphWord.font.pixelSize;font.letterSpacing:morphWord.font.letterSpacing;font.weight:expandedLyrics.activeWeight;color:morphWord.color;opacity:Math.pow(1-canvas.transitionProgress,3);renderType:Text.CurveRendering;renderTypeQuality:104;lineHeight:expandedLyrics.option("lineHeight",1.25)}
                    readonly property real fill:!modelData.timed?1:Math.max(0,Math.min(1,(canvas.controller.lyricPosition-modelData.time)/Math.max(1,modelData.end-modelData.time)))
                    Item{visible:morphWord.fill<1;x:morphWord.width*morphWord.fill;width:morphWord.width*(1-morphWord.fill);height:morphWord.height;clip:true
                        Text{x:-parent.x;text:morphWord.text;textFormat:Text.PlainText;font:morphWord.font;color:expandedLyrics.colorOption("unsungColor",canvas.secondaryColor);renderType:Text.CurveRendering;renderTypeQuality:104;lineHeight:expandedLyrics.option("lineHeight",1.25)}
                    }
                }
            }
        }
    }
    RowLayout {
        x:lyricSlot.x;y:lyricSlot.y;width:lyricSlot.width;height:42
        visible:canvas.displayingLyrics&&!canvas.controller.lyrics.length&&canvas.controller.title.length>0
        Label{Layout.fillWidth:true;text:canvas.controller.lyricsLoading?qsTr("Matching lyrics…"):qsTr("No lyrics");color:canvas.secondaryColor;horizontalAlignment:Text.AlignLeft}
        Button{text:qsTr("Match lyrics");flat:true;enabled:!canvas.controller.lyricsLoading;onClicked:canvas.matchRequested()}
    }
}
