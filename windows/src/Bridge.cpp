#include "Bridge.h"
#include "SettingsSchema.h"
#include "AppearanceConfig.h"
#include "LyricTimeline.h"
#include <QJsonDocument>
#include <QJSValue>
#include <QNetworkProxy>
#include <QNetworkInterface>
#include <QtEndian>
#include <QJsonArray>
#include <QDateTime>
#include <QUuid>
#include <QHostInfo>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslError>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QImage>
#include <QImageReader>
#include <QBuffer>
#include <QUrlQuery>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QPalette>
#include <windows.h>
#include <wincrypt.h>
#include <algorithm>
#include <cmath>
#include <limits>
static const QStringList booleanSettings{"light","motion","showLyrics","closeToTray","connection/autoReconnect","lyrics/online","media/smtc","general/autostart","startHidden","notifications","desktopClickThrough","overlayTopmost"};
static Bridge *nativeOwner=nullptr;
static void nativeCommand(const char *name,double value){QString command=QString::fromUtf8(name);if(auto app=QCoreApplication::instance())QMetaObject::invokeMethod(app,[command,value]{if(nativeOwner)nativeOwner->command(command,value);},Qt::QueuedConnection);}
static QByteArray protect(QByteArray bytes,bool decrypt){DATA_BLOB input{DWORD(bytes.size()),reinterpret_cast<BYTE*>(bytes.data())},output{};BOOL ok=decrypt?CryptUnprotectData(&input,nullptr,nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output):CryptProtectData(&input,L"DroidMediaLink",nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output);if(!ok)return {};QByteArray result(reinterpret_cast<char*>(output.pbData),output.cbData);SecureZeroMemory(output.pbData,output.cbData);LocalFree(output.pbData);return result;}
Bridge::Bridge(QObject *parent):QObject(parent),m_status(tr("Disconnected")){
 if(!m_settings.value("preferences/lyrics/autoMatchMigration",false).toBool()){m_settings.setValue("preferences/lyrics/online",true);m_settings.setValue("preferences/lyrics/autoMatch",true);m_settings.setValue("preferences/lyrics/autoMatchMigration",true);}
 if(!m_settings.contains("preferences/desktopLyrics/fontSize")&&m_settings.contains("preferences/lyricSize"))m_settings.setValue("preferences/desktopLyrics/fontSize",m_settings.value("preferences/lyricSize"));
 // LAN discovery and phone traffic must bypass HTTP/SOCKS proxies.
 m_socket.setProxy(QNetworkProxy::NoProxy);m_mdns.setProxy(QNetworkProxy::NoProxy);
 // Portable Schannel/Android 11 autonegotiation failed in a real connection.
 // TLS 1.2 retains certificate/SAS verification and the platform cipher policy.
 auto linkTls=m_socket.sslConfiguration();linkTls.setProtocol(QSsl::TlsV1_2);m_socket.setSslConfiguration(linkTls);
 if(!m_settings.contains("identity/clientId"))m_settings.setValue("identity/clientId",QUuid::createUuid().toString(QUuid::WithoutBraces));
 m_lyricProviders=new LyricProviders(&m_network,this);m_lyricProviders->changed=[this]{emit lyricProvidersChanged();};connect(this,&Bridge::settingsChanged,this,[this]{emit lyricProvidersChanged();});
 m_socket.setMaxAllowedIncomingMessageSize(4*1024*1024);m_socket.setMaxAllowedIncomingFrameSize(4*1024*1024);
 connect(&m_socket,&QWebSocket::sslErrors,this,[this](const QList<QSslError>&errors){
  const auto cert=m_socket.sslConfiguration().peerCertificate();const auto actual=QString::fromLatin1(cert.digest(QCryptographicHash::Sha256).toHex());
  if(cert.isNull()){m_manual=true;m_socket.abort();return;}
  // Only the certificate on this connection gets temporary trust; credentials remain pinned.
  QList<QSslError> allowed;for(const auto&e:errors){if(e.error()==QSslError::SelfSignedCertificate||e.error()==QSslError::HostNameMismatch||e.error()==QSslError::UnableToGetLocalIssuerCertificate||e.error()==QSslError::CertificateUntrusted)allowed<<e;else{m_manual=true;setStatus(tr("Certificate rejected"));m_socket.abort();return;}}
  m_observedPin=actual;m_socket.ignoreSslErrors(allowed);
 });
 connect(&m_socket,&QWebSocket::connected,this,[this]{
  const auto cert=m_socket.sslConfiguration().peerCertificate();m_observedPin=QString::fromLatin1(cert.digest(QCryptographicHash::Sha256).toHex());if(cert.isNull()){disconnectDevice();return;}
  m_backoff=0;m_peerFeatures.clear();m_artworkRetryAttempts=0;m_lastPong.start();m_heartbeat.start(15000);
  if(!m_pin.isEmpty()&&m_observedPin==m_pin&&!token().isEmpty()){m_authSent=true;setStatus(tr("Authenticating"));send("auth",{{"clientId",m_settings.value("identity/clientId").toString()},{"token",QString::fromUtf8(token())}});}
  else {if(!m_pin.isEmpty()&&m_observedPin!=m_pin)emit notification(tr("Device certificate changed. Compare the code on your phone again."));beginPairing();}
 });
 connect(&m_socket,&QWebSocket::textMessageReceived,this,&Bridge::receive);
 connect(&m_socket,&QWebSocket::disconnected,this,[this]{m_online=false;m_authSent=false;m_artworkRetry.stop();m_heartbeat.stop();if(m_pairing)m_manual=true;clearPairing();m_pending.clear();emit changed();updateNative();if(!m_manual){setStatus(tr("Reconnecting"));retry();}});
 connect(&m_socket,&QWebSocket::errorOccurred,this,[this](QAbstractSocket::SocketError){setStatus(tr("Connection failed: %1").arg(m_socket.errorString()));if(!m_manual)retry();});
 m_pairTimeout.setSingleShot(true);connect(&m_pairTimeout,&QTimer::timeout,this,[this]{if(m_pairing){cancelPairing();setStatus(tr("Pairing expired"));}});
 connect(&m_progress,&QTimer::timeout,this,[this]{emit progressChanged();updateLyricClock();});m_progress.start(250);
 m_artworkRetry.setSingleShot(true);connect(&m_artworkRetry,&QTimer::timeout,this,[this]{if(!m_online||!m_peerFeatures.contains("get_snapshot")||!m_artwork.isEmpty()||title().isEmpty())return;send("get_snapshot");++m_artworkRetryAttempts;const int delays[]={800,1800,4000,8000};if(m_artworkRetryAttempts<4)m_artworkRetry.start(delays[m_artworkRetryAttempts]);});
 m_lyricClock.setTimerType(Qt::PreciseTimer);m_lyricClock.setInterval(16);connect(&m_lyricClock,&QTimer::timeout,this,[this]{updateLyricClock();});
 connect(this,&Bridge::changed,this,&Bridge::updateLyricClock);connect(this,&Bridge::settingsChanged,this,&Bridge::updateLyricClock);
 connect(&m_heartbeat,&QTimer::timeout,this,[this]{if(m_lastPong.elapsed()>45000){m_socket.abort();return;}send("ping");const auto now=QDateTime::currentMSecsSinceEpoch();for(auto i=m_pending.begin();i!=m_pending.end();){if(now-i.value()>8000){emit notification(tr("Command timed out"));i=m_pending.erase(i);}else ++i;}});
 m_retry.setSingleShot(true);connect(&m_retry,&QTimer::timeout,this,[this]{if(!m_manual&&setting("connection/autoReconnect",true).toBool())m_socket.open(QUrl(m_url));});
 connect(&m_mdns,&QUdpSocket::readyRead,this,[this]{
  while(m_mdns.hasPendingDatagrams()){
   QByteArray data(int(m_mdns.pendingDatagramSize()),0);m_mdns.readDatagram(data.data(),data.size());if(data.size()<12)continue;
   auto word=[&](int at)->quint16 {return at>=0&&at+1<data.size()?(quint8(data[at])<<8)|quint8(data[at+1]):0;};
   std::function<QString(int&,int)> name=[&](int &at,int depth)->QString{if(depth>20)return {};QStringList parts;int labels=0;while(at<data.size()&&++labels<128){int length=quint8(data[at++]);if(!length)return parts.join('.');if((length&0xc0)==0xc0){if(at>=data.size())return {};int pointer=((length&0x3f)<<8)|quint8(data[at++]);auto suffix=name(pointer,depth+1);if(suffix.isEmpty())return {};parts<<suffix;return parts.join('.');}if(length>63||at+length>data.size())return {};parts<<QString::fromUtf8(data.mid(at,length));at+=length;}return {};};
   int at=12;bool bad=false;for(int i=0;i<word(4);++i){name(at,0);if(at+4>data.size()){bad=true;break;}at+=4;}if(bad)continue;
   auto &hosts=m_hosts;auto &services=m_services;if(hosts.size()>512)hosts.clear();if(services.size()>512)services.clear();if(m_discoveryInfo.size()>512)m_discoveryInfo.clear();
   int count=word(6)+word(8)+word(10);for(int i=0;i<count&&i<512;++i){auto displayOwner=name(at,0);auto owner=displayOwner.toLower();if(at+10>data.size())break;int type=word(at),len=word(at+8);at+=10;if(at+len>data.size())break;int next=at+len;
    if(type==1&&len==4)hosts[owner]=QString("%1.%2.%3.%4").arg(quint8(data[at])).arg(quint8(data[at+1])).arg(quint8(data[at+2])).arg(quint8(data[at+3]));
    if(type==16&&owner.endsWith("._droidmedialink._tcp.local")){QVariantMap info;int cursor=at;while(cursor<next){int size=quint8(data[cursor++]);if(cursor+size>next)break;auto field=data.mid(cursor,size);cursor+=size;int eq=field.indexOf('=');if(eq>0)info[QString::fromUtf8(field.left(eq))]=QString::fromUtf8(field.mid(eq+1));}m_discoveryInfo[owner]=info;}
    if(type==33&&len>=7&&owner.endsWith("._droidmedialink._tcp.local")){int port=word(at+4),cursor=at+6;QString host=name(cursor,0).toLower();if(!host.isEmpty()&&port>0){services[owner]={host,port};if(!m_discoveryInfo[owner].contains("name"))m_discoveryInfo[owner]["name"]=displayOwner.section("._droidmedialink",0,0,QString::SectionCaseInsensitiveSeps);}}at=next;
   }
   refreshDevices();
  }
 }); refreshDevices();
 const auto saved=m_settings.value("connection/url").toString();if(!saved.isEmpty()&&setting("connection/autoReconnect",true).toBool()){QTimer::singleShot(0,this,&Bridge::discover);QTimer::singleShot(800,this,[this]{connectDevice(m_settings.value("connection/url").toString());});}
}
Bridge::~Bridge(){cancelLyricRequest();if(m_lyricProviders){m_lyricProviders->changed={};delete m_lyricProviders;m_lyricProviders=nullptr;}if(nativeOwner==this){nativeOwner=nullptr;using Shutdown=void(*)();if(auto shutdown=reinterpret_cast<Shutdown>(m_native.resolve("dml_smtc_shutdown")))shutdown();}}
QString Bridge::deviceKey(QString url)const{for(const auto &key:m_settings.allKeys())if(key.startsWith("devices/")&&key.endsWith("/url")&&m_settings.value(key).toString()==url)return key.chopped(4);return "devices/"+QString::fromLatin1(QCryptographicHash::hash(url.toUtf8(),QCryptographicHash::Sha256).toHex());}
QByteArray Bridge::token()const{return protect(QByteArray::fromBase64(m_settings.value(deviceKey(m_url)+"/credential").toByteArray()),true);}
bool Bridge::storeToken(QByteArray bytes){if(!m_compareReady||m_deviceId.isEmpty()||m_observedPin.isEmpty())return false;const auto encrypted=protect(bytes,false);if(encrypted.isEmpty())return false;const auto key="devices/"+m_deviceId;const auto previous=deviceKey(m_url);if(previous!=key)m_settings.remove(previous);m_settings.setValue(key+"/credential",encrypted.toBase64());m_settings.setValue(key+"/fingerprint",m_observedPin);m_settings.setValue(key+"/url",m_url);m_settings.setValue(key+"/name",m_deviceName);m_settings.setValue(key+"/deviceId",m_deviceId);m_settings.setValue("connection/url",m_url);m_pin=m_observedPin;refreshDevices();return true;}
void Bridge::setStatus(QString text){m_status=text;emit changed();}
QString Bridge::sasCode(QByteArray fingerprint,QByteArray clientNonce,QByteArray serverNonce,QString clientId){if(fingerprint.size()!=32||clientNonce.size()!=32||serverNonce.size()!=32||clientId.isEmpty())return {};const auto hash=QCryptographicHash::hash(QByteArray("DroidMediaLink-SAS-v2")+fingerprint+clientNonce+serverNonce+clientId.toUtf8(),QCryptographicHash::Sha256);return QString::number(qFromBigEndian<quint32>(hash.constData())%1000000).rightJustified(6,'0');}
void Bridge::clearPairing(){m_pairTimeout.stop();m_pairing=false;m_compareReady=false;m_code.clear();m_clientNonce.clear();m_serverCommit.clear();}
void Bridge::beginPairing(){clearPairing();m_pairing=true;m_clientNonce.resize(32);for(int i=0;i<32;i+=4){auto random=QRandomGenerator::system()->generate();memcpy(m_clientNonce.data()+i,&random,4);}m_pairTimeout.start(90000);setStatus(tr("Compare the code on your phone"));send("pair_begin",{{"clientId",m_settings.value("identity/clientId").toString()},{"clientName",QHostInfo::localHostName().left(80)},{"clientCommit",QString::fromLatin1(QCryptographicHash::hash(m_clientNonce,QCryptographicHash::Sha256).toHex())}});}
void Bridge::cancelPairing(){if(m_pairing)send("pair_cancel");disconnectDevice();}
void Bridge::connectDevice(QString url,QString,QString){
 if(!url.contains("://"))url="wss://"+url;QUrl target(url);if(target.port()<0)target.setPort(18123);
 if(target.scheme()!="wss"||target.host().isEmpty()||!target.userInfo().isEmpty()||target.hasFragment()){setStatus(tr("Invalid device address"));return;}
 if(m_url==target.toString()&&(m_socket.state()==QAbstractSocket::ConnectingState||m_socket.state()==QAbstractSocket::ConnectedState))return;
 m_manual=true;m_retry.stop();m_socket.abort();clearPairing();m_online=false;m_url=target.toString();m_seen.clear();m_observedPin.clear();m_pin=m_settings.value(deviceKey(m_url)+"/fingerprint").toString();m_deviceId.clear();m_deviceName=target.host();m_manual=false;setStatus(tr("Connecting"));m_socket.open(target);
}
void Bridge::disconnectDevice(){m_artworkRetry.stop();m_manual=true;m_retry.stop();m_heartbeat.stop();clearPairing();m_socket.close();m_online=false;setStatus(tr("Disconnected"));updateNative();}
void Bridge::retry(){if(m_retry.isActive()||!setting("connection/autoReconnect",true).toBool())return;m_retry.start(qMin(60000,1000*(1<<qMin(m_backoff++,6)))+int(QRandomGenerator::global()->bounded(800)));}
void Bridge::send(QString type,QJsonObject payload){if(m_socket.state()!=QAbstractSocket::ConnectedState)return;QJsonObject e{{"protocolVersion",1},{"messageId",QUuid::createUuid().toString(QUuid::WithoutBraces)},{"timestamp",QDateTime::currentMSecsSinceEpoch()},{"type",type},{"payload",payload}};m_socket.sendTextMessage(QString::fromUtf8(QJsonDocument(e).toJson(QJsonDocument::Compact)));}
void Bridge::receive(QString message){if(message.toUtf8().size()>4*1024*1024){m_socket.close();return;}QJsonParseError error;const auto doc=QJsonDocument::fromJson(message.toUtf8(),&error);if(error.error!=QJsonParseError::NoError||!doc.isObject())return;const auto e=doc.object();if(e.value("protocolVersion").toInt()!=1){setStatus(tr("Incompatible protocol"));m_manual=true;m_socket.close();return;}if(!e.value("messageId").isString()||e.value("messageId").toString().isEmpty()||!e.value("timestamp").isDouble()||!e.value("payload").isObject()||!e.value("type").isString())return;if(qAbs(double(QDateTime::currentMSecsSinceEpoch())-e.value("timestamp").toDouble())>120000)return;const auto messageId=e.value("messageId").toString();if(m_seen.contains(messageId))return;m_seen<<messageId;if(m_seen.size()>256)m_seen.removeFirst();const auto p=e.value("payload").toObject();const auto type=e.value("type").toString();
 if(type=="ping"){send("pong");return;}if(type=="pong"){m_lastPong.restart();return;}
 if(type=="pair_challenge"){if(!m_pairing||!m_serverCommit.isEmpty()||!QRegularExpression("^[0-9a-f]{64}$").match(p.value("serverCommit").toString()).hasMatch()){cancelPairing();return;}m_serverCommit=QByteArray::fromHex(p.value("serverCommit").toString().toLatin1());send("pair_reveal",{{"clientNonce",QString::fromLatin1(m_clientNonce.toHex())}});return;}
 if(type=="pair_compare"){const auto hex=p.value("serverNonce").toString();const auto nonce=QByteArray::fromHex(hex.toLatin1());if(!m_pairing||m_compareReady||m_serverCommit.size()!=32||!QRegularExpression("^[0-9a-f]{64}$").match(hex).hasMatch()||QCryptographicHash::hash(nonce,QCryptographicHash::Sha256)!=m_serverCommit){cancelPairing();setStatus(tr("Pairing verification failed"));return;}m_deviceId=p.value("deviceId").toString().toLower();m_deviceName=p.value("name").toString(m_deviceName).left(100);if(m_deviceId!=m_observedPin){cancelPairing();setStatus(tr("Pairing verification failed"));return;}m_code=sasCode(QByteArray::fromHex(m_observedPin.toLatin1()),m_clientNonce,nonce,m_settings.value("identity/clientId").toString());m_compareReady=true;emit changed();return;}
 if(type=="pair_approve"){if(!m_pairing||!m_compareReady||p.value("deviceId").toString().toLower()!=m_deviceId||p.value("clientId").toString()!=m_settings.value("identity/clientId").toString())return;if(p.value("token").toString().isEmpty()||p.value("token").toString().size()>4096||!storeToken(p.value("token").toString().toUtf8())){disconnectDevice();setStatus(tr("Unable to save credentials"));return;}clearPairing();m_authSent=true;setStatus(tr("Authenticating"));send("auth",{{"clientId",m_settings.value("identity/clientId").toString()},{"token",p.value("token")}});return;}
 if(type=="auth_result"){if(!m_authSent)return;m_online=p.value("authenticated").toBool();if(m_online){setStatus(tr("Connected"));m_lastPong.restart();}else{m_settings.remove(deviceKey(m_url)+"/credential");m_manual=true;setStatus(tr("Authorization rejected; pair again"));m_socket.close();}updateNative();return;}
 if(type=="error"){const auto code=p.value("code").toString();if(code=="unauthorized")m_settings.remove(deviceKey(m_url)+"/credential");if(code=="unauthorized"||code.startsWith("pairing_")||code.startsWith("pair_")){clearPairing();m_manual=true;m_online=false;m_socket.close();setStatus(tr("Authorization rejected; pair again"));}emit notification(p.value("message").toString(tr("Device rejected request")));return;}
 if(!m_online)return;
 if(type=="capabilities"){m_peerFeatures.clear();const auto features=p.value("features").toArray();for(int i=0;i<16&&i<features.size();++i){const auto feature=features[i].toString();if(feature=="volume_state"||feature=="get_snapshot")m_peerFeatures.insert(feature);}send("capabilities",{{"features",QJsonArray{"volume_state","get_snapshot"}}});return;}
 if(type=="session_snapshot"||type=="session_changed"||type=="metadata_changed"||type=="playback_state"||type=="position_anchor"||type=="available_commands"||type=="volume_state")applyMedia(p);
 if(type=="command_result"){const auto request=p.value("requestId").toString();if(!m_pending.contains(request))return;m_pending.remove(request);if(p.contains("volume")&&p.contains("maxVolume"))applyMedia({{"volume",p.value("volume")},{"maxVolume",p.value("maxVolume")}});emit commandFinished(request,p.value("success").toBool(),p.value("error").toString());if(p.contains("success")&&!p.value("success").toBool())emit notification(p.value("error").toString(tr("Command unavailable")));}
 if(type=="lyrics_payload"){m_embeddedLyrics=p.value("lrc").toString();m_embeddedKey=lyricKey();if(!m_settings.contains("lyrics/bindings/"+lyricIdentity())){cancelLyricRequest();setLyrics(parseLyrics(m_embeddedLyrics),"phone");}}
}
QStringList Bridge::commands()const{QStringList list;for(const auto&v:m_media.value("commands").toArray())list<<v.toString();return list;}
qint64 Bridge::position()const{qint64 value=m_position;if(playing()&&m_anchor.isValid())value+=qint64(m_anchor.elapsed()*qBound(-16.,m_media.value("speed").toDouble(1),16.));return qBound<qint64>(0,value,duration()>0?duration():std::numeric_limits<qint64>::max());}
int Bridge::currentLine()const{return m_currentLyricLine;}
void Bridge::setLyrics(QVariantList rows,QString source){m_lyrics=LyricTimeline::validated(rows);m_lyricSource=std::move(source);m_lyricsLoading=false;m_lyricTimes.clear();m_lyricTimes.reserve(m_lyrics.size());if(m_lyrics.isEmpty()||!m_lyrics.first().toMap().value("untimed").toBool())for(const auto &row:m_lyrics)m_lyricTimes<<row.toMap().value("time").toLongLong();emit lyricsChanged();updateLyricClock();}
void Bridge::updateLyricClock(){const auto line=LyricTimeline::lineAt(m_lyricTimes,lyricPosition());if(line!=m_currentLyricLine){m_currentLyricLine=line;emit lyricLineChanged();}emit lyricProgressChanged();const bool run=m_uiVisible&&!m_lyricSurfaces.isEmpty()&&playing()&&!m_lyrics.isEmpty();if(run&&!m_lyricClock.isActive())m_lyricClock.start();else if(!run)m_lyricClock.stop();}
void Bridge::setLyricsVisible(QString surface,bool visible){if(surface.size()>100)return;if(visible)m_lyricSurfaces.insert(surface);else m_lyricSurfaces.remove(surface);updateLyricClock();}
void Bridge::cancelLyricRequest(){++m_lyricGeneration;if(m_lyricProviders)m_lyricProviders->cancel();if(m_lyricReply){auto reply=m_lyricReply;m_lyricReply=nullptr;reply->abort();}m_lyricsLoading=false;}

