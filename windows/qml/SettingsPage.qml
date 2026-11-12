pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "Icons.js" as Icons

// Controller contract: settingsCatalog supplies flat, localized descriptors;
// values stay in the controller, and every editor commits through its public API.
Item {
    id: page
    property var controller
    property int settingsRevision: 0
    property int initialGroup: 0
    property int initialSection: 0
    property bool light: false
    property bool motion: true
    property color accent: "#bac9ed"
    property color textColor: light?"#202431":"#f5f5fa"
    property url iconFontSource: "qrc:/icons/lucide.ttf"
    signal closeRequested()
    signal desktopLyricsRequested()
    signal floatingRequested()
    signal importLyricsRequested()
    signal matchLyricsRequested()
    signal searchDevicesRequested()
    signal forgetDeviceRequested(string url)
    signal actionRequested(string key)
    property int activeGroup: initialGroup
    property int activeSection: initialSection
    property string appearanceScope: "global"
    property string highlightedKey: ""
    property var expandedGroups: ({})
    property var pendingReset: ({})
    property var scrollPositions: ({})
    property string scrollKey:"0/0"
    property var navigationHistory: []
    property bool restoringHistory:false
    function rememberSection(){
        if(!restoreScroll.running&&contentScroll.contentItem)scrollPositions[scrollKey]=contentScroll.contentItem.contentY
        highlightedKey="";restoreScroll.restart()
    }
    Timer{id:restoreScroll;interval:0;onTriggered:{
        const key=page.activeGroup+"/"+page.activeSection
        if(key!==page.scrollKey){if(!page.restoringHistory)page.navigationHistory=page.navigationHistory.concat([page.scrollKey]).slice(-40);page.scrollKey=key}
        page.restoringHistory=false
        if(contentScroll.contentItem)contentScroll.contentItem.contentY=Math.max(0,Math.min(page.scrollPositions[key]||0,Math.max(0,contentScroll.contentItem.contentHeight-contentScroll.contentItem.height)))
    }}
    function goBack(){if(!navigationHistory.length)return;const history=navigationHistory.slice();const key=history.pop().split("/");navigationHistory=history;restoringHistory=true;navigate(Number(key[0]),Number(key[1]))}
    Shortcut{sequence:"Alt+Left";enabled:page.visible&&page.navigationHistory.length>0;onActivated:page.goBack()}

    readonly property bool advancedExpanded: !!expandedGroups[String(activeGroup)]
    readonly property var groupNames: [qsTr("General"),qsTr("Devices and connection"),qsTr("Media"),qsTr("Lyrics"),qsTr("Appearance"),qsTr("Advanced")]
    readonly property var sectionNames: [
        [qsTr("Language and localization"),qsTr("Startup and exit"),qsTr("Window behavior"),qsTr("Tray and shortcuts")],
        [qsTr("Paired devices"),qsTr("Discovery and pairing"),qsTr("Connection and reconnection"),qsTr("Network and security")],
        [qsTr("Player selection"),qsTr("Playback controls"),qsTr("Windows SMTC"),qsTr("Volume control")],
        [qsTr("Lyric sources"),qsTr("Automatic matching"),qsTr("Main lyrics"),qsTr("Desktop lyrics"),qsTr("Animation and sync")],
        [qsTr("Theme and colors"),qsTr("Ambient background"),qsTr("Glass material"),qsTr("Fonts and motion")],
        [qsTr("Performance and rendering"),qsTr("Cache and data"),qsTr("Updates"),qsTr("Logs and diagnostics")]
    ]
    readonly property var catalog: controller?controller.settingsCatalog:[]
    readonly property bool appearanceSection: activeGroup===4 && (activeSection===1||activeSection===2)
    readonly property var appearanceSnapshot: {settingsRevision;return controller?controller.appearanceConfig(appearanceScope):({})}
    readonly property bool inherited: {settingsRevision;return controller&&appearanceScope!=="global"?controller.appearanceInherited(appearanceScope):false}
    readonly property var scopes: activeSection===1?
        [{label:qsTr("Global"),value:"global"},{label:qsTr("Main background"),value:"background"}]:
        [{label:qsTr("Global"),value:"global"},{label:qsTr("Playback dock"),value:"dock"},{label:qsTr("Popups"),value:"popup"},{label:qsTr("Floating player"),value:"floating"},{label:qsTr("Desktop lyrics"),value:"desktop"}]
    readonly property var selectedRows: catalog.filter(function(row){return row.group===activeGroup&&row.section===activeSection&&(!row.advanced||advancedExpanded)})
    readonly property var materialRows: {
        if(!controller||!appearanceSection)return []
        return controller.appearanceDescriptors.filter(function(row){return (row.group===(activeSection===1?"background":"glass")||row.group==="rendering")&&(!row.advanced||advancedExpanded)})
    }
    readonly property bool hasAdvanced: catalog.some(function(row){return row.group===activeGroup&&row.section===activeSection&&row.advanced}) || appearanceSection
    readonly property var searchResults: {
        const query=searchField.text.trim().toLocaleLowerCase()
        if(!query)return []
        let rows=catalog.slice()
        if(controller)controller.appearanceDescriptors.forEach(function(row){rows.push({key:"appearance/"+row.key,label:row.label,group:4,section:row.group==="background"?1:2,advanced:row.advanced})})
        return rows.filter(function(row){return (row.label+" "+row.key+" "+(row.tags||"")+" "+(row.help||"")).toLocaleLowerCase().indexOf(query)>=0}).slice(0,60)
    }
    onInitialGroupChanged: {activeGroup=Math.max(0,Math.min(5,initialGroup));activeSection=0}
    onInitialSectionChanged: activeSection=Math.max(0,Math.min(sectionNames[activeGroup].length-1,initialSection))
    onActiveGroupChanged: rememberSection()
    onActiveSectionChanged: {
        highlightedKey=""
        if(appearanceSection && !scopes.some(function(row){return row.value===appearanceScope}))appearanceScope="global"
        rememberSection()
    }
    function value(key,fallback){settingsRevision;return controller?controller.setting(key,fallback):fallback}
    function save(key,value){if(controller)controller.setSetting(key,value)}
    function navigate(group,section){activeGroup=Math.max(0,Math.min(5,group));activeSection=Math.max(0,Math.min(sectionNames[activeGroup].length-1,section));searchField.clear()}
    function expandAdvanced(){const state=Object.assign({},expandedGroups);state[String(activeGroup)]=true;expandedGroups=state}
    function toggleAdvanced(){const state=Object.assign({},expandedGroups);state[String(activeGroup)]=!advancedExpanded;expandedGroups=state}
    function jumpTo(row){
        activeGroup=row.group;activeSection=row.section
        if(row.advanced)expandAdvanced()
        searchField.text="";highlightedKey=row.key
        jumpTimer.restart();highlightTimer.restart()
    }
    function scrollToKey(key){
        let target=null
        for(let i=0;i<normalRows.count;i++){const row=normalRows.itemAt(i);if(row&&row.descriptor.key===key){target=row;break}}
        if(!target)for(let j=0;j<appearanceRows.count;j++){const row=appearanceRows.itemAt(j);if(row&&"appearance/"+row.descriptor.key===key){target=row;break}}
        if(target){const point=target.mapToItem(contentColumn,0,0);const flick=contentScroll.contentItem;flick.contentY=Math.max(0,Math.min(point.y-12,Math.max(0,flick.contentHeight-flick.height)));target.forceActiveFocus()}
    }
    function resetCurrent(){pendingReset=appearanceSection?{kind:"appearance",scope:appearanceScope}:{kind:"section",group:activeGroup,section:activeSection};resetDialog.open()}
    function runAction(row){
        const action=row.action||row.key
        switch(action){
        case "desktopLyrics":desktopLyricsRequested();break
        case "floating":floatingRequested();break
        case "importLyrics":importLyricsRequested();break
        case "matchLyrics":matchLyricsRequested();break
        case "searchDevices":searchDevicesRequested();break
        case "disconnect":controller.disconnectDevice();break
        case "retryLyrics":controller.fetchLyrics();break
        case "checkUpdates":controller.checkUpdates();break
        case "exportSettings":exportFile.open();break
        case "importSettings":importFile.open();break
        case "exportDiagnostics":diagnosticsFile.open();break
        case "clearLyricsCache":cacheDialog.open();break
        case "resetSettings":pendingReset={kind:"all"};resetDialog.open();break
        case "quit":Qt.quit();break
        default:actionRequested(action);break
        }
    }
    DesignTokens {id:tokens;light:page.light;motion:page.motion;accent:page.accent}
    FontLoader {id:lucide;source:page.iconFontSource}
    Timer {id:jumpTimer;interval:0;onTriggered:Qt.callLater(function(){page.scrollToKey(page.highlightedKey)})}
    Timer {id:highlightTimer;interval:2200;onTriggered:page.highlightedKey=""}

    component FlatButton: Button {
        id:button
        implicitHeight:36;implicitWidth:Math.max(36,contentItem.implicitWidth+22)
        font.family:tokens.family;font.pixelSize:tokens.body
        hoverEnabled:true
        contentItem:Label{text:button.text;textFormat:Text.PlainText;color:button.enabled?page.textColor:tokens.faintText;font:button.font;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter;elide:Text.ElideRight}
        background:Rectangle{radius:tokens.radius;color:button.down?tokens.pressed:button.checked?tokens.selected:button.hovered?tokens.hover:"transparent";border.color:button.visualFocus?page.accent:"transparent";border.width:button.visualFocus?1:0;Behavior on color{ColorAnimation{duration:tokens.fast}}}
    }
    component Field: TextField {
        id:field
        implicitHeight:36;selectByMouse:true;color:page.textColor;selectionColor:page.accent;selectedTextColor:tokens.accentText
        placeholderTextColor:tokens.faintText;font.family:tokens.family;font.pixelSize:tokens.body;padding:9
        background:Rectangle{radius:tokens.radius;color:tokens.surface;border.width:1;border.color:field.visualFocus?page.accent:tokens.border}
    }
    component Choice: ComboBox {
        id:choice
        implicitHeight:36;implicitWidth:180
        font.family:tokens.family;font.pixelSize:tokens.body
        textRole:"label";valueRole:"value"
        palette.text:page.textColor;palette.buttonText:page.textColor;palette.base:tokens.elevated;palette.window:tokens.elevated;palette.highlight:page.accent;palette.highlightedText:tokens.accentText
        background:Rectangle{radius:tokens.radius;color:choice.pressed?tokens.pressed:tokens.surface;border.width:1;border.color:choice.visualFocus?page.accent:tokens.border}
        contentItem:Label{leftPadding:10;rightPadding:30;text:choice.displayText;textFormat:Text.PlainText;color:page.textColor;verticalAlignment:Text.AlignVCenter;elide:Text.ElideRight;font:choice.font}
        indicator:Text{x:choice.width-width-11;y:(choice.height-height)/2;text:Icons.glyph("chevron-down");font.family:lucide.name;font.pixelSize:14;color:tokens.secondaryText}
        delegate:ItemDelegate{required property var modelData;required property int index;width:choice.width;height:38;text:modelData.label;highlighted:choice.highlightedIndex===index;contentItem:Label{text:parent.text;textFormat:Text.PlainText;color:page.textColor;verticalAlignment:Text.AlignVCenter;leftPadding:10;elide:Text.ElideRight}background:Rectangle{color:parent.highlighted?tokens.selected:parent.hovered?tokens.hover:tokens.elevated}}
    }
    component Toggle: Switch {
        id:toggle
        implicitWidth:50;implicitHeight:32;padding:0
        indicator:Rectangle{implicitWidth:42;implicitHeight:24;x:(toggle.width-width)/2;y:(toggle.height-height)/2;radius:12;color:toggle.checked?page.accent:tokens.surface;border.width:1;border.color:toggle.visualFocus?page.accent:tokens.border;Rectangle{width:18;height:18;radius:9;x:toggle.checked?21:3;y:3;color:toggle.checked?tokens.accentText:tokens.secondaryText;Behavior on x{NumberAnimation{duration:tokens.fast;easing.type:Easing.OutCubic}}}}
    }
    component ParameterRow: Item {
        id:row
        required property var descriptor
        property bool appearance: false
        property string key: descriptor.key||""
        property string kind: descriptor.type||"bool"
        property var currentValue: {page.settingsRevision;return appearance?(page.appearanceSnapshot[key]===undefined?descriptor.defaultValue:page.appearanceSnapshot[key]):page.value(key,descriptor.defaultValue)}
        property bool editable: !appearance||!page.inherited
        implicitHeight:44
        Layout.fillWidth:true
        objectName:"setting-"+(appearance?"appearance/":"")+key
        activeFocusOnTab:true
        function commit(value){if(!page.controller)return;if(appearance)page.controller.setAppearance(page.appearanceScope,key,value);else page.save(key,value)}
        Rectangle{anchors.fill:parent;radius:tokens.radius;color:page.highlightedKey===(row.appearance?"appearance/":"")+row.key?tokens.selected:"transparent"}
        RowLayout {
            anchors.fill:parent;anchors.leftMargin:8;anchors.rightMargin:4;spacing:8
            Label{Layout.fillWidth:true;text:row.descriptor.label||row.key;textFormat:Text.PlainText;color:page.textColor;font.family:tokens.family;font.pixelSize:tokens.body;elide:Text.ElideRight;Accessible.ignored:true}
            FlatButton {visible:!!row.descriptor.help;text:Icons.glyph("info");font.family:lucide.name;font.pixelSize:16;implicitWidth:30;Accessible.name:qsTr("Help for %1").arg(row.descriptor.label);ToolTip.visible:hovered||activeFocus;ToolTip.text:row.descriptor.help||""}
            Toggle{objectName:"editor-"+row.key;visible:row.kind==="bool";enabled:row.editable;checked:row.currentValue===true;Accessible.name:row.descriptor.label;onClicked:row.commit(checked)}
            RowLayout {
                visible:row.kind==="real"||row.kind==="number"||row.kind==="int"
                enabled:row.editable;spacing:8;Layout.fillWidth:false;Layout.preferredWidth:Math.min(260,page.width*.43);Layout.maximumWidth:Layout.preferredWidth
                Slider {
                    id:numberSlider;Layout.fillWidth:true;implicitHeight:32
                    from:row.descriptor.min===undefined?0:row.descriptor.min;to:row.descriptor.max===undefined?1:row.descriptor.max;stepSize:row.descriptor.step||.01
                    value:Number(row.currentValue);Accessible.name:row.descriptor.label;onMoved:row.commit(row.kind==="int"?Math.round(value):value)
                    background:Rectangle{x:numberSlider.leftPadding;y:numberSlider.topPadding+(numberSlider.availableHeight-height)/2;implicitHeight:4;width:numberSlider.availableWidth;height:implicitHeight;radius:2;color:tokens.border;Rectangle{width:numberSlider.visualPosition*parent.width;height:parent.height;radius:2;color:page.accent}}
                    handle:Rectangle{x:numberSlider.leftPadding+numberSlider.visualPosition*(numberSlider.availableWidth-width);y:numberSlider.topPadding+(numberSlider.availableHeight-height)/2;implicitWidth:14;implicitHeight:14;radius:7;color:numberSlider.enabled?page.accent:tokens.faintText;border.width:numberSlider.visualFocus?2:0;border.color:page.textColor}
                }
                Field {
                    id:numberField;objectName:"number-"+row.key;Layout.preferredWidth:70
                    text:Number(row.currentValue).toFixed(row.kind==="int"?0:(row.descriptor.step||1)<.1?2:(row.descriptor.step||1)<1?1:0)
                    horizontalAlignment:Text.AlignRight;inputMethodHints:Qt.ImhFormattedNumbersOnly
                    Accessible.name:row.descriptor.label
                    onEditingFinished:{const value=Number(text);if(text.trim().length&&isFinite(value)&&value>=numberSlider.from&&value<=numberSlider.to)row.commit(row.kind==="int"?Math.round(value):value);text=Number(row.currentValue).toFixed(row.kind==="int"?0:(row.descriptor.step||1)<.1?2:(row.descriptor.step||1)<1?1:0)}
                }
            }
            Choice {
                objectName:"enum-"+row.key
                visible:row.kind==="enum";enabled:row.editable;Layout.preferredWidth:Math.min(240,page.width*.42);Layout.maximumWidth:Layout.preferredWidth
                model:row.descriptor.options||[]
                currentIndex:{const options=row.descriptor.options||[];for(let i=0;i<options.length;i++)if(options[i].value===row.currentValue)return i;return -1}
                Accessible.name:row.descriptor.label
                onActivated:if(currentIndex>=0)row.commit(row.descriptor.options[currentIndex].value)
            }
            RowLayout {
                visible:row.kind==="color";enabled:row.editable;spacing:6
                Field {id:colorText;objectName:"color-"+row.key;Layout.preferredWidth:110;text:String(row.currentValue);Accessible.name:row.descriptor.label;onEditingFinished:{row.commit(text.trim());text=String(row.currentValue)}}
                FlatButton {implicitWidth:32;Accessible.name:qsTr("Choose %1").arg(row.descriptor.label);onClicked:{colorPicker.selectedColor=row.currentValue==="auto"?page.accent:String(row.currentValue);colorPicker.open()}background:Rectangle{anchors.centerIn:parent;width:24;height:24;radius:7;color:row.currentValue==="auto"?page.accent:String(row.currentValue);border.width:1;border.color:tokens.border}}
                ColorDialog{id:colorPicker;title:row.descriptor.label;onAccepted:row.commit(selectedColor.toString())}
            }
            Field {
                objectName:"shortcut-"+row.key
                visible:row.kind==="shortcut";enabled:row.editable;Layout.preferredWidth:Math.min(230,page.width*.42);Layout.maximumWidth:Layout.preferredWidth
                text:String(row.currentValue||"");placeholderText:qsTr("Press shortcut");Accessible.name:row.descriptor.label
                selectByMouse:true
                Keys.onPressed:function(event){
                    if(event.key===Qt.Key_Tab||event.key===Qt.Key_Backtab)return
                    if([Qt.Key_Control,Qt.Key_Shift,Qt.Key_Alt,Qt.Key_Meta].indexOf(event.key)>=0){event.accepted=true;return}
                    if(event.key===Qt.Key_Backspace||event.key===Qt.Key_Delete){text="";row.commit("");event.accepted=true;return}
                    let keyName=""
                    if(event.key>=Qt.Key_F1&&event.key<=Qt.Key_F35)keyName="F"+(event.key-Qt.Key_F1+1)
                    else if(event.key>=Qt.Key_A&&event.key<=Qt.Key_Z)keyName=String.fromCharCode(event.key)
                    else if(event.key>=Qt.Key_0&&event.key<=Qt.Key_9)keyName=String.fromCharCode(event.key)
                    else {const names={};names[Qt.Key_Space]="Space";names[Qt.Key_Left]="Left";names[Qt.Key_Right]="Right";names[Qt.Key_Up]="Up";names[Qt.Key_Down]="Down";names[Qt.Key_Home]="Home";names[Qt.Key_End]="End";names[Qt.Key_PageUp]="PgUp";names[Qt.Key_PageDown]="PgDown";keyName=names[event.key]||""}
                    if(!keyName)return
                    let parts=[];if(event.modifiers&Qt.ControlModifier)parts.push("Ctrl");if(event.modifiers&Qt.AltModifier)parts.push("Alt");if(event.modifiers&Qt.ShiftModifier)parts.push("Shift");if(event.modifiers&Qt.MetaModifier)parts.push("Meta");parts.push(keyName)
                    text=parts.join("+");row.commit(text);event.accepted=true
                }
                onEditingFinished:row.commit(text.trim())
            }
            FlatButton {visible:row.kind==="action";text:row.descriptor.buttonLabel||qsTr("Open");Accessible.name:row.descriptor.label;onClicked:page.runAction(row.descriptor)}
            Label {visible:row.kind==="readonly";Layout.maximumWidth:Math.min(260,page.width*.45);text:String(row.currentValue||row.descriptor.value||"");textFormat:Text.PlainText;color:tokens.secondaryText;elide:Text.ElideRight;Accessible.name:row.descriptor.label}
        }
    }

    ColumnLayout {
        anchors.fill:parent;anchors.leftMargin:Math.max(0,(page.width-760)/2);anchors.rightMargin:anchors.leftMargin;spacing:10
        RowLayout {
            Layout.fillWidth:true
            Label{text:qsTr("Settings");font.family:tokens.family;font.pixelSize:tokens.heading;color:page.textColor;Layout.fillWidth:true}
            FlatButton{text:Icons.glyph("x");font.family:lucide.name;implicitWidth:36;Accessible.name:qsTr("Close settings");onClicked:page.closeRequested()}
        }
        Field{id:searchField;objectName:"settingsSearch";Layout.fillWidth:true;placeholderText:qsTr("Search settings");Accessible.name:placeholderText}
        Flow {
            Layout.fillWidth:true;spacing:4
            Repeater {model:page.groupNames
                FlatButton{required property int index;required property string modelData;text:modelData;checked:page.activeGroup===index;onClicked:{page.activeGroup=index;page.activeSection=0;searchField.clear()}}
            }
        }
        Flow {
            Layout.fillWidth:true;spacing:3
            Repeater {model:page.sectionNames[page.activeGroup]
                FlatButton{required property int index;required property string modelData;text:modelData;font.pixelSize:tokens.caption;implicitHeight:32;checked:page.activeSection===index;onClicked:{page.activeSection=index;searchField.clear()}}
            }
        }
        ScrollView {
            id:resultsScroll;visible:searchField.text.trim().length>0;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;contentWidth:availableWidth
            Column {
                width:resultsScroll.availableWidth;spacing:4
                Repeater {model:page.searchResults
                    ItemDelegate {
                        id:searchHit;required property var modelData;width:parent.width;height:52;objectName:"search-"+modelData.key
                        Accessible.name:modelData.label
                        onClicked:page.jumpTo(modelData)
                        contentItem:Column{spacing:3;Label{text:searchHit.modelData.label;textFormat:Text.PlainText;color:page.textColor;font.pixelSize:tokens.body}Label{text:page.groupNames[searchHit.modelData.group]+" / "+page.sectionNames[searchHit.modelData.group][searchHit.modelData.section];color:tokens.secondaryText;font.pixelSize:tokens.caption}}
                        background:Rectangle{radius:tokens.radius;color:searchHit.hovered?tokens.hover:"transparent";border.width:searchHit.visualFocus?1:0;border.color:page.accent}
                    }
                }
                Label{visible:page.searchResults.length===0;text:qsTr("No matching settings");color:tokens.secondaryText;padding:12}
            }
        }
        ScrollView {
            id:contentScroll;objectName:"settingsScroll";visible:!resultsScroll.visible;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;contentWidth:availableWidth
            ScrollBar.vertical:ScrollBar{policy:ScrollBar.AsNeeded;contentItem:Rectangle{implicitWidth:5;radius:3;color:tokens.secondaryText;opacity:.45}}
            ColumnLayout {
                id:contentColumn;width:contentScroll.availableWidth;spacing:3
                ColumnLayout {
                    visible:page.activeGroup===1&&(page.activeSection===0||page.activeSection===1);Layout.fillWidth:true;spacing:8
                    RowLayout {
                        Layout.fillWidth:true
                        Label{text:page.controller?page.controller.status:"";textFormat:Text.PlainText;color:tokens.secondaryText;Layout.fillWidth:true;elide:Text.ElideRight}
                        FlatButton{text:qsTr("Search devices");onClicked:page.searchDevicesRequested()}
                        FlatButton{text:qsTr("Disconnect");visible:!!page.controller&&page.controller.online;onClicked:page.controller.disconnectDevice()}
                    }
                    ListView {
                        id:deviceList;Layout.fillWidth:true;Layout.preferredHeight:Math.max(64,Math.min(4,count)*56);clip:true;spacing:4
                        model:page.controller?page.controller.devices:[]
                        delegate:ItemDelegate {
                            id:deviceRow;required property var modelData;width:deviceList.width;height:52
                            Accessible.name:qsTr("Connect to %1").arg(modelData.name||qsTr("Android device"))
                            onClicked:page.controller.connectDevice(modelData.url)
                            background:Rectangle{radius:tokens.radius;color:deviceRow.hovered?tokens.hover:tokens.selected;border.width:deviceRow.visualFocus?1:0;border.color:page.accent}
                            contentItem:RowLayout {
                                spacing:10
                                Text{text:Icons.glyph("smartphone");font.family:lucide.name;font.pixelSize:22;color:page.accent}
                                Label{text:deviceRow.modelData.name||qsTr("Android device");textFormat:Text.PlainText;color:page.textColor;Layout.fillWidth:true;elide:Text.ElideRight}
                                FlatButton {visible:deviceRow.modelData.paired===true;text:Icons.glyph("unlink");font.family:lucide.name;implicitWidth:36;Accessible.name:qsTr("Forget device");ToolTip.visible:hovered;ToolTip.text:qsTr("Forget device");onClicked:page.forgetDeviceRequested(deviceRow.modelData.url)}
                                Text{text:Icons.glyph("chevron-right");font.family:lucide.name;font.pixelSize:18;color:tokens.secondaryText}
                            }
                        }
                        Label{anchors.centerIn:parent;visible:deviceList.count===0;text:qsTr("No devices");color:tokens.secondaryText}
                    }
                }
                ColumnLayout {
                    id:appearanceParametersPanel;objectName:"appearanceParametersPanel"
                    visible:page.appearanceSection;Layout.fillWidth:true;spacing:6
                    RowLayout {
                        Layout.fillWidth:true
                        Choice{objectName:"appearanceScope";Layout.fillWidth:true;Layout.maximumWidth:260;model:page.scopes;Accessible.name:qsTr("Appearance scope");currentIndex:{for(let i=0;i<page.scopes.length;i++)if(page.scopes[i].value===page.appearanceScope)return i;return 0}onActivated:page.appearanceScope=page.scopes[currentIndex].value}
                        Item{Layout.fillWidth:true}
                        Label{visible:page.appearanceScope!=="global";text:qsTr("Use global settings");color:page.textColor}
                        Toggle{visible:page.appearanceScope!=="global";checked:page.inherited;Accessible.name:qsTr("Use global settings");onClicked:page.controller.setAppearanceInherited(page.appearanceScope,checked)}
                        Choice{visible:page.appearanceScope==="global";Layout.preferredWidth:155;Accessible.name:qsTr("Appearance preset");model:[{label:qsTr("Preset"),value:""},{label:qsTr("Clear"),value:"clear"},{label:qsTr("Balanced"),value:"balanced"},{label:qsTr("Vivid"),value:"vivid"}];currentIndex:0;onActivated:if(currentIndex>0){page.controller.applyAppearancePreset(model[currentIndex].value);currentIndex=0}}
                    }
                }
                Repeater {id:normalRows;model:page.selectedRows;ParameterRow{required property var modelData;descriptor:modelData}}
                Repeater {id:appearanceRows;model:page.materialRows;ParameterRow{required property var modelData;descriptor:modelData;appearance:true}}
                RowLayout {
                    visible:page.hasAdvanced;Layout.fillWidth:true;Layout.topMargin:8
                    FlatButton{objectName:"advancedOptions";text:page.advancedExpanded?qsTr("Hide advanced options"):qsTr("Advanced options");checked:page.advancedExpanded;onClicked:page.toggleAdvanced()}
                    Item{Layout.fillWidth:true}
                }
                RowLayout {
                    Layout.fillWidth:true;Layout.topMargin:16;Layout.bottomMargin:8
                    Item{Layout.fillWidth:true}
                    FlatButton{text:qsTr("Restore section defaults");onClicked:page.resetCurrent()}
                }
            }
        }
    }
    Dialog {
        id:resetDialog;title:qsTr("Restore defaults?");modal:true;anchors.centerIn:Overlay.overlay;width:Math.min(420,page.width-24)
        standardButtons:Dialog.Ok|Dialog.Cancel
        palette.window:tokens.elevated;palette.windowText:page.textColor;palette.buttonText:page.textColor
        contentItem:Label{text:page.pendingReset.kind==="all"?qsTr("This replaces your settings. Paired devices are kept."):page.pendingReset.kind==="appearance"?qsTr("This restores all appearance parameters for this scope."):qsTr("This restores the options in this section.");color:page.textColor;wrapMode:Text.Wrap}
        onAccepted:{if(page.pendingReset.kind==="all")page.controller.resetSettings();else if(page.pendingReset.kind==="appearance")page.controller.resetAppearance(page.pendingReset.scope);else page.controller.resetSettingsGroup(page.pendingReset.group,page.pendingReset.section)}
    }
    Dialog {
        id:cacheDialog;title:qsTr("Clear lyrics cache?");modal:true;anchors.centerIn:Overlay.overlay;width:Math.min(420,page.width-24)
        standardButtons:Dialog.Ok|Dialog.Cancel
        contentItem:Label{text:qsTr("Cached lyrics will be removed.");color:page.textColor;wrapMode:Text.Wrap}
        palette.window:tokens.elevated;palette.windowText:page.textColor;palette.buttonText:page.textColor
        onAccepted:page.controller.clearLyricsCache()
    }
    FileDialog{id:exportFile;title:qsTr("Export settings");fileMode:FileDialog.SaveFile;defaultSuffix:"json";nameFilters:[qsTr("JSON files (*.json)")];onAccepted:page.controller.exportSettings(selectedFile)}
    FileDialog{id:importFile;title:qsTr("Import settings");nameFilters:[qsTr("JSON files (*.json)")];onAccepted:page.controller.importSettings(selectedFile)}
    FileDialog{id:diagnosticsFile;title:qsTr("Export diagnostics");fileMode:FileDialog.SaveFile;defaultSuffix:"json";nameFilters:[qsTr("JSON files (*.json)")];onAccepted:page.controller.exportDiagnostics(selectedFile)}
}
