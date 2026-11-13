#include "CoverArtProvider.h"
#include <QDateTime>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUrlQuery>
#include <QRegularExpression>
#include <QFile>
#include <QDir>
#include <QSaveFile>
#include <QBuffer>
#include <QImageReader>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QCoreApplication>
#include <QDebug>
#include <algorithm>
namespace {
QString hash(QString text){return QString::fromLatin1(QCryptographicHash::hash(text.toUtf8(),QCryptographicHash::Sha256).toHex());}
QString normalized(QString text){text=text.normalized(QString::NormalizationForm_KC).toCaseFolded();text.replace(QRegularExpression("[^\\p{L}\\p{N}]+")," ");return text.simplified();}
QString variants(QString text){QStringList found;for(const auto &word:QStringList{"live","remix","acoustic","instrumental","demo","remaster","radio edit","extended","single","album"})if(normalized(text).contains(QRegularExpression("\\b"+word+"\\b")))found<<word;return found.join('|');}
QString artist(const QJsonObject &row){QString text;for(const auto &entry:row["artist-credit"].toArray()){auto o=entry.toObject();text+=o.value("name").toString(o.value("artist").toObject().value("name").toString())+o["joinphrase"].toString();}return text;}
bool validId(QString id){return QRegularExpression("^[0-9a-fA-F]{8}(-[0-9a-fA-F]{4}){3}-[0-9a-fA-F]{12}$").match(id).hasMatch();}
bool allowed(QUrl url){auto h=url.host().toLower();return url.scheme()=="https"&&(h=="musicbrainz.org"||h=="coverartarchive.org"||h=="archive.org"||h.endsWith(".archive.org"))&&url.userInfo().isEmpty()&&(url.port()==-1||url.port()==443);}
}
QString CoverArtTrack::key()const{return hash(normalized(title)+"\n"+normalized(artist)+"\n"+normalized(album)+"\n"+QString::number(duration/1000));}
void MusicBrainzCoverArtProvider::cancel(){++generation;delay.stop();if(reply)reply->abort();reply=nullptr;}
void MusicBrainzCoverArtProvider::get(QUrl url,bool json,std::function<void(QByteArray)> done){
 if(!allowed(url)){done({});return;}
 QNetworkRequest request(url);request.setRawHeader("User-Agent",("DroidMediaLink/"+QCoreApplication::applicationVersion()+" (https://github.com/XHZ-Steve/DroidMusicLink)").toUtf8());request.setRawHeader("Accept",json?"application/json":"image/*");request.setTransferTimeout(12000);request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::UserVerifiedRedirectPolicy);auto r=network->get(request);reply=r;QTimer::singleShot(15000,r,[r]{if(!r->isFinished())r->abort();});const auto token=generation;const qint64 limit=json?1024*1024:10*1024*1024;
 connect(r,&QNetworkReply::redirected,r,[r](QUrl target){if(allowed(target))r->redirectAllowed();else r->abort();});
 connect(r,&QNetworkReply::downloadProgress,r,[r,limit](qint64 n,qint64 total){if(n>limit||total>limit)r->abort();});
 connect(r,&QNetworkReply::finished,this,[this,r,token,limit,done]{auto data=r->isOpen()?r->read(limit+1):QByteArray{};if(r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()==429||r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()==503){bool ok=false;auto seconds=r->rawHeader("Retry-After").toLongLong(&ok);if(!ok){auto date=QDateTime::fromString(QString::fromLatin1(r->rawHeader("Retry-After")),Qt::RFC2822Date);seconds=date.isValid()?QDateTime::currentDateTimeUtc().secsTo(date):60;}nextRequest=qMax(nextRequest,QDateTime::currentMSecsSinceEpoch()+qBound<qint64>(60LL,seconds,86400LL)*1000);}const bool ok=r->error()==QNetworkReply::NoError&&r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()==200&&data.size()<=limit;r->deleteLater();if(reply==r)reply=nullptr;if(token==generation)done(ok?data:QByteArray{});});
}
QString MusicBrainzCoverArtProvider::selectRelease(const CoverArtTrack &track,const QJsonObject &response){
 // Do not guess an album from a title alone. Editions with conflicting version
 // markers or ambiguous album groups are rejected rather than substituted.
 if(track.title.trimmed().isEmpty()||track.artist.trimmed().isEmpty()||track.album.trimmed().isEmpty()||track.duration<=0)return {};
 QMap<QString,QStringList> groups;
 for(const auto &value:response["recordings"].toArray()){
  auto recording=value.toObject();if(normalized(recording["title"].toString())!=normalized(track.title)||normalized(artist(recording))!=normalized(track.artist)||variants(recording["title"].toString())!=variants(track.title))continue;
  const qint64 length=recording["length"].toInteger();if(length<=0||qAbs(length-track.duration)>qMax<qint64>(2500,qMin<qint64>(5000,track.duration/100)))continue;
  for(const auto &r:recording["releases"].toArray()){
   auto release=r.toObject(),group=release["release-group"].toObject();if(normalized(release["title"].toString())!=normalized(track.album)||variants(release["title"].toString())!=variants(track.album))continue;
   auto type=group["primary-type"].toString();auto secondary=group["secondary-types"].toArray();const auto tags=variants(track.title+" "+track.album);
   if((tags.contains("single")&&type!="Single")||(tags.contains("album")&&type!="Album")||secondary.contains("Compilation"))continue;
   if(secondary.contains("Live")&&!tags.contains("live"))continue;
   auto id=release["id"].toString(),gid=group["id"].toString();if(validId(id)&&validId(gid))groups[gid]<<id;
  }
 }
 if(groups.size()!=1)return {};return groups.firstKey();
}
void MusicBrainzCoverArtProvider::request(const CoverArtTrack &track,int resolution,Done done){
 cancel();if(track.title.trimmed().isEmpty()||track.artist.trimmed().isEmpty()||track.album.trimmed().isEmpty()||track.duration<=0||track.title.size()>512||track.artist.size()>512||track.album.size()>512){done({},{},false);return;}const auto token=generation;const bool fresh=bypassCache;auto start=[this,track,resolution,done,token,fresh]{if(token!=generation)return;nextRequest=QDateTime::currentMSecsSinceEpoch()+1100;
  auto quote=[](QString text){text.replace('\\',"\\\\");text.replace('"',"\\\"");return '"'+text+'"';};
  QUrl url("https://musicbrainz.org/ws/2/recording");QUrlQuery query;query.addQueryItem("query","recording:"+quote(track.title)+" AND artist:"+quote(track.artist)+" AND release:"+quote(track.album));query.addQueryItem("fmt","json");query.addQueryItem("limit","25");url.setQuery(query);
  get(url,true,[this,track,resolution,done,fresh](QByteArray data){auto id=selectRelease(track,QJsonDocument::fromJson(data).object());if(id.isEmpty()){done({},{},false);return;}if(!fresh&&cachedImage&&cachedImage(id,resolution)){done({},id,true);return;}
   // CAA's front endpoint is the documented canonical cover, not an arbitrary
   // image URL from search results. All redirect hosts are checked as well.
   auto suffix=resolution<=500?"-500":resolution<=1200?"-1200":"";
   get(QUrl("https://coverartarchive.org/release-group/"+id+"/front"+suffix),false,[done,id](QByteArray image){done(image,id,false);});
  });
 };
 const auto wait=qMax<qint64>(0,nextRequest-QDateTime::currentMSecsSinceEpoch());if(wait){delay.setSingleShot(true);disconnect(&delay,nullptr,this,nullptr);connect(&delay,&QTimer::timeout,this,start);delay.start(int(wait));}else start();
}
CoverArtService::CoverArtService(QString path,QObject *parent,QNetworkAccessManager *transport):QObject(parent),provider(this,transport),directory(path){QDir().mkpath(directory);provider.cachedImage=[this](QString id,int size){QFileInfo file(directory+"/"+hash(id+QString::number(size))+".png");return file.isFile()&&file.size()<=32*1024*1024;};}
void CoverArtService::cancel(){++generation;epoch->store(generation);provider.cancel();}
qint64 CoverArtService::bytes()const{qint64 n=0;for(auto file:QDir(directory).entryInfoList(QDir::Files))n+=file.size();return n;}
void CoverArtService::trim(){auto files=QDir(directory).entryInfoList(QDir::Files,QDir::Time|QDir::Reversed);auto total=bytes();for(auto file:files){if(total<=budget)break;if(file.absoluteFilePath()!=activePath&&file.absoluteFilePath()!=activeThumbnailPath&&QFile::remove(file.absoluteFilePath()))total-=file.size();}}
void CoverArtService::setBudget(int mb){budget=qBound(32,mb,2048)*1024LL*1024;trim();}
void CoverArtService::clear(){cancel();activePath.clear();activeThumbnailPath.clear();for(auto f:QDir(directory).entryInfoList(QDir::Files))QFile::remove(f.absoluteFilePath());failures.clear();}
void CoverArtService::decode(QByteArray data,const QString &key,QString release,int resolution,bool cached,std::function<void(CoverArtResult)> done){
 const auto token=generation;auto watcher=new QFutureWatcher<CoverArtResult>(this);const auto root=directory;const auto guard=epoch;
 connect(watcher,&QFutureWatcher<CoverArtResult>::finished,this,[this,watcher,token,done]{auto result=watcher->result();watcher->deleteLater();if(token==generation&&!result.path.isEmpty()){activePath=result.path;activeThumbnailPath=result.thumbnailPath;}trim();if(token!=generation)return;done(result);});
 watcher->setFuture(QtConcurrent::run([data,key,release,resolution,cached,root,guard,token]()mutable{
  if(cached&&data.isEmpty()){QFile image(root+"/"+hash(release+QString::number(resolution))+".png");if(image.open(QIODevice::ReadOnly)&&image.size()<=32*1024*1024)data=image.readAll();}
  CoverArtResult result;if(guard->load()!=token)return result;if(data.isEmpty()||data.size()>(cached?32:10)*1024*1024)return result;QBuffer input;input.setData(data);input.open(QIODevice::ReadOnly);QImageReader reader(&input);auto size=reader.size();if(!size.isValid()||size.width()>8192||size.height()>8192||qint64(size.width())*size.height()>16777216)return result;auto image=reader.read();if(guard->load()!=token)return result;if(image.isNull())return result;
  if(qMax(image.width(),image.height())>resolution)image=image.scaled(resolution,resolution,Qt::KeepAspectRatio,Qt::SmoothTransformation);
  auto path=root+"/"+hash(release+QString::number(resolution))+".png";
  if(!cached){if(guard->load()!=token)return result;QSaveFile file(path);if(!file.open(QIODevice::WriteOnly)||!image.save(&file,"PNG")||guard->load()!=token||!file.commit())return result;}
  const auto thumbnail=path+".smtc.jpg";QSaveFile small(thumbnail);
  if(guard->load()!=token||!small.open(QIODevice::WriteOnly)||!image.scaled(512,512,Qt::KeepAspectRatio,Qt::SmoothTransformation).save(&small,"JPEG",90)||guard->load()!=token||!small.commit())return result;
  result={path,release,"musicbrainz",image.size(),QCryptographicHash::hash(data,QCryptographicHash::Sha256),cached,thumbnail,CoverPalette::fromImage(image)};return result;
 }));
}
void CoverArtService::request(CoverArtTrack track,int resolution,bool refresh,std::function<void(CoverArtResult)> done){
 cancel();for(auto it=failures.begin();it!=failures.end();)if(it.value()<=QDateTime::currentMSecsSinceEpoch())it=failures.erase(it);else ++it;const auto token=generation;const auto key=track.key()+"-"+QString::number(resolution);const auto mapping=directory+"/"+key+".json";
 provider.bypassCache=refresh;
 auto fetch=[this,track,resolution,key,mapping,done,token]{if(token!=generation)return;if(failures.value(key)>QDateTime::currentMSecsSinceEpoch()){done({});return;}
  provider.request(track,resolution,[this,key,mapping,resolution,done,token](QByteArray image,QString release,bool cached){if(token!=generation)return;if(image.isEmpty()&&!cached){failures[key]=QDateTime::currentMSecsSinceEpoch()+600000;done({});return;}
   decode(image,key,release,resolution,cached,[this,key,mapping,done](CoverArtResult result){if(!result.path.isEmpty()){for(const auto &manifest:QStringList{mapping}){QSaveFile file(manifest);if(file.open(QIODevice::WriteOnly)){file.write(QJsonDocument(QJsonObject{{"release",result.release},{"created",QDateTime::currentMSecsSinceEpoch()}}).toJson());file.commit();}}trim();}else failures[key]=QDateTime::currentMSecsSinceEpoch()+600000;done(result);});
  });
 };
 if(refresh){QFile::remove(mapping);failures.remove(key);}
 QFile file(mapping);if(!refresh&&file.open(QIODevice::ReadOnly)&&file.size()<16384){auto info=QJsonDocument::fromJson(file.readAll()).object();auto id=info["release"].toString();if(validId(id)&&QDateTime::currentMSecsSinceEpoch()-info["created"].toInteger()<30LL*86400000){QFileInfo image(directory+"/"+hash(id+QString::number(resolution))+".png");if(image.isFile()&&image.size()<=32*1024*1024){decode({},key,id,resolution,true,[this,id,resolution,fetch,done](CoverArtResult result){if(result.path.isEmpty()){QFile::remove(directory+"/"+hash(id+QString::number(resolution))+".png");fetch();}else done(result);});return;}}}
 fetch();
}

