#pragma once
#include <QObject>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QNetworkReply>
#include <QImage>
#include "CoverPalette.h"
#include <QTimer>
#include <functional>
#include <atomic>
#include <memory>
struct CoverArtTrack {QString title,artist,album; qint64 duration=0;QString uri; QString key()const;};
struct CoverArtResult {QString path,release,source;QSize pixels;QByteArray digest;bool cached=false;QString thumbnailPath;CoverPalette palette;};
class CoverArtProvider:public QObject {
public:using Done=std::function<void(QByteArray,QString,bool)>;using QObject::QObject;
 virtual QString id()const=0;virtual void request(const CoverArtTrack &,int,Done)=0;virtual void cancel()=0;
};
class MusicBrainzCoverArtProvider final:public CoverArtProvider {
 QNetworkAccessManager ownedNetwork;QNetworkAccessManager *network=&ownedNetwork;QPointer<QNetworkReply> reply;QTimer delay; qint64 nextRequest=0;quint64 generation=0;
 void get(QUrl,bool,std::function<void(QByteArray)>);
public:explicit MusicBrainzCoverArtProvider(QObject *parent=nullptr,QNetworkAccessManager *transport=nullptr):CoverArtProvider(parent){if(transport)network=transport;}
 ~MusicBrainzCoverArtProvider()override{cancel();}
 std::function<bool(QString,int)> cachedImage;bool bypassCache=false;
 QString id()const override{return "musicbrainz";}
 void request(const CoverArtTrack &,int,Done)override;void cancel()override;
 static QString selectRelease(const CoverArtTrack &,const QJsonObject &);
};
class CoverArtService:public QObject {
 MusicBrainzCoverArtProvider provider;QString directory,activePath,activeThumbnailPath;quint64 generation=0;std::shared_ptr<std::atomic<quint64>> epoch=std::make_shared<std::atomic<quint64>>(0);qint64 budget=256*1024*1024;QHash<QString,qint64> failures;
 void decode(QByteArray,const QString &,QString,int,bool,std::function<void(CoverArtResult)>);
 void trim();
public:explicit CoverArtService(QString,QObject *parent=nullptr,QNetworkAccessManager *transport=nullptr);
 ~CoverArtService()override{cancel();}
 void cancel();void clear();void setBudget(int mb);qint64 bytes()const;
 void request(CoverArtTrack,int,bool,std::function<void(CoverArtResult)>);
};

// A published MediaSession URI is original artwork, not a metadata search.
class PublishedCoverArtProvider final:public CoverArtProvider {
 QNetworkAccessManager owned;QNetworkAccessManager *network=&owned;QPointer<QNetworkReply> reply;quint64 generation=0;
public:explicit PublishedCoverArtProvider(QObject *parent=nullptr,QNetworkAccessManager *transport=nullptr):CoverArtProvider(parent){if(transport)network=transport;}
 ~PublishedCoverArtProvider()override{cancel();}
 QString id()const override{return "phone-uri";}
 static bool trusted(QUrl);
 void request(const CoverArtTrack &,int,Done)override;void cancel()override;
};
