import QtQuick
import QtTest
import "../../qml"

Item {
    width: 780; height: 720
    Rectangle { anchors.fill: parent; color: "#10121b" }
    QtObject {
        id: controller
        property var lyricPresets:[{name:"soft",label:"Soft",builtIn:true}]
        property string lastAppliedPreset:""
        function saveLyricPreset(name){lyricPresets=lyricPresets.concat([{name:name,builtIn:false}]);revision++}
        function applyLyricPreset(name){lastAppliedPreset=name}
        function renameLyricPreset(oldName,newName){lyricPresets=lyricPresets.map(function(row){return row.name===oldName?{name:newName,builtIn:false}:row});revision++}
        function deleteLyricPreset(name){lyricPresets=lyricPresets.filter(function(row){return row.name!==name});revision++}
        property int revision: 0
        property var values: ({motion:true})
        property var appearance: ({glassMotion:true,glassBlur:2.4,glassDispersion:.07})
        property bool inherit: true
        property var settingsCatalog: [
            {key:"motion",label:"TEST animations",group:0,section:0,type:"bool",defaultValue:true},
            {key:"language",label:"TEST language",group:0,section:0,type:"enum",defaultValue:"en",options:[{label:"English",value:"en"},{label:"简体中文",value:"zh_CN"}]},
            {key:"shortcut",label:"TEST shortcut",group:0,section:0,type:"shortcut",defaultValue:""},
            {key:"lyrics/offset",label:"TEST sync offset",group:3,section:4,type:"int",defaultValue:0,min:-10000,max:10000,step:100,advanced:true}
        ]
        property var appearanceDescriptors: [
            {key:"glassMotion",label:"TEST optical motion",group:"glass",type:"bool",defaultValue:true},
            {key:"glassBlur",label:"TEST blur",group:"glass",type:"real",defaultValue:2.4,min:0,max:20,step:.2},
            {key:"glassDispersion",label:"TEST dispersion",group:"glass",type:"real",defaultValue:.07,min:0,max:.3,step:.01,advanced:true}
        ]
        property var devices: []
        property string status: "Disconnected"
        property bool online: false
        property string lyricProvider: "lrclib"
        property string lastProviderTest: ""
        property var lyricProviders: [
            {id:"lrclib",name:"LRCLIB TEST",capabilities:["search","line"],status:"unknown",enabled:true,enabledKey:"lyrics/providers/lrclib/enabled"},
            {id:"netease",name:"NetEase TEST",capabilities:["line","word","translation"],status:"available",enabled:true,enabledKey:"lyrics/providers/netease/enabled"}
        ]
        function setting(key,fallback){return values[key]===undefined?fallback:values[key]}
        function setSetting(key,value){const next=Object.assign({},values);next[key]=value;values=next;revision++}
        function appearanceConfig(scope){return appearance}
        function appearanceInherited(scope){return inherit}
        function setAppearance(scope,key,value){const next=Object.assign({},appearance);next[key]=value;appearance=next;revision++}
        function setAppearanceInherited(scope,value){inherit=value;revision++}
        function testLyricProvider(id){lastProviderTest=id}
    }
    SettingsPage {
        id: page
        anchors.fill:parent;anchors.margins:16
        controller:controller;settingsRevision:controller.revision
        iconFontSource:Qt.resolvedUrl("../../../assets/vendor/lucide/font/lucide.ttf")
        motion:false
    }
    SignalSpy { id: searchSpy; target:page; signalName:"searchDevicesRequested" }
    TestCase {
        name:"SettingsPage"
        when:windowShown
        function init(){page.navigate(0,0);controller.values={motion:true};controller.appearance={glassMotion:true,glassBlur:2.4,glassDispersion:.07};controller.inherit=true;controller.revision++;searchSpy.clear();waitForRendering(page)}
        function test_preset_dialogs_persist_rename_and_delete(){
            page.navigate(3,6);waitForRendering(page)
            page.editPresetName("save");wait(30)
            const field=findChild(page,"presetNameField");verify(field);field.text="TEST custom"
            const save=findChild(page,"presetNameSave");verify(save.enabled);mouseClick(save,save.width/2,save.height/2);wait(30)
            verify(controller.lyricPresets.some(function(row){return row.name==="TEST custom"}));compare(page.selectedPresetName,"TEST custom")
            page.editPresetName("rename");wait(30);field.text="TEST renamed";mouseClick(save,save.width/2,save.height/2);wait(30)
            verify(controller.lyricPresets.some(function(row){return row.name==="TEST renamed"}))
            const dialog=findChild(page,"presetDeleteDialog");dialog.open();wait(30)
            const confirm=findChild(page,"presetDeleteConfirm");mouseClick(confirm,confirm.width/2,confirm.height/2);wait(30)
            verify(!controller.lyricPresets.some(function(row){return row.name==="TEST renamed"}))
        }
        function test_switch_commits_actual_boolean(){
            const toggle=findChild(page,"editor-motion")
            verify(toggle);verify(toggle.checked)
            mouseClick(toggle,toggle.width/2,toggle.height/2)
            compare(controller.values.motion,false)
            compare(typeof controller.values.motion,"boolean")
            compare(toggle.checked,false)
        }
        function test_search_navigates_and_reveals_advanced_row(){
            page.setSectionExpanded(3,4,false)
            const field=findChild(page,"settingsSearch")
            field.text="TEST sync offset"
            compare(page.searchResults.length,1)
            waitForRendering(page)
            const hit=findChild(page,"search-lyrics/offset")
            verify(hit)
            mouseClick(hit,hit.width/2,hit.height/2)
            compare(page.activeGroup,3);compare(page.activeSection,4)
            verify(page.advancedExpanded)
            compare(page.isCollapsed(3,4),false)
            tryVerify(function(){return !!findChild(page,"setting-lyrics/offset")})
            compare(page.highlightedKey,"lyrics/offset")
            compare(field.text,"")
        }
        function test_scoped_parameters_respect_inheritance(){
            page.navigate(4,2)
            page.appearanceScope="dock"
            const toggle=findChild(page,"editor-glassMotion")
            verify(toggle);compare(toggle.enabled,false)
            controller.setAppearanceInherited("dock",false)
            tryCompare(toggle,"enabled",true)
            waitForRendering(page)
            mouseClick(toggle,toggle.width/2,toggle.height/2)
            compare(controller.appearance.glassMotion,false)
        }
        function test_action_routes_to_host(){
            page.runAction({key:"test-search",action:"searchDevices"})
            compare(searchSpy.count,1)
        }
        function test_enum_commits_underlying_value(){
            const choice=findChild(page,"enum-language")
            verify(choice);choice.forceActiveFocus()
            keyClick(Qt.Key_Down)
            compare(controller.values.language,"zh_CN")
        }
        function test_numeric_editor_validates_bounds(){
            page.navigate(3,4);page.expandAdvanced();waitForRendering(page)
            const field=findChild(page,"number-lyrics/offset")
            verify(field);field.forceActiveFocus();field.text="-300";keyClick(Qt.Key_Return)
            compare(controller.values["lyrics/offset"],-300)
            field.text="-99999";keyClick(Qt.Key_Return)
            compare(controller.values["lyrics/offset"],-300)
            compare(field.text,"-300")
        }
        function test_shortcut_captures_real_key_chord(){
            const field=findChild(page,"shortcut-shortcut")
            verify(field);field.forceActiveFocus();keyClick(Qt.Key_L,Qt.ControlModifier|Qt.AltModifier)
            compare(controller.values.shortcut,"Ctrl+Alt+L")
        }
        function test_sections_share_one_page_and_collapse(){
            const section=findChild(page,"section-0/0")
            const heading=findChild(page,"section-heading-0/0")
            verify(section);verify(heading)
            const height=section.height
            mouseClick(heading,heading.width/2,heading.height/2)
            compare(page.isCollapsed(0,0),true)
            tryVerify(function(){return section.height<height})
            mouseClick(heading,heading.width/2,heading.height/2)
            compare(page.isCollapsed(0,0),false)
        }
        function test_category_and_hidden_indexes_are_searchable(){
            const field=findChild(page,"settingsSearch")
            field.text="tongyong"
            verify(page.searchResults.length>0)
            verify(page.searchResults.some(function(row){return row.group===0}))
            field.text="歌词"
            verify(page.searchResults.some(function(row){return row.group===3}))
            field.text="Automatic matching"
            verify(page.searchResults.some(function(row){return row.group===3&&row.section===1}))
        }
        function test_provider_test_and_enable_are_live(){
            page.navigate(3,0);waitForRendering(page)
            const testButton=findChild(page,"provider-test-lrclib")
            const enabled=findChild(page,"provider-enabled-lrclib")
            verify(testButton);verify(enabled)
            mouseClick(testButton,testButton.width/2,testButton.height/2)
            compare(controller.lastProviderTest,"lrclib")
            mouseClick(enabled,enabled.width/2,enabled.height/2)
            compare(controller.values["lyrics/providers/lrclib/enabled"],false)
        }
        function test_provider_drag_persists_priority(){
            page.navigate(3,0);waitForRendering(page)
            const grip=findChild(page,"provider-drag-lrclib")
            verify(grip)
            mousePress(grip,grip.width/2,grip.height/2,Qt.LeftButton)
            mouseMove(grip,grip.width/2,grip.height/2+20,30)
            mouseMove(grip,grip.width/2,grip.height/2+94,30)
            verify(grip.dragActive)
            mouseRelease(grip,grip.width/2,grip.height/2+94,Qt.LeftButton)
            compare(controller.values["lyrics/providerPriority"],"netease,lrclib")
            compare(page.providerRows[0].id,"netease")
        }
    }
}
