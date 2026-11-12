import QtQuick
import QtTest
import "../../qml" as UI
Item {
    id:host;width:900;height:620
    QtObject {
        id:controller
        property var values:({"home/layout":0,"showLyrics":true,"lyrics/fadeTop":0,"lyrics/fadeBottom":0})
        property int revision:0
        property url artwork:""
        property string ambientPrimary:"#6d829e"
        property string title:"TEST album title"
        property string artist:"TEST artist"
        property string appName:"TEST player"
        property bool online:true
        property bool lyricsLoading:false
        property var lyrics:[{time:1000,end:5000,text:"TEST first lyric"},{time:5000,end:9000,text:"TEST next lyric"}]
        property int currentLine:0
        property real lyricPosition:1000
        function setting(key,fallback){return values[key]===undefined?fallback:values[key]}
        function setSetting(key,value){const next=Object.assign({},values);next[key]=value;values=next;revision++}
        function setLyricsVisible(surface,visible){}
    }
    UI.NowPlayingCanvas{id:canvas;anchors.fill:parent;controller:controller;settingsRevision:controller.revision;motion:true}
    TestCase {
        name:"NowPlayingLayouts";when:windowShown
        function init(){controller.title="TEST album title";controller.artist="TEST artist";controller.appName="TEST player";controller.setSetting("home/layout",0);wait(550)}
        function test_cover_cycles_three_persistent_modes(){
            const cover=findChild(canvas,"heroArtwork");const control=findChild(canvas,"coverLayoutSwitch");const lyrics=findChild(canvas,"homeLyrics")
            verify(cover);verify(control);verify(lyrics);compare(canvas.mode,0);compare(lyrics.compact,true)
            const centered=cover.x+cover.width/2;verify(Math.abs(centered-canvas.width/2)<1)
            mouseClick(control,control.width/2,control.height/2);compare(controller.values["home/layout"],1);tryCompare(canvas,"mode",1);wait(520)
            compare(lyrics.visible,false);compare(controller.values.showLyrics,true)
            mouseClick(control,control.width/2,control.height/2);tryCompare(canvas,"mode",2);wait(520)
            compare(lyrics.compact,false);compare(lyrics.visible,true)
            mouseClick(control,control.width/2,control.height/2);tryCompare(canvas,"mode",0);wait(520)
            tryVerify(function(){return Math.abs(cover.x+cover.width/2-canvas.width/2)<1},1500,"Cover must settle at canvas center")
        }
        function test_geometry_interpolates_between_modes(){
            const cover=findChild(canvas,"heroArtwork");const x=cover.x
            controller.setSetting("home/layout",2);wait(50)
            const during=cover.x;verify(Math.abs(during-x)<canvas.width*.5)
            wait(500);verify(Math.abs(cover.x-x)>10)
        }
        function test_all_surfaces_share_motion_clock(){
            const cover=findChild(canvas,"heroArtwork"),metadata=findChild(canvas,"trackMetadata"),title=findChild(canvas,"trackTitle")
            for(let mode=0;mode<3;mode++){
                canvas.nextLayout();tryVerify(function(){return canvas.transitionProgress>0&&canvas.transitionProgress<1})
                for(let frame=0;frame<24;frame++){
                    const from=canvas.fromGeometry,to=canvas.toGeometry,p=canvas.transitionProgress
                    const outward=to.cover.x<from.cover.x,travel=Math.abs(to.cover.x-from.cover.x)>1?(outward?1-Math.pow(1-p,6):Math.pow(p,6)):p
                    verify(Math.abs(cover.x-(from.cover.x+(to.cover.x-from.cover.x)*travel))<.01)
                    verify(Math.abs(metadata.x-(from.metadata.x+(to.metadata.x-from.metadata.x)*p))<.01)
                    const vertical=Math.abs(to.cover.x-from.cover.x)>1?(outward?Math.pow(p,3):1-Math.pow(1-p,3)):p
                    const tx=from.titleX+(to.titleX-from.titleX)*travel
                    const clearance=Math.max(0,Math.min(1,(tx-cover.x-cover.width)/32)),released=clearance*clearance*(3-2*clearance)
                    const y=from.metadata.y+(to.metadata.y-from.metadata.y)*vertical
                    const expectedY=y+(Math.max(y,cover.y+cover.height+14)-y)*(1-released)
                    verify(Math.abs(metadata.y-expectedY)<.01)
                    const point=title.mapToItem(canvas,0,0)
                    const overlaps=point.x<cover.x+cover.width&&point.x+title.width>cover.x&&point.y<cover.y+cover.height&&point.y+title.height>cover.y
                    verify(!overlaps,"Artwork crossed the title: mode="+canvas.mode+" progress="+p+" cover="+JSON.stringify([cover.x,cover.y,cover.width,cover.height])+" title="+JSON.stringify([point.x,point.y,title.width,title.height]))
                    wait(20)
                }
                tryCompare(canvas,"changingLayout",false,1500)
                verify(Math.abs(cover.x-canvas.coverTarget.x)<.01)
                verify(Math.abs(metadata.y-canvas.metadataTarget.y)<.01)
            }
        }
        function test_interrupted_motion_starts_at_visible_geometry(){
            const cover=findChild(canvas,"heroArtwork"),metadata=findChild(canvas,"trackMetadata"),title=findChild(canvas,"trackTitle")
            for(let click=0;click<8;click++){
                const x=cover.x,y=metadata.y,titleX=title.mapToItem(canvas,0,0).x
                canvas.nextLayout()
                verify(Math.abs(cover.x-x)<.01,"Cover jumped on interruption")
                verify(Math.abs(metadata.y-y)<.01,"Metadata jumped on interruption")
                verify(Math.abs(title.mapToItem(canvas,0,0).x-titleX)<.01,"Title jumped on interruption")
                wait(55)
            }
            tryCompare(canvas,"changingLayout",false,1500)
            verify(Math.abs(cover.x-canvas.coverTarget.x)<.01)
        }
        function test_long_metadata_settles_without_tail_jump(){
            controller.title="A moderately long music title crossing the wrap boundary"
            const metadata=findChild(canvas,"trackMetadata")
            canvas.nextLayout();tryCompare(canvas,"changingLayout",false,1500)
            canvas.nextLayout();tryVerify(function(){return canvas.transitionProgress>.8})
            const target=canvas.toGeometry.metadata
            tryCompare(canvas,"changingLayout",false,1500)
            verify(Math.abs(metadata.y-target.y)<.01,"Metadata target moved at animation completion")
            verify(Math.abs(metadata.height-target.height)<.01,"Metadata height changed at animation completion")
            controller.artist="";controller.appName="";wait(50)
            verify(isFinite(metadata.height));verify(metadata.height>0)
        }
        function test_narrow_lyrics_remain_centered_and_focused(){
            host.width=520;host.height=520;controller.setSetting("home/layout",2)
            tryCompare(canvas,"changingLayout",false,1500)
            const lyrics=findChild(canvas,"homeLyrics"),timeline=findChild(lyrics,"lyricTimeline")
            compare(lyrics.alignment,"center")
            tryVerify(function(){return timeline.itemAtIndex(controller.currentLine)!==null})
            lyrics.recover();wait(550)
            const row=timeline.itemAtIndex(controller.currentLine),position=row.mapToItem(lyrics,0,0)
            verify(Math.abs(position.y+row.height/2-lyrics.height*lyrics.focusPosition)<2,"Current lyric drifted away from focus")
            host.width=900;host.height=620
        }
        function test_resize_invalidates_pending_layout_callback(){
            canvas.nextLayout();host.width=640;wait(80)
            compare(canvas.changingLayout,false);compare(canvas.transitionProgress,1)
            const cover=findChild(canvas,"heroArtwork")
            verify(Math.abs(cover.x+cover.width/2-canvas.width/2)<1)
            host.width=900
        }
        function test_track_update_invalidates_pending_layout_callback(){
            canvas.nextLayout();controller.title="TEST a newly delivered media title";wait(80)
            compare(canvas.changingLayout,false);compare(canvas.transitionProgress,1)
        }
    }
}
