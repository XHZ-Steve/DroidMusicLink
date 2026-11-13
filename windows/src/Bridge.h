#pragma once
#include <QObject>
#include <QVariant>
#include <QSettings>
#include <QWebSocket>
#include <QNetworkAccessManager>
#include <QElapsedTimer>
#include <QTimer>
#include <QUdpSocket>
#include <QJsonObject>
#include <QLibrary>
#include <QPointer>
#include <QSet>
#include "CoverPalette.h"
#include "CoverArtProvider.h"
#include "LyricProviders.h"
class Bridge : public QObject {
 Q_OBJECT
 Q_PROPERTY(QString pairingCode READ pairingCode NOTIFY changed)
 Q_PROPERTY(bool pairingPending READ pairingPending NOTIFY changed)
 Q_PROPERTY(QString status READ status NOTIFY changed)
 Q_PROPERTY(bool online READ online NOTIFY changed)
 Q_PROPERTY(QString title READ title NOTIFY changed)
 Q_PROPERTY(QString artist READ artist NOTIFY changed)
 Q_PROPERTY(QString album READ album NOTIFY changed)
 Q_PROPERTY(QString appName READ appName NOTIFY changed)
 Q_PROPERTY(QString artwork READ artwork NOTIFY changed)
 Q_PROPERTY(qint64 duration READ duration NOTIFY changed)
 Q_PROPERTY(qint64 position READ position NOTIFY progressChanged)
 Q_PROPERTY(bool playing READ playing NOTIFY changed)
 Q_PROPERTY(QStringList commands READ commands NOTIFY changed)
 Q_PROPERTY(bool favorited READ favorited NOTIFY changed)
 Q_PROPERTY(bool favoriteKnown READ favoriteKnown NOTIFY changed)
 Q_PROPERTY(double volume READ volume NOTIFY changed)
 Q_PROPERTY(bool volumeKnown READ volumeKnown NOTIFY changed)
 Q_PROPERTY(QVariantList lyrics READ lyrics NOTIFY lyricsChanged)
 Q_PROPERTY(int currentLine READ currentLine NOTIFY lyricLineChanged)
 Q_PROPERTY(qint64 lyricPosition READ lyricPosition NOTIFY lyricProgressChanged)
 Q_PROPERTY(bool lyricsLoading READ lyricsLoading NOTIFY lyricsChanged)
 Q_PROPERTY(QString lyricSource READ lyricSource NOTIFY lyricsChanged)
 Q_PROPERTY(QString lyricProvider READ lyricProvider NOTIFY lyricsChanged)
 Q_PROPERTY(double lyricMatchScore READ lyricMatchScore NOTIFY lyricsChanged)
 Q_PROPERTY(QVariantList lyricProviders READ lyricProviders NOTIFY lyricProvidersChanged)
 Q_PROPERTY(QVariantList lyricPresets READ lyricPresets NOTIFY settingsChanged)
 Q_PROPERTY(QVariantList devices READ devices NOTIFY changed)
 Q_PROPERTY(QVariantList settingsCatalog READ settingsCatalog CONSTANT)
 Q_PROPERTY(QVariantList appearanceDescriptors READ appearanceDescriptors CONSTANT)
 Q_PROPERTY(QString ambientAccent READ ambientAccent NOTIFY changed)
 Q_PROPERTY(QString activeDeviceUrl READ activeDeviceUrl NOTIFY changed)
 Q_PROPERTY(QString accent READ accent NOTIFY changed)
 Q_PROPERTY(QString ambientPrimary READ ambientPrimary NOTIFY changed)
 Q_PROPERTY(QString ambientSecondary READ ambientSecondary NOTIFY changed)
 Q_PROPERTY(QString ambientDeep READ ambientDeep NOTIFY changed)
 Q_PROPERTY(QVariantList lyricResults READ lyricResults NOTIFY lyricResultsChanged)
public:
 Q_INVOKABLE void refreshCoverArt();
 Q_INVOKABLE void clearCoverArtCache();
 void setDragging(bool dragging);
 explicit Bridge(QObject *parent=nullptr);
 ~Bridge() override;
 QString status()const{return m_status;} bool online()const{return m_online;}
 QString title()const{return m_media.value("title").toString();}
 QString artist()const{return m_media.value("artist").toString();}
 QString album()const{return m_media.value("album").toString();}
 QString appName()const{return m_media.value("appName").toString();}
 QString artwork()const{return m_artwork;} QString accent()const{return m_accent;}
 QString ambientPrimary()const{return m_coverPalette.primary.name();}
 QString ambientSecondary()const{return m_coverPalette.secondary.name();}
 QString ambientDeep()const{return m_coverPalette.deep.name();}
 qint64 duration()const{return qBound<qint64>(0,m_media.value("durationMs").toInteger(),9000000000000LL);}
 qint64 position()const;
 bool playing()const{return m_online&&m_media.value("state").toString()=="playing";}
 bool favorited()const{return m_media.value("favorite").toBool();}
 bool favoriteKnown()const{return m_media.value("favoriteKnown").toBool();}
 QStringList commands()const; QVariantList lyrics()const{return m_lyrics;} int currentLine()const;
 qint64 lyricPosition()const{return position()+setting("lyrics/offset",0).toInt();}
 bool lyricsLoading()const{return m_lyricsLoading;}
 QString lyricSource()const{return m_lyricSource;}
 QString lyricProvider()const{return m_lyricProvider;}
 double lyricMatchScore()const{return m_lyricMatchScore;}
 QVariantList lyricProviders()const;
 QVariantList lyricPresets()const;
 Q_INVOKABLE void saveLyricPreset(QString name);
 Q_INVOKABLE void applyLyricPreset(QString name);
 Q_INVOKABLE void renameLyricPreset(QString oldName,QString newName);
 Q_INVOKABLE void deleteLyricPreset(QString name);
 Q_INVOKABLE void resetLyricSettings(QString scope);
 bool volumeKnown()const{return m_media.value("maxVolume").toDouble()>0;}
 double volume()const{const auto maximum=m_media.value("maxVolume").toDouble();return maximum>0?qBound(0.,m_media.value("volume").toDouble()/maximum,1.):0.;}
 QVariantList devices()const{return m_devices;}
 QVariantList settingsCatalog()const;
 QVariantList appearanceDescriptors()const;
 QString ambientAccent()const{auto color=m_coverPalette.primary;if(color.hslHueF()>=0)color.setHslF(color.hslHueF(),qMin(.85,color.hslSaturationF()+.12),qMin(.8,color.lightnessF()+.15));return color.name();}
 Q_INVOKABLE QVariantMap appearanceConfig(QString scope)const;
 Q_INVOKABLE bool appearanceInherited(QString scope)const;
 Q_INVOKABLE void setAppearance(QString scope,QString key,QVariant value);
 Q_INVOKABLE void setAppearanceInherited(QString scope,bool inherited);
 Q_INVOKABLE void applyAppearancePreset(QString preset);
 Q_INVOKABLE void resetAppearance(QString scope);
 Q_INVOKABLE void resetSettingsGroup(int group,int section);
 Q_INVOKABLE void exportDiagnostics(QUrl url);
 QString activeDeviceUrl()const{return m_url;}
 QVariantList lyricResults()const{return m_lyricResults;}
 QString pairingCode()const{return m_code;} bool pairingPending()const{return m_pairing;}
 Q_INVOKABLE void connectDevice(QString url,QString fingerprint={},QString code={});
 Q_INVOKABLE void cancelPairing();
 static QString sasCode(QByteArray fingerprint,QByteArray clientNonce,QByteArray serverNonce,QString clientId);
 Q_INVOKABLE void disconnectDevice();
 Q_INVOKABLE void command(QString name,double value=0);
 Q_INVOKABLE void discover();
 Q_INVOKABLE QVariant setting(QString key,QVariant fallback=QVariant())const;
 Q_INVOKABLE void setSetting(QString key,QVariant value);
 Q_INVOKABLE void resetSettings();
 Q_INVOKABLE void exportSettings(QUrl url);
 Q_INVOKABLE void importSettings(QUrl url);
 Q_INVOKABLE void importLyrics(QUrl url);
 Q_INVOKABLE void fetchLyrics();
 Q_INVOKABLE void searchLyrics(QString query);
 Q_INVOKABLE void bindLyrics(qint64 id);
 Q_INVOKABLE void bindLyricCandidate(QString provider,QString sourceId);
 Q_INVOKABLE void clearLyricBinding();
 Q_INVOKABLE void testLyricProvider(QString provider);
 Q_INVOKABLE void clearLyricsCache();
 Q_INVOKABLE void forgetDevice(QString url);
 Q_INVOKABLE void checkUpdates();
 Q_INVOKABLE void setUiVisible(bool visible);
 Q_INVOKABLE void setLyricsVisible(QString surface,bool visible);
 bool initializeNative(quintptr hwnd);
 void previewCoverForTest(QString path);
  void previewMediaForTest(QJsonObject media){applyMedia(media);}
 int nativeStatus() const{return m_nativeStatus;}
 static QVariantList parseLyrics(const QString &text);
signals:
 void changed();void progressChanged();void lyricsChanged();void settingsChanged();
 void lyricResultsChanged();
 void lyricLineChanged();void lyricProgressChanged();
 void lyricProvidersChanged();
 void commandFinished(QString requestId,bool success,QString error);
 void notification(QString message); void updateAvailable(QString version,QString url);
private:
 friend class LyricsEngineTests;
 QSettings m_settings;QWebSocket m_socket;QNetworkAccessManager m_network;QUdpSocket m_mdns;
 QTimer m_progress,m_heartbeat,m_retry,m_pairTimeout;QElapsedTimer m_anchor,m_lastPong;
 void enhanceCover(bool refresh=false);void applyEnhancedCover(CoverArtResult result);
 CoverArtService *m_coverService=nullptr;QString m_coverRequestKey,m_phoneArtwork;QByteArray m_phoneArtworkDigest;QSize m_phonePixels;CoverArtResult m_pendingCover;
 QTimer m_artworkRetry;int m_artworkRetryAttempts=0;
 QTimer m_lyricClock;bool m_uiVisible=true,m_lyricsLoading=false;int m_currentLyricLine=-1;
 QSet<QString> m_lyricSurfaces,m_peerFeatures;
 QVector<qint64> m_lyricTimes;QString m_lyricSource,m_embeddedLyrics,m_embeddedTranslation,m_embeddedKey;
 QPointer<QNetworkReply> m_lyricReply,m_lyricSearchReply;
 qint64 m_lyricRetryAfter=0;
 LyricProviders *m_lyricProviders=nullptr;QString m_lyricProvider;double m_lyricMatchScore=0;
 QElapsedTimer m_dragReceipt;bool m_dragging=false;QJsonObject m_deferredMedia;
 QJsonObject m_media;QVariantList m_lyrics,m_devices;
 QVariantList m_lyricResults;
 QString m_artworkAccent="#9bc3c6";quint64 m_searchGeneration=0,m_lyricGeneration=0;
 CoverPalette m_coverPalette;QByteArray m_artworkDigest;
 QString m_status,m_url,m_pin,m_code,m_artwork,m_accent="#9bc3c6",m_lyricKey;
 bool m_online=false,m_manual=false;int m_backoff=0;qint64 m_position=0;
 QHash<QString,qint64> m_pending; QLibrary m_native;
 QStringList m_seen;
 QString m_deviceId,m_deviceName,m_observedPin;QByteArray m_clientNonce,m_serverCommit;
 bool m_pairing=false,m_compareReady=false,m_authSent=false;
 QHash<QString,QString> m_hosts;QHash<QString,QPair<QString,int>> m_services;QHash<QString,QVariantMap> m_discoveryInfo;
 void clearPairing();void refreshDevices();void beginPairing();
 using NativeUpdate=int(*)(const char*);NativeUpdate m_nativeUpdate=nullptr;int m_nativeStatus=-1;
 void send(QString type,QJsonObject payload={});void receive(QString message);void setStatus(QString text);
 void applyMedia(QJsonObject media);void updateNative();void retry();QString deviceKey(QString url)const;
 QString lyricKey() const;void applyTheme();
 QString lyricIdentity() const;void setLyrics(QVariantList rows,QString source);void updateLyricClock();void cancelLyricRequest();
 void requestLyrics(QUrl url,QString key,quint64 generation,bool manual,bool searchFallback=false);
 QVariantList phoneLyricRows()const;bool phoneLyricsEnabled()const;void supplementCurrentLyrics();
 LyricTrack lyricTrack()const;QStringList lyricProviderOrder()const;
 void tryLyricProviders(QStringList providers,int index,QString key,quint64 generation);
 void tryLyricCandidates(QList<LyricCandidate> candidates,int index,QStringList providers,int nextProvider,QString key,quint64 generation);
 void acceptLyricDocument(LyricDocument document,bool manual);
 void searchNextLyricProvider(QString query,QStringList providers,int index,QString key,quint64 generation);
 void supplementLyricTranslation(QStringList providers,int index,QString key,quint64 generation);
 QByteArray token()const;bool storeToken(QByteArray token);QString cachePath()const;
};

