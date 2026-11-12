#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariant>
#include <functional>

struct LyricTrack {QString title,artist,album,packageName;qint64 duration=0;};
struct LyricCandidate {
 QString provider,id,title,artist,album;qint64 duration=0;double score=0;QVariantMap extra;
 QVariantMap toVariant()const;
 static LyricCandidate fromVariant(QVariantMap row);
};
struct LyricDocument {QVariantList rows;QString provider,sourceId,format;double score=0;};

// Anonymous, isolated provider adapters. Non-public routes are explicitly
// experimental; no cookies, private API credentials, or music files are used.
class LyricProviders:public QObject {
public:
 using SearchDone=std::function<void(QList<LyricCandidate>,QString)>;
 using FetchDone=std::function<void(LyricDocument,QString)>;
 explicit LyricProviders(QNetworkAccessManager *network,QObject *parent=nullptr);
 ~LyricProviders()override;
 void cancel();
 void search(QString provider,QString query,SearchDone done);
 void fetch(LyricCandidate candidate,FetchDone done);
 void test(QString provider);
 QVariantList descriptors()const;
 std::function<void()> changed;
 static QStringList ids();
 static double matchScore(const LyricTrack &track,const LyricCandidate &candidate,bool strict=true);
 static QString decodeLocal(QByteArray bytes);
private:
 QNetworkAccessManager *m_network;quint64 m_epoch=0;
 QList<QPointer<QNetworkReply>> m_replies;
 QHash<QString,QVariantMap> m_status;
 QHash<QString,qint64> m_nextRequest;
 using JsonDone=std::function<void(QJsonObject,QJsonArray,QString)>;
 void get(QString provider,QUrl url,JsonDone done);
 void setStatus(QString provider,QString status,QString error={});
 void parseDocument(LyricCandidate candidate,QString text,QString translation,QString romanization,QString format,FetchDone done,QByteArray compressed={});
 void fetchKugou(LyricCandidate candidate,FetchDone done);
};
