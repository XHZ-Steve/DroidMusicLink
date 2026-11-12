#include "Bridge.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QEventLoop>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimer>
#include <QDebug>

class LyricsEngineTests {
public:
 static int run(){
  QTemporaryDir files;QSettings saved;saved.setValue("preferences/lyrics/online",false);saved.setValue("preferences/lyrics/autoMatchMigration",true);saved.sync();Bridge bridge;
  const auto metadata=QJsonObject{{"title","TEST ONLY lyric engine A"},{"artist","TEST fixture"},{"album","TEST"},{"durationMs",0},{"state","paused"}};
  bridge.applyMedia(metadata);const auto unknownDurationKey=bridge.m_lyricKey;
  bridge.applyMedia({{"durationMs",30000}});if(unknownDurationKey==bridge.m_lyricKey||bridge.m_lyricKey!=bridge.lyricKey())return 1;
  auto write=[&](QString name,QString text){QFile file(files.path()+"/"+name);if(!file.open(QIODevice::WriteOnly))return QUrl();file.write(text.toUtf8());file.close();return QUrl::fromLocalFile(file.fileName());};
  const auto first=write("first.lrc","[00:01.00]TEST FIRST\n[00:05.00]TEST SECOND");
  bridge.importLyrics(first);if(bridge.lyrics().size()!=2)return 2;
  bridge.m_position=1500;bridge.updateLyricClock();if(bridge.currentLine()!=0)return 3;
  bridge.m_position=6500;bridge.updateLyricClock();if(bridge.currentLine()!=1)return 4;
  bridge.m_position=0;bridge.updateLyricClock();if(bridge.currentLine()!=-1)return 5;
  // Exercise a real in-flight HTTP reply, then supersede it with a manual import.
  // The local fixture contains explicitly marked TEST lyrics and never leaves loopback.
  QTcpServer server;if(!server.listen(QHostAddress::LocalHost,0))return 6;
  QObject::connect(&server,&QTcpServer::newConnection,&server,[&]{auto socket=server.nextPendingConnection();QObject::connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);QTimer::singleShot(120,socket,[socket]{if(socket->state()!=QAbstractSocket::ConnectedState)return;const auto bytes=QJsonDocument(QJsonObject{{"syncedLyrics","[00:01]STALE TEST RESPONSE"}}).toJson(QJsonDocument::Compact);socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "+QByteArray::number(bytes.size())+"\r\nConnection: close\r\n\r\n"+bytes);socket->disconnectFromHost();});});
  bridge.requestLyrics(QUrl("http://127.0.0.1:"+QString::number(server.serverPort())+"/test"),bridge.lyricKey(),bridge.m_lyricGeneration,false);
  const auto replacement=write("replacement.lrc","[00:00]MANUAL TEST WINS");
  QEventLoop wait;QTimer::singleShot(30,&wait,[&]{bridge.importLyrics(replacement);});QTimer::singleShot(250,&wait,&QEventLoop::quit);wait.exec();
  if(bridge.lyrics().size()!=1||bridge.lyrics().first().toMap().value("text").toString()!="MANUAL TEST WINS")return 7;
  bridge.applyMedia({{"durationMs",31000}});if(bridge.lyrics().size()!=1||bridge.lyricSource()!="manual")return 8;
  bridge.clearLyricsCache();if(bridge.lyrics().size()!=1||bridge.lyricSource()!="manual")return 9;
  // All cache and mapping material is isolated under the test app identity.
  const auto directory=bridge.cachePath();if(!directory.contains(QCoreApplication::applicationName()))return 10;
  QDir cache(directory);for(const auto &name:cache.entryList({"*.lrc"},QDir::Files))cache.remove(name);
  if(QCoreApplication::arguments().contains("--live")){
   bridge.m_settings.setValue("preferences/lyrics/online",true);bridge.m_settings.setValue("preferences/lyrics/autoMatch",true);
   bridge.applyMedia({{"title","The Scientist"},{"artist","Coldplay"},{"album","A Rush of Blood to the Head"},{"durationMs",309000}});
   QEventLoop network;QObject::connect(&bridge,&Bridge::lyricsChanged,&network,[&]{if(!bridge.lyricsLoading())network.quit();});QTimer::singleShot(25000,&network,&QEventLoop::quit);if(bridge.lyrics().isEmpty())network.exec();
   if(bridge.lyrics().isEmpty()||bridge.lyricSource()!="lrclib")return 11;
   const auto count=bridge.lyrics().size();bridge.fetchLyrics();if(bridge.lyricSource()!="cache"||bridge.lyrics().size()!=count)return 12;
   qInfo()<<"PASS live LRCLIB automatic match and cached replay; rows:"<<count;
   cache.remove(bridge.lyricKey()+".lrc");
  }
  qInfo()<<"PASS: late duration invalidation, seek/backward clock, abort/stale HTTP reply, durable manual mapping and cache priority";
  return 0;
 }
};
int main(int argc,char **argv){QCoreApplication app(argc,argv);QTemporaryDir config;app.setOrganizationName("DmlTests");app.setApplicationName("lyrics-engine-"+QString::number(QCoreApplication::applicationPid()));QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,config.path());return LyricsEngineTests::run();}