void Bridge::command(QString name,double value){if(!QStringList{"play","pause","toggle","next","previous","seek","volume"}.contains(name))return;if(!m_online||m_pending.size()>=64)return;static QElapsedTimer limit;static int burst=0;if(!limit.isValid()||limit.elapsed()>1000){limit.restart();burst=0;}if(++burst>20)return;QString capability=name=="toggle"?(playing()?"pause":"play"):name;if(!commands().contains(capability)&&!commands().contains(name)){emit notification(tr("Command unavailable"));return;}if(!std::isfinite(value))return;const auto id=QUuid::createUuid().toString(QUuid::WithoutBraces);QJsonObject p{{"requestId",id},{"command",name},{"value",value}};if(name=="seek"){value=qBound(0.,value,double(duration()));p["positionMs"]=value;p["value"]=value;}if(name=="volume")p["volume"]=qBound(0.,value,1.);m_pending[id]=QDateTime::currentMSecsSinceEpoch();send("command_request",p);}
void Bridge::applyMedia(QJsonObject media){
 const auto old=title()+"|"+artist()+"|"+album();const auto previousSession=m_media.value("sessionId");m_position=position();
 for(auto it=media.begin();it!=media.end();++it)m_media[it.key()]=it.value();
 if(media.contains("positionMs"))m_position=qMax<qint64>(0,media.value("positionMs").toInteger());m_anchor.restart();
 auto clearArtwork=[this]{m_artwork.clear();m_artworkDigest.clear();m_coverPalette={};m_artworkAccent="#9bc3c6";};
 if(media.contains("artwork")){
  const auto bytes=QByteArray::fromBase64(media.value("artwork").toString().toLatin1());
  const auto digest=QCryptographicHash::hash(bytes,QCryptographicHash::Sha256);
  // A media-state callback often repeats the same cover. Preserve its URL and
  // decoded texture so pause/progress updates do not trigger a crossfade/reload.
  if(m_artwork.isEmpty()||digest!=m_artworkDigest){
   QImage image;QBuffer buffer;buffer.setData(bytes);buffer.open(QIODevice::ReadOnly);QImageReader reader(&buffer);const auto size=reader.size();
   if(bytes.size()<=2*1024*1024&&size.isValid()&&size.width()<=4096&&size.height()<=4096&&qint64(size.width())*size.height()<=16777216&&!(image=reader.read()).isNull()){
    m_coverPalette=CoverPalette::fromImage(image);m_artworkAccent=m_coverPalette.primary.name();
    image=image.scaled(1024,1024,Qt::KeepAspectRatio,Qt::SmoothTransformation);const auto path=cachePath()+"/artwork.jpg";
    if(image.save(path,"JPG",90)){m_artworkDigest=digest;m_artwork=QUrl::fromLocalFile(path).toString()+"?v="+QString::fromLatin1(digest.toHex().left(16));}else clearArtwork();
   }else clearArtwork();
  }
 }
 if(old!=title()+"|"+artist()+"|"+album()){if(!media.contains("artwork"))clearArtwork();}
 if(old!=title()+"|"+artist()+"|"+album()||previousSession!=m_media.value("sessionId")){m_artworkRetry.stop();m_artworkRetryAttempts=0;}
 if(!m_artwork.isEmpty()){m_artworkRetry.stop();m_artworkRetryAttempts=0;}else if(m_online&&m_peerFeatures.contains("get_snapshot")&&!title().isEmpty()&&m_artworkRetryAttempts<4&&!m_artworkRetry.isActive()){const int delays[]={800,1800,4000,8000};m_artworkRetry.start(delays[m_artworkRetryAttempts]);}
 if(m_lyricKey!=lyricKey()){++m_searchGeneration;if(m_lyricSearchReply)m_lyricSearchReply->abort();m_lyricResults.clear();emit lyricResultsChanged();fetchLyrics();}
 applyTheme();emit changed();emit progressChanged();updateNative();
}
QString Bridge::cachePath()const{auto path=QStandardPaths::writableLocation(QStandardPaths::CacheLocation);QDir().mkpath(path);return path;}
void Bridge::previewCoverForTest(QString path){
 QImageReader reader(path);const auto size=reader.size();
 if(!size.isValid()||size.width()>8192||size.height()>8192||qint64(size.width())*size.height()>33554432)return;
 const auto image=reader.read();if(image.isNull())return;
 m_coverPalette=CoverPalette::fromImage(image);m_artworkAccent=m_coverPalette.primary.name();
 m_artworkDigest.clear();m_artwork=QUrl::fromLocalFile(QFileInfo(path).absoluteFilePath()).toString();
 applyTheme();emit changed();
}
bool Bridge::initializeNative(quintptr hwnd){m_native.setFileName(QCoreApplication::applicationDirPath()+"/WindowsNative.dll");using Init=int(*)(void*,void(*)(const char*,double));auto init=reinterpret_cast<Init>(m_native.resolve("dml_smtc_init"));m_nativeUpdate=reinterpret_cast<NativeUpdate>(m_native.resolve("dml_smtc_update"));if(!init||!m_nativeUpdate){emit notification(tr("System media bridge unavailable"));return false;}nativeOwner=this;if(init(reinterpret_cast<void*>(hwnd),nativeCommand)!=1){emit notification(tr("System media controls unavailable"));m_nativeUpdate=nullptr;return false;}updateNative();return true;}void Bridge::updateNative(){if(m_nativeUpdate){auto media=m_media;media["online"]=m_online&&setting("media/smtc",true).toBool();media["positionMs"]=position();media["artwork"]=QDir::toNativeSeparators(QUrl(m_artwork).toLocalFile());media["artworkRevision"]=QString::fromLatin1(m_artworkDigest.toHex());const auto json=QJsonDocument(media).toJson(QJsonDocument::Compact);const auto previous=m_nativeStatus;m_nativeStatus=m_nativeUpdate(json.constData());if(m_nativeStatus!=0&&m_nativeStatus!=previous)qWarning()<<"SMTC update failed, HRESULT:"<<Qt::hex<<quint32(m_nativeStatus);}}
#include "SettingsBackend.inc"
void Bridge::exportSettings(QUrl url){
 QJsonObject preferences,appearance,presets;for(const auto &key:m_settings.allKeys()){
  if(key.startsWith("preferences/"))preferences[key.mid(12)]=QJsonValue::fromVariant(m_settings.value(key));
  else if(key.startsWith("appearance/"))appearance[key.mid(11)]=QJsonValue::fromVariant(m_settings.value(key));
  else if(key.startsWith("lyricPresets/custom/"))presets[key.mid(QStringLiteral("lyricPresets/custom/").size())]=QJsonValue::fromVariant(m_settings.value(key));
 }
 QSaveFile file(url.toLocalFile());if(file.open(QIODevice::WriteOnly)){file.write(QJsonDocument(QJsonObject{{"schemaVersion",2},{"preferences",preferences},{"appearance",appearance},{"lyricPresets",presets}}).toJson());file.commit();}
}
void Bridge::importSettings(QUrl url){
 QFile file(url.toLocalFile());if(!file.open(QIODevice::ReadOnly)||file.size()>1024*1024)return;
 const auto doc=QJsonDocument::fromJson(file.readAll()).object();const int version=doc.value("schemaVersion").toInt();
 if(version!=1&&version!=2){emit notification(tr("Unsupported settings file"));return;}
 const auto preferences=doc.value("preferences").toObject();for(auto i=preferences.begin();i!=preferences.end();++i)if(i.value().isBool()||i.value().isDouble()||i.value().isString())setSetting(i.key(),i.value().toVariant());
 const auto appearance=doc.value("appearance").toObject();
 if(version==2){applyAppearancePreset(appearance.value("preset").toString("balanced"));for(auto i=appearance.begin();i!=appearance.end();++i){const auto parts=i.key().split('/');if(parts.size()!=2)continue;if(parts[1]=="inherit")setAppearanceInherited(parts[0],i.value().toBool(true));else setAppearance(parts[0],parts[1],i.value().toVariant());}}
 const auto presets=doc.value("lyricPresets").toObject();int imported=0;
 for(auto it=presets.begin();it!=presets.end()&&imported<50;++it){const auto row=it.value().toObject();const auto name=row.value("name").toString();if(!validLyricPresetName(name)||builtinLyricPreset(name))continue;QVariantMap values;const auto input=row.value("values").toObject();for(auto value=input.begin();value!=input.end();++value){QVariant normalized;if(SettingsSchema::descriptor(value.key()).value("lyricStyle").toBool()&&SettingsSchema::normalize(value.key(),value.value().toVariant(),&normalized))values[value.key()]=normalized;}if(!values.isEmpty()){m_settings.setValue(lyricPresetKey(name),QVariantMap{{"name",name},{"values",values}});++imported;}}
 emit settingsChanged();
}
void Bridge::forgetDevice(QString url){if(!url.contains("://"))url="wss://"+url;QUrl address(url);if(address.port()<0)address.setPort(18123);url=address.toString();if(url==m_url)disconnectDevice();m_settings.remove(deviceKey(url));for(int i=m_devices.size()-1;i>=0;--i)if(m_devices[i].toMap().value("url")==url)m_devices.removeAt(i);if(m_settings.value("connection/url")==url)m_settings.remove("connection/url");emit changed();}
void Bridge::setUiVisible(bool visible){m_uiVisible=visible;if(visible)m_progress.start(250);else m_progress.stop();updateLyricClock();}
void Bridge::refreshDevices(){QString redirectUrl;m_devices.clear();QHash<QString,int> byUrl;for(const auto &key:m_settings.allKeys())if(key.startsWith("devices/")&&key.endsWith("/url")){auto prefix=key.chopped(4);const auto url=m_settings.value(key).toString();byUrl[url]=m_devices.size();m_devices<<QVariantMap{{"name",m_settings.value(prefix+"/name",QUrl(url).host())},{"url",url},{"paired",m_settings.contains(prefix+"/credential")},{"deviceId",m_settings.value(prefix+"/deviceId")}};}
 for(auto it=m_services.begin();it!=m_services.end();++it)if(m_hosts.contains(it.value().first)){const auto url="wss://"+m_hosts[it.value().first]+":"+QString::number(it.value().second);auto info=m_discoveryInfo.value(it.key());auto id=info.value("deviceId").toString().toLower();auto name=info.value("name",it.key().section("._droidmedialink",0,0)).toString().left(100);if(!id.isEmpty()){auto key="devices/"+id;if(m_settings.contains(key+"/credential")){const auto oldUrl=m_settings.value(key+"/url").toString();m_settings.setValue(key+"/url",url);if(m_url==oldUrl&&!m_online&&oldUrl!=url)redirectUrl=url;if(m_settings.value("connection/url").toString()==oldUrl)m_settings.setValue("connection/url",url);for(int i=0;i<m_devices.size();++i)if(m_devices[i].toMap().value("deviceId").toString()==id){byUrl.remove(oldUrl);byUrl[url]=i;break;}}}QVariantMap row{{"name",name},{"url",url},{"deviceId",id},{"paired",m_settings.contains("devices/"+id+"/credential")}};if(byUrl.contains(url)){row["paired"]=row.value("paired").toBool()||m_devices[byUrl[url]].toMap().value("paired").toBool();m_devices[byUrl[url]]=row;}else{byUrl[url]=m_devices.size();m_devices<<row;}}emit changed();if(!redirectUrl.isEmpty())QTimer::singleShot(0,this,[this,redirectUrl]{if(!m_online&&!m_manual)connectDevice(redirectUrl);});}
