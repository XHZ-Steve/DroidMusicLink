#pragma once
#include <QCoreApplication>
#include <QVariant>
#include <QColor>
#include <QFontDatabase>
#include <QKeySequence>
#include <QGuiApplication>
#include <cmath>
#include <QHash>
#include <QSet>

namespace SettingsSchema {
inline QVariantList options(const QStringList &values){QVariantList out;for(const auto &value:values)out<<QVariantMap{{"label",QCoreApplication::translate("Settings",value.toUtf8().constData())},{"value",value}};return out;}
#include "LyricStyleSettings.h"
inline QVariantList descriptors(){
 static const QVariantList catalog=[] {
 QVariantList out;
 const QStringList fonts=qobject_cast<QGuiApplication*>(QCoreApplication::instance())?QFontDatabase::families():QStringList{"Microsoft YaHei UI"};
 auto add=[&](QString key,const char *label,int group,int section,QString type,QVariant value,double min=0,double max=1,double step=1,QVariantList choices={},bool advanced=false,QString help={}){out<<QVariantMap{{"key",key},{"label",QCoreApplication::translate("Settings",label)},{"group",group},{"section",section},{"type",type},{"defaultValue",value},{"min",min},{"max",max},{"step",step},{"options",choices},{"advanced",advanced},{"help",help}};};
 auto action=[&](const char *key,const char *label,int group,int section){add("action/"+QString(key),label,group,section,"action",{});auto row=out.takeLast().toMap();row["action"]=key;out<<row;};
#define S(key,label,g,s,type,value,...) add(key,QT_TRANSLATE_NOOP("Settings",label),g,s,type,value __VA_OPT__(,) __VA_ARGS__)
 S("language","Language",0,0,"enum","zh_CN",0,1,1,options({"zh_CN","en"}));
 S("general/autostart","Start with Windows",0,1,"bool",false);
 S("startHidden","Start in tray",0,1,"bool",false);
 S("closeToTray","Close to tray",0,1,"bool",true);
 S("window/autoHideControls","Auto-hide window controls",0,2,"bool",true);
 S("shortcut/settings","Settings shortcut",0,3,"shortcut","Ctrl+,");
 S("shortcut/devices","Connection shortcut",0,3,"shortcut","Ctrl+Shift+D");
 S("shortcut/desktop","Desktop lyrics shortcut",0,3,"shortcut","Ctrl+Shift+L");
 S("shortcut/floating","Floating player shortcut",0,3,"shortcut","Ctrl+Shift+P");
 S("connection/autoReconnect","Automatic reconnect",1,2,"bool",true);
 S("info/transport","Encrypted connection",1,3,"readonly","WSS / TLS 1.2");
 S("info/player","Current player",2,0,"readonly","");
 S("info/playerSelection","Player selection",2,0,"readonly",QCoreApplication::translate("Settings","Select the player in the phone app"));
 action("floating","Floating player",2,1);
 S("media/smtc","System media controls",2,2,"bool",true);
 S("info/volume","Phone volume",2,3,"readonly","");
 S("lyrics/online","Online lyrics",3,0,"bool",true);
 S("lyrics/providerPriority","Provider priority",3,0,"providerOrder","netease,qq,kugou,lrclib");
 S("lyrics/preferredProvider","Preferred lyric source",3,0,"enum","auto",0,1,1,options({"auto","netease","qq","kugou","lrclib"}));
 S("lyrics/preferCurrentApp","Prefer current player source",3,0,"bool",true);
 S("lyrics/crossSourceTranslation","Supplement translation from other sources",3,0,"bool",false);
 S("lyrics/matchStrategy","Matching strategy",3,1,"enum","default",0,1,1,QVariantList{QVariantMap{{"label",QCoreApplication::translate("Settings","Default")},{"value","default"}},QVariantMap{{"label",QCoreApplication::translate("Settings","strict")},{"value","strict"}},QVariantMap{{"label",QCoreApplication::translate("Settings","balanced")},{"value","balanced"}}});
 S("lyrics/providers/netease/enabled","NetEase lyrics",3,0,"bool",true);
 S("lyrics/providers/qq/enabled","QQ Music lyrics",3,0,"bool",true);
 S("lyrics/providers/kugou/enabled","Kugou lyrics",3,0,"bool",true);
 S("lyrics/providers/lrclib/enabled","LRCLIB lyrics",3,0,"bool",true);
 S("home/layout","Home layout",4,0,"enum",0,0,2,1,QVariantList{QVariantMap{{"label",QCoreApplication::translate("Settings","Album focus")},{"value",0}},QVariantMap{{"label",QCoreApplication::translate("Settings","Expanded artwork")},{"value",1}},QVariantMap{{"label",QCoreApplication::translate("Settings","Expanded lyrics")},{"value",2}}});
 S("desktopLyrics/windowWidth","Lyric window width",3,3,"int",800,280,4096,10,{},true);
 S("desktopLyrics/windowHeight","Lyric window height",3,3,"int",260,100,2160,10,{},true);
 action("importLyrics","Import lyrics",3,0);
 S("lyrics/autoMatch","Automatic matching",3,1,"bool",true);
 action("matchLyrics","Match lyrics",3,1);action("retryLyrics","Retry lyrics",3,1);
 S("showLyrics","Home lyrics",3,2,"bool",true);
 action("desktopLyrics","Desktop lyrics",3,3);action("editDesktopLyrics","Edit desktop lyrics",3,3);
 S("desktopClickThrough","Desktop lyrics click-through",3,3,"bool",false);
 S("desktopLyrics/topmost","Keep desktop lyrics on top",3,3,"bool",true);
 S("overlayTopmost","Keep floating player on top",0,2,"bool",true);
 S("desktopLyrics/inactiveColor","Inactive lyric color",3,3,"color","#b3fff6e9",0,1,1,{},true);
 S("desktopLyrics/background","Desktop lyric background",3,3,"bool",false);
 S("desktopLyrics/opacity","Desktop lyric opacity",3,3,"number",1,.1,1,.05);
 S("desktopLyrics/mode","Desktop lyric mode",3,3,"enum","auto",0,1,1,options({"auto","single"}));
 S("desktopLyrics/pausePolicy","When paused",3,3,"enum","show",0,1,1,options({"show","hide"}));
 S("desktopLyrics/emptyPolicy","Without lyrics",3,3,"enum","hide",0,1,1,options({"show","hide"}));
 S("desktopLyrics/fullscreenPolicy","When another app is fullscreen",3,3,"enum","show",0,1,1,options({"show","hide"}));
 S("lyrics/offset","Lyrics offset",3,4,"int",0,-10000,10000,100);
 S("lyrics/inactiveOpacity","Inactive lyric opacity",3,4,"number",.58,.1,.85,.05);
 S("light","Light appearance",4,0,"bool",false);
 S("theme/source","Theme source",4,0,"enum","auto",0,1,1,options({"auto","artwork","system","brand"}));
 S("theme/brand","Brand",4,0,"enum","spotify",0,1,1,options({"spotify","netease","qqmusic","kugou","youtube","apple"}));
 S("appearance/style","Software style",4,2,"enum","liquid",0,1,1,options({"standard","liquid"}));
 S("motion","Animations",4,3,"bool",true);
 S("info/renderer","Renderer",5,0,"readonly","Qt Quick / QWindowKit");
 action("resetSettings","Restore defaults",5,1);action("clearLyricsCache","Clear lyrics cache",5,1);action("exportSettings","Export settings",5,1);action("importSettings","Import settings",5,1);
 action("checkUpdates","Check updates",5,2);action("exportDiagnostics","Export diagnostics",5,3);
 out+=LyricStyleSettings::descriptors(fonts);
 const QList<QPair<QString,QString>> aliases{{"font","字体 ziti"},{"size","字号 大小 zihao daxiao"},{"weight","字重 粗细 zizhong cuxi"},{"color","颜色 填充 yanse tianchong"},{"opacity","透明度 toumingdu"},{"scale","缩放 放大 suofang fangda"},{"letterspacing","字距 ziju"},{"linespacing","行距 hangju"},{"spacing","间距 jianju"},{"align","对齐 duiqi"},{"translation","翻译 译文 fanyi yiwen"},{"romanization","罗马音 luomayin"},{"shadow","阴影 yinying"},{"glow","辉光 huiguang"},{"outline","描边 miaobian"},{"scroll","滚动 gundong"},{"focus","焦点 jiaodian"},{"fade","渐隐 jianyin"},{"word","逐字 zhuzi"},{"width","宽度 kuandu"},{"height","高度 gaodu"},{"render","渲染 xuanran"},{"provider","来源 优先级 laiyuan youxianji"},{"language","语言 yuyan"},{"autostart","开机 自启 kaiji ziqi"},{"shortcut","快捷键 kuaijiejian"},{"volume","音量 yinliang"},{"reconnect","重连 chonglian"},{"cache","缓存 huancun"},{"theme","主题 zhuti"},{"background","背景 beijing"},{"motion","动画 donghua"}};
 for(auto &value:out){auto row=value.toMap();const auto key=row["key"].toString().toLower();QString tags=row.value("tags").toString();if(key.startsWith("lyrics/"))tags.remove("zhuomian");for(const auto &alias:aliases)if(key.contains(alias.first))tags+=" "+alias.second;row["tags"]=tags;value=row;}

 // Compatibility keys stay valid but do not appear as duplicated settings rows.
#undef S
 return out;
 }();return catalog;
}
inline QVariantMap descriptor(const QString &key){static const QHash<QString,QVariantMap> index=[]{QHash<QString,QVariantMap> result;for(const auto &row:descriptors()){auto map=row.toMap();result.insert(map["key"].toString(),map);}return result;}();return index.value(key);}
inline bool normalize(const QString &key,const QVariant &input,QVariant *output){
 auto row=descriptor(key);if(row.isEmpty())return false;const auto type=row["type"].toString();QVariant result=input;
 if(type=="bool"){if(input.metaType().id()==QMetaType::Bool)result=input.toBool();else if(input.toString()=="true"||input.toString()=="false")result=input.toString()=="true";else return false;}
 else if(type=="int"||type=="number"){bool ok=false;double number=input.toDouble(&ok);if(!ok||!std::isfinite(number))return false;number=qBound(row["min"].toDouble(),number,row["max"].toDouble());result=type=="int"?QVariant(qRound(number)):QVariant(number);}
 else if(type=="color"){if(input.toString()=="auto"&&row.value("defaultValue").toString()=="auto"){if(output)*output="auto";return true;}QColor color(input.toString());if(!color.isValid())return false;result=color.name(QColor::HexArgb);}
 else if(type=="shortcut"){auto keys=QKeySequence::fromString(input.toString(),QKeySequence::PortableText);if(keys.isEmpty()||keys.count()>2)return false;result=keys.toString(QKeySequence::PortableText);}
 else if(type=="providerOrder"){auto ids=input.toString().split(',',Qt::SkipEmptyParts);if(ids.size()!=4||QSet<QString>(ids.begin(),ids.end())!=QSet<QString>{"netease","qq","kugou","lrclib"})return false;result=ids.join(',');}
 else if(type=="enum"){bool found=false;for(const auto &option:row["options"].toList())if(option.toMap()["value"].toString()==input.toString()){found=true;result=option.toMap()["value"];}if(!found)return false;}
 else return false;
 if(output)*output=result;return true;
}
}
