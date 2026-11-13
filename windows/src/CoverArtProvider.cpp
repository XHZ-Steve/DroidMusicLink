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
#include <QElapsedTimer>
#include <algorithm>
namespace {
QString hash(QString text){return QString::fromLatin1(QCryptographicHash::hash(text.toUtf8(),QCryptographicHash::Sha256).toHex());}
QString normalized(QString text){text=text.normalized(QString::NormalizationForm_KC).toCaseFolded();text.replace(QRegularExpression("[^\\p{L}\\p{N}]+")," ");return text.simplified();}
QString variants(QString text){QStringList found;for(const auto &word:QStringList{"live","remix","acoustic","instrumental","demo","remaster","radio edit","extended","single","album"})if(normalized(text).contains(QRegularExpression("\\b"+word+"\\b")))found<<word;return found.join('|');}
QString artist(const QJsonObject &row){QString text;for(const auto &entry:row["artist-credit"].toArray()){auto o=entry.toObject();text+=o.value("name").toString(o.value("artist").toObject().value("name").toString())+o["joinphrase"].toString();}return text;}
bool validId(QString id){return QRegularExpression("^[0-9a-fA-F]{8}(-[0-9a-fA-F]{4}){3}-[0-9a-fA-F]{12}$").match(id).hasMatch();}
bool validCoverId(QString id){return validId(id)||QRegularExpression("^(netease:[1-9][0-9]{0,18}|qq:[A-Za-z0-9]{5,32})$").match(id).hasMatch();}
QString coverSource(QString id){return id.contains(':')?id.section(':',0,0):QStringLiteral("musicbrainz");}
QString names(QJsonArray entries){QStringList names;for(auto v:entries){auto n=v.toObject().value("name").toString();if(!n.isEmpty())names<<n;}return names.join('/');}
QStringList artistSet(QString text){QStringList out;for(auto name:text.split(QRegularExpression("[/,;、&]|\\s+(?:feat\\.?|ft\\.?|featuring|with)\\s+",QRegularExpression::CaseInsensitiveOption),Qt::SkipEmptyParts)){auto n=normalized(name);if(!n.isEmpty()&&!out.contains(n))out<<n;}out.sort();return out;}
bool sameTrack(const CoverArtTrack &a,const CoverArtTrack &b){return !normalized(a.title).isEmpty()&&normalized(a.title)==normalized(b.title)&&variants(a.title)==variants(b.title)&&!artistSet(a.artist).isEmpty()&&artistSet(a.artist)==artistSet(b.artist)&&a.duration>0&&b.duration>0&&qAbs(a.duration-b.duration)<=qBound<qint64>(2500LL,a.duration/100,5000LL)&&(a.album.isEmpty()||normalized(a.album)==normalized(b.album));}
QUrl queryUrl(QString address,QList<QPair<QString,QString>> params){QUrl url(address);QUrlQuery q;for(auto p:params)q.addQueryItem(p.first,p.second);url.setQuery(q);return url;}
bool allowed(QUrl url){auto h=url.host().toLower();return url.scheme()=="https"&&(h=="musicbrainz.org"||h=="coverartarchive.org"||h=="archive.org"||h.endsWith(".archive.org"))&&url.userInfo().isEmpty()&&(url.port()==-1||url.port()==443);}
}
QString CoverArtTrack::key()const{return hash(normalized(title)+"\n"+normalized(artist)+"\n"+normalized(album)+"\n"+QString::number(duration/1000));}
void MusicBrainzCoverArtProvider::cancel(){++generation;delay.stop();if(reply)reply->abort();reply=nullptr;}
void MusicBrainzCoverArtProvider::get(QUrl url,bool json,std::function<void(QByteArray)> done){
 if(!allowed(url)){failure="error";lastError=QCoreApplication::translate("CoverArt","Untrusted cover URL");done({});return;}if(stage)stage(json?"searching":"downloading");
 QNetworkRequest request(url);request.setRawHeader("User-Agent",("DroidMediaLink/"+QCoreApplication::applicationVersion()+" (https://github.com/XHZ-Steve/DroidMusicLink)").toUtf8());request.setRawHeader("Accept",json?"application/json":"image/*");request.setTransferTimeout(12000);request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::UserVerifiedRedirectPolicy);auto r=network->get(request);reply=r;QTimer::singleShot(15000,r,[r]{if(!r->isFinished())r->abort();});const auto token=generation;const qint64 limit=json?1024*1024:10*1024*1024;
 connect(r,&QNetworkReply::redirected,r,[r](QUrl target){if(allowed(target))r->redirectAllowed();else r->abort();});
 connect(r,&QNetworkReply::downloadProgress,r,[r,limit](qint64 n,qint64 total){if(n>limit||total>limit)r->abort();});
 connect(r,&QNetworkReply::finished,this,[this,r,token,limit,done]{auto data=r->isOpen()?r->read(limit+1):QByteArray{};if(r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()==429||r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()==503){bool ok=false;auto seconds=r->rawHeader("Retry-After").toLongLong(&ok);if(!ok){auto date=QDateTime::fromString(QString::fromLatin1(r->rawHeader("Retry-After")),Qt::RFC2822Date);seconds=date.isValid()?QDateTime::currentDateTimeUtc().secsTo(date):60;}nextRequest=qMax(nextRequest,QDateTime::currentMSecsSinceEpoch()+qBound<qint64>(60LL,seconds,86400LL)*1000);}const bool ok=r->error()==QNetworkReply::NoError&&r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()==200&&data.size()<=limit;r->deleteLater();if(reply==r)reply=nullptr;if(token==generation){if(!ok){failure=(r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()==429||r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()==503)?"rate-limited":r->error()==QNetworkReply::TimeoutError?"timeout":"error";lastError=r->errorString();}done(ok?data:QByteArray{});}});
}
QString MusicBrainzCoverArtProvider::selectRelease(const CoverArtTrack &track,const QJsonObject &response){
 // Do not guess an album from a title alone. Editions with conflicting version
 // markers or ambiguous album groups are rejected rather than substituted.
 if(track.title.trimmed().isEmpty()||track.artist.trimmed().isEmpty()||track.album.trimmed().isEmpty()||track.duration<=0)return {};
 QMap<QString,QStringList> groups;
 for(const auto &value:response["recordings"].toArray()){
  auto recording=value.toObject();if(normalized(recording["title"].toString())!=normalized(track.title)||artistSet(artist(recording))!=artistSet(track.artist)||variants(recording["title"].toString())!=variants(track.title))continue;
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
 cancel();failure="empty";lastError.clear();if(track.title.trimmed().isEmpty()||track.artist.trimmed().isEmpty()||track.album.trimmed().isEmpty()||track.duration<=0||track.title.size()>512||track.artist.size()>512||track.album.size()>512){failure="incomplete";done({},{},false);return;}const auto token=generation;const bool fresh=bypassCache;auto start=[this,track,resolution,done,token,fresh]{if(token!=generation)return;nextRequest=QDateTime::currentMSecsSinceEpoch()+1100;
  auto quote=[](QString text){text.replace('\\',"\\\\");text.replace('"',"\\\"");return '"'+text+'"';};
  QUrl url("https://musicbrainz.org/ws/2/recording");QUrlQuery query;query.addQueryItem("query","recording:"+quote(track.title)+" AND artist:"+quote(track.artist.section('/',0,0))+" AND release:"+quote(track.album));query.addQueryItem("fmt","json");query.addQueryItem("limit","25");url.setQuery(query);
  get(url,true,[this,track,resolution,done,fresh](QByteArray data){auto id=selectRelease(track,QJsonDocument::fromJson(data).object());if(id.isEmpty()){done({},{},false);return;}if(!fresh&&cachedImage&&cachedImage(id,resolution)){done({},id,true);return;}
   // CAA's front endpoint is the documented canonical cover, not an arbitrary
   // image URL from search results. All redirect hosts are checked as well.
   auto suffix=resolution<=500?"-500":resolution<=1200?"-1200":"";
   get(QUrl("https://coverartarchive.org/release-group/"+id+"/front"+suffix),false,[done,id](QByteArray image){done(image,id,false);});
  });
 };
 const auto wait=qMax<qint64>(0,nextRequest-QDateTime::currentMSecsSinceEpoch());if(wait>1500){failure="rate-limited";lastError=QCoreApplication::translate("CoverArt","Source cooling down");done({},{},false);return;}if(wait){delay.setSingleShot(true);disconnect(&delay,nullptr,this,nullptr);connect(&delay,&QTimer::timeout,this,start);delay.start(int(wait));}else start();
}
CoverArtService::CoverArtService(QString path,QObject *parent,QNetworkAccessManager *transport):QObject(parent),provider(this,transport),netease("netease",this,transport),qq("qq",this,transport),directory(path){QDir().mkpath(directory);auto exists=[this](QString id,int size){QFileInfo file(directory+"/"+hash(id+QString::number(size))+".png");return file.isFile()&&file.size()<=32*1024*1024;};provider.cachedImage=exists;netease.cachedImage=exists;qq.cachedImage=exists;}
void CoverArtService::cancel(){++generation;epoch->store(generation);provider.cancel();netease.cancel();qq.cancel();}
void CoverArtService::setPriority(QStringList order){QStringList checked;for(auto id:order)if(QStringList{"netease","qq","musicbrainz"}.contains(id)&&!checked.contains(id))checked<<id;if(priority!=checked){cancel();priority=checked;}}
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
  result={path,release,coverSource(release),image.size(),QCryptographicHash::hash(data,QCryptographicHash::Sha256),cached,thumbnail,CoverPalette::fromImage(image)};return result;
 }));
}
void CoverArtService::request(CoverArtTrack track,int resolution,bool refresh,std::function<void(CoverArtResult)> done,int minimum){
 minimumPixels=qMax(0,minimum);
 cancel();for(auto it=failures.begin();it!=failures.end();)if(it.value()<=QDateTime::currentMSecsSinceEpoch())it=failures.erase(it);else ++it;const auto token=generation;const auto key=track.key()+"-"+QString::number(resolution)+"-"+hash(priority.join(','))+"-"+QString::number(minimumPixels);const auto mapping=directory+"/"+key+".json";
 auto fetch=[this,track,resolution,key,mapping,done,token,refresh]{fetchAt(track,resolution,key,mapping,refresh,priority,0,done,token);};
 if(refresh){QFile::remove(mapping);failures.remove(key);}
 QFile file(mapping);if(!refresh&&file.open(QIODevice::ReadOnly)&&file.size()<16384){auto info=QJsonDocument::fromJson(file.readAll()).object();auto id=info["release"].toString();const auto age=QDateTime::currentMSecsSinceEpoch()-info["created"].toInteger();if(validCoverId(id)&&priority.contains(coverSource(id))&&age>=0&&age<30LL*86400000){QFileInfo image(directory+"/"+hash(id+QString::number(resolution))+".png");if(image.isFile()&&image.size()<=32*1024*1024){decode({},key,id,resolution,true,[this,id,resolution,fetch,done](CoverArtResult result){if(result.path.isEmpty()){QFile::remove(directory+"/"+hash(id+QString::number(resolution))+".png");fetch();}else if(qMin(result.pixels.width(),result.pixels.height())<=minimumPixels)fetch();else {if(progress)progress(result.source,{{"status","available"},{"width",result.pixels.width()},{"height",result.pixels.height()},{"elapsedMs",0},{"cached",true}});done(result);}});return;}}}
 fetch();
}
void CoverArtService::fetchAt(CoverArtTrack track,int resolution,QString key,QString mapping,bool refresh,QStringList order,int index,std::function<void(CoverArtResult)> done,quint64 token){
 if(token!=generation)return;
 if(index>=order.size()||failures.value(key)>QDateTime::currentMSecsSinceEpoch()){failures[key]=QDateTime::currentMSecsSinceEpoch()+600000;done({});return;}
 const auto source=order[index];CoverArtProvider *selected=&provider;
 if(source=="netease"){selected=&netease;netease.bypassCache=refresh;}else if(source=="qq"){selected=&qq;qq.bypassCache=refresh;}else provider.bypassCache=refresh;
 auto next=[this,track,resolution,key,mapping,refresh,order,index,done,token]{fetchAt(track,resolution,key,mapping,refresh,order,index+1,done,token);};
 auto finished=std::make_shared<bool>(false);QElapsedTimer clock;clock.start();
 auto report=[this,source,clock](QString status,QString error={},QSize pixels={},bool cached=false){if(progress)progress(source,{{"status",status},{"lastError",error},{"width",pixels.width()},{"height",pixels.height()},{"elapsedMs",clock.elapsed()},{"cached",cached}});};
 selected->stage=[this,token,finished,report](QString status){if(token==generation&&!*finished)report(status);};report("searching");
 QTimer::singleShot(12000,this,[this,token,finished,selected,report,next]{if(token!=generation||*finished)return;*finished=true;selected->cancel();report("timeout");next();});
 selected->request(track,resolution,[this,key,mapping,resolution,done,token,next,selected,finished,report](QByteArray image,QString release,bool cached){
  if(token!=generation||*finished)return;*finished=true;
  if((image.isEmpty()&&!cached)||!validCoverId(release)){report(selected->failure,selected->lastError);next();return;}
  report("decoding");
  decode(image,key,release,resolution,cached,[this,mapping,done,next,report](CoverArtResult result){
   if(result.path.isEmpty()){report("error",QCoreApplication::translate("CoverArt","Invalid cover image"));next();return;}
   if(qMin(result.pixels.width(),result.pixels.height())<=minimumPixels){report("small",{},result.pixels,result.cached);next();return;}
   QSaveFile file(mapping);if(file.open(QIODevice::WriteOnly)){file.write(QJsonDocument(QJsonObject{{"release",result.release},{"created",QDateTime::currentMSecsSinceEpoch()}}).toJson());file.commit();}trim();report("available",{},result.pixels,result.cached);done(result);
  });
 });
}