void Bridge::discover(){
 m_hosts.clear();m_services.clear();m_discoveryInfo.clear();refreshDevices();
 // The default multicast route can point at a VPN instead of the phone's LAN.
 // Rebind to discard memberships from disconnected or replaced interfaces.
 m_mdns.close();
 if(!m_mdns.bind(QHostAddress::AnyIPv4,5353,QUdpSocket::ShareAddress|QUdpSocket::ReuseAddressHint))return;
 m_mdns.setSocketOption(QAbstractSocket::MulticastTtlOption,255);
 const QHostAddress group("224.0.0.251");
 QByteArray query=QByteArray::fromHex("000000000001000000000000");
 for(const auto &part:QByteArray("_droidmedialink._tcp.local").split('.')){query.append(char(part.size()));query.append(part);}
 query.append(QByteArray::fromHex("00000c0001"));
 for(const auto &iface:QNetworkInterface::allInterfaces()){
  const auto flags=iface.flags();
  if(!flags.testFlag(QNetworkInterface::IsUp)||!flags.testFlag(QNetworkInterface::IsRunning)||!flags.testFlag(QNetworkInterface::CanMulticast)||flags.testFlag(QNetworkInterface::IsLoopBack))continue;
  const auto addresses=iface.addressEntries();
  if(std::none_of(addresses.cbegin(),addresses.cend(),[](const auto &entry){return entry.ip().protocol()==QAbstractSocket::IPv4Protocol;}))continue;
  if(!m_mdns.joinMulticastGroup(group,iface))continue;
  m_mdns.setMulticastInterface(iface);
  m_mdns.writeDatagram(query,group,5353);
 }
}
QVariantList Bridge::parseLyrics(const QString &text){return LyricTimeline::parse(text);}
void Bridge::importLyrics(QUrl url){
 QFile file(url.toLocalFile());if(!file.open(QIODevice::ReadOnly)||file.size()>2*1024*1024){emit notification(tr("Unable to open lyrics"));return;}
 const auto text=LyricProviders::decodeLocal(file.readAll());auto rows=parseLyrics(text);if(rows.isEmpty()){emit notification(tr("Unable to open lyrics"));return;}
 cancelLyricRequest();m_lyricKey=lyricKey();m_lyricProvider="local";m_lyricMatchScore=100;
 if(!title().isEmpty()){const auto identity=lyricIdentity();QSaveFile output(cachePath()+"/"+identity+".manual.lrc");if(output.open(QIODevice::WriteOnly)&&output.write(text.toUtf8())>=0&&output.commit())m_settings.setValue("lyrics/bindings/"+identity,QString("local"));}
 setLyrics(rows,"local");
}
LyricTrack Bridge::lyricTrack()const{return {title(),artist(),album(),m_media.value("packageName").toString(),duration()};}
QVariantList Bridge::lyricProviders()const{auto rows=m_lyricProviders?m_lyricProviders->descriptors():QVariantList();for(auto &value:rows){auto row=value.toMap();row["enabled"]=setting(row.value("enabledKey").toString(),true).toBool();value=row;}return rows;}
QStringList Bridge::lyricProviderOrder()const{
 QStringList order;for(auto id:setting("lyrics/providerPriority","netease,qq,kugou,lrclib").toString().split(',')){id=id.trimmed();if(LyricProviders::ids().contains(id)&&!order.contains(id))order<<id;}
 for(const auto &id:LyricProviders::ids())if(!order.contains(id))order<<id;
 auto preferred=setting("lyrics/preferredProvider","auto").toString();
 if(preferred=="auto"&&setting("lyrics/preferCurrentApp",true).toBool()){const auto package=m_media.value("packageName").toString().toLower();if(package.contains("netease"))preferred="netease";else if(package.contains("tencent")||package.contains("qqmusic"))preferred="qq";else if(package.contains("kugou"))preferred="kugou";}
 if(order.contains(preferred)){order.removeAll(preferred);order.prepend(preferred);}
 order.erase(std::remove_if(order.begin(),order.end(),[this](const auto &id){return !setting("lyrics/providers/"+id+"/enabled",true).toBool();}),order.end());return order;
}
void Bridge::fetchLyrics(){
 cancelLyricRequest();const auto key=lyricKey();m_lyricKey=key;m_lyricProvider.clear();m_lyricMatchScore=0;setLyrics({},{});if(key.isEmpty())return;
 const auto identity=lyricIdentity();const auto binding=m_settings.value("lyrics/bindings/"+identity).toString();
 const auto jsonName=!binding.isEmpty()?identity+".manual.json":key+".lyrics.json";
 QFile jsonCache(cachePath()+"/"+jsonName);if(jsonCache.open(QIODevice::ReadOnly)&&jsonCache.size()<=2*1024*1024){const auto doc=QJsonDocument::fromJson(jsonCache.readAll()).object();const auto rows=LyricTimeline::validated(doc.value("rows").toArray().toVariantList());const bool identityMatches=!doc.contains("trackIdentity")||doc.value("trackIdentity").toString()==identity;const bool durationMatches=!binding.isEmpty()||!doc.contains("durationMs")||qAbs(doc.value("durationMs").toInteger()-duration())<=1500;if(!rows.isEmpty()&&identityMatches&&durationMatches){m_lyricProvider=doc.value("provider").toString();m_lyricMatchScore=doc.value("score").toDouble();setLyrics(rows,binding.isEmpty()?"cache":"manual");return;}}
 const auto cacheName=!binding.isEmpty()?identity+".manual.lrc":key+".lrc";
 QFile cached(cachePath()+"/"+cacheName);if(cached.open(QIODevice::ReadOnly)&&cached.size()<=2*1024*1024){auto rows=parseLyrics(QString::fromUtf8(cached.readAll()));if(!rows.isEmpty()){m_lyricProvider=binding=="local"?"local":"lrclib";m_lyricMatchScore=100;setLyrics(rows,binding.isEmpty()?"cache":"manual");return;}}
 if(m_embeddedKey==key&&!m_embeddedLyrics.isEmpty()&&binding.isEmpty()){auto rows=parseLyrics(m_embeddedLyrics);if(!rows.isEmpty()){m_lyricProvider="phone";setLyrics(rows,"phone");return;}}
 if(!setting("lyrics/online",true).toBool()||!setting("lyrics/autoMatch",true).toBool())return;
 if(!binding.isEmpty()&&binding!="local"){
  const auto json=QJsonDocument::fromJson(binding.toUtf8()).object();const auto provider=json.isEmpty()?QString("lrclib"):json.value("provider").toString();const auto id=json.isEmpty()?binding:json.value("sourceId").toString();
  if(LyricProviders::ids().contains(provider)){bindLyricCandidate(provider,id);return;}
 }
 m_lyricsLoading=true;emit lyricsChanged();tryLyricProviders(lyricProviderOrder(),0,key,m_lyricGeneration);
}
void Bridge::tryLyricProviders(QStringList providers,int index,QString key,quint64 generation){
 if(generation!=m_lyricGeneration||key!=lyricKey())return;
 if(index>=providers.size()){m_lyricsLoading=false;emit lyricsChanged();return;}
 const auto provider=providers[index];const auto track=lyricTrack();
 const auto strategy=setting("lyrics/matchStrategy","default").toString();
 const auto query=(strategy=="default"?LyricProviders::baseTitle(track.title):track.title)+" "+track.artist;
 m_lyricProviders->search(provider,query,[this,providers,index,key,generation,track,strategy](QList<LyricCandidate> candidates,QString){
  if(generation!=m_lyricGeneration||key!=lyricKey())return;const bool strict=strategy=="strict";
  for(auto &candidate:candidates)candidate.score=strategy=="default"?LyricProviders::defaultMatchScore(track,candidate):LyricProviders::matchScore(track,candidate,strict);
  std::stable_sort(candidates.begin(),candidates.end(),[](const auto&a,const auto&b){return a.score>b.score;});
  for(const auto &candidate:candidates){bool exists=false;for(const auto &value:m_lyricResults){const auto row=value.toMap();if(row.value("provider")==candidate.provider&&row.value("sourceId")==candidate.id){exists=true;break;}}if(!exists&&m_lyricResults.size()<48)m_lyricResults<<candidate.toVariant();}emit lyricResultsChanged();
  candidates.erase(std::remove_if(candidates.begin(),candidates.end(),[strict](const auto &candidate){return candidate.score<(strict?90.:80.);}),candidates.end());if(candidates.size()>3)candidates=candidates.mid(0,3);
  tryLyricCandidates(candidates,0,providers,index+1,key,generation);
 });
}
void Bridge::tryLyricCandidates(QList<LyricCandidate> candidates,int index,QStringList providers,int nextProvider,QString key,quint64 generation){
 if(generation!=m_lyricGeneration||key!=lyricKey())return;if(index>=candidates.size()){tryLyricProviders(providers,nextProvider,key,generation);return;}
 m_lyricProviders->fetch(candidates[index],[this,candidates,index,providers,nextProvider,key,generation](LyricDocument document,QString){if(generation!=m_lyricGeneration||key!=lyricKey())return;if(document.rows.isEmpty()){tryLyricCandidates(candidates,index+1,providers,nextProvider,key,generation);return;}acceptLyricDocument(document,false);if(setting("lyrics/crossSourceTranslation",false).toBool())supplementLyricTranslation(providers,nextProvider,key,generation);});
}
void Bridge::acceptLyricDocument(LyricDocument document,bool manual){
 if(document.rows.isEmpty())return;m_lyricProvider=document.provider;m_lyricMatchScore=document.score;
 const auto identity=lyricIdentity();const auto name=manual?identity+".manual.json":lyricKey()+".lyrics.json";
 QSaveFile output(cachePath()+"/"+name);if(output.open(QIODevice::WriteOnly)){output.write(QJsonDocument(QJsonObject{{"schemaVersion",2},{"trackIdentity",identity},{"durationMs",duration()},{"provider",document.provider},{"sourceId",document.sourceId},{"score",document.score},{"rows",QJsonArray::fromVariantList(document.rows)}}).toJson(QJsonDocument::Compact));output.commit();}
 if(manual)m_settings.setValue("lyrics/bindings/"+identity,QString::fromUtf8(QJsonDocument(QJsonObject{{"provider",document.provider},{"sourceId",document.sourceId}}).toJson(QJsonDocument::Compact)));
 setLyrics(document.rows,manual?"manual":document.provider);
}
void Bridge::supplementLyricTranslation(QStringList providers,int index,QString key,quint64 generation){
 if(generation!=m_lyricGeneration||key!=lyricKey()||index>=providers.size())return;
 for(const auto &row:m_lyrics)if(!row.toMap().value("translation").toString().isEmpty())return;
 const auto track=lyricTrack();m_lyricProviders->search(providers[index],title()+" "+artist(),[this,providers,index,key,generation,track](QList<LyricCandidate> candidates,QString){
  if(generation!=m_lyricGeneration||key!=lyricKey())return;
  auto found=std::find_if(candidates.begin(),candidates.end(),[&track](const auto &candidate){return LyricProviders::matchScore(track,candidate,true)>=98.;});
  if(found==candidates.end()){supplementLyricTranslation(providers,index+1,key,generation);return;}
  m_lyricProviders->fetch(*found,[this,providers,index,key,generation](LyricDocument document,QString){if(generation!=m_lyricGeneration||key!=lyricKey())return;auto rows=LyricTimeline::mergeDocumentAuxiliary(m_lyrics,document.rows,250,true);if(rows==m_lyrics){supplementLyricTranslation(providers,index+1,key,generation);return;}const auto source=m_lyricSource;setLyrics(rows,source);
    const auto name=lyricKey()+".lyrics.json";QFile original(cachePath()+"/"+name);if(original.open(QIODevice::ReadOnly)){auto cached=QJsonDocument::fromJson(original.readAll()).object();original.close();cached["rows"]=QJsonArray::fromVariantList(rows);QSaveFile output(cachePath()+"/"+name);if(output.open(QIODevice::WriteOnly)){output.write(QJsonDocument(cached).toJson(QJsonDocument::Compact));output.commit();}}
});
 });
}
void Bridge::requestLyrics(QUrl url,QString key,quint64 generation,bool manual,bool searchFallback){
 if(QDateTime::currentMSecsSinceEpoch()<m_lyricRetryAfter){m_lyricsLoading=false;emit lyricsChanged();return;}
 QNetworkRequest request(url);request.setRawHeader("User-Agent","DroidMediaLink/0.1.0 (https://github.com/XHZ-Steve/DroidMusicLink)");request.setTransferTimeout(10000);auto reply=m_network.get(request);m_lyricReply=reply;
 connect(reply,&QNetworkReply::downloadProgress,reply,[reply](qint64 received,qint64){if(received>2*1024*1024)reply->abort();});
 connect(reply,&QNetworkReply::finished,this,[this,reply,key,generation,manual,searchFallback]{
  const bool current=key==lyricKey()&&key==m_lyricKey&&generation==m_lyricGeneration;
  if(m_lyricReply==reply)m_lyricReply=nullptr;
  if(!current){reply->deleteLater();return;}
  const auto bytes=reply->isOpen()?reply->read(2*1024*1024):QByteArray();
  const auto http=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  if(http==429||http==503){bool ok=false;auto seconds=reply->rawHeader("Retry-After").toLongLong(&ok);if(!ok){const auto date=QDateTime::fromString(QString::fromLatin1(reply->rawHeader("Retry-After")),Qt::RFC2822Date);seconds=date.isValid()?QDateTime::currentDateTimeUtc().secsTo(date):30;}m_lyricRetryAfter=QDateTime::currentMSecsSinceEpoch()+qBound(qint64(1),qint64(seconds),qint64(86400))*1000+int(QRandomGenerator::global()->bounded(1000));m_lyricsLoading=false;emit lyricsChanged();reply->deleteLater();return;}
  QJsonObject record;const auto document=QJsonDocument::fromJson(bytes);
  if(reply->error()==QNetworkReply::NoError){
   if(searchFallback){double best=-1e9;for(const auto &item:document.array()){const auto candidate=item.toObject();const auto normalize=[](QString s){return s.normalized(QString::NormalizationForm_KC).simplified().toCaseFolded();};if(normalize(candidate.value("trackName").toString())!=normalize(title())||normalize(candidate.value("artistName").toString())!=normalize(artist()))continue;const auto difference=qAbs(candidate.value("duration").toDouble()*1000-duration());if(duration()>0&&difference>3000)continue;const double score=(candidate.value("syncedLyrics").toString().isEmpty()?0:100)+(normalize(candidate.value("albumName").toString())==normalize(album())?10:0)-difference/1000;if(score>best){best=score;record=candidate;}}}
   else record=document.object();
  }
  auto text=record.value("syncedLyrics").toString();if(text.isEmpty())text=record.value("plainLyrics").toString();auto rows=parseLyrics(text);
  if(rows.isEmpty()&&!text.trimmed().isEmpty()&&!text.trimmed().startsWith('<')&&!text.contains(QRegularExpression("\\[\\d+:"))){int n=0;for(const auto &line:text.split('\n'))if(!line.trimmed().isEmpty())rows<<QVariantMap{{"time",0},{"end",9000000000000LL},{"text",line.trimmed()},{"untimed",true}};}
  if(!rows.isEmpty()){
   QSaveFile cache(cachePath()+"/"+(manual?lyricIdentity()+".manual.lrc":key+".lrc"));if(cache.open(QIODevice::WriteOnly)){cache.write(text.toUtf8());cache.commit();}if(manual){const auto id=reply->url().path().section('/',-1).toLongLong();if(id>0)m_settings.setValue("lyrics/bindings/"+lyricIdentity(),id);}setLyrics(rows,manual?"manual":"lrclib");
  }else if(!manual&&!searchFallback&&!record.value("instrumental").toBool()&&(http==404||reply->error()==QNetworkReply::NoError)){QUrl fallback("https://lrclib.net/api/search");QUrlQuery query;query.addQueryItem("track_name",title());query.addQueryItem("artist_name",artist());fallback.setQuery(query);QTimer::singleShot(250,this,[this,fallback,key,generation]{if(generation==m_lyricGeneration&&key==lyricKey())requestLyrics(fallback,key,generation,false,true);});reply->deleteLater();return;}
  else {m_lyricsLoading=false;emit lyricsChanged();}
  reply->deleteLater();
 });
}
void Bridge::clearLyricsCache(){cancelLyricRequest();QDir dir(cachePath());for(const auto &file:dir.entryList({"*.lrc","*.lyrics.json"},QDir::Files))if(!file.endsWith(".manual.lrc"))dir.remove(file);m_lyricKey.clear();fetchLyrics();}
void Bridge::checkUpdates(){QNetworkRequest request(QUrl("https://api.github.com/repos/XHZ-Steve/DroidMusicLink/releases/latest"));request.setRawHeader("User-Agent","DroidMediaLink/0.1.0");request.setTransferTimeout(10000);auto reply=m_network.get(request);connect(reply,&QNetworkReply::downloadProgress,reply,[reply](qint64 received,qint64){if(received>2*1024*1024)reply->abort();});connect(reply,&QNetworkReply::finished,this,[this,reply]{if(reply->error()==QNetworkReply::NoError){auto obj=QJsonDocument::fromJson(reply->read(1024*1024)).object();auto tag=obj.value("tag_name").toString();if(!tag.isEmpty()&&tag!="v"+QCoreApplication::applicationVersion())emit updateAvailable(tag,obj.value("html_url").toString());else emit notification(tr("You are up to date"));}else emit notification(tr("No release available or network unavailable"));reply->deleteLater();});}





