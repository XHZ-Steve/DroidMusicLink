pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import DroidMediaLink 1.0 as NativeFrame

import "Icons.js" as Icons

ApplicationWindow {
    id: root
    width: 1120; height: 760; minimumWidth: 560; minimumHeight: 520
    visible: false; title: "DroidMediaLink"
    color: bg
    property bool highContrast:false
    property color bg: highContrast?nativePalette.window:design.base
    property color fg: highContrast?nativePalette.windowText:design.text
    property color muted: design.secondaryText
    property color accent: backend.accent || "#b7d9c6"
    property bool liquidStyle: !highContrast&&root.pref("appearance/style","liquid")==="liquid"
    property string selectedDeviceUrl: ""
    property real glass: root.pref("glass",0.16)
    property int lyricSize: root.pref("lyricSize",32)
    property bool light: highContrast?nativePalette.window.hslLightness>.5:root.pref("light", false)
    property bool motion: root.pref("motion", true)
    property bool showLyrics: root.pref("showLyrics", true)
    property int page: 0
    property bool desktopClickThrough: root.pref("desktopClickThrough",false)
    property bool overlayTopmost: root.pref("overlayTopmost",true)
    property int settingsRevision: 0
    function pref(key, fallback) { var revision=root.settingsRevision; return backend.setting(key,fallback) }
    property string search: ""
    property int settingsCategory: 0
    font.family: design.family
    font.pixelSize: design.body
    palette.window: bg; palette.windowText: fg; palette.text: fg
    palette.base: design.surface
    palette.button: design.surface
    palette.buttonText: fg; palette.highlight: accent; palette.highlightedText: design.accentText
    function settingVisible(group,text) {return (root.settingsCategory===0 || root.settingsCategory===group) && (!root.search || root.matches(text))}
    function matches(text) { return root.search.split(/\s+/).every(function(word){ return text.toLowerCase().indexOf(word)>=0 }) }
    property bool autoHideControls:root.pref("window/autoHideControls",true)
    property bool chromeRevealed:!autoHideControls||mainChrome.pointerInTitleBar||minimizeControl.visualFocus||maximizeControl.visualFocus||closeControl.visualFocus
    property bool gearRevealed:!autoHideControls||mainChrome.pointerNearControls||settingsGear.hovered||settingsGear.visualFocus
    function material(scope){var revision=root.settingsRevision;return backend.appearanceConfig(scope)}
    function openSettings(group,section){root.page=2;settingsPage.navigate(group===undefined?0:group,section===undefined?0:section)}
    onPageChanged:if(page===1)openSettings(1,0)
    function save(key, value) { backend.setSetting(key, value) }
    function time(ms) { var s = Math.max(0, Math.floor(ms/1000)); return Math.floor(s/60)+":"+(s%60<10?"0":"")+(s%60) }
    function supports(cmd) { return backend.online && backend.commands.indexOf(cmd)>=0 }
    onVisibleChanged: backend.setUiVisible((root.visible&&root.visibility!==Window.Minimized)||desktop.visible||floating.visible||trayWindow.visible)
    onVisibilityChanged:backend.setUiVisible((root.visible&&root.visibility!==Window.Minimized)||desktop.visible||floating.visible||trayWindow.visible)
    function showTrayMenu(px, py) {
        if(trayWindow.visible) { trayWindow.hide(); return }
        trayWindow.x=px;trayWindow.y=py;
        trayWindow.show();trayWindow.raise();trayWindow.requestActivate();trayFocusGuard.restart();
    }
    onClosing: function(close) { if (root.pref("closeToTray", true)) { close.accepted=false; hide() } else Qt.quit() }
    FontLoader { id: lucide; source: "qrc:/icons/lucide.ttf" }
    SystemPalette{id:nativePalette;colorGroup:SystemPalette.Active}
    DesignTokens {id:design;light:root.light;motion:root.motion;accent:root.accent}
    property bool wide: width>=940 && width/height>1.2
    property int canvasMargin: Math.round(Math.max(28,Math.min(64,width*.055)))
    component Action: Button {
        id:a
        property string iconName:""
        property bool primary:false
        implicitHeight:design.control;implicitWidth:iconName?40:Math.max(72,contentItem.implicitWidth+28)
        hoverEnabled:true;Accessible.name:text
        background:Rectangle {
            radius:design.radius
            color:a.primary?root.accent:a.down?design.pressed:a.checked?design.selected:a.hovered?design.hover:"transparent"
            border.width:a.visualFocus?2:0;border.color:root.accent
            Behavior on color {ColorAnimation{duration:design.fast}}
        }
        contentItem:Text {text:a.iconName?Icons.glyph(a.iconName):a.text;font.family:a.iconName?lucide.name:root.font.family;font.pixelSize:a.iconName?20:design.body;font.weight:a.primary?Font.DemiBold:Font.Medium;color:a.primary?design.accentText:root.fg;opacity:a.enabled?1:.35;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter}
        ToolTip.visible:iconName.length>0&&hovered;ToolTip.delay:650;ToolTip.text:text
    }
    component WindowButton: Button {
        id: captionButton
        property string iconName: ""
        implicitWidth:42;implicitHeight:32;padding:0;display:AbstractButton.IconOnly
        icon.source:"qrc:/icons/window/"+iconName+".svg";icon.width:14;icon.height:14;icon.color:root.fg
        hoverEnabled:true;Accessible.name:text;transformOrigin:Item.Center
        background:Rectangle{radius:8;color:captionButton.down?design.pressed:captionButton.hovered?design.hover:"transparent";border.width:captionButton.visualFocus?1:0;border.color:root.accent}
        ToolTip.visible:hovered;ToolTip.delay:650;ToolTip.text:text
    }
    component Field: TextField {
        id:field
        implicitHeight:design.control;color:root.fg;placeholderTextColor:design.faintText;leftPadding:14;rightPadding:14;selectByMouse:true
        background:Rectangle {radius:design.radius;color:design.surface;border.width:field.visualFocus?2:1;border.color:field.visualFocus?root.accent:design.border}
    }
    component Choice: ComboBox {
        id:choice
        implicitWidth:192;implicitHeight:design.control;leftPadding:12;rightPadding:34
        contentItem:Text {text:choice.displayText;font:choice.font;color:root.fg;elide:Text.ElideRight;verticalAlignment:Text.AlignVCenter}
        indicator:Text {x:choice.width-width-12;y:(choice.height-height)/2;text:Icons.glyph("chevron-down");color:root.muted;font.family:lucide.name;font.pixelSize:16}
        background:Rectangle {radius:design.radius;color:choice.down?design.pressed:choice.hovered?design.hover:design.surface;border.color:choice.visualFocus?root.accent:design.border;border.width:choice.visualFocus?2:1}
        delegate:ItemDelegate {id:option;required property int index;required property var modelData;width:choice.width;implicitHeight:40;text:String(modelData);highlighted:choice.highlightedIndex===index
            contentItem:Label{text:option.text;color:root.fg;verticalAlignment:Text.AlignVCenter;leftPadding:10}
            background:Rectangle{radius:8;color:option.highlighted?design.selected:option.hovered?design.hover:"transparent"}
        }
        popup:Popup {y:choice.height+6;width:choice.width;padding:6;implicitHeight:Math.min(280,contentItem.implicitHeight+12)
            contentItem:ListView {clip:true;implicitHeight:contentHeight;model:choice.popup.visible?choice.delegateModel:null;currentIndex:choice.highlightedIndex;ScrollIndicator.vertical:ScrollIndicator{}}
            background:Rectangle{radius:12;color:design.elevated;border.width:1;border.color:design.border}
        }
    }
    component Toggle: Switch {
        id:toggle;implicitWidth:48;implicitHeight:design.control
        contentItem:Item{}
        indicator:Rectangle {x:4;y:(toggle.height-height)/2;implicitWidth:40;implicitHeight:24;radius:12;color:toggle.checked?root.accent:design.surface;border.width:toggle.visualFocus?2:1;border.color:toggle.visualFocus?root.accent:design.border
            Behavior on color{ColorAnimation{duration:design.fast}}
            Rectangle {width:18;height:18;radius:9;y:3;x:toggle.checked?19:3;color:toggle.checked?design.accentText:design.secondaryText;Behavior on x{NumberAnimation{duration:design.fast;easing.type:Easing.OutCubic}}}
        }
    }
    component TrackSlider: Slider {
        id:track;implicitHeight:32;leftPadding:6;rightPadding:6
        Behavior on value{enabled:root.motion&&!track.pressed&&track.Window.window&&track.Window.window.visible;NumberAnimation{duration:design.progressMotion;easing.type:Easing.Linear}}
        background:Rectangle {x:track.leftPadding;y:track.topPadding+track.availableHeight/2-1.5;implicitWidth:160;implicitHeight:3;width:track.availableWidth;height:3;radius:2;color:root.light?"#24303b60":"#30e0e5ff";opacity:track.enabled?1:.35
            Rectangle{width:track.visualPosition*parent.width;height:3;radius:2;color:root.accent}
        }
        handle:Rectangle{x:track.leftPadding+track.visualPosition*(track.availableWidth-width);y:track.topPadding+track.availableHeight/2-height/2;implicitWidth:track.pressed?14:10;implicitHeight:width;radius:width/2;color:root.accent;opacity:track.enabled?1:.35;border.width:track.visualFocus?2:0;border.color:root.fg}
    }
    component NumberField: SpinBox {
        id:number;implicitWidth:152;implicitHeight:design.control;editable:true
        background:Rectangle{radius:design.radius;color:design.surface;border.color:number.visualFocus?root.accent:design.border;border.width:number.visualFocus?2:1}
        contentItem:TextInput{text:number.textFromValue(number.value,number.locale);font:number.font;color:root.fg;selectionColor:root.accent;selectedTextColor:design.accentText;horizontalAlignment:Qt.AlignHCenter;verticalAlignment:Qt.AlignVCenter;readOnly:!number.editable;validator:number.validator;inputMethodHints:Qt.ImhFormattedNumbersOnly}
        up.indicator:Rectangle{x:number.width-width;width:32;height:number.height;radius:design.radius;color:number.up.pressed?design.pressed:"transparent";Text{anchors.centerIn:parent;text:"+";color:root.fg;font.pixelSize:18}}
        down.indicator:Rectangle{width:32;height:number.height;radius:design.radius;color:number.down.pressed?design.pressed:"transparent";Text{anchors.centerIn:parent;text:"−";color:root.fg;font.pixelSize:18}}
    }
    component AppDialog: Dialog {
        id:dialogSurface;padding:24;spacing:16
        header:Label{text:dialogSurface.title;visible:text.length>0;leftPadding:24;rightPadding:24;topPadding:24;bottomPadding:4;color:root.fg;font.pixelSize:20;font.weight:Font.DemiBold}
        footer:DialogButtonBox{standardButtons:dialogSurface.standardButtons;padding:16;spacing:8;alignment:Qt.AlignRight;delegate:Action{implicitWidth:96}background:Item{}onAccepted:dialogSurface.accept();onRejected:dialogSurface.reject()}
        background:GlassMaterial{configuration:root.material("popup");motion:root.motion;sourceItem:ambient;glassEnabled:root.liquidStyle;cornerRadius:design.dialogRadius;active:dialogSurface.visible&&root.visible;dark:!root.light;backdropColor:root.bg;tintColor:design.elevated;tintStrength:root.light?.10+root.glass*.44:.04+root.glass*.25;fallbackColor:design.elevated;increasedContrast:root.highContrast?1:0}
        Overlay.modal:Rectangle{color:root.light?"#25202c48":"#65060812"}
        enter:Transition{NumberAnimation{property:"opacity";from:0;to:1;duration:design.fast}}
        exit:Transition{NumberAnimation{property:"opacity";from:1;to:0;duration:design.fast}}
    }
    component AmbientScene: AmbientField {
        primary:backend.ambientPrimary;secondary:backend.ambientSecondary;accent:backend.ambientAccent;deep:backend.ambientDeep;configuration:root.material("background")
        light:root.light;motion:root.motion;transitionDuration:design.atmosphere;active:Window.window&&Window.window.visible&&Window.window.visibility!==Window.Minimized
    }
    component Caption: Label{textFormat:Text.PlainText;color:root.muted;font.pixelSize:design.caption}
    component Transport: RowLayout {
        id: controls
        property bool compact: false
        property Item backdrop:ambient
        readonly property bool water:root.pref("dock/playStyle","solid")==="water"&&!root.highContrast
        spacing: compact?4:10
        Action { text: qsTr("Previous"); iconName:"skip-back"; Accessible.name: qsTr("Previous"); enabled: supports("previous"); onClicked: backend.command("previous") }
        Action {
            id:transportPlay;objectName:"primaryTransport"
            text: backend.playing ? qsTr("Pause") : qsTr("Play"); Accessible.name: backend.playing ? qsTr("Pause") : qsTr("Play")
            enabled: supports(backend.playing?"pause":"play") || supports("toggle")
            implicitWidth: controls.compact?44:48; implicitHeight: implicitWidth; font.pixelSize: 24
            background: GlassMaterial {
                sourceItem:controls.backdrop;glassEnabled:controls.water;active:Window.window&&Window.window.visible;motion:root.motion
                configuration:Object.assign({},root.material("dock"),{glassRadius:width/2,glassOpacity:.06,glassBlur:1.4,glassRefraction:1.6,glassDistortion:1.2,glassEdgeGlow:.65,glassEdgeWidth:1.7})
                cornerRadius:width/2;dark:!root.light;backdropColor:root.bg;tintColor:root.accent;fallbackColor:root.accent
                opacity:parent.enabled?(parent.down?.8:1):.3
                Rectangle{anchors.fill:parent;radius:width/2;color:"transparent";border.width:transportPlay.visualFocus?2:0;border.color:root.fg}
            }
            contentItem: Text { text: Icons.glyph(backend.playing?"pause":"play"); color: controls.water?root.fg:design.accentText; font.family:lucide.name; font.pixelSize:26; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            onClicked: backend.command(supports("toggle")?"toggle":backend.playing?"pause":"play")
        }
        Action { text: qsTr("Next"); iconName:"skip-forward"; Accessible.name: qsTr("Next"); enabled: supports("next"); onClicked: backend.command("next") }
    }
    AmbientScene {id:ambient;anchors.fill:parent;visible:!root.highContrast}
    ColumnLayout {
        id:canvasLayout
        anchors.fill:parent;anchors.leftMargin:root.canvasMargin;anchors.rightMargin:root.canvasMargin;anchors.topMargin:root.page===0?Math.max(40,root.canvasMargin):48;anchors.bottomMargin:root.page===0?Math.max(28,root.canvasMargin*.65):32;spacing:28
        StackLayout {
            currentIndex:root.page===0?0:1;Layout.fillWidth:true;Layout.fillHeight:true
            NowPlayingCanvas {
                id:homePage;controller:backend;settingsRevision:root.settingsRevision
                light:root.light;motion:root.motion;lyricsEnabled:root.showLyrics
                textColor:root.fg;secondaryColor:root.muted;accent:root.accent
                onConnectRequested:root.openSettings(1,0)
                onMatchRequested:{query.text=backend.title+" "+backend.artist;matchDialog.open()}
                onSeekRequested:function(position){backend.command("seek",position)}
            }
            SettingsPage {
                id:settingsPage;objectName:"settingsPage";materialSource:ambient;controller:backend;settingsRevision:root.settingsRevision;light:root.light;motion:root.motion;accent:root.accent;textColor:root.fg
                onCloseRequested:root.page=0
                onDesktopLyricsRequested:desktop.toggleRequested()
                onFloatingRequested:floating.visible=!floating.visible
                onImportLyricsRequested:lyricFile.open()
                onMatchLyricsRequested:{query.text=backend.title+" "+backend.artist;matchDialog.open()}
                onSearchDevicesRequested:{discovery.open();backend.discover()}
                onForgetDeviceRequested:function(url){root.selectedDeviceUrl=url;revoke.open()}
                onActionRequested:function(key){if(key==="editDesktopLyrics"){desktop.editRequested()}}
            }
        }
        GlassMaterial {
            id:playbackDock;objectName:"playbackDock";property bool capsule:root.pref("dock/shape","rounded")==="capsule";configuration:capsule?Object.assign({},root.material("dock"),{glassRadius:height/2}):root.material("dock");motion:root.motion
            Layout.alignment:Qt.AlignHCenter;Layout.preferredWidth:Math.min(660,canvasLayout.width);implicitHeight:capsule?84:100;visible:root.page===0
            sourceItem:ambient;glassEnabled:root.liquidStyle;cornerRadius:design.dockRadius;dark:!root.light;active:root.visible&&root.visibility!==Window.Minimized&&visible;backdropColor:root.bg;tintColor:design.elevated;tintStrength:root.light?.10+root.glass*.44:.04+root.glass*.25;fallbackColor:design.surface
            CapsuleProgress {
                anchors.left:parent.left;anchors.right:parent.right;height:playbackDock.height/2+8;radius:playbackDock.height/2;visible:playbackDock.capsule
                from:0;to:Math.max(1,backend.duration);value:backend.position;enabled:root.supports("seek");accent:root.accent;trackColor:root.light?"#4024303b":"#38e0e5ff"
                Accessible.name:qsTr("Playback position");onPressedChanged:if(!pressed)backend.command("seek",value)
            }
            ColumnLayout {
                anchors.fill:parent;anchors.leftMargin:playbackDock.capsule?26:18;anchors.rightMargin:anchors.leftMargin;anchors.topMargin:playbackDock.capsule?16:4;anchors.bottomMargin:playbackDock.capsule?16:10;spacing:0
                RowLayout {Layout.fillWidth:true;visible:!playbackDock.capsule;spacing:8
                    Caption{text:root.time(backend.position);Layout.preferredWidth:36}
                    TrackSlider{Layout.fillWidth:true;from:0;to:Math.max(1,backend.duration);value:backend.position;enabled:supports("seek");Accessible.name:qsTr("Playback position");onPressedChanged:if(!pressed)backend.command("seek",value)}
                    Caption{text:root.time(backend.duration);horizontalAlignment:Text.AlignRight;Layout.preferredWidth:36}
                }
                RowLayout {Layout.fillWidth:true;Layout.fillHeight:true;spacing:0
                    Item{Layout.fillWidth:true;Layout.preferredWidth:1;implicitHeight:48
                        Action{id:dockLyricsControl;anchors.left:parent.left;anchors.verticalCenter:parent.verticalCenter;text:qsTr("Desktop lyrics");iconName:"captions";checked:desktop.requestedVisible;onClicked:desktop.toggleRequested()}
                        Caption{visible:playbackDock.capsule;anchors.left:dockLyricsControl.right;anchors.leftMargin:6;anchors.verticalCenter:parent.verticalCenter;text:root.time(backend.position)}
                    }
                    Transport{}
                    Item{Layout.fillWidth:true;Layout.preferredWidth:1;implicitHeight:48
                        Action{id:volumeButton;objectName:"volumeButton";anchors.right:parent.right;anchors.verticalCenter:parent.verticalCenter;text:qsTr("Phone volume");iconName:backend.volumeKnown&&backend.volume<=0?"volume-x":"volume-2";enabled:root.supports("volume")&&backend.volumeKnown
                            onHoveredChanged:if(hovered){volumeDismiss.stop();if(!volumePopup.visible)volumePopup.open()}else volumeDismiss.restart()
                            onClicked:{volumePopup.pinned=!volumePopup.pinned;if(volumePopup.pinned)volumePopup.open();else volumePopup.close()}}
                        Caption{visible:playbackDock.capsule;anchors.right:volumeButton.left;anchors.rightMargin:6;anchors.verticalCenter:parent.verticalCenter;text:root.time(backend.duration)}
                    }
                }
            }
        }
    }
    AppDialog {
        id:discovery;objectName:"discovery";title:qsTr("Search devices");anchors.centerIn:parent;width:Math.min(440,root.width-48);height:Math.min(root.height-64,implicitHeight);modal:true;standardButtons:Dialog.Close
        ColumnLayout {anchors.fill:parent
            ListView {id:discoveryList;Layout.fillWidth:true;Layout.fillHeight:true;Layout.preferredHeight:Math.max(56,Math.min(3,backend.devices.length)*64);Layout.minimumHeight:56;clip:true;model:backend.devices;spacing:8
                delegate:ItemDelegate {id:discoveryRow;required property var modelData;width:ListView.view.width;height:56;text:modelData.name || qsTr("Android device");Accessible.name:text;contentItem:Label{text:discoveryRow.text;textFormat:Text.PlainText;color:root.fg;verticalAlignment:Text.AlignVCenter;leftPadding:12}background:Rectangle{radius:design.radius;color:discoveryRow.hovered?design.hover:design.selected}onClicked:{discovery.close();backend.connectDevice(modelData.url)}}
                Label{parent:discoveryList;anchors.centerIn:parent;visible:!backend.devices.length;text:qsTr("No devices");color:root.muted}
            }
            Action{text:qsTr("Search again");Layout.alignment:Qt.AlignHCenter;onClicked:backend.discover()}
        }
    }
    AppDialog {
        id:pairingDialog;title:qsTr("Confirm connection");anchors.centerIn:parent;width:Math.min(440,root.width-48);modal:true;visible:backend.pairingPending;standardButtons:Dialog.Cancel;closePolicy:Popup.CloseOnEscape
        onRejected:backend.cancelPairing()
        contentItem:ColumnLayout {spacing:20
            Label{text:backend.pairingCode || qsTr("Connecting");color:root.fg;font.pixelSize:backend.pairingCode?42:20;font.family:backend.pairingCode?"Consolas":root.font.family;font.letterSpacing:backend.pairingCode?8:0;Layout.alignment:Qt.AlignHCenter;Accessible.name:qsTr("Confirmation code")+" "+text}
            Label{text:qsTr("Compare the numbers, then confirm on your phone.");color:root.muted;wrapMode:Text.Wrap;Layout.fillWidth:true}
        }
    }
    AppDialog {
        id: matchDialog; title:qsTr("Match lyrics"); width:Math.min(root.width-60,620);height:480;anchors.centerIn:parent;modal:true;standardButtons:Dialog.Close
        ColumnLayout { anchors.fill:parent
            RowLayout { Field { id:query;Layout.fillWidth:true;placeholderText:qsTr("Title and artist");Accessible.name:qsTr("Title and artist");onAccepted:backend.searchLyrics(text) } Action { text:qsTr("Search");onClicked:backend.searchLyrics(query.text) } }
            ListView { Layout.fillWidth:true;Layout.fillHeight:true;clip:true;model:backend.lyricResults
                delegate:ItemDelegate { required property var modelData; width:ListView.view.width;text:modelData.title+" — "+modelData.artist+"  ·  "+modelData.provider;onClicked:{backend.bindLyricCandidate(modelData.provider,modelData.sourceId);matchDialog.close()} }
            }
        }
    }
    FileDialog { id: lyricFile; nameFilters:[qsTr("Lyrics")+" (*.lrc *.yrc *.qrc *.krc *.ttml *.txt)"]; onAccepted: backend.importLyrics(selectedFile) }
    FileDialog { id: exportFile; fileMode:FileDialog.SaveFile; defaultSuffix:"json"; onAccepted:backend.exportSettings(selectedFile) }
    FileDialog { id: importFile; nameFilters:["JSON (*.json)"]; onAccepted:backend.importSettings(selectedFile) }
    AppDialog { id: reset; title:qsTr("Restore defaults?"); anchors.centerIn:parent;modal:true;standardButtons:Dialog.Ok|Dialog.Cancel;onAccepted:backend.resetSettings() }
    AppDialog { id: clearCache; title:qsTr("Delete cached lyrics?");anchors.centerIn:parent;modal:true;standardButtons:Dialog.Ok|Dialog.Cancel;onAccepted:backend.clearLyricsCache() }
    AppDialog { id: revoke; title:qsTr("Forget device?"); anchors.centerIn: parent; modal:true; standardButtons:Dialog.Ok|Dialog.Cancel; Label { text:qsTr("Saved credentials will be removed. Pair again to reconnect."); wrapMode:Text.Wrap; width:300 } onAccepted:backend.forgetDevice(root.selectedDeviceUrl) }
    Window {
        id: floating; objectName:"floating"; onVisibleChanged: backend.setUiVisible((root.visible&&root.visibility!==Window.Minimized) || desktop.visible || floating.visible || trayWindow.visible); width:390; height:180; minimumWidth:280;minimumHeight:160; visible:false; title:qsTr("Floating player"); color:"transparent"; flags:Qt.Tool|Qt.FramelessWindowHint|Qt.NoDropShadowWindowHint|(root.overlayTopmost?Qt.WindowStaysOnTopHint:0)
        x:root.pref("floatX",100); y:root.pref("floatY",100)
        onXChanged: if(visible)root.save("floatX",x); onYChanged:if(visible)root.save("floatY",y)
        AmbientScene{id:floatingBackdrop;anchors.fill:parent;visible:false}
        GlassMaterial{opacity:root.highContrast?1:.72;configuration:root.material("floating");motion:root.motion;anchors.fill:parent;sourceItem:floatingBackdrop;glassEnabled:root.liquidStyle;cornerRadius:design.floatingRadius;active:floating.visible;dark:!root.light;backdropColor:root.bg;tintColor:design.elevated;tintStrength:root.light?.10+root.glass*.44:.04+root.glass*.25;fallbackColor:design.elevated}
        MouseArea{anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;height:48;onPressed:floating.startSystemMove()}
        ColumnLayout { anchors.fill:parent; anchors.margins:20; Label { textFormat:Text.PlainText; text:backend.title||qsTr("No media"); color:fg; font.pixelSize:20; elide:Text.ElideRight; Layout.fillWidth:true;Layout.rightMargin:24 } Caption { text:backend.artist;Layout.fillWidth:true;elide:Text.ElideRight } Transport { backdrop:floatingBackdrop;Layout.alignment:Qt.AlignHCenter } }
        Action{anchors.top:parent.top;anchors.right:parent.right;anchors.margins:8;implicitWidth:28;implicitHeight:28;iconName:"x";text:qsTr("Close");onClicked:floating.hide()}
        Repeater{model:[Qt.LeftEdge,Qt.RightEdge,Qt.TopEdge,Qt.BottomEdge,Qt.LeftEdge|Qt.TopEdge,Qt.RightEdge|Qt.TopEdge,Qt.LeftEdge|Qt.BottomEdge,Qt.RightEdge|Qt.BottomEdge]
            MouseArea{
                required property int modelData
                readonly property bool atLeft:(modelData&Qt.LeftEdge)!==0;readonly property bool atRight:(modelData&Qt.RightEdge)!==0
                readonly property bool atTop:(modelData&Qt.TopEdge)!==0;readonly property bool atBottom:(modelData&Qt.BottomEdge)!==0
                x:atLeft?0:atRight?floating.width-6:6;y:atTop?0:atBottom?floating.height-6:6
                width:atLeft||atRight?6:floating.width-12;height:atTop||atBottom?6:floating.height-12
                cursorShape:(atLeft||atRight)&&(atTop||atBottom)?(atLeft===atTop?Qt.SizeFDiagCursor:Qt.SizeBDiagCursor):atLeft||atRight?Qt.SizeHorCursor:Qt.SizeVerCursor
                onPressed:floating.startSystemResize(modelData)
            }
        }
    }
    DesktopLyricsWindow {id:desktop;onSettingsRequested:{if(root.visibility===Window.Minimized)root.showNormal();else root.show();root.raise();root.requestActivate();root.openSettings(3,3)}
        foregroundFullscreen:mainChrome.foregroundFullscreen;controller:backend;settingsRevision:root.settingsRevision
        onVisibleChanged:backend.setUiVisible((root.visible&&root.visibility!==Window.Minimized)||desktop.visible||floating.visible||trayWindow.visible)
    }
    Window {
        id:trayWindow;objectName:"tray";width:320;height:326;visible:false;color:"transparent";flags:Qt.Tool|Qt.FramelessWindowHint|Qt.WindowStaysOnTopHint;transientParent:null
        onVisibleChanged:backend.setUiVisible((root.visible&&root.visibility!==Window.Minimized) || desktop.visible || floating.visible || trayWindow.visible)
        onActiveChanged:if(visible&&!active)trayFocusGuard.restart()
        Shortcut{sequence:"Escape";enabled:trayWindow.visible;onActivated:trayWindow.hide()}
        AmbientScene{id:trayBackdrop;anchors.fill:parent;visible:false}
        GlassMaterial {configuration:root.material("popup");motion:root.motion;anchors.fill:parent;anchors.margins:1;sourceItem:trayBackdrop;glassEnabled:root.liquidStyle;cornerRadius:design.trayRadius;dark:!root.light;active:trayWindow.visible;backdropColor:root.bg;tintColor:design.elevated;tintStrength:root.light?.10+root.glass*.44:.04+root.glass*.25;fallbackColor:design.elevated
            ColumnLayout {anchors.fill:parent;anchors.margins:14;spacing:6
                Caption{text:backend.status;Layout.fillWidth:true;elide:Text.ElideRight}
                RowLayout {Layout.fillWidth:true;spacing:12
                    Image{source:backend.artwork;Layout.preferredWidth:38;Layout.preferredHeight:38;fillMode:Image.PreserveAspectFit;visible:backend.artwork.length>0}
                    ColumnLayout{Layout.fillWidth:true;spacing:2
                        Label{textFormat:Text.PlainText;text:backend.title||qsTr("No media");color:root.fg;font.pixelSize:17;elide:Text.ElideRight;Layout.fillWidth:true}
                        Caption{text:backend.artist;visible:text.length>0;elide:Text.ElideRight;Layout.fillWidth:true}
                    }
                }
                Transport{compact:true;backdrop:trayBackdrop;Layout.alignment:Qt.AlignHCenter}
                TrackSlider{Layout.fillWidth:true;implicitHeight:20;from:0;to:Math.max(1,backend.duration);value:backend.position;enabled:root.supports("seek");Accessible.name:qsTr("Playback position");onPressedChanged:if(!pressed)backend.command("seek",value)}
                RowLayout{Layout.fillWidth:true
                    Action{text:qsTr("Open window");implicitHeight:34;onClicked:{trayWindow.hide();root.show();root.raise();root.requestActivate()}}
                    Item{Layout.fillWidth:true}
                    Action{text:qsTr("Settings");iconName:"settings";implicitHeight:34;onClicked:{trayWindow.hide();root.openSettings(0,0);root.show();root.raise();root.requestActivate()}}
                }
                RowLayout{Layout.fillWidth:true
                    Action{text:qsTr("Desktop lyrics");implicitHeight:34;checked:desktop.requestedVisible;onClicked:desktop.toggleRequested()}
                    Action{text:qsTr("Floating player");implicitHeight:34;checked:floating.visible;onClicked:floating.visible=!floating.visible}
                }
                RowLayout{Layout.fillWidth:true
                    Action{text:root.desktopClickThrough?qsTr("Unlock lyrics"):qsTr("Lock lyrics");visible:desktop.requestedVisible;implicitHeight:32;onClicked:{root.save("desktopClickThrough",false);desktop.editRequested()}}
                    Item{Layout.fillWidth:true}
                    Action{text:qsTr("Quit");implicitHeight:32;onClicked:Qt.quit()}
                }
            }
        }
    }

    NativeFrame.NativeWindowController{id:mainChrome;objectName:"mainChrome";window:root;titleBar:titleDragRegion;minimizeButton:minimizeControl;maximizeButton:maximizeControl;closeButton:closeControl
        Component.onCompleted:setHitTestVisible(settingsGear,true)
    }
    Item{id:titleDragRegion;objectName:"titleDragRegion";anchors.top:parent.top;anchors.left:parent.left;anchors.right:parent.right;height:44;z:50
        RowLayout{id:captionControls;anchors.top:parent.top;anchors.right:parent.right;anchors.margins:4;spacing:2;opacity:root.chromeRevealed?1:0
            Behavior on opacity{NumberAnimation{duration:design.fast}}
            WindowButton{id:minimizeControl;objectName:"minimizeControl";text:qsTr("Minimize");iconName:"minus";onClicked:mainChrome.minimize()}
            WindowButton{id:maximizeControl;objectName:"maximizeControl";text:mainChrome.maximized?qsTr("Restore"):qsTr("Maximize");iconName:mainChrome.maximized?"copy":"square";onClicked:mainChrome.toggleMaximized()}
            WindowButton{id:closeControl;objectName:"closeControl";text:qsTr("Close");iconName:"x";onClicked:mainChrome.close()}
        }
        Action{id:settingsGear;objectName:"settingsGear";anchors.right:captionControls.left;anchors.rightMargin:8;anchors.top:parent.top;anchors.topMargin:4;implicitHeight:36;text:qsTr("Settings");iconName:"settings";opacity:root.gearRevealed?1:0;onClicked:root.openSettings(0,0)
            Behavior on opacity{NumberAnimation{duration:design.fast}}
        }
    }
    Shortcut{sequence:root.pref("shortcut/settings","Ctrl+,");onActivated:root.openSettings(0,0)}
    Shortcut{sequence:root.pref("shortcut/devices","Ctrl+Shift+D");onActivated:root.openSettings(1,0)}
    Shortcut{sequence:root.pref("shortcut/desktop","Ctrl+Shift+L");onActivated:desktop.toggleRequested()}
    Shortcut{sequence:root.pref("shortcut/floating","Ctrl+Shift+P");onActivated:floating.visible=!floating.visible}
    Shortcut{sequence:"Escape";enabled:root.page!==0;onActivated:root.page=0}
    Popup{
        id:volumePopup;parent:volumeButton;x:(parent.width-width)/2;y:-height-6;width:56;height:174;padding:8
        modal:false;focus:false;closePolicy:Popup.CloseOnEscape|Popup.CloseOnPressOutsideParent
        property bool pinned:false
        property real requestedVolume:0
        onClosed:{pinned=false;volumeDismiss.stop()}
        enter:Transition{NumberAnimation{property:"opacity";from:0;to:1;duration:root.motion?140:0}}
        exit:Transition{NumberAnimation{property:"opacity";from:1;to:0;duration:root.motion?140:0}}
        background:GlassMaterial{sourceItem:ambient;configuration:root.material("popup");glassEnabled:root.liquidStyle;active:volumePopup.visible;dark:!root.light;backdropColor:root.bg;fallbackColor:design.elevated;motion:root.motion}
        contentItem:ColumnLayout{spacing:8
            Caption{Layout.alignment:Qt.AlignHCenter;text:backend.volumeKnown?Math.round((volumeSlider.pressed?volumeSlider.value:backend.volume)*100)+"%":"—"}
            Slider{
                id:volumeSlider;Layout.fillHeight:true;Layout.alignment:Qt.AlignHCenter;implicitWidth:36;orientation:Qt.Vertical
                from:0;to:1;value:backend.volume;enabled:root.supports("volume")&&backend.volumeKnown;wheelEnabled:true;Accessible.name:qsTr("Phone volume")
                onMoved:{volumePopup.requestedVolume=value;if(!volumeSend.running)volumeSend.start()}
                onPressedChanged:if(pressed)volumePopup.requestedVolume=value;else{volumeSend.restart();value=Qt.binding(function(){return backend.volume});volumeDismiss.restart()}
                background:Rectangle{x:volumeSlider.leftPadding+(volumeSlider.availableWidth-width)/2;y:volumeSlider.topPadding;width:4;implicitHeight:112;height:volumeSlider.availableHeight;radius:2;color:root.light?"#24303b60":"#30e0e5ff"
                    Rectangle{x:0;y:volumeSlider.visualPosition*parent.height;width:4;height:Math.max(0,parent.height-y);radius:2;color:root.accent}
                }
                handle:Rectangle{x:volumeSlider.leftPadding+(volumeSlider.availableWidth-width)/2;y:volumeSlider.topPadding+volumeSlider.visualPosition*(volumeSlider.availableHeight-height);implicitWidth:volumeSlider.pressed?16:12;implicitHeight:width;radius:width/2;color:root.accent;border.width:volumeSlider.visualFocus?2:0;border.color:root.fg}
            }

        }
        Timer{id:volumeSend;interval:80;onTriggered:if(backend.volumeKnown)backend.command("volume",volumePopup.requestedVolume)}
        HoverHandler{id:volumeHover;onHoveredChanged:if(hovered)volumeDismiss.stop();else volumeDismiss.restart()}
        Connections{target:backend;function onCommandFinished(requestId,success,error){if(!volumeSlider.pressed)volumeSlider.value=Qt.binding(function(){return backend.volume})}}
        Shortcut{sequence:"Escape";enabled:volumePopup.visible;onActivated:volumePopup.close()}
    }
    Timer{id:volumeDismiss;interval:350;onTriggered:if(!volumePopup.pinned&&!volumeButton.hovered&&!volumeHover.hovered&&!volumeSlider.pressed)volumePopup.close()}

    Timer{id:trayFocusGuard;interval:180;onTriggered:if(trayWindow.visible&&!trayWindow.active)trayWindow.hide()}
    Popup { id: feedback; background:Rectangle{radius:14;color:design.elevated;border.color:design.border;border.width:1} padding:18; anchors.centerIn: parent; width:Math.min(500,root.width-60); modal:false; closePolicy:Popup.CloseOnEscape|Popup.CloseOnPressOutside
        contentItem: Label { id: feedbackText; wrapMode:Text.Wrap; color:fg }
        Timer { id:feedbackTimer; interval:6500; onTriggered:feedback.close() }
    }
    Connections { target: backend; ignoreUnknownSignals: true
        function onSettingsChanged() { root.settingsRevision++ }
        function onNotification(message) { feedbackText.text=message; feedback.open(); feedbackTimer.restart() }
        function onUpdateAvailable(version,url) { feedbackText.text=version+"\n"+url;feedback.open() }
        function onShowWindow() { root.show(); root.raise(); root.requestActivate() }
    }
}





