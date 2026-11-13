#pragma once
#include "VisualTree.h"
#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QElapsedTimer>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QImage>
#include <QFont>
#include <QCoreApplication>
#include <windows.h>
#include <psapi.h>
#include <memory>
#include <cmath>

// Isolated-profile diagnostic: captures the actual scene, never simulated UI.
inline void startLayoutMotionAudit(QQuickWindow *window,const QString &directory,QCoreApplication &app,bool captureFrames=true){
 auto canvases=visualItemsNamed(window,"nowPlayingCanvas");
 if(canvases.isEmpty()){app.exit(12);return;}auto canvas=canvases.first();
 auto covers=visualItemsNamed(window,"heroArtwork"),titles=visualItemsNamed(window,"trackTitle");
 if(covers.isEmpty()||titles.isEmpty()){app.exit(12);return;}
 auto cover=covers.first(),title=titles.first();QDir().mkpath(directory);
 struct State{int phase=0,frame=0;QElapsedTimer clock;QJsonArray samples;QJsonArray spacing;};auto state=std::make_shared<State>();
 auto timer=new QTimer(window);timer->setInterval(16);timer->setTimerType(Qt::PreciseTimer);
 QObject::connect(timer,&QTimer::timeout,window,[=,&app]{
  const auto elapsed=state->clock.elapsed();const auto cp=cover->mapToScene(QPointF()),tp=title->mapToScene(QPointF());
  if(state->phase==0&&state->frame==0){
   for(int i=0;i<12;++i)for(auto row:visualItemsNamed(window,"lyricRow-"+QString::number(i))){if(!row->isVisible())continue;const auto face=qvariant_cast<QFont>(row->property("face"));state->spacing.append(QJsonObject{{"selected",row->property("selected").toBool()},{"wordSpacing",face.wordSpacing()}});}
  }
  state->samples.append(QJsonObject{{"focus",canvas->property("lyricFocus").toDouble()},{"phase",state->phase},{"ms",elapsed},{"progress",canvas->property("transitionProgress").toDouble()},{"coverX",cp.x()},{"coverY",cp.y()},{"coverWidth",cover->width()},{"titleX",tp.x()},{"titleY",tp.y()}});
  if(state->frame++%6==0&&captureFrames){auto grab=window->contentItem()->grabToImage(window->size().scaled(QSize(840,570),Qt::KeepAspectRatio));if(grab)QObject::connect(grab.data(),&QQuickItemGrabResult::ready,window,[grab,path=directory+QString("/phase-%1-frame-%2.png").arg(state->phase).arg(state->frame,3,10,QChar('0'))]{grab->saveToFile(path);});}
  if(elapsed<700||canvas->property("changingLayout").toBool())return;
  if(++state->phase==3){timer->stop();QFile report(directory+"/motion-report.json");if(report.open(QIODevice::WriteOnly))report.write(QJsonDocument(QJsonObject{{"samples",state->samples},{"spacing",state->spacing},{"dpr",window->devicePixelRatio()}}).toJson());app.exit(0);return;}
  state->frame=0;state->clock.restart();QMetaObject::invokeMethod(canvas,"nextLayout");
 });
 QTimer::singleShot(1400,window,[=]{state->clock.start();QMetaObject::invokeMethod(canvas,"nextLayout");timer->start();});
}