QString Bridge::lyricIdentity()const {if(title().trimmed().isEmpty())return {};auto normalized=[](QString s){return s.normalized(QString::NormalizationForm_KC).simplified().toCaseFolded();};return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(QJsonArray{normalized(title()),normalized(artist()),normalized(album())}).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex());}
QString Bridge::lyricKey()const {if(title().trimmed().isEmpty())return {};return QString::fromLatin1(QCryptographicHash::hash((lyricIdentity()+"|"+QString::number(qRound64(duration()/1000.))).toUtf8(),QCryptographicHash::Sha256).toHex());}
void Bridge::applyTheme(){
 const auto source=setting("theme/source","auto").toString();QString brand=setting("theme/brand",m_media.value("packageName").toString()).toString().toLower();QColor color(m_artwork.isEmpty()?"#9bc3c6":m_artworkAccent);
 auto brandColor=[&]()->QColor{if(brand.contains("spotify"))return QColor("#9ac8ab");if(brand.contains("netease"))return QColor("#c69b9e");if(brand.contains("qqmusic")||brand.contains("tencent"))return QColor("#9bc9bc");if(brand.contains("kugou"))return QColor("#9db9d2");if(brand.contains("youtube"))return QColor("#cba6a1");if(brand.contains("apple"))return QColor("#c6a4ba");return QColor("#9bc3c6");};
 if(source=="system")color=QGuiApplication::palette().color(QPalette::Highlight);else if(source=="brand"||(source=="auto"&&m_artwork.isEmpty()))color=brandColor();
 color.setHslF(color.hslHueF(),qBound(.12,color.hslSaturationF(),.35),.68);m_accent=color.name();
}
void Bridge::searchLyrics(QString query){
 cancelLyricRequest();emit lyricsChanged();const auto generation=++m_searchGeneration;if(m_lyricSearchReply)m_lyricSearchReply->abort();m_lyricResults.clear();emit lyricResultsChanged();if(query.trimmed().size()<2)return;searchNextLyricProvider(query.trimmed().left(240),lyricProviderOrder(),0,lyricKey(),generation);
}
void Bridge::searchNextLyricProvider(QString query,QStringList providers,int index,QString key,quint64 generation){
 if(generation!=m_searchGeneration||key!=lyricKey()||index>=providers.size())return;const auto track=lyricTrack();
 m_lyricProviders->search(providers[index],query,[this,query,providers,index,key,generation,track](QList<LyricCandidate> candidates,QString){if(generation!=m_searchGeneration||key!=lyricKey())return;for(auto &candidate:candidates){const auto strategy=setting("lyrics/matchStrategy","default").toString();candidate.score=strategy=="default"?LyricProviders::defaultMatchScore(track,candidate):LyricProviders::matchScore(track,candidate,strategy=="strict");if(m_lyricResults.size()<48)m_lyricResults<<candidate.toVariant();}emit lyricResultsChanged();searchNextLyricProvider(query,providers,index+1,key,generation);});
}
void Bridge::bindLyrics(qint64 id){if(id>0)bindLyricCandidate("lrclib",QString::number(id));}
void Bridge::bindLyricCandidate(QString provider,QString sourceId){
 if(!LyricProviders::ids().contains(provider)||sourceId.isEmpty()||sourceId.size()>128||title().isEmpty())return;cancelLyricRequest();m_lyricKey=lyricKey();const auto key=m_lyricKey;const auto generation=m_lyricGeneration;
 LyricCandidate candidate{provider,sourceId,title(),artist(),album(),duration(),100,{}};for(const auto &value:m_lyricResults){const auto row=value.toMap();if(row.value("provider")==provider&&row.value("sourceId")==sourceId){candidate=LyricCandidate::fromVariant(row);candidate.score=100;break;}}
 m_lyricsLoading=true;emit lyricsChanged();m_lyricProviders->fetch(candidate,[this,key,generation](LyricDocument document,QString error){if(generation!=m_lyricGeneration||key!=lyricKey())return;if(!document.rows.isEmpty())acceptLyricDocument(document,true);else {m_lyricsLoading=false;emit lyricsChanged();emit notification(tr("Lyrics unavailable"));}});
}
void Bridge::clearLyricBinding(){const auto identity=lyricIdentity();m_settings.remove("lyrics/bindings/"+identity);QFile::remove(cachePath()+"/"+identity+".manual.lrc");QFile::remove(cachePath()+"/"+identity+".manual.json");fetchLyrics();}
void Bridge::testLyricProvider(QString provider){if(m_lyricProviders)m_lyricProviders->test(provider);}
