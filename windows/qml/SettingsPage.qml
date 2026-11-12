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
    property Item materialSource: null
    readonly property real contentInset: Math.max(16,(width-700)/2)
    property bool componentReady: false
    property var collapsedSections: ({})
    property var appearanceScopes: ({})
    property var collapsedSubgroups: ({})
    property string selectedPresetName: ""
    property string presetNameAction: "save"
    property string presetRenameFrom: ""
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
    property string scrollKey: "0"
    property var navigationHistory: []
    property bool restoringHistory: false
    readonly property bool advancedExpanded: !!expandedGroups[activeGroup+"/"+activeSection]
    readonly property var groupNames: [qsTr("General"),qsTr("Devices and connection"),qsTr("Media"),qsTr("Lyrics"),qsTr("Appearance"),qsTr("Advanced")]
    readonly property var sectionNames: [
        [qsTr("Language and localization"),qsTr("Startup and exit"),qsTr("Window behavior"),qsTr("Tray and shortcuts")],
        [qsTr("Paired devices"),qsTr("Discovery and pairing"),qsTr("Connection and reconnection"),qsTr("Network and security")],
        [qsTr("Player selection"),qsTr("Playback controls"),qsTr("Windows SMTC"),qsTr("Volume control")],
        [qsTr("Lyric sources"),qsTr("Automatic matching"),qsTr("Main lyrics"),qsTr("Desktop lyrics"),qsTr("Animation and sync"),qsTr("Translation and romanization"),qsTr("Style presets")],
        [qsTr("Theme and colors"),qsTr("Ambient background"),qsTr("Glass material"),qsTr("Fonts and motion")],
        [qsTr("Performance and rendering"),qsTr("Cache and data"),qsTr("Updates"),qsTr("Logs and diagnostics")]
    ]
    readonly property var groupKeywords: [
        "general changgui tongyong language startup window tray 通用 常规 语言 启动 窗口 托盘 快捷键",
        "device connection shebei lianjie peidui wangluo 设备 手机 连接 配对 网络 安全 重连",
        "media player volume meiti bofang yinliang 媒体 播放器 控制 音量 系统 SMTC",
        "lyrics geci 字幕 歌词",
        "appearance theme glass waiguan zhuti boli 外观 主题 配色 背景 玻璃 字体 动画",
        "advanced performance cache update diagnostics gaoji xingneng huancun 高级 性能 缓存 更新 日志 诊断"
    ]
    readonly property var catalog: controller?controller.settingsCatalog:[]
    readonly property bool appearanceSection: activeGroup===4&&(activeSection===1||activeSection===2)
    readonly property var appearanceSnapshot: {settingsRevision;return controller?controller.appearanceConfig(appearanceScope):({})}
    readonly property bool inherited: {settingsRevision;return controller&&appearanceScope!=="global"?controller.appearanceInherited(appearanceScope):false}
    readonly property var providerRows: {
        settingsRevision
        const sources=controller&&controller.lyricProviders?controller.lyricProviders.slice():[]
        const order=String(value("lyrics/providerPriority","lrclib,netease,qq,kugou")).split(",")
        return sources.sort(function(a,b){const ai=order.indexOf(a.id),bi=order.indexOf(b.id);return (ai<0?100:ai)-(bi<0?100:bi)})
    }
    readonly property var lyricPresetRows: {settingsRevision;return controller&&controller.lyricPresets?controller.lyricPresets.map(function(row){return {label:row.label||row.name,value:row.name,builtIn:row.builtIn}}):[]}
    readonly property string effectivePresetName: lyricPresetRows.some(function(row){return row.value===selectedPresetName})?selectedPresetName:lyricPresetRows.length?lyricPresetRows[0].value:""
    readonly property bool selectedPresetBuiltIn: {for(let i=0;i<lyricPresetRows.length;i++)if(lyricPresetRows[i].value===effectivePresetName)return lyricPresetRows[i].builtIn;return true}
    readonly property var searchResults: {
        const query=searchField.text.trim().toLocaleLowerCase()
        if(!query)return []
        let rows=catalog.slice()
        if(controller)controller.appearanceDescriptors.forEach(function(row){rows.push({key:"appearance/"+row.key,label:row.label,group:4,section:row.group==="background"?1:2,advanced:row.advanced,tags:row.tags||""})})
        providerRows.forEach(function(row){rows.push({key:row.enabledKey||"lyrics/providers/"+row.id+"/enabled",label:row.name,group:3,section:0,tags:providerCapabilities(row.capabilities)+" provider source priority 歌词源 来源 优先级"})})
        for(let group=0;group<groupNames.length;group++){
            rows.push({key:"@group/"+group,label:groupNames[group],group:group,section:0,navigation:true})
            sectionsForGroup(group).forEach(function(section){rows.push({key:"@section/"+group+"/"+section.index,label:section.label,group:group,section:section.index,navigation:true})})
        }
        const terms=query.split(/\s+/).map(normalizedSearch)
        const seen={}
        return rows.filter(function(row){
            if(seen[row.key])return false
            seen[row.key]=true
            const index=normalizedSearch((row.label||"")+" "+row.key+" "+(row.tags||"")+" "+(row.help||"")+" "+groupNames[row.group]+" "+sectionTitle(row.group,row.section)+" "+groupKeywords[row.group])
            return terms.every(function(term){return index.indexOf(term)>=0})
        }).slice(0,100)
    }
    onInitialGroupChanged: if(componentReady)navigate(initialGroup,0)
    onInitialSectionChanged: if(componentReady)navigate(activeGroup,initialSection)
    onActiveGroupChanged: if(componentReady)rememberCategory()
    onActiveSectionChanged: {if(activeGroup===4)appearanceScope=scopeFor(activeSection)}
    onAppearanceScopeChanged: {
        if(activeGroup!==4)return
        const state=Object.assign({},appearanceScopes);state[String(activeSection)]=appearanceScope;appearanceScopes=state
    }
    Component.onCompleted: {componentReady=true;if(initialGroup||initialSection)navigate(initialGroup,initialSection)}
    function normalizedSearch(text){return String(text).toLocaleLowerCase().replace(/[\s_-]+/g,"")}
    function value(key,fallback){settingsRevision;return controller?controller.setting(key,fallback):fallback}
    function save(key,input){if(controller)controller.setSetting(key,input)}
    function sectionTitle(group,section){
        for(let i=0;i<catalog.length;i++)if(catalog[i].group===group&&catalog[i].section===section&&catalog[i].sectionLabel)return catalog[i].sectionLabel
        return sectionNames[group]&&sectionNames[group][section]?sectionNames[group][section]:qsTr("Other options")
    }
    function sectionsForGroup(group){
        const indexes=(sectionNames[group]||[]).map(function(_,i){return i})
        catalog.forEach(function(row){if(row.group===group&&indexes.indexOf(row.section)<0)indexes.push(row.section)})
        indexes.sort(function(a,b){return a-b})
        return indexes.map(function(index){return {index:index,label:sectionTitle(group,index)}})
    }
    function isCollapsed(group,section){return !!collapsedSections[group+"/"+section]}
    function setSectionExpanded(group,section,expanded){const state=Object.assign({},collapsedSections);state[group+"/"+section]=!expanded;collapsedSections=state}
    function advancedFor(group,section){return !!expandedGroups[group+"/"+section]}
    function setAdvanced(group,section,expanded){const state=Object.assign({},expandedGroups);state[group+"/"+section]=expanded;expandedGroups=state}
    function expandAdvanced(){setAdvanced(activeGroup,activeSection,true)}
    function toggleAdvanced(){setAdvanced(activeGroup,activeSection,!advancedExpanded)}
    function rowsFor(group,section){
        return catalog.filter(function(row){return row.group===group&&row.section===section&&(!row.advanced||advancedFor(group,section))&&row.key!=="lyrics/providerPriority"&&!(String(row.key).indexOf("lyrics/providers/")===0&&String(row.key).endsWith("/enabled"))})
    }
    function subgroupsFor(group,section){
        const keys=[];catalog.forEach(function(row){if(row.group===group&&row.section===section&&row.subgroup&&keys.indexOf(row.subgroup)<0)keys.push(row.subgroup)})
        const names={layout:qsTr("Typography"),current:qsTr("Current line"),inactive:qsTr("Other lines"),word:qsTr("Word highlighting"),scroll:qsTr("Scrolling"),translation:qsTr("Translation"),romanization:qsTr("Romanization")}
        return keys.map(function(key){let label=names[key]||key;for(let i=0;i<catalog.length;i++)if(catalog[i].group===group&&catalog[i].section===section&&catalog[i].subgroup===key&&catalog[i].subgroupLabel){label=catalog[i].subgroupLabel;break}return {key:key,label:label}})
    }
    function subgroupCollapsed(group,section,key){const value=collapsedSubgroups[group+"/"+section+"/"+key];return value===undefined?key!=="layout":value}
    function setSubgroupExpanded(group,section,key,expanded){const state=Object.assign({},collapsedSubgroups);state[group+"/"+section+"/"+key]=!expanded;collapsedSubgroups=state}
    function parameterDescriptors(section,includeAdvanced){
        if(!controller)return []
        return controller.appearanceDescriptors.filter(function(row){return (row.group===(section===1?"background":"glass")||row.group==="rendering")&&(includeAdvanced||!row.advanced)})
    }
    function scopeChoices(section){return section===1?[{label:qsTr("Global"),value:"global"},{label:qsTr("Main background"),value:"background"}]:[{label:qsTr("Global"),value:"global"},{label:qsTr("Playback dock"),value:"dock"},{label:qsTr("Popups"),value:"popup"},{label:qsTr("Floating player"),value:"floating"},{label:qsTr("Desktop lyrics"),value:"desktop"}]}
    function scopeFor(section){return appearanceScopes[String(section)]||"global"}
    function setScope(section,scope){const state=Object.assign({},appearanceScopes);state[String(section)]=scope;appearanceScopes=state;activeSection=section;appearanceScope=scope}
    function rememberCategory(){
        if(!restoreScroll.running&&contentScroll.contentItem)scrollPositions[scrollKey]=contentScroll.contentItem.contentY
        highlightedKey="";restoreScroll.restart()
    }
    function navigate(group,section){
        activeGroup=Math.max(0,Math.min(5,group));activeSection=Math.max(0,section)
        setSectionExpanded(activeGroup,activeSection,true);searchField.clear();sectionJump.restart()
    }
    function goBack(){if(!navigationHistory.length)return;const history=navigationHistory.slice();const key=Number(history.pop());navigationHistory=history;restoringHistory=true;activeGroup=key;activeSection=0;searchField.clear()}
    function jumpTo(row){
        navigate(row.group,row.section)
        if(row.advanced)expandAdvanced()
        if(row.subgroup)setSubgroupExpanded(row.group,row.section,row.subgroup,true)
        highlightedKey=row.key;jumpTimer.restart();highlightTimer.restart()
    }
    function findNamed(item,name){
        if(item.objectName===name)return item
        const children=item.children||[]
        for(let i=0;i<children.length;i++){const match=findNamed(children[i],name);if(match)return match}
        return null
    }
    function revealItem(target){if(!target||!contentScroll.contentItem)return;const point=target.mapToItem(contentColumn,0,0);const flick=contentScroll.contentItem;flick.contentY=Math.max(0,Math.min(point.y-12,Math.max(0,flick.contentHeight-flick.height)))}
    function scrollToKey(key){
        const container=findNamed(contentColumn,"section-"+activeGroup+"/"+activeSection)||contentColumn
        let target=String(key).indexOf("@")===0?container:findNamed(container,"setting-"+key)
        if(key==="lyrics/providerPriority")target=findNamed(container,"lyricProviderList")
        if(!target&&String(key).indexOf("lyrics/providers/")===0)target=findNamed(container,"provider-enabled-"+String(key).split("/")[2])
        revealItem(target);if(target)target.forceActiveFocus()
    }
    function resetSection(group,section,scope){pendingReset=group===4&&(section===1||section===2)?{kind:"appearance",scope:scope}:{kind:"section",group:group,section:section};resetDialog.open()}
    function resetCurrent(){resetSection(activeGroup,activeSection,appearanceScope)}
    function moveProvider(from,to){
        const rows=providerRows.slice();to=Math.max(0,Math.min(rows.length-1,to))
        if(from<0||from>=rows.length||from===to)return
        const moved=rows.splice(from,1)[0];rows.splice(to,0,moved)
        save("lyrics/providerPriority",rows.map(function(row){return row.id}).join(","))
    }
    function providerCapabilities(capabilities){
        const labels={search:qsTr("Search"),line:qsTr("Line timing"),word:qsTr("Word timing"),translation:qsTr("Translation"),romanization:qsTr("Romanization")}
        return (capabilities||[]).map(function(key){return labels[key]||key}).join(" · ")
    }
    function providerStatus(status){const labels={unknown:"",testing:qsTr("Testing"),available:qsTr("Available"),empty:qsTr("No lyrics"),error:qsTr("Unavailable"),"rate-limited":qsTr("Rate limited")};return labels[status]===undefined?status:labels[status]}
    function providerName(id){for(let i=0;i<providerRows.length;i++)if(providerRows[i].id===id)return providerRows[i].name;return id==="local"?qsTr("Local lyrics"):id==="phone"?qsTr("Phone lyrics"):id||qsTr("None")}
    function validPresetName(input,oldName){const name=String(input).trim();return name.length>0&&name.length<=64&&!/[\x00-\x1f\\/]/.test(name)&&!lyricPresetRows.some(function(row){return row.value===name&&row.value!==oldName})}
    function editPresetName(action){presetNameAction=action;presetRenameFrom=action==="rename"?effectivePresetName:"";presetNameDialog.open()}
    Timer{id:restoreScroll;interval:0;onTriggered:{
        const key=String(page.activeGroup)
        if(key!==page.scrollKey){if(!page.restoringHistory)page.navigationHistory=page.navigationHistory.concat([page.scrollKey]).slice(-40);page.scrollKey=key}
        page.restoringHistory=false
        if(contentScroll.contentItem)contentScroll.contentItem.contentY=Math.max(0,Math.min(page.scrollPositions[key]||0,Math.max(0,contentScroll.contentItem.contentHeight-contentScroll.contentItem.height)))
    }}
    Timer{id:sectionJump;interval:0;onTriggered:Qt.callLater(function(){page.revealItem(page.findNamed(contentColumn,"section-"+page.activeGroup+"/"+page.activeSection))})}
    Shortcut{sequence:"Alt+Left";enabled:page.visible&&page.navigationHistory.length>0;onActivated:page.goBack()}
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
        property bool capsule: false
        property bool frosted: false
        implicitHeight:36;implicitWidth:Math.max(36,contentItem.implicitWidth+22)
        font.family:tokens.family;font.pixelSize:tokens.body
        hoverEnabled:true
        contentItem:Label{text:button.text;textFormat:Text.PlainText;color:button.enabled?page.textColor:tokens.faintText;font:button.font;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter;elide:Text.ElideRight}
        background:Rectangle{radius:button.capsule?height/2:tokens.radius;color:button.down?tokens.pressed:button.checked?tokens.selected:button.hovered?tokens.hover:button.frosted?Qt.rgba(tokens.elevated.r,tokens.elevated.g,tokens.elevated.b,page.light?.45:.24):"transparent";border.color:button.visualFocus?page.accent:button.frosted?tokens.border:"transparent";border.width:button.visualFocus||button.frosted?1:0;Behavior on color{ColorAnimation{duration:tokens.fast}}}
    }
    component Field: TextField {
        id:field
        implicitHeight:36;selectByMouse:true;color:page.textColor;selectionColor:page.accent;selectedTextColor:tokens.accentText
        placeholderTextColor:tokens.faintText;font.family:tokens.family;font.pixelSize:tokens.body;padding:9
        background:Rectangle{radius:tokens.radius;color:Qt.rgba(tokens.elevated.r,tokens.elevated.g,tokens.elevated.b,page.light?.54:.40);border.width:1;border.color:field.visualFocus?page.accent:tokens.border}
    }
    component Choice: ComboBox {
        id:choice
        implicitHeight:36;implicitWidth:180
        font.family:tokens.family;font.pixelSize:tokens.body
        textRole:"label";valueRole:"value"
        palette.text:page.textColor;palette.buttonText:page.textColor;palette.base:tokens.elevated;palette.window:tokens.elevated;palette.highlight:page.accent;palette.highlightedText:tokens.accentText
        background:Rectangle{radius:tokens.radius;color:choice.pressed?tokens.pressed:Qt.rgba(tokens.elevated.r,tokens.elevated.g,tokens.elevated.b,page.light?.54:.40);border.width:1;border.color:choice.visualFocus?page.accent:tokens.border}
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
        property int sectionIndex: page.activeSection
        property string scope: page.scopeFor(sectionIndex)
        property string key: descriptor.key||""
        property string kind: descriptor.type||"bool"
        property var currentValue: {page.settingsRevision;if(!appearance)return page.value(key,descriptor.defaultValue);const config=page.controller?page.controller.appearanceConfig(scope):({});return config[key]===undefined?descriptor.defaultValue:config[key]}
        property bool editable: {page.settingsRevision;return !appearance||!page.controller||scope==="global"||!page.controller.appearanceInherited(scope)}
        implicitHeight:44
        Layout.fillWidth:true
        objectName:"setting-"+(appearance?"appearance/":"")+key
        activeFocusOnTab:true
        function commit(value){if(!page.controller)return;if(appearance)page.controller.setAppearance(scope,key,value);else page.save(key,value)}
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
                ColorDialog{id:colorPicker;title:row.descriptor.label;options:ColorDialog.ShowAlphaChannel;onAccepted:row.commit(selectedColor.toString())}
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

    component ProviderPanel: ColumnLayout {
        spacing:6
        RowLayout {
            Layout.fillWidth:true
            Label{text:qsTr("Current source");color:tokens.secondaryText;font.pixelSize:tokens.caption}
            Label{Layout.fillWidth:true;text:page.providerName(page.controller&&page.controller.lyricProvider?page.controller.lyricProvider:"");textFormat:Text.PlainText;color:page.textColor;elide:Text.ElideRight}
            FlatButton{text:qsTr("Choose lyrics");frosted:true;onClicked:page.matchLyricsRequested()}
        }
        ListView {
            id:providerList;objectName:"lyricProviderList";Layout.fillWidth:true;Layout.preferredHeight:count*56
            interactive:false;clip:false;model:page.providerRows
            delegate:Item {
                id:providerRow;required property int index;required property var modelData
                width:providerList.width;height:56;z:providerDrag.active?10:0
                property string enabledKey:modelData.enabledKey||"lyrics/providers/"+modelData.id+"/enabled"
                Item {
                    anchors.fill:parent
                    transform:Translate{y:providerDrag.active?providerDrag.activeTranslation.y:0}
                    Rectangle{anchors.fill:parent;anchors.margins:1;radius:tokens.radius;color:providerDrag.active?tokens.selected:"transparent";border.width:providerDrag.active?1:0;border.color:tokens.border}
                    RowLayout {
                        anchors.fill:parent;spacing:8
                        Item {
                            id:grip;objectName:"provider-drag-"+providerRow.modelData.id;Layout.preferredWidth:30;Layout.fillHeight:true;activeFocusOnTab:true
                            readonly property bool dragActive:providerDrag.active
                            readonly property real dragOffset:providerDrag.activeTranslation.y
                            property real pendingDragOffset:0
                            Accessible.role:Accessible.Button;Accessible.name:qsTr("Move %1").arg(providerRow.modelData.name)
                            Text{anchors.centerIn:parent;text:Icons.glyph("grip-vertical");font.family:lucide.name;font.pixelSize:18;color:grip.activeFocus?page.accent:tokens.secondaryText}
                            HoverHandler{cursorShape:Qt.SizeVerCursor}
                            DragHandler {
                                id:providerDrag;target:null;xAxis.enabled:false;yAxis.enabled:true
                                onActiveTranslationChanged:if(providerDrag.active)grip.pendingDragOffset=providerDrag.activeTranslation.y
                                onActiveChanged:{if(providerDrag.active)grip.pendingDragOffset=0;else page.moveProvider(providerRow.index,providerRow.index+Math.round(grip.pendingDragOffset/providerRow.height))}
                            }
                            Keys.onUpPressed:function(event){page.moveProvider(providerRow.index,providerRow.index-1);event.accepted=true}
                            Keys.onDownPressed:function(event){page.moveProvider(providerRow.index,providerRow.index+1);event.accepted=true}
                        }
                        ColumnLayout {
                            Layout.fillWidth:true;spacing:4
                            Label{Layout.fillWidth:true;text:providerRow.modelData.name;textFormat:Text.PlainText;color:page.textColor;font.weight:Font.Medium;elide:Text.ElideRight}
                            Label{Layout.fillWidth:true;text:page.providerCapabilities(providerRow.modelData.capabilities);textFormat:Text.PlainText;color:tokens.secondaryText;font.pixelSize:tokens.caption;elide:Text.ElideRight}
                        }
                        Label{visible:!!text;text:page.providerStatus(providerRow.modelData.status||"unknown");color:providerRow.modelData.status==="available"?tokens.success:providerRow.modelData.status==="error"?tokens.danger:tokens.secondaryText;font.pixelSize:tokens.caption}
                        FlatButton{objectName:"provider-test-"+providerRow.modelData.id;text:qsTr("Test");enabled:providerRow.modelData.status!=="testing";Accessible.name:qsTr("Test %1").arg(providerRow.modelData.name);ToolTip.visible:hovered&&!!providerRow.modelData.lastError;ToolTip.text:providerRow.modelData.lastError||"";onClicked:page.controller.testLyricProvider(providerRow.modelData.id)}
                        Toggle{objectName:"provider-enabled-"+providerRow.modelData.id;checked:page.value(providerRow.enabledKey,providerRow.modelData.enabled!==false)===true;Accessible.name:qsTr("Enable %1").arg(providerRow.modelData.name);onClicked:page.save(providerRow.enabledKey,checked)}
                    }
                }
            }
        }
    }
    component DevicePanel: ColumnLayout {
        required property int sectionIndex
        spacing:8
        RowLayout {
            Layout.fillWidth:true
            Label{text:page.controller?page.controller.status:"";textFormat:Text.PlainText;color:tokens.secondaryText;Layout.fillWidth:true;elide:Text.ElideRight}
            FlatButton{text:qsTr("Search devices");visible:sectionIndex===1;frosted:true;onClicked:page.searchDevicesRequested()}
            FlatButton{text:qsTr("Disconnect");visible:sectionIndex===0&&!!page.controller&&page.controller.online;onClicked:page.controller.disconnectDevice()}
        }
        ListView {
            id:deviceList;Layout.fillWidth:true;Layout.preferredHeight:Math.max(56,Math.min(4,count)*56);clip:true;spacing:4
            model:page.controller?page.controller.devices.filter(function(row){return sectionIndex!==0||row.paired===true}):[]
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
    component AppearancePanel: RowLayout {
        id:appearancePanel;required property int sectionIndex
        property string scope:page.scopeFor(sectionIndex)
        property var choices:page.scopeChoices(sectionIndex)
        property bool inherited:{page.settingsRevision;return scope!=="global"&&page.controller?page.controller.appearanceInherited(scope):false}
        Layout.fillWidth:true
        Choice{objectName:"appearanceScope-"+appearancePanel.sectionIndex;Layout.fillWidth:true;Layout.maximumWidth:260;model:appearancePanel.choices;Accessible.name:qsTr("Appearance scope");currentIndex:{for(let i=0;i<appearancePanel.choices.length;i++)if(appearancePanel.choices[i].value===appearancePanel.scope)return i;return 0}onActivated:page.setScope(appearancePanel.sectionIndex,appearancePanel.choices[currentIndex].value)}
        Item{Layout.fillWidth:true}
        Label{visible:appearancePanel.scope!=="global";text:qsTr("Use global settings");color:page.textColor}
        Toggle{visible:appearancePanel.scope!=="global";checked:appearancePanel.inherited;Accessible.name:qsTr("Use global settings");onClicked:page.controller.setAppearanceInherited(appearancePanel.scope,checked)}
        Choice{visible:appearancePanel.scope==="global";Layout.preferredWidth:155;Accessible.name:qsTr("Appearance preset");model:[{label:qsTr("Preset"),value:""},{label:qsTr("Clear"),value:"clear"},{label:qsTr("Balanced"),value:"balanced"},{label:qsTr("Vivid"),value:"vivid"}];currentIndex:0;onActivated:if(currentIndex>0){page.controller.applyAppearancePreset(model[currentIndex].value);currentIndex=0}}
    }
    component LyricPresetsPanel: ColumnLayout {
        spacing:10
        RowLayout {
            Layout.fillWidth:true
            Choice{objectName:"lyricPresetChoice";Layout.fillWidth:true;Layout.maximumWidth:350;model:page.lyricPresetRows;Accessible.name:qsTr("Lyric style preset");currentIndex:{for(let i=0;i<page.lyricPresetRows.length;i++)if(page.lyricPresetRows[i].value===page.effectivePresetName)return i;return -1}onActivated:if(currentIndex>=0){page.selectedPresetName=page.lyricPresetRows[currentIndex].value;page.controller.applyLyricPreset(page.selectedPresetName)}}
            Item{Layout.fillWidth:true}
            FlatButton{objectName:"lyricPresetSave";text:qsTr("Save as");frosted:true;onClicked:page.editPresetName("save")}
            FlatButton{objectName:"lyricPresetRename";text:Icons.glyph("pencil");font.family:lucide.name;implicitWidth:36;enabled:!!page.effectivePresetName&&!page.selectedPresetBuiltIn;Accessible.name:qsTr("Rename preset");ToolTip.visible:hovered;ToolTip.text:qsTr("Rename preset");onClicked:page.editPresetName("rename")}
            FlatButton{objectName:"lyricPresetDelete";text:Icons.glyph("trash-2");font.family:lucide.name;implicitWidth:36;enabled:!!page.effectivePresetName&&!page.selectedPresetBuiltIn;Accessible.name:qsTr("Delete preset");ToolTip.visible:hovered;ToolTip.text:qsTr("Delete preset");onClicked:presetDeleteDialog.open()}
        }
        Flow {
            Layout.fillWidth:true;spacing:6
            FlatButton{text:qsTr("Import settings");onClicked:importFile.open()}
            FlatButton{text:qsTr("Export settings");onClicked:exportFile.open()}
        }
        RowLayout {
            Layout.fillWidth:true
            Choice{id:lyricResetScope;model:[{label:qsTr("Main lyrics"),value:"main"},{label:qsTr("Desktop lyrics"),value:"desktop"},{label:qsTr("All lyric styles"),value:"all"}];Accessible.name:qsTr("Lyric reset scope");Layout.fillWidth:true;Layout.maximumWidth:260}
            Item{Layout.fillWidth:true}
            FlatButton{text:qsTr("Restore lyric defaults");onClicked:{page.pendingReset={kind:"lyrics",scope:lyricResetScope.model[lyricResetScope.currentIndex].value};resetDialog.open()}}
        }
    }
    component LyricSubgroup: ColumnLayout {
        id:subgroup
        required property int groupIndex
        required property int sectionIndex
        required property var subgroupData
        property bool collapsed:page.subgroupCollapsed(groupIndex,sectionIndex,subgroupData.key)
        objectName:"subgroup-"+groupIndex+"/"+sectionIndex+"/"+subgroupData.key
        Layout.fillWidth:true;spacing:2
        FlatButton {
            id:subgroupHeading;objectName:"subgroup-heading-"+subgroup.groupIndex+"/"+subgroup.sectionIndex+"/"+subgroup.subgroupData.key
            Layout.fillWidth:true;text:subgroup.subgroupData.label;implicitHeight:36
            Accessible.description:subgroup.collapsed?qsTr("Collapsed"):qsTr("Expanded")
            onClicked:page.setSubgroupExpanded(subgroup.groupIndex,subgroup.sectionIndex,subgroup.subgroupData.key,subgroup.collapsed)
            contentItem:RowLayout{Label{Layout.fillWidth:true;text:subgroupHeading.text;textFormat:Text.PlainText;color:tokens.secondaryText;font.family:tokens.family;font.pixelSize:tokens.body;font.weight:Font.Medium;elide:Text.ElideRight}Text{text:Icons.glyph("chevron-down");font.family:lucide.name;font.pixelSize:14;color:tokens.secondaryText;rotation:subgroup.collapsed?-90:0;Behavior on rotation{NumberAnimation{duration:tokens.fast}}}}
        }
        Item {
            Layout.fillWidth:true
            property real reveal:subgroup.collapsed?0:1
            implicitHeight:subgroupRows.implicitHeight*reveal;opacity:reveal;visible:reveal>0;clip:reveal<1
            Behavior on reveal{NumberAnimation{duration:tokens.fast;easing.type:Easing.OutCubic}}
            ColumnLayout {
                id:subgroupRows;width:parent.width;height:implicitHeight;spacing:3
                Repeater{model:page.rowsFor(subgroup.groupIndex,subgroup.sectionIndex).filter(function(row){return row.subgroup===subgroup.subgroupData.key});ParameterRow{required property var modelData;descriptor:modelData;sectionIndex:subgroup.sectionIndex}}
            }
        }
    }
    component SectionBlock: ColumnLayout {
        id:sectionBlock
        required property int sectionIndex
        property int groupIndex:page.activeGroup
        property bool appearance:groupIndex===4&&(sectionIndex===1||sectionIndex===2)
        property var allRows:page.catalog.filter(function(row){return row.group===groupIndex&&row.section===sectionIndex})
        property bool hasAdvanced:appearance||allRows.some(function(row){return row.advanced})
        property bool advanced:page.advancedFor(groupIndex,sectionIndex)
        property bool collapsed:page.isCollapsed(groupIndex,sectionIndex)
        property bool specialized:(groupIndex===1&&(sectionIndex===0||sectionIndex===1))||(groupIndex===3&&(sectionIndex===0||sectionIndex===6))||appearance
        objectName:"section-"+groupIndex+"/"+sectionIndex
        Layout.fillWidth:true;spacing:4;visible:specialized||allRows.length>0
        RowLayout {
            Layout.fillWidth:true;Layout.topMargin:12;spacing:2
            FlatButton {
                id:sectionHeading;objectName:"section-heading-"+sectionBlock.groupIndex+"/"+sectionBlock.sectionIndex
                Layout.fillWidth:true;text:page.sectionTitle(sectionBlock.groupIndex,sectionBlock.sectionIndex);implicitHeight:40
                Accessible.description:sectionBlock.collapsed?qsTr("Collapsed"):qsTr("Expanded")
                onClicked:{page.activeSection=sectionBlock.sectionIndex;page.setSectionExpanded(sectionBlock.groupIndex,sectionBlock.sectionIndex,sectionBlock.collapsed)}
                contentItem:RowLayout{Label{Layout.fillWidth:true;text:sectionHeading.text;textFormat:Text.PlainText;color:page.textColor;font.family:tokens.family;font.pixelSize:16;font.weight:Font.DemiBold;elide:Text.ElideRight}Text{text:Icons.glyph("chevron-down");font.family:lucide.name;font.pixelSize:16;color:tokens.secondaryText;rotation:sectionBlock.collapsed?-90:0;Behavior on rotation{NumberAnimation{duration:tokens.fast;easing.type:Easing.OutCubic}}}}
            }
            FlatButton{text:Icons.glyph("rotate-ccw");font.family:lucide.name;implicitWidth:32;Accessible.name:qsTr("Reset %1").arg(page.sectionTitle(sectionBlock.groupIndex,sectionBlock.sectionIndex));ToolTip.visible:hovered;ToolTip.text:qsTr("Restore group defaults");onClicked:page.resetSection(sectionBlock.groupIndex,sectionBlock.sectionIndex,page.scopeFor(sectionBlock.sectionIndex))}
        }
        Item {
            Layout.fillWidth:true
            property real reveal:sectionBlock.collapsed?0:1
            implicitHeight:sectionInner.implicitHeight*reveal;opacity:reveal;visible:reveal>0;clip:reveal<1
            Behavior on reveal{NumberAnimation{duration:tokens.fast;easing.type:Easing.OutCubic}}
            ColumnLayout {
                id:sectionInner;width:parent.width;height:implicitHeight;spacing:3
                Loader{Layout.fillWidth:true;active:sectionBlock.groupIndex===1&&(sectionBlock.sectionIndex===0||sectionBlock.sectionIndex===1);sourceComponent:DevicePanel{sectionIndex:sectionBlock.sectionIndex}}
                Loader{Layout.fillWidth:true;active:sectionBlock.groupIndex===3&&sectionBlock.sectionIndex===0;sourceComponent:ProviderPanel{}}
                Loader{Layout.fillWidth:true;active:sectionBlock.groupIndex===3&&sectionBlock.sectionIndex===6;sourceComponent:LyricPresetsPanel{}}
                Loader{Layout.fillWidth:true;active:sectionBlock.appearance;sourceComponent:AppearancePanel{sectionIndex:sectionBlock.sectionIndex}}
                Repeater {model:page.rowsFor(sectionBlock.groupIndex,sectionBlock.sectionIndex).filter(function(row){return !row.subgroup});ParameterRow{required property var modelData;descriptor:modelData;sectionIndex:sectionBlock.sectionIndex}}
                Repeater {model:page.subgroupsFor(sectionBlock.groupIndex,sectionBlock.sectionIndex);LyricSubgroup{required property var modelData;groupIndex:sectionBlock.groupIndex;sectionIndex:sectionBlock.sectionIndex;subgroupData:modelData}}
                Repeater {model:sectionBlock.appearance?page.parameterDescriptors(sectionBlock.sectionIndex,sectionBlock.advanced):[];ParameterRow{required property var modelData;descriptor:modelData;appearance:true;sectionIndex:sectionBlock.sectionIndex;scope:page.scopeFor(sectionBlock.sectionIndex)}}
                RowLayout {
                    visible:sectionBlock.hasAdvanced;Layout.fillWidth:true;Layout.topMargin:4
                    FlatButton{objectName:"advancedOptions-"+sectionBlock.groupIndex+"/"+sectionBlock.sectionIndex;text:sectionBlock.advanced?qsTr("Hide advanced options"):qsTr("Advanced options");checked:sectionBlock.advanced;onClicked:{page.activeSection=sectionBlock.sectionIndex;page.setAdvanced(sectionBlock.groupIndex,sectionBlock.sectionIndex,!sectionBlock.advanced)}}
                    Item{Layout.fillWidth:true}
                }
            }
        }
    }

    // A single optional optical backing is shared by the page. Individual
    // controls tint that material with lightweight scene-graph geometry only.
    Loader {
        anchors.fill:parent;anchors.leftMargin:Math.max(0,page.contentInset-20);anchors.rightMargin:anchors.leftMargin
        active:page.materialSource!==null&&page.value("appearance/style","standard")==="liquid"
        sourceComponent:GlassMaterial {
            anchors.fill:parent;sourceItem:page.materialSource;glassEnabled:true;active:page.visible;motion:false
            dark:!page.light;backdropColor:tokens.base;cornerRadius:24
            configuration:({glassOpacity:.025,glassBlur:1.6,glassRefraction:.18,glassDistortion:.4,glassDispersion:0,glassEdgeGlow:.05,glassShadow:0,glassRadius:24,glassMotion:false,quality:0})
        }
    }
    ColumnLayout {
        anchors.fill:parent;anchors.leftMargin:page.contentInset;anchors.rightMargin:anchors.leftMargin;anchors.topMargin:16;anchors.bottomMargin:12;spacing:14
        RowLayout {
            Layout.fillWidth:true;spacing:12
            FlatButton{objectName:"settingsBack";text:Icons.glyph("arrow-left");font.family:lucide.name;implicitWidth:38;implicitHeight:38;capsule:true;frosted:true;Accessible.name:qsTr("Back");onClicked:page.closeRequested()}
            Label{text:qsTr("Settings");font.family:tokens.family;font.pixelSize:tokens.heading;color:page.textColor;Layout.fillWidth:true}
        }
        Field{id:searchField;objectName:"settingsSearch";Layout.fillWidth:true;placeholderText:qsTr("Search settings");Accessible.name:placeholderText}
        Flow {
            Layout.fillWidth:true;spacing:6
            Repeater {model:page.groupNames
                FlatButton{required property int index;required property string modelData;objectName:"settings-category-"+index;text:modelData;capsule:true;frosted:true;checked:page.activeGroup===index;onClicked:{page.activeGroup=index;page.activeSection=0;searchField.clear()}}
            }
        }
        ScrollView {
            id:resultsScroll;visible:searchField.text.trim().length>0;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;contentWidth:availableWidth
            Column {
                width:resultsScroll.availableWidth;spacing:4
                Repeater {model:page.searchResults
                    ItemDelegate {
                        id:searchHit;required property var modelData;width:parent.width;height:52;objectName:"search-"+modelData.key
                        Accessible.name:modelData.label;onClicked:page.jumpTo(modelData)
                        contentItem:Column{spacing:3;Label{text:searchHit.modelData.label;textFormat:Text.PlainText;color:page.textColor;font.pixelSize:tokens.body}Label{text:page.groupNames[searchHit.modelData.group]+" / "+page.sectionTitle(searchHit.modelData.group,searchHit.modelData.section);color:tokens.secondaryText;font.pixelSize:tokens.caption}}
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
                id:contentColumn;width:contentScroll.availableWidth-12;spacing:10
                Repeater{model:page.sectionsForGroup(page.activeGroup);SectionBlock{required property var modelData;sectionIndex:modelData.index}}
                Item{Layout.fillWidth:true;implicitHeight:20}
            }
        }
    }
    Dialog {
        id:resetDialog;title:qsTr("Restore defaults?");modal:true;anchors.centerIn:Overlay.overlay;width:Math.min(420,page.width-24)
        standardButtons:Dialog.Ok|Dialog.Cancel
        palette.window:tokens.elevated;palette.windowText:page.textColor;palette.buttonText:page.textColor
        contentItem:Label{text:page.pendingReset.kind==="all"?qsTr("This replaces your settings. Paired devices are kept."):page.pendingReset.kind==="appearance"?qsTr("This restores all appearance parameters for this scope."):qsTr("This restores the options in this section.");color:page.textColor;wrapMode:Text.Wrap}
        onAccepted:{if(page.pendingReset.kind==="all")page.controller.resetSettings();else if(page.pendingReset.kind==="lyrics")page.controller.resetLyricSettings(page.pendingReset.scope);else if(page.pendingReset.kind==="appearance")page.controller.resetAppearance(page.pendingReset.scope);else page.controller.resetSettingsGroup(page.pendingReset.group,page.pendingReset.section)}
    }
    Dialog {
        id:presetNameDialog;objectName:"presetNameDialog";title:page.presetNameAction==="rename"?qsTr("Rename preset"):qsTr("Save preset")
        modal:true;anchors.centerIn:Overlay.overlay;width:Math.min(380,page.width-24);padding:20
        onOpened:{presetNameField.text=page.presetNameAction==="rename"?page.presetRenameFrom:"";presetNameField.forceActiveFocus();presetNameField.selectAll()}
        background:Rectangle{radius:16;color:tokens.elevated;border.width:1;border.color:tokens.border}
        contentItem:Field{id:presetNameField;objectName:"presetNameField";placeholderText:qsTr("Preset name");maximumLength:64;Accessible.name:qsTr("Preset name");onAccepted:if(page.validPresetName(text,page.presetRenameFrom))presetNameDialog.accept()}
        footer:RowLayout {spacing:8
            Item{Layout.fillWidth:true}
            FlatButton{text:qsTr("Cancel");onClicked:presetNameDialog.reject()}
            FlatButton{objectName:"presetNameSave";text:qsTr("Save");frosted:true;enabled:page.validPresetName(presetNameField.text,page.presetRenameFrom);onClicked:presetNameDialog.accept()}
        }
        onAccepted:{const name=presetNameField.text.trim();if(!page.validPresetName(name,page.presetRenameFrom))return;if(page.presetNameAction==="rename")page.controller.renameLyricPreset(page.presetRenameFrom,name);else page.controller.saveLyricPreset(name);page.selectedPresetName=name}
    }
    Dialog {
        id:presetDeleteDialog;objectName:"presetDeleteDialog";title:qsTr("Delete preset?");modal:true;anchors.centerIn:Overlay.overlay;width:Math.min(380,page.width-24);padding:20
        property string presetName:""
        onOpened:presetName=page.effectivePresetName
        background:Rectangle{radius:16;color:tokens.elevated;border.width:1;border.color:tokens.border}
        contentItem:Label{text:qsTr("The saved style “%1” will be removed.").arg(presetDeleteDialog.presetName);color:page.textColor;wrapMode:Text.Wrap}
        footer:RowLayout {spacing:8
            Item{Layout.fillWidth:true}
            FlatButton{text:qsTr("Cancel");onClicked:presetDeleteDialog.reject()}
            FlatButton{objectName:"presetDeleteConfirm";text:qsTr("Delete");onClicked:presetDeleteDialog.accept()}
        }
        onAccepted:{page.controller.deleteLyricPreset(presetName);page.selectedPresetName=""}
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
