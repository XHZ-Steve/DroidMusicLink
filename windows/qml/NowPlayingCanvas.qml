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
    readonly property bool split: mode!==0 && width>780 && width/Math.max(1,height)>1.12
    readonly property bool lyricsMode: mode===2
    readonly property bool displayingLyrics: lyricsEnabled && mode!==1
    readonly property string trackIdentity:controller.title+"\u0000"+controller.artist+"\u0000"+controller.appName
    readonly property bool active: visible && Window.window && Window.window.visible && Window.window.visibility!==Window.Minimized
    signal connectRequested()
    signal matchRequested()
    signal seekRequested(real position)
    property real transitionProgress: 1
    property real textAlignment: split?1:0
    property var fromGeometry: ({})
    property var toGeometry: ({})
    property int transitionGeneration: 0
    function rectOf(item){return Qt.rect(item.x,item.y,item.width,item.height)}
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
        const current={cover:rectOf(artwork),metadata:rectOf(metadata),lyrics:rectOf(homeLyrics),alignment:textAlignment,titleX:metadata.x+titleLabel.x,titleHeight:titleLabel.height,artistX:metadata.x+artistLabel.x,sourceX:metadata.x+sourceLabel.x,lyricOpacity:homeLyrics.opacity}
        layoutMotion.stop();fromGeometry=current;toGeometry=current
        transitionProgress=0;changingLayout=true
    }
    function scheduleTransition(){
        const generation=++transitionGeneration
        // Layout is polished first; every surface then starts on the same clock.
        Qt.callLater(function(){
            if(generation!==canvas.transitionGeneration)return
            slots.ensurePolished()
            toGeometry={cover:coverTarget,metadata:metadataTarget,lyrics:lyricTarget,alignment:split?1:0,titleX:textTargetX(titleLabel),titleHeight:titleMeasure.implicitHeight,artistX:textTargetX(artistLabel),sourceX:textTargetX(sourceLabel),lyricOpacity:displayingLyrics&&controller.lyrics.length?1:0}
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
    function nextLayout(){freezeGeometry();controller.setSetting("home/layout",(mode+1)%3)}
    onModeChanged:if(ready){if(!changingLayout)freezeGeometry();scheduleTransition()}
    onWidthChanged:cancelTransitionForResize()
    onHeightChanged:cancelTransitionForResize()
    onActiveChanged:if(!active)cancelTransitionForResize()
    onTrackIdentityChanged:if(ready)cancelTransitionForResize()
    function cancelTransitionForResize(){if(!ready||!changingLayout)return;++transitionGeneration;layoutMotion.stop();transitionProgress=1;changingLayout=false}
    Component.onCompleted:ready=true
    NumberAnimation{id:layoutMotion;target:canvas;property:"transitionProgress";from:0;to:1;duration:460;easing.type:Easing.InOutCubic;onFinished:canvas.changingLayout=false}
    Binding{target:canvas;property:"textAlignment";value:canvas.changingLayout?canvas.mix(canvas.fromGeometry.alignment,canvas.toGeometry.alignment):canvas.split?1:0}
    TypographyTokens{id:typography}
    DesignTokens{id:tokens;light:canvas.light;motion:canvas.motion;accent:canvas.accent}
    GridLayout {
        id: slots;anchors.fill:parent;anchors.topMargin:canvas.split&&canvas.lyricsMode?Math.min(40,canvas.height*.07):0;anchors.bottomMargin:anchors.topMargin;columns:canvas.split?2:1;rowSpacing:canvas.split?24:14;columnSpacing:Math.max(32,Math.min(72,canvas.width*.065))
        Item {
            id: coverSlot
            Layout.row:0;Layout.column:0;Layout.rowSpan:canvas.split?2:1
            Layout.fillWidth:true;Layout.fillHeight:canvas.split||!canvas.lyricsMode
            Layout.preferredWidth:canvas.split?slots.width*.46:slots.width
            Layout.preferredHeight:canvas.split?slots.height:canvas.lyricsMode?Math.min(180,slots.height*.32):Math.max(110,slots.height-metadata.implicitHeight-(canvas.displayingLyrics?74:0)-20)
            Layout.minimumHeight:90
        }
        Item {
            id: metadataSlot
            Layout.topMargin:canvas.split&&canvas.lyricsMode?28:0
            Layout.row:canvas.split?0:1;Layout.column:canvas.split?1:0
            Layout.fillWidth:true;Layout.fillHeight:canvas.split&&!canvas.lyricsMode
            Layout.preferredWidth:canvas.split?slots.width*.49:slots.width
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
    readonly property real artworkExtent:Math.max(80,Math.min(coverSlot.width,coverSlot.height,mode===1?620:split?440:460))
    CoverArtwork {
        id: artwork;objectName:"heroArtwork"
        width:canvas.coverGeometry.width;height:canvas.coverGeometry.height
        x:canvas.coverGeometry.x;y:canvas.coverGeometry.y
        source:canvas.controller.artwork;motion:canvas.motion&&canvas.active;transitionDuration:tokens.change
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
    LyricView {
        id:homeLyrics;objectName:"homeLyrics"
        x:canvas.lyricGeometry.x;y:canvas.lyricGeometry.y;width:canvas.lyricGeometry.width;height:canvas.lyricGeometry.height
        controller:canvas.controller;settingsRevision:canvas.settingsRevision;settingsPrefix:"lyrics"
        compact:!canvas.lyricsMode;compactLines:1
        configuration:canvas.lyricsMode?({alignment:canvas.split?canvas.controller.setting("lyrics/alignment","left"):"center",fontSize:canvas.controller.setting("lyrics/fontSize",28)*(canvas.split?1:.9)}):({fontSize:canvas.controller.setting("lyrics/fontSize",28)*.85,activeWeight:canvas.controller.setting("lyrics/activeWeight",500),longLine:"elide",alignment:"center",translation:false,romanization:false,autoScroll:true})
        textColor:canvas.textColor;inactiveColor:canvas.secondaryColor;motion:canvas.motion&&!canvas.changingLayout
        opacity:canvas.changingLayout?canvas.fromGeometry.lyricOpacity+(canvas.toGeometry.lyricOpacity-canvas.fromGeometry.lyricOpacity)*(canvas.toGeometry.lyricOpacity>canvas.fromGeometry.lyricOpacity?canvas.transitionProgress*canvas.transitionProgress:1-Math.pow(1-canvas.transitionProgress,2)):canvas.displayingLyrics&&canvas.controller.lyrics.length?1:0;visible:opacity>.001
        onSeekRequested:function(position){canvas.seekRequested(position)}
    }
    RowLayout {
        x:lyricSlot.x;y:lyricSlot.y;width:lyricSlot.width;height:42
        visible:canvas.displayingLyrics&&!canvas.controller.lyrics.length&&canvas.controller.title.length>0
        Label{Layout.fillWidth:true;text:canvas.controller.lyricsLoading?qsTr("Matching lyrics…"):qsTr("No lyrics");color:canvas.secondaryColor;horizontalAlignment:Text.AlignLeft}
        Button{text:qsTr("Match lyrics");flat:true;enabled:!canvas.controller.lyricsLoading;onClicked:canvas.matchRequested()}
    }
}
