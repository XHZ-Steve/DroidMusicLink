import QtQuick
import QtTest
import "../../qml" as UI
Item {
    id:host;width:740;height:480
    QtObject {
        id:controller
        property var lyrics:[{time:1000,end:5000,text:"There's gotta be another way out",words:[]},{time:5000,end:9000,text:"歌声穿过夜晚 落在窗边",translation:"A quiet line",words:[]}]
        property int currentLine:0
        property real lyricPosition:1000
        function setting(key,fallback){return fallback}
        function setLyricsVisible(surface,visible){}
    }
    UI.LyricView {
        id:lyricView;anchors.fill:parent;controller:controller;motion:false
        configuration:({fontSize:32,fontWeight:400,activeWeight:700,activeScale:1.1,longLine:"wrap",fadeTop:0,fadeBottom:0})
    }
    SignalSpy{id:seeks;target:lyricView;signalName:"seekRequested"}
    TestCase {
        name:"LyricLayout";when:windowShown
        function init(){controller.lyrics=[{time:1000,end:5000,text:"There's gotta be another way out",words:[]},{time:5000,end:9000,text:"歌声穿过夜晚 落在窗边",translation:"A quiet line",words:[]}];lyricView.configuration=({fontSize:32,fontWeight:400,activeWeight:700,activeScale:1.1,longLine:"wrap",fadeTop:0,fadeBottom:0});lyricView.flushAlignment=false;controller.currentLine=0;controller.lyricPosition=1000;seeks.clear();lyricView.recover();wait(30)}
        function glyphPositions(row){const flow=findChild(row,"lyricWords");verify(flow!==null);return JSON.stringify(flow.children.filter(function(child){return child.objectName==="lyricGlyph"}).map(function(child){return [child.x,child.y,child.width,child.height]}))}
        function test_weight_and_scale_preserve_line_breaks(){
            const timeline=findChild(lyricView,"lyricTimeline");tryVerify(function(){return timeline.itemAtIndex(0)!==null})
            const row=timeline.itemAtIndex(0);const layout=glyphPositions(row);const rowHeight=row.height
            controller.currentLine=1;controller.lyricPosition=5100;wait(40)
            const inactive=JSON.parse(glyphPositions(row)),active=JSON.parse(layout)
            compare(inactive.length,active.length)
            for(let i=0;i<active.length;i++){compare(inactive[i][1],active[i][1]);verify(inactive[i][2]<=active[i][2]+.01)}
            compare(row.height,rowHeight)
            controller.currentLine=0;controller.lyricPosition=1500;wait(40)
            compare(glyphPositions(row),layout);compare(row.height,rowHeight)
        }
        function test_click_requests_timeline_seek(){
            const timeline=findChild(lyricView,"lyricTimeline");const row=timeline.itemAtIndex(0);verify(row!==null)
            mouseClick(row,20,20,Qt.LeftButton);tryCompare(seeks,"count",1);compare(seeks.signalArguments[0][0],1000)
        }
        function test_manual_scroll_defers_follow(){
            const timeline=findChild(lyricView,"lyricTimeline");timeline.manual();verify(timeline.userScrolling)
            const y=timeline.contentY;controller.currentLine=1;controller.lyricPosition=6000;wait(30);compare(timeline.contentY,y)
            lyricView.recover();verify(!timeline.userScrolling)
        }
        function test_long_line_scroll_is_linear_and_seekable(){
            const words=[];for(let i=0;i<50;i++)words.push({text:"Longword ",time:1000+i*200,end:1200+i*200})
            controller.lyrics=[{time:1000,end:11000,text:words.map(function(w){return w.text}).join(""),words:words}]
            lyricView.configuration=({fontSize:32,activeScale:1.1,longLine:"scroll",alignment:"left",maxWidth:300,fadeTop:0,fadeBottom:0})
            lyricView.flushAlignment=true;wait(60)
            const row=findChild(lyricView,"lyricTimeline").itemAtIndex(0)
            verify(row.scrolling);verify(row.scrollDistance>0);compare(row.scrollViewportWidth,300)
            const flow=findChild(row,"lyricWords")
            flow.children.filter(function(item){return item.objectName==="lyricGlyph"}).forEach(function(glyph){compare(glyph.y,0)})
            controller.lyricPosition=3500;wait(20);verify(Math.abs(row.scrollOffset-row.scrollDistance*.25)<.1)
            controller.lyricPosition=8500;wait(20);verify(Math.abs(row.scrollOffset-row.scrollDistance*.75)<.1)
            const paused=row.scrollOffset;wait(60);compare(row.scrollOffset,paused)
            controller.lyricPosition=2000;wait(20);verify(Math.abs(row.scrollOffset-row.scrollDistance*.1)<.1)
        }
    }
}
