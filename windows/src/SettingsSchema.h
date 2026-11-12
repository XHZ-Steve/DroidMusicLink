#pragma once
#include <QCoreApplication>
#include <QVariant>
#include <QColor>
#include <QFontDatabase>
#include <QKeySequence>
#include <QGuiApplication>
#include <cmath>

namespace SettingsSchema {
inline QVariantList options(const QStringList &values){QVariantList out;for(const auto &value:values)out<<QVariantMap{{"label",QCoreApplication::translate("Settings",value.toUtf8().constData())},{"value",value}};return out;}
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
 S("media/unmuteVolume","Initial unmute level",2,3,"number",.25,.05,1,.05);
 S("lyrics/online","Online lyrics",3,0,"bool",true);
 S("lyrics/providerPriority","Online provider",3,0,"readonly","LRCLIB");
 action("importLyrics","Import lyrics",3,0);
 S("lyrics/autoMatch","Automatic matching",3,1,"bool",true);
 action("matchLyrics","Match lyrics",3,1);action("retryLyrics","Retry lyrics",3,1);
 S("showLyrics","Home lyrics",3,2,"bool",true);
 S("lyrics/fontFamily","Lyric font",3,2,"enum","Microsoft YaHei UI",0,1,1,options(fonts));
 S("lyrics/fontSize","Main lyric size",3,2,"int",28,16,56,1);
 S("lyrics/translation","Translation",3,2,"bool",true);
 S("lyrics/romanization","Romanization",3,2,"bool",false);
 action("desktopLyrics","Desktop lyrics",3,3);action("editDesktopLyrics","Edit desktop lyrics",3,3);
 S("desktopClickThrough","Desktop lyrics click-through",3,3,"bool",false);
 S("overlayTopmost","Keep overlays on top",3,3,"bool",true);
 S("desktopLyrics/fontFamily","Desktop lyric font",3,3,"enum","Microsoft YaHei UI",0,1,1,options(fonts));
 S("desktopLyrics/fontSize","Desktop lyrics size",3,3,"int",36,18,96,1);
 S("desktopLyrics/color","Lyric color",3,3,"color","#fff6e9");
 S("desktopLyrics/inactiveColor","Inactive lyric color",3,3,"color","#b3fff6e9",0,1,1,{},true);
 S("desktopLyrics/outlineColor","Outline color",3,3,"color","#99000000",0,1,1,{},true);
 S("desktopLyrics/outlineWidth","Outline width",3,3,"number",1,0,4,.5,{},true);
 S("desktopLyrics/background","Desktop lyric background",3,3,"bool",false);
 S("desktopLyrics/shadow","Text shadow",3,3,"bool",true);
 S("desktopLyrics/opacity","Desktop lyric opacity",3,3,"number",1,.1,1,.05);
 S("desktopLyrics/mode","Desktop lyric mode",3,3,"enum","two",0,1,1,options({"single","two"}));
 S("desktopLyrics/pausePolicy","When paused",3,3,"enum","show",0,1,1,options({"show","hide"}));
 S("desktopLyrics/emptyPolicy","Without lyrics",3,3,"enum","hide",0,1,1,options({"show","hide"}));
 S("desktopLyrics/fullscreenPolicy","When another app is fullscreen",3,3,"enum","show",0,1,1,options({"show","hide"}));
 S("lyrics/offset","Lyrics offset",3,4,"int",0,-10000,10000,100);
 S("lyrics/scrollDuration","Scroll duration",3,4,"int",420,100,1200,20);
 S("lyrics/easing","Scroll easing",3,4,"enum","outCubic",0,1,1,options({"linear","outCubic","inOutCubic"}));
 S("lyrics/focus","Lyric focus position",3,4,"number",.45,.2,.7,.05);
 S("lyrics/lineSpacing","Lyric line spacing",3,4,"number",.8,.2,1.8,.1);
 S("lyrics/activeScale","Active lyric scale",3,4,"number",1.04,1,1.2,.02);
 S("lyrics/inactiveOpacity","Inactive lyric opacity",3,4,"number",.58,.1,.85,.05);
 S("lyrics/highlight","Highlight mode",3,4,"enum","word",0,1,1,options({"word","line"}));
 S("light","Light appearance",4,0,"bool",false);
 S("theme/source","Theme source",4,0,"enum","auto",0,1,1,options({"auto","artwork","system","brand"}));
 S("theme/brand","Brand",4,0,"enum","spotify",0,1,1,options({"spotify","netease","qqmusic","kugou","youtube","apple"}));
 S("appearance/style","Software style",4,2,"enum","liquid",0,1,1,options({"standard","liquid"}));
 S("motion","Animations",4,3,"bool",true);
 S("info/renderer","Renderer",5,0,"readonly","Qt Quick / QWindowKit");
 action("resetSettings","Restore defaults",5,1);action("clearLyricsCache","Clear lyrics cache",5,1);action("exportSettings","Export settings",5,1);action("importSettings","Import settings",5,1);
 action("checkUpdates","Check updates",5,2);action("exportDiagnostics","Export diagnostics",5,3);
 // Compatibility keys stay valid but do not appear as duplicated settings rows.
#undef S
 return out;
 }();return catalog;
}
inline QVariantMap descriptor(const QString &key){for(const auto &row:descriptors()){auto map=row.toMap();if(map["key"]==key)return map;}return {};}
inline bool normalize(const QString &key,const QVariant &input,QVariant *output){
 auto row=descriptor(key);if(row.isEmpty())return false;const auto type=row["type"].toString();QVariant result=input;
 if(type=="bool"){if(input.metaType().id()==QMetaType::Bool)result=input.toBool();else if(input.toString()=="true"||input.toString()=="false")result=input.toString()=="true";else return false;}
 else if(type=="int"||type=="number"){bool ok=false;double number=input.toDouble(&ok);if(!ok||!std::isfinite(number))return false;number=qBound(row["min"].toDouble(),number,row["max"].toDouble());result=type=="int"?QVariant(qRound(number)):QVariant(number);}
 else if(type=="color"){QColor color(input.toString());if(!color.isValid())return false;result=color.name(QColor::HexArgb);}
 else if(type=="shortcut"){auto keys=QKeySequence::fromString(input.toString(),QKeySequence::PortableText);if(keys.isEmpty()||keys.count()>2)return false;result=keys.toString(QKeySequence::PortableText);}
 else if(type=="enum"){bool found=false;for(const auto &option:row["options"].toList())if(option.toMap()["value"].toString()==input.toString())found=true;if(!found)return false;result=input.toString();}
 else return false;
 if(output)*output=result;return true;
}
}
