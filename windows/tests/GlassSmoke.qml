import QtQuick
import "../qml"
Window {
    id: window
    width: 900; height: 620; visible: true; color: "#121923"
    title: "DroidMediaLink liquidglass hardware smoke"
    property bool glassOn: true
    Item {
        id: backdrop
        anchors.fill: parent
        Rectangle { anchors.fill: parent; gradient: Gradient {
            GradientStop { position: 0; color: "#697477" }
            GradientStop { position: 1; color: "#37293f" }
        } }
        Repeater { model: 16
            Rectangle { required property int index
                x: index * 64 - 60; y: -40; width: 18; height: 800; rotation: 18
                color: index % 2 ? "#b78f6e" : "#8cb2ba"; opacity: 0.7
            }
        }
        Rectangle { x: 520; y: 55; width: 200; height: 200; radius: 100; color: "#b7aaca" }
        Text { x: 85; y: 220; text: "OPTICAL BACKDROP"; color: "#edf4f5"; font.pixelSize: 52; font.bold: true }
    }
    GlassMaterial {
        objectName: "glass"
        x: 60; y: 190; width: 780; height: 240
        sourceItem: backdrop; cornerRadius: 44; glassEnabled: window.glassOn
        tintStrength: 0.16; tintColor: "#273442"
        Text { anchors.centerIn: parent; text: "DroidMediaLink"; color: "white"; font.pixelSize: 28 }
    }
    // Hidden per-window scene: capture must work without leaking its square edges.
    Item { id: hiddenScene; x: 640; y: 490; width: 200; height: 80; visible: false
        Rectangle { anchors.fill: parent; color: "#cd8377" }
        Rectangle { x: 65; width: 30; height: parent.height; color: "#f4cb83" }
    }
    GlassMaterial { x: 640; y: 490; width: 200; height: 80; sourceItem: hiddenScene
        glassEnabled: window.glassOn; cornerRadius: 40; tintStrength: 0.1
    }
}
