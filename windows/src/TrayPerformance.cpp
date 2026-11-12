#include "TrayPerformance.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QTimer>
#include <QDebug>
#include <windows.h>
TrayPerformance::TrayPerformance(QQuickWindow *popup,qint64 engineReadyMs,QString reportPath,QObject *parent)
 :QObject(parent),m_popup(popup),m_reportPath(std::move(reportPath)),m_engineReadyMs(engineReadyMs){
 m_clock.start();
 if(!popup)return;
 popup->setPersistentGraphics(true);
 popup->setPersistentSceneGraph(true);
 connect(popup,&QQuickWindow::frameSwapped,this,[this]{
  if(m_warming){finishWarm(true);return;}
  if(m_pending.isEmpty())return;
  const qint64 now=m_clock.nsecsElapsed();
  m_pending["firstFrameMs"]=now/1e6;
  m_pending["latencyMs"]=(now-m_pending["requestNs"].toInteger())/1e6;
  m_pending.remove("requestNs");
  m_pending["under100ms"]=m_pending["latencyMs"].toDouble()<100;
  m_pending["nativeWindowId"]=QString::number(m_popup->winId(),16);
  m_samples.append(m_pending);
  qInfo().noquote()<<"Tray first frame queued:"<<QJsonDocument(m_pending).toJson(QJsonDocument::Compact);
  m_pending={};writeReport();
 });
 connect(popup,&QWindow::visibleChanged,this,[this](bool visible){
  if(visible&&!m_warming&&!m_pending.isEmpty())m_pending["showMs"]=m_clock.nsecsElapsed()/1e6;
 });
}
void TrayPerformance::prewarm(){
 if(!m_popup||m_popup->isVisible()||m_warmed||m_warming){emit prewarmFinished();return;}
 m_warming=true;m_warmStart=m_clock.nsecsElapsed();m_originalOpacity=m_popup->opacity();
 m_nativeCreateStartMs=m_clock.nsecsElapsed()/1e6;
 const auto hwnd=reinterpret_cast<HWND>(m_popup->winId());
 m_nativeCreateEndMs=m_clock.nsecsElapsed()/1e6;
 m_originalExStyle=GetWindowLongPtrW(hwnd,GWL_EXSTYLE);
 // Prevent focus theft without changing Qt flags/recreating the native window.
 SetWindowLongPtrW(hwnd,GWL_EXSTYLE,m_originalExStyle|WS_EX_NOACTIVATE);
 m_popup->setOpacity(0);m_popup->show();m_popup->update();
 m_prewarmActivated=GetForegroundWindow()==hwnd;
 QTimer::singleShot(1800,this,[this]{if(m_warming)finishWarm(false);});
}
void TrayPerformance::finishWarm(bool rendered){
 if(!m_warming||!m_popup)return;
 m_prewarmActivated=m_prewarmActivated||GetForegroundWindow()==reinterpret_cast<HWND>(m_popup->winId());
 m_warming=false;m_warmed=rendered;
 m_prewarmMs=(m_clock.nsecsElapsed()-m_warmStart)/1e6;
 m_popup->hide();m_popup->setOpacity(m_originalOpacity);
 SetWindowLongPtrW(reinterpret_cast<HWND>(m_popup->winId()),GWL_EXSTYLE,m_originalExStyle);
 qInfo()<<"Tray prewarm rendered:"<<rendered<<"durationMs:"<<(m_clock.nsecsElapsed()-m_warmStart)/1e6;
 writeReport();emit prewarmFinished();
}
void TrayPerformance::requested(){
 if(m_warming)finishWarm(false);
 m_pending={{"index",m_samples.size()+1},{"requestMs",m_clock.nsecsElapsed()/1e6},
  {"requestNs",m_clock.nsecsElapsed()},{"prewarmed",m_warmed}};
}
bool TrayPerformance::meetsGoal() const {
 if(!m_warmed||m_prewarmActivated||m_samples.size()<2)return false;
 const auto firstId=m_samples.first().toObject()["nativeWindowId"].toString();
 for(const auto &sample:m_samples){const auto object=sample.toObject();
  if(object["latencyMs"].toDouble()>=100||object["nativeWindowId"].toString()!=firstId)return false;
 }
 return true;
}
void TrayPerformance::writeReport(){
 if(m_reportPath.isEmpty())return;
 QDir().mkpath(QFileInfo(m_reportPath).absolutePath());
 QFile file(m_reportPath);
 if(file.open(QIODevice::WriteOnly))file.write(QJsonDocument(QJsonObject{
  {"qtVersion",QString::fromLatin1(qVersion())},{"engineReadyMs",m_engineReadyMs},{"engineCount",1},{"popupReused",true},
  {"nativeCreateStartMs",m_nativeCreateStartMs},{"nativeCreateEndMs",m_nativeCreateEndMs},
  {"prewarmMs",m_prewarmMs},{"readyAfterStartupMs",m_engineReadyMs+(m_warmStart/1e6)+m_prewarmMs},
  {"prewarmed",m_warmed},{"prewarmActivated",m_prewarmActivated},{"under100msGoalMet",meetsGoal()},{"samples",m_samples}}).toJson());
}