// Explicit diagnostic only: exercises real QWindow resize/render synchronization.
inline void startRenderAudit(QQuickWindow *window,const QString &directory,QCoreApplication &app){
 struct State {int step=0,frames=0,surfacesValidated=0,mismatches=0,maxBackgroundJump=0;QElapsedTimer clock;QVector<double> intervals;qint64 lastFrame=0;quint64 cpu=0;double dockCenterError=0;};
 auto state=std::make_shared<State>();QDir().mkpath(directory);
 auto cpu=[](){FILETIME created,exited,kernel,user;GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user);ULARGE_INTEGER k,u;k.LowPart=kernel.dwLowDateTime;k.HighPart=kernel.dwHighDateTime;u.LowPart=user.dwLowDateTime;u.HighPart=user.dwHighDateTime;return k.QuadPart+u.QuadPart;};
 auto timer=new QTimer(window);timer->setTimerType(Qt::PreciseTimer);timer->setInterval(12);
 QObject::connect(window,&QQuickWindow::frameSwapped,window,[state,window]{
  if(!state->clock.isValid()||state->step>=240)return;
  const auto now=state->clock.nsecsElapsed();if(state->lastFrame)state->intervals<<double(now-state->lastFrame)/1e6;state->lastFrame=now;++state->frames;
  for(auto dock:visualItemsNamed(window,"playbackDock"))if(dock->isVisible())for(auto button:visualItemsNamed(window,"primaryTransport"))if(button->isVisible()&&button->window()==window){
   const auto dockCenter=dock->mapToScene(QPointF(dock->width()/2,0));const auto buttonCenter=button->mapToScene(QPointF(button->width()/2,0));
   state->dockCenterError=std::max(state->dockCenterError,std::abs(dockCenter.x()-buttonCenter.x()));
  }
  for(auto object:visualItemsNamed(window,"opticalMaterial")){
   auto item=qobject_cast<QQuickItem*>(object);if(!item||!item->isVisible()||!item->window()||!item->window()->isVisible())continue;
   ++state->surfacesValidated;
   const auto texture=object->property("captureTextureSize").toSize();const auto material=object->property("materialSize").toPointF();
   const double dpr=window->devicePixelRatio();
   if(texture.width()!=int(std::ceil(item->width()*dpr))+4||texture.height()!=int(std::ceil(item->height()*dpr))+4||material.x()!=std::ceil(item->width()*dpr)||material.y()!=std::ceil(item->height()*dpr))++state->mismatches;
  }
 },Qt::QueuedConnection);
 QObject::connect(timer,&QTimer::timeout,window,[=,&app]{
  if(state->step>=240){
   timer->stop();const double seconds=state->clock.elapsed()/1000.;const double cpuPercent=(cpu()-state->cpu)/1e7/seconds*100.;
   auto sorted=state->intervals;std::sort(sorted.begin(),sorted.end());
   PROCESS_MEMORY_COUNTERS memory{};GetProcessMemoryInfo(GetCurrentProcess(),&memory,sizeof(memory));
   QJsonObject result{{"resizeSteps",state->step},{"presentedFrames",state->frames},{"surfaceSamplesValidated",state->surfacesValidated},{"windowExposed",window->isExposed()},{"elapsedSeconds",seconds},{"cpuPercentOfOneCoreIncludingReadbacks",cpuPercent},{"workingSetMiB",double(memory.WorkingSetSize)/1048576.},{"frameIntervalP95Ms",sorted.isEmpty()?0.:sorted[int((sorted.size()-1)*.95)]},{"maximumDockCenterError",state->dockCenterError},{"geometryMismatches",state->mismatches},{"maxBackgroundAdjacentChannelJump",state->maxBackgroundJump},{"dpr",window->devicePixelRatio()},{"passed",state->dockCenterError<1.5&&state->mismatches==0&&state->maxBackgroundJump<12&&state->frames>20&&state->surfacesValidated>20}};
   window->hide();const auto hiddenCpu=cpu();
   QTimer::singleShot(2000,window,[=,&app]()mutable{result["hiddenCpuPercentOfOneCore"]=double(cpu()-hiddenCpu)/1e7/2*100.;QFile report(directory+"/resize-report.json");if(report.open(QIODevice::WriteOnly))report.write(QJsonDocument(result).toJson());app.exit(result["passed"].toBool()?0:11);});return;
  }
  const double phase=state->step*0.083;
  window->resize(560+int((std::sin(phase)+1)*440),520+int((std::cos(phase*.73)+1)*230));
  if(state->step%40==0){
   const auto shot=window->grabWindow();shot.save(directory+QString("/resize-%1.png").arg(state->step));
   if(!shot.isNull()){int y=shot.height()-12;for(int x=12;x<shot.width()-12;++x){const auto a=shot.pixelColor(x,y),b=shot.pixelColor(x+1,y);state->maxBackgroundJump=std::max({state->maxBackgroundJump,std::abs(a.red()-b.red()),std::abs(a.green()-b.green()),std::abs(a.blue()-b.blue())});}}
  }
  ++state->step;
 });
 QTimer::singleShot(600,window,[=]{window->hide();window->show();window->raise();window->requestActivate();});
 QTimer::singleShot(1200,window,[=]{state->cpu=cpu();state->clock.start();timer->start();});
}
