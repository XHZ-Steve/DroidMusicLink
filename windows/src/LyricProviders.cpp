#include "LyricProviders.h"
#include "LyricTimeline.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUrlQuery>
#include <QTimer>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#include <QDateTime>
#include <QTextDocumentFragment>
#include <QStringDecoder>
#include <QtZlib/zlib.h>
#include <algorithm>

namespace {
QString normalized(QString value){value=value.normalized(QString::NormalizationForm_KC).toCaseFolded();value.replace(QRegularExpression("[^\\p{L}\\p{N}]+")," ");return value.simplified();}
QString identifier(QJsonValue value){return value.isString()?value.toString():QString::number(value.toInteger());}
QString artists(QJsonArray values){QStringList names;for(const auto &v:values){auto name=v.toObject().value("name").toString();if(!name.isEmpty())names<<name;}return names.join('/');}
QUrl queryUrl(QString base,QList<QPair<QString,QString>> parameters){QUrl url(base);QUrlQuery query;for(const auto &p:parameters)query.addQueryItem(p.first,p.second);url.setQuery(query);return url;}
QString decodeHtml(QString value){value.replace('<',"&lt;");value.replace('>',"&gt;");value.replace('\n',"<br>");return QTextDocumentFragment::fromHtml(value).toPlainText();}
LyricDocument document(LyricCandidate candidate,QString text,QString translation={},QString romanization={}){
 LyricDocument result;result.provider=candidate.provider;result.sourceId=candidate.id;result.score=candidate.score;result.rows=LyricTimeline::parse(text);
 result.rows=LyricTimeline::mergeAuxiliary(result.rows,translation,"translation",200);
 result.rows=LyricTimeline::mergeAuxiliary(result.rows,romanization,"romanization",200);
 for(auto &value:result.rows){auto row=value.toMap();row["source"]=candidate.provider;row["sourceId"]=candidate.id;row["matchScore"]=candidate.score;if(!row.contains("background"))row["background"]=false;value=row;}
 return result;
}
}
QVariantMap LyricCandidate::toVariant()const{return {{"provider",provider},{"sourceId",id},{"id",provider=="lrclib"?id.toLongLong():0},{"title",title},{"artist",artist},{"album",album},{"duration",duration/1000.},{"score",score},{"extra",extra}};}
LyricCandidate LyricCandidate::fromVariant(QVariantMap row){return {row.value("provider").toString(),row.value("sourceId").toString(),row.value("title").toString(),row.value("artist").toString(),row.value("album").toString(),qint64(row.value("duration").toDouble()*1000),row.value("score").toDouble(),row.value("extra").toMap()};}
LyricProviders::LyricProviders(QNetworkAccessManager *network,QObject *parent):QObject(parent),m_network(network){}
LyricProviders::~LyricProviders(){cancel();}
QStringList LyricProviders::ids(){return {"lrclib","netease","qq","kugou"};}
void LyricProviders::cancel(){++m_epoch;auto replies=m_replies;m_replies.clear();for(const auto &reply:replies)if(reply)reply->abort();for(auto it=m_status.begin();it!=m_status.end();++it)if(it.value().value("status")=="testing")it.value()["status"]="unknown";if(changed)changed();}
void LyricProviders::setStatus(QString provider,QString status,QString error){m_status[provider]={{"status",status},{"lastError",error.left(240)},{"lastChecked",QDateTime::currentMSecsSinceEpoch()}};if(changed)changed();}
QVariantList LyricProviders::descriptors()const{
 QVariantList result;for(const auto &id:ids()){
  const auto name=id=="lrclib"?QString("LRCLIB"):id=="netease"?QCoreApplication::translate("LyricProviders","NetEase Cloud Music"):id=="qq"?QCoreApplication::translate("LyricProviders","QQ Music"):QCoreApplication::translate("LyricProviders","Kugou Music");
  QStringList capabilities{"search","line"};if(id=="netease"||id=="kugou")capabilities<<"word"<<"translation"<<"romanization";else if(id=="qq")capabilities<<"translation";
  QVariantMap row{{"id",id},{"name",name},{"capabilities",capabilities},{"experimental",id!="lrclib"},{"status","unknown"},{"enabledKey","lyrics/providers/"+id+"/enabled"},{"lastError",QString()},{"lastChecked",qint64(0)}};
  const auto status=m_status.value(id);for(auto it=status.cbegin();it!=status.cend();++it)row[it.key()]=it.value();result<<row;
 }return result;
}
double LyricProviders::matchScore(const LyricTrack &track,const LyricCandidate &candidate,bool strict){
 if(normalized(track.title).isEmpty()||normalized(track.title)!=normalized(candidate.title))return 0;
 if(normalized(track.artist).isEmpty()||normalized(track.artist)!=normalized(candidate.artist))return 0;
 const auto difference=qAbs(track.duration-candidate.duration);
 if(track.duration<=0||candidate.duration<=0)return strict?0:80;
 if(difference>(strict?3500:6000))return 0;
 double score=94.-difference/1000.;if(!track.album.isEmpty()&&normalized(track.album)==normalized(candidate.album))score+=6.;
 return qBound(0.,score,100.);
}
void LyricProviders::get(QString provider,QUrl url,JsonDone done){
 const auto epoch=m_epoch;const auto now=QDateTime::currentMSecsSinceEpoch();const auto wait=m_nextRequest.value(provider)-now;
 if(std::any_of(m_replies.cbegin(),m_replies.cend(),[&provider](const auto &reply){return reply&&reply->property("lyricProvider")==provider;})){QTimer::singleShot(250,this,[this,provider,url,done,epoch]{if(epoch==m_epoch)get(provider,url,done);});return;}
 if(wait>0){if(wait>3000){done({}, {},"rate-limited");return;}QTimer::singleShot(int(wait),this,[this,provider,url,done,epoch]{if(epoch==m_epoch)get(provider,url,done);});return;}
 m_nextRequest[provider]=now+250;
 QNetworkRequest request(url);request.setTransferTimeout(8000);request.setRawHeader("User-Agent","DroidMediaLink/0.1.0 (https://github.com/XHZ-Steve/DroidMusicLink)");
 if(provider=="netease")request.setRawHeader("Referer","https://music.163.com/");if(provider=="qq")request.setRawHeader("Referer","https://y.qq.com/");
 auto reply=m_network->get(request);reply->setProperty("lyricProvider",provider);m_replies<<reply;
 connect(reply,&QNetworkReply::downloadProgress,reply,[reply](qint64 received,qint64){if(received>2*1024*1024)reply->abort();});
 connect(reply,&QNetworkReply::finished,this,[this,reply,provider,epoch,done]{
  m_replies.removeAll(reply);if(epoch!=m_epoch){reply->deleteLater();return;}
  const auto status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  if(status==429||status==503){bool ok=false;auto seconds=reply->rawHeader("Retry-After").toLongLong(&ok);if(!ok)seconds=30;m_nextRequest[provider]=QDateTime::currentMSecsSinceEpoch()+qBound(qint64(1),qint64(seconds),qint64(86400))*1000;setStatus(provider,"rate-limited","HTTP "+QString::number(status));done({}, {},"rate-limited");reply->deleteLater();return;}
  if(reply->error()!=QNetworkReply::NoError){const auto error="HTTP "+QString::number(status)+": "+reply->errorString();setStatus(provider,"error",error);done({}, {},error);reply->deleteLater();return;}
  auto bytes=reply->read(2*1024*1024);QJsonParseError parseError;auto doc=QJsonDocument::fromJson(bytes,&parseError);
  if(parseError.error!=QJsonParseError::NoError){setStatus(provider,"error","invalid-json");done({}, {},"invalid-json");}else done(doc.object(),doc.array(),{});
  reply->deleteLater();
 });
}
void LyricProviders::search(QString provider,QString query,SearchDone done){
 if(!ids().contains(provider)){done({},"unknown-provider");return;}setStatus(provider,"testing");QUrl url;
 if(provider=="lrclib")url=queryUrl("https://lrclib.net/api/search",{{"q",query.left(240)}});
 if(provider=="netease")url=queryUrl("https://music.163.com/api/search/get/web",{{"s",query.left(240)},{"type","1"},{"offset","0"},{"limit","12"}});
 if(provider=="qq")url=queryUrl("https://c.y.qq.com/soso/fcgi-bin/client_search_cp",{{"w",query.left(240)},{"p","1"},{"n","12"},{"cr","1"},{"format","json"},{"inCharset","utf8"},{"outCharset","utf-8"}});
 if(provider=="kugou")url=queryUrl("https://mobiles.kugou.com/api/v3/search/song",{{"format","json"},{"keyword",query.left(240)},{"page","1"},{"pagesize","12"},{"showtype","1"}});
 get(provider,url,[this,provider,done](QJsonObject object,QJsonArray array,QString error){
  QList<LyricCandidate> result;if(!error.isEmpty()){done({},error);return;}
  if(provider=="netease")array=object.value("result").toObject().value("songs").toArray();
  if(provider=="qq")array=object.value("data").toObject().value("song").toObject().value("list").toArray();
  if(provider=="kugou")array=object.value("data").toObject().value("info").toArray();
  for(const auto &value:array){const auto row=value.toObject();LyricCandidate candidate;candidate.provider=provider;
   if(provider=="lrclib"){candidate.id=identifier(row.value("id"));candidate.title=row.value("trackName").toString();candidate.artist=row.value("artistName").toString();candidate.album=row.value("albumName").toString();candidate.duration=qint64(row.value("duration").toDouble()*1000);}
   if(provider=="netease"){candidate.id=identifier(row.value("id"));candidate.title=row.value("name").toString();candidate.artist=artists(row.value("artists").toArray());candidate.album=row.value("album").toObject().value("name").toString();candidate.duration=row.value("duration").toInteger();}
   if(provider=="qq"){candidate.id=row.value("songmid").toString();candidate.title=row.value("songname").toString();candidate.artist=artists(row.value("singer").toArray());candidate.album=row.value("albumname").toString();candidate.duration=row.value("interval").toInteger()*1000;}
   if(provider=="kugou"){candidate.id=row.value("hash").toString();candidate.title=row.value("songname_original").toString(row.value("songname").toString());candidate.artist=row.value("singername").toString();candidate.album=row.value("album_name").toString();candidate.duration=row.value("duration").toInteger()*1000;candidate.extra["hash"]=candidate.id;}
   if(!candidate.id.isEmpty()&&!candidate.title.isEmpty())result<<candidate;if(result.size()>=12)break;
  }
  setStatus(provider,result.isEmpty()?"empty":"available",result.isEmpty()?"no-results":QString());done(result,{});
 });
}
void LyricProviders::fetch(LyricCandidate candidate,FetchDone done){
 const auto provider=candidate.provider;if(!ids().contains(provider)||!QRegularExpression("^[A-Za-z0-9_-]{1,128}$").match(candidate.id).hasMatch()){done({},"invalid-candidate");return;}
 if(provider=="kugou"){fetchKugou(candidate,done);return;}
 QUrl url;if(provider=="lrclib")url=QUrl("https://lrclib.net/api/get/"+candidate.id);
 if(provider=="netease")url=queryUrl("https://music.163.com/api/song/lyric/v1",{{"id",candidate.id},{"cp","false"},{"tv","0"},{"lv","0"},{"rv","0"},{"kv","0"},{"yv","0"},{"ytv","0"},{"yrv","0"}});
 if(provider=="qq")url=queryUrl("https://c.y.qq.com/lyric/fcgi-bin/fcg_query_lyric_new.fcg",{{"songmid",candidate.id},{"g_tk","5381"},{"format","json"},{"inCharset","utf8"},{"outCharset","utf-8"}});
 get(provider,url,[this,candidate,done](QJsonObject row,QJsonArray,QString error){
  if(!error.isEmpty()){done({},error);return;}QString text,translation,romanization,format="lrc";
  if(candidate.provider=="lrclib"){text=row.value("syncedLyrics").toString();if(text.isEmpty())text=row.value("plainLyrics").toString();}
  if(candidate.provider=="netease"){text=row.value("yrc").toObject().value("lyric").toString();if(!text.isEmpty())format="yrc";else text=row.value("lrc").toObject().value("lyric").toString();translation=row.value("ytlrc").toObject().value("lyric").toString();if(translation.isEmpty())translation=row.value("tlyric").toObject().value("lyric").toString();romanization=row.value("romalrc").toObject().value("lyric").toString();}
  if(candidate.provider=="qq"){text=decodeHtml(QString::fromUtf8(QByteArray::fromBase64(row.value("lyric").toString().toUtf8())));translation=decodeHtml(QString::fromUtf8(QByteArray::fromBase64(row.value("trans").toString().toUtf8())));}
  parseDocument(candidate,text,translation,romanization,format,done);
 });
}
void LyricProviders::fetchKugou(LyricCandidate candidate,FetchDone done){
 const auto url=queryUrl("https://lyrics.kugou.com/search",{{"ver","1"},{"man","yes"},{"client","pc"},{"keyword",candidate.artist+" - "+candidate.title},{"duration",QString::number(candidate.duration)},{"hash",candidate.id}});
 get("kugou",url,[this,candidate,done](QJsonObject result,QJsonArray,QString error){
  const auto candidates=result.value("candidates").toArray();if(!error.isEmpty()||candidates.isEmpty()){done({},error.isEmpty()?"no-lyrics":error);return;}
  QJsonObject choice;for(const auto &item:candidates){const auto c=item.toObject();if(normalized(c.value("song").toString())==normalized(candidate.title)&&normalized(c.value("singer").toString())==normalized(candidate.artist)&&qAbs(c.value("duration").toInteger()-candidate.duration)<=4000){choice=c;break;}}
  if(choice.isEmpty()){done({},"untrusted-lyric-version");return;}
  const auto download=queryUrl("https://lyrics.kugou.com/download",{{"ver","1"},{"client","pc"},{"id",identifier(choice.value("id"))},{"accesskey",choice.value("accesskey").toString()},{"fmt","krc"},{"charset","utf8"}});
  get("kugou",download,[this,candidate,done](QJsonObject row,QJsonArray,QString error){if(!error.isEmpty()){done({},error);return;}parseDocument(candidate,{},{},{},"krc",done,QByteArray::fromBase64(row.value("content").toString().toUtf8()));});
 });
}
void LyricProviders::parseDocument(LyricCandidate candidate,QString text,QString translation,QString romanization,QString format,FetchDone done,QByteArray compressed){
 const auto epoch=m_epoch;auto watcher=new QFutureWatcher<LyricDocument>(this);
 connect(watcher,&QFutureWatcher<LyricDocument>::finished,this,[this,watcher,epoch,done,candidate]{auto result=watcher->result();watcher->deleteLater();if(epoch!=m_epoch)return;setStatus(candidate.provider,result.rows.isEmpty()?"empty":"available",result.rows.isEmpty()?"no-lyrics":QString());done(result,result.rows.isEmpty()?"no-lyrics":QString());});
 watcher->setFuture(QtConcurrent::run([candidate,text,translation,romanization,format,compressed]{auto result=document(candidate,compressed.isEmpty()?text:LyricProviders::decodeLocal(compressed),translation,romanization);result.rows=LyricTimeline::validated(result.rows);result.format=format;return result;}));
}
QString LyricProviders::decodeLocal(QByteArray bytes){
 if(bytes.size()>2*1024*1024)return {};
 if(!bytes.startsWith("krc1")){
  if(bytes.startsWith("\xff\xfe")||bytes.startsWith("\xfe\xff")){QStringDecoder decoder(QStringDecoder::Utf16);const auto text=decoder(bytes);return decoder.hasError()?QString():QString(text);}
  if(bytes.contains('\0'))return {};QStringDecoder decoder(QStringDecoder::Utf8);const auto text=decoder(bytes);return decoder.hasError()?QString():QString(text);
 }
 const unsigned char key[16]={64,71,97,119,94,50,116,71,81,54,49,45,206,210,110,105};bytes.remove(0,4);for(qsizetype i=0;i<bytes.size();++i)bytes[i]=char(quint8(bytes[i])^key[i%16]);
 QByteArray output(2*1024*1024,Qt::Uninitialized);uLongf size=uLongf(output.size());const auto status=uncompress(reinterpret_cast<Bytef*>(output.data()),&size,reinterpret_cast<const Bytef*>(bytes.constData()),uLong(bytes.size()));if(status!=Z_OK)return {};output.resize(qsizetype(size));return QString::fromUtf8(output);
}
void LyricProviders::test(QString provider){search(provider,"The Scientist Coldplay",[this,provider](QList<LyricCandidate> candidates,QString error){if(!error.isEmpty()||candidates.isEmpty()){setStatus(provider,"error",error.isEmpty()?"no-results":error);return;}fetch(candidates.first(),[this,provider](LyricDocument doc,QString failure){setStatus(provider,doc.rows.isEmpty()?"error":"available",failure);});});}
