#include "Bridge.h"
#include <QGuiApplication>
#include <QCommandLineParser>
#include <QTemporaryDir>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <cstdio>
#include <QWindow>
#include <windows.h>
#include <shobjidl.h>
// Connects to the real passive Android service. Pairing approval remains on the phone.
int main(int argc,char **argv){
 SetCurrentProcessExplicitAppUserModelID(L"DroidMediaLink.Desktop");QGuiApplication app(argc,argv);qInstallMessageHandler([](QtMsgType,const QMessageLogContext&,const QString &message){fprintf(stderr,"%s\n",message.toUtf8().constData());fflush(stderr);});app.setOrganizationName("DroidMediaLinkTests");app.setApplicationName("LinkProbe");QTemporaryDir config;QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,config.path());
 QCommandLineParser parser;parser.addHelpOption();parser.addOptions({{{"u","url"},"WSS URL","url"},{"discover","Verify actual LAN DNS-SD discovery"},{"expect-auth-rejection","Expect phone pairing rejection"},{"fixture","Verify explicit Android instrumentation TEST SESSION play/pause/seek"},{"native","Initialize actual SMTC for a visible test window"}});parser.process(app);
 if(parser.isSet("discover")){
  Bridge discovery;
  QSet<QString> found;
  QObject::connect(&discovery,&Bridge::changed,&app,[&]{for(const auto &item:discovery.devices()){auto row=item.toMap();auto url=row.value("url").toString();if(!found.contains(url)){found.insert(url);qInfo()<<"Discovered:"<<row.value("name").toString()<<url;}}});
  QTimer::singleShot(0,&discovery,&Bridge::discover);
  QTimer::singleShot(6000,&discovery,&Bridge::discover);
  QTimer::singleShot(12000,&app,[&]{QSet<QString> urls;for(const auto &item:discovery.devices())urls.insert(item.toMap().value("url").toString());const bool passed=!urls.isEmpty()&&urls.size()==discovery.devices().size();qInfo()<<"Discovery after repeated search; rows:"<<discovery.devices().size()<<"unique URLs:"<<urls.size()<<"passed:"<<passed;app.exit(passed?0:9);});
  return app.exec();
 }
 if(!parser.isSet("url"))return 64;
 Bridge bridge;bool online=false,finished=false,ack=false,reconnected=false,reconnecting=false;int snapshots=0,phase=0;QElapsedTimer authenticated,stage;QTimer fixturePoll;
 QWindow nativeWindow;if(parser.isSet("native")){nativeWindow.setTitle("DroidMediaLink · TEST SESSION integration");nativeWindow.resize(480,160);nativeWindow.show();if(!bridge.initializeNative(nativeWindow.winId())){qCritical()<<"FAIL real GetForWindow SMTC initialization";return 6;}qInfo()<<"PASS real GetForWindow SMTC initialization; system panel/media key UI remains a separate check";}
 QObject::connect(&bridge,&Bridge::commandFinished,&app,[&](QString,bool success,QString error){if(!success){qCritical()<<"FAIL fixture command rejected:"<<error;finished=true;app.exit(5);}else ack=true;});
 QObject::connect(&fixturePoll,&QTimer::timeout,&app,[&]{if(finished||!bridge.online())return;if(phase==0&&bridge.title().contains("TEST SESSION")){qInfo()<<"TEST SESSION received; dispatching real MediaSession commands";phase=1;ack=false;stage.start();bridge.command("play");}else if(phase==1&&ack&&bridge.playing()&&stage.elapsed()>8000){phase=2;ack=false;bridge.command("pause");}else if(phase==2&&ack&&!bridge.playing()){phase=3;ack=false;bridge.command("seek",42000);}else if(phase==3&&ack&&!bridge.playing()&&qAbs(bridge.position()-42000)<500){phase=4;if(parser.isSet("native")&&bridge.nativeStatus()!=0){finished=true;qCritical()<<"FAIL SMTC metadata/timeline setters:"<<bridge.nativeStatus();app.exit(7);return;}qInfo()<<"PASS fixture play/pause/seek callbacks and command results; SMTC update status:"<<bridge.nativeStatus();}if(phase==4&&authenticated.elapsed()>62000){finished=true;qInfo()<<"PASS authenticated WSS, snapshots, controls and 62s heartbeat survival";bridge.disconnectDevice();app.exit(0);}});
 QObject::connect(&bridge,&Bridge::changed,&app,[&]{
  if(finished)return;if(reconnecting&&bridge.pairingPending()){finished=true;qCritical()<<"FAIL saved pinned identity unexpectedly requested pairing";app.exit(8);return;}if(reconnecting&&bridge.online()){reconnecting=false;reconnected=true;qInfo()<<"PASS saved device identity reconnected with certificate pin and no pairing prompt";}static QString lastStatus;if(!bridge.pairingCode().isEmpty()){static QString lastCode;if(lastCode!=bridge.pairingCode()){lastCode=bridge.pairingCode();qInfo()<<"PAIRING SAS:"<<lastCode;}}if(lastStatus!=bridge.status()){lastStatus=bridge.status();qInfo()<<"Link state:"<<lastStatus;}
  if(bridge.status().contains("Authorization rejected")&&parser.isSet("expect-auth-rejection")){finished=true;qInfo()<<"PASS phone pairing rejected";app.exit(0);}
  if(bridge.online()){++snapshots;if(!online){online=true;authenticated.start();qInfo()<<"Authenticated with actual Android service";QTimer::singleShot(20000,&app,[&]{if(finished)return;reconnecting=true;bridge.disconnectDevice();QTimer::singleShot(300,&app,[&]{bridge.connectDevice(parser.value("url"));});});if(parser.isSet("fixture"))fixturePoll.start(250);else QTimer::singleShot(62000,&app,[&]{finished=true;if(!bridge.online()||!reconnected){qCritical()<<"FAIL disconnected or reconnect failed during heartbeat verification";app.exit(3);return;}qInfo().noquote()<<QJsonDocument(QJsonObject{{"result","PASS"},{"authenticated",true},{"stateUpdates",snapshots},{"durationMs",bridge.duration()},{"commands",bridge.commands().join(',')},{"heartbeatObservedBySurvival",true}}).toJson(QJsonDocument::Compact);bridge.disconnectDevice();app.exit(0);});}}
 });
 QObject::connect(&bridge,&Bridge::notification,&app,[&](QString message){qInfo()<<"Device response:"<<message;});
 QTimer::singleShot(150000,&app,[&]{if(!finished){qCritical()<<"FAIL connection/authentication/fixture timed out; state:"<<bridge.status()<<"fixture phase:"<<phase;app.exit(2);}});
 QTimer::singleShot(0,&app,[&]{bridge.connectDevice(parser.value("url"));});
 return app.exec();
}





