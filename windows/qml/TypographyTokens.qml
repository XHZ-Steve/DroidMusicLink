import QtQuick
QtObject {
 readonly property string uiFamily: "Segoe UI Variable Text"
 readonly property string displayFamily: "Segoe UI Variable Display"
 readonly property string cjkFallback: "Microsoft YaHei UI"
 function resolvedFamily(text,requested){return /[\u2e80-\u9fff\u3040-\u30ff]/.test(text)&&/^Segoe UI/.test(requested)?cjkFallback:/[\uac00-\ud7af]/.test(text)&&/^Segoe UI/.test(requested)?"Malgun Gothic":requested}
 readonly property int regular: 400
 readonly property int medium: 500
 readonly property int emphasis: 600
 readonly property real lineHeight: 1.25
 readonly property int staticRendering: Text.NativeRendering
 readonly property int animatedRendering: Text.CurveRendering
}
