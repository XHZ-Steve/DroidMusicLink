import QtQuick
import QtTest
import "../../qml" as UI
Item {
    width:500;height:150
    UI.CapsuleProgress {id:progress;width:460;radius:42;from:0;to:100;value:0}
    TestCase {
        name:"CapsuleProgress";when:windowShown
        function test_wide_hit_target_keeps_native_slider_input(){
            mouseClick(progress,progress.width/2,16,Qt.LeftButton)
            verify(progress.value>45&&progress.value<55,"16 px below the visible stroke must remain clickable")
            const previous=progress.value
            mouseClick(progress,progress.width*.8,38,Qt.LeftButton)
            compare(progress.value,previous,"Distant Dock content must not seek")
            progress.forceActiveFocus();keyClick(Qt.Key_Right);verify(progress.value>previous)
        }
    }
}