bool PublishedCoverArtProvider::trusted(QUrl url){
 if(url.scheme()!="https"||!url.userInfo().isEmpty()||(url.port()!=-1&&url.port()!=443)||url.toString().size()>4096)return false;
 const auto host=url.host().toLower();
 for(const auto &domain:QStringList{"music.126.net","gtimg.cn","qpic.cn","qq.com","kugou.com","kuwo.cn","scdn.co","spotifycdn.com","mzstatic.com","media-amazon.com","dzcdn.net","ytimg.com","googleusercontent.com","ggpht.com","resources.tidal.com","static.qobuz.com","sndcdn.com","bcbits.com","coverartarchive.org","archive.org"})if(host==domain||host.endsWith("."+domain))return true;
 return false;
}
void PublishedCoverArtProvider::cancel(){++generation;if(reply)reply->abort();reply=nullptr;}
void PublishedCoverArtProvider::request(const CoverArtTrack &track,int resolution,Done done){
 cancel();const auto token=generation;const QUrl url(track.uri);if(!trusted(url)){done({},{},false);return;}
 QNetworkRequest req(url);req.setRawHeader("User-Agent",("DroidMediaLink/"+QCoreApplication::applicationVersion()+" (https://github.com/XHZ-Steve/DroidMusicLink)").toUtf8());req.setTransferTimeout(10000);req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::UserVerifiedRedirectPolicy);
 auto r=network->get(req);reply=r;QTimer::singleShot(15000,r,[r]{if(!r->isFinished())r->abort();});
 connect(r,&QNetworkReply::redirected,r,[r](QUrl next){if(trusted(next))r->redirectAllowed();else r->abort();});
 connect(r,&QNetworkReply::downloadProgress,r,[r](qint64 n,qint64 total){if(n>2*1024*1024||total>2*1024*1024)r->abort();});
 connect(r,&QNetworkReply::finished,this,[this,r,token,resolution,done]{auto bytes=r->isOpen()?r->read(2*1024*1024+1):QByteArray{};const bool ok=r->error()==QNetworkReply::NoError&&r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()==200&&bytes.size()<=2*1024*1024;r->deleteLater();if(reply==r)reply=nullptr;if(token!=generation)return;if(!ok){done({},{},false);return;}
  auto watcher=new QFutureWatcher<QByteArray>(this);connect(watcher,&QFutureWatcher<QByteArray>::finished,this,[this,watcher,token,done]{auto image=watcher->result();watcher->deleteLater();if(token==generation)done(image,{},false);});
  watcher->setFuture(QtConcurrent::run([bytes,resolution]{QBuffer input;input.setData(bytes);input.open(QIODevice::ReadOnly);QImageReader reader(&input);auto size=reader.size();if(!size.isValid()||size.width()>8192||size.height()>8192||qint64(size.width())*size.height()>16777216)return QByteArray{};auto image=reader.read();if(image.isNull())return QByteArray{};const int limit=qBound(500,resolution,2048);if(qMax(size.width(),size.height())<=limit)return bytes;image=image.scaled(limit,limit,Qt::KeepAspectRatio,Qt::SmoothTransformation);QByteArray encoded;QBuffer out(&encoded);out.open(QIODevice::WriteOnly);if(!image.save(&out,"JPEG",92)||encoded.size()>2*1024*1024)return QByteArray{};return encoded;}));
 });
}
