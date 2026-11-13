#include "CoverArtProvider.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTcpServer>
#include <QTcpSocket>
#include <QBuffer>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <cmath>
#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif
class FixtureNetwork:public QNetworkAccessManager {
public:quint16 port=0;int requests=0;QStringList urls;
 QNetworkReply *createRequest(Operation op,const QNetworkRequest &req,QIODevice *data)override{++requests;urls<<req.url().toString();auto request=req;QUrl url("http://127.0.0.1:"+QString::number(port)+req.url().path());url.setQuery(req.url().query());request.setUrl(url);return QNetworkAccessManager::createRequest(op,request,data);}
};
int main(int argc,char **argv){
 QCoreApplication app(argc,argv);app.setApplicationVersion("1.0.0");
 const QString rid="11111111-1111-4111-8111-111111111111",gid="22222222-2222-4222-8222-222222222222";
 CoverArtTrack track{"TEST song","TEST artist","TEST album",200000};
 QJsonObject group{{"id",gid},{"primary-type","Album"}};
 QJsonObject release{{"id",rid},{"title",track.album},{"release-group",group}};
 QJsonObject recording{{"title",track.title},{"length",200000},{"artist-credit",QJsonArray{QJsonObject{{"name",track.artist}}}},{"releases",QJsonArray{release}}};
 QJsonObject response{{"recordings",QJsonArray{recording}}};
 auto select=[&](CoverArtTrack t){return MusicBrainzCoverArtProvider::selectRelease(t,response);};
 if(select(track)!=gid)return 1;
 for(const auto &bad:QList<CoverArtTrack>{{"TEST song (Live)",track.artist,track.album,200000},{"TEST song (Remix)",track.artist,track.album,200000},{track.title,"",track.album,200000},{track.title,track.artist,"TEST album Single",200000},{track.title,track.artist,track.album,220000}})if(!select(bad).isEmpty())return 2;
 auto duplicate=release;auto otherGroup=group;otherGroup["id"]="33333333-3333-4333-8333-333333333333";duplicate["release-group"]=otherGroup;auto ambiguous=recording;ambiguous["releases"]=QJsonArray{release,duplicate};if(!MusicBrainzCoverArtProvider::selectRelease(track,QJsonObject{{"recordings",QJsonArray{ambiguous}}}).isEmpty())return 3;
 QJsonObject activeResponse=response;QTemporaryDir directory;QTcpServer server;if(!server.listen(QHostAddress::LocalHost,0))return 4;
 QImage original(1600,1600,QImage::Format_RGB32);original.fill(QColor("#2478c9"));QByteArray png;QBuffer output(&png);output.open(QIODevice::WriteOnly);original.save(&output,"PNG");bool corrupt=false;
 const auto fileIndex=app.arguments().indexOf("--artwork");if(fileIndex>=0&&fileIndex+1<app.arguments().size()){
  QFile file(app.arguments()[fileIndex+1]);if(!file.open(QIODevice::ReadOnly))return 30;png=file.readAll();QImage reference=QImage::fromData(png);if(reference.isNull())return 31;
  reference.scaled(512,512,Qt::KeepAspectRatio,Qt::SmoothTransformation).save(QFileInfo(file).absolutePath()+"/hd-phone512.png");
  const auto baseline=reference.scaled(512,512,Qt::KeepAspectRatio,Qt::SmoothTransformation).scaled(reference.size(),Qt::IgnoreAspectRatio,Qt::SmoothTransformation);double error=0;
  for(int y=0;y<reference.height();y++)for(int x=0;x<reference.width();x++){const auto a=reference.pixelColor(x,y),b=baseline.pixelColor(x,y);error+=std::pow(a.red()-b.red(),2)+std::pow(a.green()-b.green(),2)+std::pow(a.blue()-b.blue(),2);}
  qInfo()<<"REAL CAA source pixels"<<reference.size()<<"512px resample MSE"<<error/(3.*reference.width()*reference.height())<<"source-native MSE 0";
 }

 QObject::connect(&server,&QTcpServer::newConnection,&server,[&]{auto socket=server.nextPendingConnection();QObject::connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);QObject::connect(socket,&QTcpSocket::readyRead,socket,[&,socket]{auto request=socket->readAll();if(request.isEmpty())return;QByteArray body=request.contains("/ws/2/")?QJsonDocument(activeResponse).toJson():corrupt?QByteArray("BROKEN TEST IMAGE"):png;socket->write("HTTP/1.1 200 OK\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body);socket->disconnectFromHost();});});
 FixtureNetwork network;network.port=server.serverPort();CoverArtService service(directory.path(),nullptr,&network);CoverArtResult result;
 auto run=[&](CoverArtTrack t,bool refresh){result={};QEventLoop wait;bool finished=false;service.request(t,1200,refresh,[&](CoverArtResult r){result=r;finished=true;wait.quit();});QTimer::singleShot(5000,&wait,&QEventLoop::quit);if(!finished)wait.exec();return finished;};
 QElapsedTimer clock;clock.start();if(!run(track,false)||result.pixels!=QSize(1200,1200)||result.cached||network.requests!=2)return 5;
 if(!QFileInfo::exists(result.thumbnailPath)||QFileInfo(result.thumbnailPath).size()>2*1024*1024||QImage(result.thumbnailPath).size()!=QSize(512,512))return 14;
 qInfo()<<"Fixture: phone baseline 512x512, downloaded 1600x1600, decoded cache"<<result.pixels<<"elapsed ms"<<clock.elapsed();
 if(!run(track,false)||!result.cached||network.requests!=2)return 6;
 auto albumTrack=track;albumTrack.title="TEST other song";auto nextRecording=recording;nextRecording["title"]=albumTrack.title;activeResponse["recordings"]=QJsonArray{nextRecording};if(!run(albumTrack,false)||!result.cached||network.requests!=3)return 7;
 auto mismatch=track;mismatch.title="TEST unverified song";if(!run(mismatch,false)||!result.path.isEmpty())return 13;activeResponse=response;
 service.clear();if(service.bytes()!=0)return 8;
 corrupt=true;if(!run(track,true)||!result.path.isEmpty())return 9;

 QFile oversized(directory.path()+"/TEST-budget.tmp");if(!oversized.open(QIODevice::WriteOnly)||!oversized.resize(34LL*1024*1024))return 10;oversized.close();service.setBudget(32);if(service.bytes()>32LL*1024*1024)return 11;
 bool stale=false;service.request(track,1200,true,[&](CoverArtResult){stale=true;});service.cancel();QEventLoop cancelled;QTimer::singleShot(100,&cancelled,&QEventLoop::quit);cancelled.exec();if(stale)return 12;
#ifdef Q_OS_WIN
 PROCESS_MEMORY_COUNTERS memory{};GetProcessMemoryInfo(GetCurrentProcess(),&memory,sizeof(memory));qInfo()<<"Worker test working set MiB"<<memory.WorkingSetSize/(1024.*1024)<<"peak MiB"<<memory.PeakWorkingSetSize/(1024.*1024);
#endif
 service.cancel();qInfo()<<"PASS matching (6 rejection cases), decoding, album/song cache hits, corrupt download fallback and cache clear";
 if(app.arguments().contains("--live")){
  CoverArtService live(directory.path()+"/live");CoverArtTrack actual{"The Scientist","Coldplay","A Rush of Blood to the Head",309000};QEventLoop wait;bool completed=false;live.request(actual,1200,false,[&](CoverArtResult r){result=r;completed=true;wait.quit();});QTimer::singleShot(45000,&wait,&QEventLoop::quit);wait.exec();
  if(!completed||result.path.isEmpty()){qWarning()<<"LIVE endpoint unavailable or no unambiguous cover; phone fallback retained";return 20;}
  qInfo()<<"LIVE CAA:"<<result.release<<result.pixels<<result.path;
  completed=false;live.request(actual,1200,false,[&](CoverArtResult r){result=r;completed=true;wait.quit();});if(!completed)wait.exec();if(!result.cached)return 21;qInfo()<<"LIVE cache replay passed";
 }
 return 0;
}
