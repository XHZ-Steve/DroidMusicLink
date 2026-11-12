import QtQuick

QtObject {
    property bool light: false
    property bool motion: true
    property color accent: "#bac9ed"
    readonly property string family: "Microsoft YaHei UI"
    readonly property int caption: 12
    readonly property int body: 14
    readonly property int heading: 24
    readonly property int control: 40
    readonly property int radius: 10
    readonly property int dialogRadius:24
    readonly property int dockRadius:28
    readonly property int floatingRadius:20
    readonly property int trayRadius:22
    readonly property int display:42
    readonly property int artist:18
    readonly property int gap: 8
    readonly property int fast: motion ? 140 : 0
    readonly property int change: motion ? 340 : 0
    readonly property int atmosphere: motion ? 900 : 0
    readonly property int progressMotion:motion?220:0
    readonly property color text: light ? "#202431" : "#f5f5fa"
    readonly property color secondaryText: light ? "#555c70" : "#bfc2d1"
    readonly property color faintText: light ? "#626a7c" : "#9299ad"
    readonly property color base: light ? "#f5f6fa" : "#10121b"
    readonly property color surface: light ? "#edf0f7" : "#222633"
    readonly property color elevated: light ? "#fbfcff" : "#202432"
    readonly property color border: light ? "#213b4668" : "#24c5cbe4"
    readonly property color hover: light ? "#0d24305b" : "#13e0e5ff"
    readonly property color pressed: light ? "#2024305b" : "#25e0e5ff"
    readonly property color selected: Qt.rgba(accent.r,accent.g,accent.b,light?.22:.17)
    readonly property color success: light ? "#357864" : "#8ed3b2"
    readonly property color danger: light ? "#ad3c49" : "#f5a1ae"
    readonly property color accentText: "#17202d"
}

