import QtQuick
Window {
 id:window;width:1280;height:650;visible:true;color:"#151c28";title:"DroidMediaLink typography comparison"
 Component.onCompleted:console.log("Available UI families",Qt.fontFamilies().filter(function(name){return /Segoe|YaHei|微软雅黑/i.test(name)}))
 Row {
  x:24;y:24;spacing:16
  Repeater {model:[Text.NativeRendering,Text.QtRendering,Text.CurveRendering]
   Item {
    required property int modelData
    required property int index
    width:400;height:600
    Rectangle{anchors.fill:parent;radius:14;color:"#202b3c"}
    Text{x:20;y:20;text:["Native rendering","Qt distance field","Curve rendering"][parent.index];font.pixelSize:18;font.family:"Microsoft YaHei UI";color:"#b8cce4"}
    Text{x:20;y:90;text:"风经过窗边";font.family:"Microsoft YaHei UI";font.pixelSize:48;font.weight:500;font.contextFontMerging:true;renderType:parent.modelData;renderTypeQuality:104;color:"#f5f7fc";scale:1.10;transformOrigin:Item.Left}
    Text{x:20;y:180;text:"A quiet rhythm";font.family:"Microsoft YaHei UI";font.pixelSize:40;font.weight:600;renderType:parent.modelData;renderTypeQuality:104;color:"#f5f7fc";scale:1.10;transformOrigin:Item.Left}
    Text{x:20;y:265;text:"微光与呼吸  012345";font.family:"Microsoft YaHei UI";font.pixelSize:22;font.contextFontMerging:true;renderType:parent.modelData;renderTypeQuality:104;color:"#aebdd2";scale:1.10;transformOrigin:Item.Left}
    Rectangle{x:12;y:340;width:376;height:230;radius:10;color:"#eff2f9"
     Text{x:15;y:25;text:"风经过窗边";font.family:"Microsoft YaHei UI";font.pixelSize:42;font.weight:500;font.contextFontMerging:true;renderType:parent.parent.modelData;renderTypeQuality:104;color:"#253348";scale:1.10;transformOrigin:Item.Left}
     Text{x:15;y:115;text:"A quiet rhythm";font.family:"Microsoft YaHei UI";font.pixelSize:34;font.weight:600;renderType:parent.parent.modelData;renderTypeQuality:104;color:"#253348";scale:1.10;transformOrigin:Item.Left}
    }
   }
  }
 }
 Timer{interval:1200;running:true;onTriggered:window.contentItem.grabToImage(function(result){result.saveToFile("artifacts/screenshots/typography-rendering-comparison.png");Qt.quit()})}
}