bool CatalogCoverArtProvider::trusted(QString source,QUrl url,bool image){
 if(url.scheme()!="https"||!url.userInfo().isEmpty()||(url.port()!=-1&&url.port()!=443)||url.toString().size()>4096)return false;
 auto host=url.host().toLower();if(source=="netease")return image?(host=="music.126.net"||host.endsWith(".music.126.net")):host=="music.163.com";
 if(source=="qq")return image?host=="y.gtimg.cn":host=="c.y.qq.com";return false;
}
void CatalogCoverArtProvider::cancel(){++generation;delay.stop();if(reply)reply->abort();reply=nullptr;}
void CatalogCoverArtProvider::get(QUrl url,bool json,std::function<void(QByteArray)> done){
 if(!trusted(source,url,!json)){failure="error";lastError=QCoreApplication::translate("CoverArt","Untrusted cover URL");done({});return;}if(stage)stage(json?"searching":"downloading");const auto token=generation;QNetworkRequest req(url);req.setRawHeader("User-Agent",("DroidMediaLink/"+QCoreApplication::applicationVersion()+" (https://github.com/XHZ-Steve/DroidMusicLink)").toUtf8());req.setRawHeader("Referer",source=="netease"?"https://music.163.com/":"https://y.qq.com/");req.setTransferTimeout(15000);req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::UserVerifiedRedirectPolicy);
 auto r=network->get(req);reply=r;const qint64 limit=(json?1:10)*1024*1024;QTimer::singleShot(15000,r,[r]{if(!r->isFinished())r->abort();});connect(r,&QNetworkReply::redirected,r,[r,s=source,json](QUrl url){if(trusted(s,url,!json))r->redirectAllowed();else r->abort();});connect(r,&QNetworkReply::downloadProgress,r,[r,limit](qint64 n,qint64 total){if(n>limit||total>limit)r->abort();});
 connect(r,&QNetworkReply::finished,this,[this,r,token,limit,done]{const auto code=r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();auto bytes=r->isOpen()?r->read(limit+1):QByteArray{};const bool ok=r->error()==QNetworkReply::NoError&&code==200&&bytes.size()<=limit;r->deleteLater();if(reply==r)reply=nullptr;if(token!=generation)return;if(code==429||code==503){bool parsed=false;auto seconds=r->rawHeader("Retry-After").toLongLong(&parsed);nextRequest=QDateTime::currentMSecsSinceEpoch()+qBound<qint64>(60LL,parsed?seconds:60LL,86400LL)*1000;}if(!ok){failure=(code==429||code==503)?"rate-limited":r->error()==QNetworkReply::TimeoutError?"timeout":"error";lastError=r->errorString();}done(ok?bytes:QByteArray{});});
}
QJsonObject CatalogCoverArtProvider::selectCandidate(QString source,const CoverArtTrack &track,const QJsonArray &rows){
 QMap<QString,QJsonObject> albums;
 for(auto value:rows){auto row=value.toObject();const bool netease=source=="netease";auto album=row.value("album").toObject();CoverArtTrack candidate{row.value(netease?"name":"songname").toString(),names(row.value(netease?"artists":"singer").toArray()),netease?album.value("name").toString():row.value("albumname").toString(),row.value(netease?"duration":"interval").toInteger()*(netease?1:1000)};
  if(!sameTrack(track,candidate)||candidate.album.isEmpty())continue;
  auto id=netease?QString::number(album.value("id").toInteger()):row.value("albummid").toString();if(!validCoverId(source+":"+id))continue;
  if(netease&&row.value("id").toInteger()<=0)continue;albums[id]=row;
 }return albums.size()==1?albums.first():QJsonObject{};
}
void CatalogCoverArtProvider::request(const CoverArtTrack &track,int resolution,Done done){
 cancel();failure="empty";lastError.clear();if(track.title.isEmpty()||track.artist.isEmpty()||track.duration<=0||track.title.size()>512||track.artist.size()>512||track.album.size()>512){failure="incomplete";done({},{},false);return;}const auto token=generation;const bool refresh=bypassCache;
 auto start=[this,track,resolution,done,token,refresh]{if(token!=generation)return;nextRequest=QDateTime::currentMSecsSinceEpoch()+1100;const auto term=(track.title+" "+track.artist.section('/',0,0)).left(240);
  auto url=source=="netease"?queryUrl("https://music.163.com/api/search/get/web",{{"s",term},{"type","1"},{"offset","0"},{"limit","25"}}):queryUrl("https://c.y.qq.com/soso/fcgi-bin/client_search_cp",{{"w",term},{"p","1"},{"n","25"},{"cr","1"},{"format","json"},{"inCharset","utf8"},{"outCharset","utf-8"}});
  get(url,true,[this,track,resolution,done,refresh](QByteArray bytes){auto object=QJsonDocument::fromJson(bytes).object();const auto rows=source=="netease"?object.value("result").toObject().value("songs").toArray():object.value("data").toObject().value("song").toObject().value("list").toArray();auto row=selectCandidate(source,track,rows);if(row.isEmpty()){done({},{},false);return;}
   const auto album=row.value("album").toObject();auto id=source+":"+(source=="netease"?QString::number(album.value("id").toInteger()):row.value("albummid").toString());if(!refresh&&cachedImage&&cachedImage(id,resolution)){done({},id,true);return;}
   if(source=="qq"){auto url=QUrl("https://y.gtimg.cn/music/photo_new/T002R800x800M000"+row.value("albummid").toString()+".jpg");get(url,false,[done,id](QByteArray image){done(image,id,false);});return;}
   const auto songId=QString::number(row.value("id").toInteger());get(queryUrl("https://music.163.com/api/song/detail/",{{"id",songId},{"ids","["+songId+"]"}}),true,[this,track,done,id,songId](QByteArray detail){auto songs=QJsonDocument::fromJson(detail).object().value("songs").toArray();auto verified=selectCandidate("netease",track,songs);if(QString::number(verified.value("id").toInteger())!=songId||"netease:"+QString::number(verified.value("album").toObject().value("id").toInteger())!=id){done({},{},false);return;}auto url=QUrl(verified.value("album").toObject().value("picUrl").toString());if(url.scheme()=="http")url.setScheme("https");get(url,false,[done,id](QByteArray image){done(image,id,false);});});
  });
 };const auto wait=qMax<qint64>(0,nextRequest-QDateTime::currentMSecsSinceEpoch());if(wait>1500){failure="rate-limited";lastError=QCoreApplication::translate("CoverArt","Source cooling down");done({},{},false);return;}if(wait){delay.setSingleShot(true);disconnect(&delay,nullptr,this,nullptr);connect(&delay,&QTimer::timeout,this,start);delay.start(int(wait));}else start();
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
