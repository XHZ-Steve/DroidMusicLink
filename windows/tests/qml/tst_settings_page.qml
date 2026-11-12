import QtQuick
import QtTest
import "../../qml"

Item {
    width: 780; height: 720
    Rectangle { anchors.fill: parent; color: "#10121b" }
    QtObject {
        id: controller
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
        function setting(key,fallback){return values[key]===undefined?fallback:values[key]}
        function setSetting(key,value){const next=Object.assign({},values);next[key]=value;values=next;revision++}
        function appearanceConfig(scope){return appearance}
        function appearanceInherited(scope){return inherit}
        function setAppearance(scope,key,value){const next=Object.assign({},appearance);next[key]=value;appearance=next;revision++}
        function setAppearanceInherited(scope,value){inherit=value;revision++}
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
        function test_switch_commits_actual_boolean(){
            const toggle=findChild(page,"editor-motion")
            verify(toggle);verify(toggle.checked)
            mouseClick(toggle,toggle.width/2,toggle.height/2)
            compare(controller.values.motion,false)
            compare(typeof controller.values.motion,"boolean")
            compare(toggle.checked,false)
        }
        function test_search_navigates_and_reveals_advanced_row(){
            const field=findChild(page,"settingsSearch")
            field.text="TEST sync offset"
            compare(page.searchResults.length,1)
            waitForRendering(page)
            const hit=findChild(page,"search-lyrics/offset")
            verify(hit)
            mouseClick(hit,hit.width/2,hit.height/2)
            compare(page.activeGroup,3);compare(page.activeSection,4)
            verify(page.advancedExpanded)
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
    }
}
