#include "Bridge.h"
#include <QCoreApplication>
#include <QDebug>
#include <QTemporaryDir>
#include <QJSEngine>
#include <QElapsedTimer>
int main(int argc,char **argv){QCoreApplication app(argc,argv);QTemporaryDir dir;QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,dir.path());app.setOrganizationName("DmlTests");app.setApplicationName("isolated");auto rows=Bridge::parseLyrics("[00:02.50]B\n[00:01.005][00:03]A\n[offset:-5]");if(rows.size()!=3||rows[0].toMap()["time"].toLongLong()!=1000||rows[1].toMap()["time"].toLongLong()!=2495)return 1;auto ttml=Bridge::parseLyrics("<tt><body><p begin=\"00:00:01.500\">Hello <span>world</span></p></body></tt>");if(ttml.size()!=1||ttml[0].toMap()["time"].toLongLong()!=1500||ttml[0].toMap()["text"]!="Hello world")return 2;if(!Bridge::parseLyrics("<tt><p begin=\"1s\">broken").isEmpty())return 3;
 auto words=Bridge::parseLyrics("[00:01.00]<00:01.00>Hello <00:01.50>world");auto segments=words[0].toMap()["words"].toList();if(segments.size()!=2||segments[1].toMap()["time"]!=1500||words[0].toMap()["text"]!="Hello world")return 4;
 auto timed=Bridge::parseLyrics("<tt><p begin=\"1s\"><span begin=\"1s\">A</span><span begin=\"1.5s\">B</span></p></tt>");if(timed[0].toMap()["words"].toList().size()!=2)return 5;
 Bridge bridge;bridge.setSetting("glass",99.);bridge.setSetting("lyricSize",-20);bridge.setSetting("lyrics/offset",-20000);bridge.setSetting("media/smtc","wrong");bridge.setSetting("arbitrary","blocked");if(bridge.setting("glass").toDouble()!=.5||bridge.setting("lyricSize").toInt()!=18||bridge.setting("lyrics/offset").toInt()!=-10000||bridge.setting("media/smtc").isValid()||bridge.setting("arbitrary").isValid())return 6;bridge.connectDevice("ws://127.0.0.1","invalid","");if(bridge.online())return 7;
 QJSEngine engine;QJSEngine::setObjectOwnership(&bridge,QJSEngine::CppOwnership);engine.globalObject().setProperty("backend",engine.newQObject(&bridge));if(engine.evaluate("backend.setSetting('motion', false); backend.setSetting('light', true)").isError()||bridge.setting("motion",true).toBool()||!bridge.setting("light",false).toBool())return 8;bridge.setSetting("showLyrics",QVariant::fromValue(QJSValue(false)));if(bridge.setting("showLyrics",true).toBool())return 9;
 // Legacy profiles may have home lyrics disabled by the former Dock action.
 bridge.setSetting("home/layout",1);if(bridge.setting("showLyrics",true).toBool())return 70;
 bridge.setSetting("home/layout",2);if(!bridge.setting("showLyrics",false).toBool())return 71;
 bridge.setSetting("showLyrics",false);bridge.setSetting("home/layout",0);if(!bridge.setting("showLyrics",false).toBool())return 72;
 bridge.setSetting("showLyrics",false);if(bridge.setting("showLyrics",true).toBool())return 73;
 QByteArray fingerprint,clientNonce,serverNonce;for(int i=0;i<32;++i){fingerprint+=char(i);clientNonce+=char(i+32);serverNonce+=char(i+64);}if(Bridge::sasCode(fingerprint,clientNonce,serverNonce,"test-client")!="363475"||!Bridge::sasCode({},clientNonce,serverNonce,"test-client").isEmpty())return 10;
 QSettings persisted;persisted.setValue("preferences/light",QString("false"));persisted.setValue("preferences/motion",QString("true"));persisted.setValue("preferences/glass",QString("0.25"));persisted.sync();Bridge reloaded;if(reloaded.setting("light").metaType().id()!=QMetaType::Bool||reloaded.setting("light").toBool()||reloaded.setting("motion").metaType().id()!=QMetaType::Bool||!reloaded.setting("motion").toBool()||reloaded.setting("glass").metaType().id()!=QMetaType::Double||reloaded.setting("glass").toDouble()!=.25)return 11;
 auto hueDistance=[](QColor a,QColor b){const double d=std::abs(a.hslHueF()-b.hslHueF());return std::min(d,1.-d);};
 QImage warm(80,80,QImage::Format_RGB32);warm.fill(QColor("#e47432"));auto warmPalette=CoverPalette::fromImage(warm);if(warmPalette.primary.hslHueF()>.15||warmPalette.primary.hslSaturationF()<.5)return 12;
 QImage cool(80,80,QImage::Format_RGB32);cool.fill(QColor("#3679d8"));auto coolPalette=CoverPalette::fromImage(cool);if(coolPalette.primary.hslHueF()<.5||coolPalette.primary.hslHueF()>.7||coolPalette.primary.hslSaturationF()<.5)return 13;
 QImage opposite(80,80,QImage::Format_RGB32);for(int y=0;y<80;++y)for(int x=0;x<80;++x)opposite.setPixelColor(x,y,QColor(x<40?"#eb284b":"#24cbd5"));auto distinct=CoverPalette::fromImage(opposite);if(distinct.primary.hslSaturationF()<.5||distinct.secondary.hslSaturationF()<.5||hueDistance(distinct.primary,distinct.secondary)<.25)return 14;
 QImage neutral(80,80,QImage::Format_RGB32);for(int y=0;y<80;++y)for(int x=0;x<80;++x)neutral.setPixelColor(x,y,QColor(x<40?"#171717":"#eeeeee"));auto mono=CoverPalette::fromImage(neutral);if(mono.primary.hslSaturationF()>.01||mono.secondary.hslSaturationF()>.01||mono.deep.hslSaturationF()>.01)return 15;
 QImage pale(80,80,QImage::Format_RGB32);pale.fill(QColor("#eed5e5"));auto palePalette=CoverPalette::fromImage(pale);if(palePalette.primary.hslSaturationF()<.15||palePalette.primary.lightnessF()>.65||palePalette.deep.lightnessF()>.17)return 16;
 for(int i=1;i<argc;++i){const QImage cover(QString::fromLocal8Bit(argv[i]));if(cover.isNull())return 17;const auto palette=CoverPalette::fromImage(cover);QElapsedTimer timer;timer.start();for(int n=0;n<100;++n)CoverPalette::fromImage(cover);qInfo()<<"Actual cover palette:"<<palette.primary.name()<<palette.secondary.name()<<palette.deep.name()<<"mean sampling us:"<<timer.nsecsElapsed()/100000.;}

 bridge.setAppearance("global","glassBlur",3.6);bridge.setAppearance("dock","glassBlur",7.2);
 if(bridge.appearanceConfig("dock")["glassBlur"].toDouble()!=3.6)return 18;
 bridge.setAppearanceInherited("dock",false);if(bridge.appearanceConfig("dock")["glassBlur"].toDouble()!=7.2)return 19;
 bridge.setAppearance("dock","glassBlur",-1);if(bridge.appearanceConfig("dock")["glassBlur"].toDouble()!=7.2)return 20;
 const auto settingsFile=QUrl::fromLocalFile(dir.filePath("settings.json"));bridge.exportSettings(settingsFile);
 bridge.resetAppearance("dock");bridge.resetAppearance("global");bridge.importSettings(settingsFile);
 if(bridge.appearanceInherited("dock")||bridge.appearanceConfig("dock")["glassBlur"].toDouble()!=7.2||bridge.appearanceConfig("global")["glassBlur"].toDouble()!=3.6)return 21;
 bridge.setSetting("lyrics/activeScale",1.18);bridge.setSetting("desktopLyrics/activeScale",1.02);bridge.saveLyricPreset("TEST style");
 bridge.setSetting("lyrics/activeScale",1.);bridge.applyLyricPreset("TEST style");if(qAbs(bridge.setting("lyrics/activeScale").toDouble()-1.18)>.001||qAbs(bridge.setting("desktopLyrics/activeScale").toDouble()-1.02)>.001)return 22;
 bridge.renameLyricPreset("TEST style","TEST renamed");bridge.exportSettings(settingsFile);bridge.deleteLyricPreset("TEST renamed");bridge.importSettings(settingsFile);bridge.setSetting("lyrics/activeScale",1.);bridge.applyLyricPreset("TEST renamed");if(qAbs(bridge.setting("lyrics/activeScale").toDouble()-1.18)>.001)return 23;
 bridge.resetLyricSettings("main");if(bridge.setting("lyrics/activeScale").isValid()||qAbs(bridge.setting("desktopLyrics/activeScale").toDouble()-1.02)>.001)return 24;
 qInfo()<<"LRC, word timings, TTML, settings, WSS and warm/cool/complementary/monochrome/pale cover palette checks passed";return 0;}
