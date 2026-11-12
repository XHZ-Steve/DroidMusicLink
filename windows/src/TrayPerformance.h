#pragma once
#include <QObject>
#include <QPointer>
#include <QQuickWindow>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>

// Reuses the QML-owned popup and renderer; records request-to-first-present timings.
class TrayPerformance : public QObject {
 Q_OBJECT
public:
 TrayPerformance(QQuickWindow *popup, qint64 engineReadyMs, QString reportPath, QObject *parent=nullptr);
 void prewarm();
 void requested();
 bool warmed() const { return m_warmed; }
 bool meetsGoal() const;
signals:
 void prewarmFinished();
private:
 void finishWarm(bool rendered);
 void writeReport();
 QPointer<QQuickWindow> m_popup;
 QElapsedTimer m_clock;
 QString m_reportPath;
 QJsonArray m_samples;
 QJsonObject m_pending;
 bool m_warming=false,m_warmed=false,m_prewarmActivated=false;
 qint64 m_engineReadyMs=0,m_warmStart=0;
 double m_nativeCreateStartMs=0,m_nativeCreateEndMs=0,m_prewarmMs=0;
 qreal m_originalOpacity=1;
 qintptr m_originalExStyle=0;
};
