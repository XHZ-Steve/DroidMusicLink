#include "Bridge.h"
#include "LyricTimeline.h"
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
#include <QBuffer>
#include <QImage>

class LoopbackProviderNetwork:public QNetworkAccessManager {
public:quint16 port=0;
protected:QNetworkReply *createRequest(Operation operation,const QNetworkRequest &request,QIODevice *data)override{
 QNetworkRequest local(request);QUrl url("http://127.0.0.1:"+QString::number(port)+"/"+request.url().host()+request.url().path());url.setQuery(request.url().query());local.setUrl(url);return QNetworkAccessManager::createRequest(operation,local,data);
}
};
class LyricsEngineTests {
public:
 static int run(){
  QTemporaryDir files;QSettings saved;saved.setValue("preferences/lyrics/online",false);saved.setValue("preferences/lyrics/autoMatchMigration",true);saved.sync();Bridge bridge;
  const auto metadata=QJsonObject{{"title","TEST ONLY lyric engine A"},{"artist","TEST fixture"},{"album","TEST"},{"durationMs",0},{"state","paused"}};
  bridge.applyMedia(metadata);const auto unknownDurationKey=bridge.m_lyricKey;
  bridge.applyMedia({{"durationMs",30000}});if(unknownDurationKey==bridge.m_lyricKey||bridge.m_lyricKey!=bridge.lyricKey())return 1;
  const auto oldTitle=bridge.title();bridge.setDragging(true);bridge.applyMedia({{"title","TEST deferred drag title"},{"positionMs",1234}});bridge.applyMedia({{"volume",3},{"maxVolume",10}});
  if(bridge.title()!=oldTitle||bridge.m_progress.isActive()||bridge.m_lyricClock.isActive())return 85;
  bridge.setDragging(false);if(bridge.title()!="TEST deferred drag title"||bridge.volume()!=.3||!bridge.m_deferredMedia.isEmpty())return 86;
  bridge.applyMedia(metadata);bridge.applyMedia({{"durationMs",30000}});
  static QJsonObject nativePayload;
  bridge.m_nativeUpdate=[](const char* json){nativePayload=QJsonDocument::fromJson(json).object();return 0;};
  auto artwork=[](QColor color){QImage image(12,12,QImage::Format_RGB32);image.fill(color);QByteArray bytes;QBuffer output(&bytes);output.open(QIODevice::WriteOnly);image.save(&output,"PNG");return QString::fromLatin1(bytes.toBase64());};
  bridge.applyMedia({{"artwork",artwork(Qt::red)}});const auto firstRevision=nativePayload["artworkRevision"].toString();
  const auto nativePath=nativePayload["artwork"].toString();
  if(firstRevision.isEmpty()||nativePath.contains('/')||nativePath.contains('?')||!QFile::exists(nativePath))return 70;
  bridge.applyMedia({{"positionMs",100}});if(nativePayload["artworkRevision"]!=firstRevision)return 71;
  bridge.applyMedia({{"artwork",artwork(Qt::blue)}});if(nativePayload["artworkRevision"]==firstRevision||nativePayload["artworkRevision"].toString().isEmpty())return 72;
  bridge.applyMedia({{"artwork",""}});if(!nativePayload["artwork"].toString().isEmpty()||!nativePayload["artworkRevision"].toString().isEmpty())return 73;
  bridge.m_nativeUpdate=nullptr;
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
  // The real provider chain runs through a loopback transport: first source
  // fails, QQ supplies a version/artist/duration-verified TEST lyric document.
  QTcpServer chainServer;if(!chainServer.listen(QHostAddress::LocalHost,0))return 13;
  QStringList requests;QObject::connect(&chainServer,&QTcpServer::newConnection,&chainServer,[&]{auto socket=chainServer.nextPendingConnection();QObject::connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);QObject::connect(socket,&QTcpSocket::readyRead,socket,[&,socket]{const auto bytes=socket->readAll();if(bytes.isEmpty())return;const auto path=QString::fromUtf8(bytes.split(' ').value(1));requests<<path;QJsonObject response;bool failed=false;
   if(path.contains("music.163.com")){response={{"code",503}};failed=true;}
   else if(path.contains("client_search_cp")){const QJsonObject song{{"songmid","testfallback"},{"songname","TEST fallback"},{"singer",QJsonArray{QJsonObject{{"name","TEST fixture"}}}},{"albumname","TEST"},{"interval",30}};response={{"data",QJsonObject{{"song",QJsonObject{{"list",QJsonArray{song}}}}}}};}
   else response={{"lyric",QString::fromLatin1(QByteArray("[00:00]TEST FALLBACK FIRST\n[00:05]TEST FALLBACK NEXT").toBase64())},{"trans",QString::fromLatin1(QByteArray("[00:00]TEST translated first\n[00:05]TEST translated next").toBase64())}};
   const auto body=QJsonDocument(response).toJson(QJsonDocument::Compact);socket->write(QByteArray(failed?"HTTP/1.1 503 Service Unavailable\r\n":"HTTP/1.1 200 OK\r\n")+"Content-Type: application/json\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body);socket->disconnectFromHost();});});
  LoopbackProviderNetwork providerNetwork;providerNetwork.port=chainServer.serverPort();Bridge fallback;
  delete fallback.m_lyricProviders;fallback.m_lyricProviders=new LyricProviders(&providerNetwork,&fallback);
  fallback.m_settings.setValue("preferences/lyrics/online",true);fallback.m_settings.setValue("preferences/lyrics/autoMatch",true);fallback.m_settings.setValue("preferences/lyrics/preferCurrentApp",false);fallback.m_settings.setValue("preferences/lyrics/preferredProvider","auto");fallback.m_settings.setValue("preferences/lyrics/providerPriority","netease,qq,kugou,lrclib");
  fallback.applyMedia({{"title","TEST fallback"},{"artist","TEST fixture"},{"album","TEST"},{"durationMs",30000}});
  QEventLoop chainWait;QObject::connect(&fallback,&Bridge::lyricsChanged,&chainWait,[&]{if(!fallback.lyricsLoading()&&!fallback.lyrics().isEmpty())chainWait.quit();});QTimer::singleShot(5000,&chainWait,&QEventLoop::quit);chainWait.exec();
  if(fallback.lyricProvider()!="qq"||fallback.lyrics().size()!=2||requests.size()!=3||!requests.first().contains("music.163.com"))return 14;
  fallback.fetchLyrics();if(fallback.lyricSource()!="cache"||fallback.lyricProvider()!="qq"||fallback.lyrics().size()!=2)return 15;
  qInfo()<<"PASS: actual provider chain survives first-source HTTP failure, selects next trusted source, and replays rich cache";
  fallback.m_settings.setValue("preferences/lyrics/crossSourceTranslation",true);
  fallback.applyMedia({{"phoneLyrics","[00:00]<00:00>TEST FALLBACK FIRST\n[00:05]TEST FALLBACK NEXT"},{"phoneTranslation",""}});
  QEventLoop translatedWait;QObject::connect(&fallback,&Bridge::lyricsChanged,&translatedWait,[&]{if(!fallback.lyrics().isEmpty()&&!fallback.lyrics().first().toMap()["translation"].toString().isEmpty())translatedWait.quit();});QTimer::singleShot(5000,&translatedWait,&QEventLoop::quit);translatedWait.exec();
  if(fallback.lyricProvider()!="phone"||fallback.lyrics().first().toMap()["translation"]!="TEST translated first"||fallback.lyrics().first().toMap()["words"].toList().isEmpty())return 84;
  qInfo()<<"PASS: online translation supplements phone word lyrics through the real provider chain";
  Bridge phone;phone.m_settings.setValue("preferences/lyrics/online",false);phone.m_settings.setValue("preferences/lyrics/crossSourceTranslation",true);
  phone.applyMedia({{"title","TEST PHONE SOURCE"},{"artist","TEST fixture"},{"durationMs",30000},{"phoneLyrics","[00:01]TEST first\n[00:05]TEST second\n[00:09]TEST third"},{"phoneTranslation","[00:01]Translated first\n[00:05]Translated second\n[00:09]Translated third"}});
  if(phone.lyricProvider()!="phone"||phone.lyrics().size()!=3||phone.lyrics()[1].toMap()["translation"]!="Translated second")return 80;
  auto manual=LyricTimeline::parse("[00:01]TEST first\n[00:05]TEST second\n[00:09]TEST third");
  phone.acceptLyricDocument(LyricDocument{manual,"local","test","lrc",100},true);
  if(phone.lyricSource()!="manual"||phone.lyricProvider()!="local"||phone.lyrics()[0].toMap()["translation"]!="Translated first")return 81;
  phone.applyMedia({{"phoneLyrics","[00:01]TEST first\n[00:05]TEST second\n[00:09]TEST third"},{"phoneTranslation","[00:01]Replacement translation\n[00:05]Translated second\n[00:09]Translated third"}});
  if(phone.lyricProvider()!="local"||phone.lyrics()[0].toMap()["translation"]!="Translated first")return 82;
  phone.applyMedia({{"title","TEST next phone track"},{"phoneLyrics",""},{"phoneTranslation",""}});if(!phone.phoneLyricRows().isEmpty()||!phone.lyrics().isEmpty())return 83;
  qInfo()<<"PASS: phone source automatically loads, supplements manual originals without overwriting translation, and clears on track change";
  if(QCoreApplication::arguments().contains("--live")){
   bridge.m_settings.setValue("preferences/lyrics/online",true);bridge.m_settings.setValue("preferences/lyrics/autoMatch",true);
   bridge.m_settings.setValue("preferences/lyrics/preferredProvider","lrclib");
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
