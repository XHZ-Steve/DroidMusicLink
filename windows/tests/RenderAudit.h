#pragma once
#include "VisualTree.h"
#include <QQuickWindow>
#include <QQuickItem>
#include <QElapsedTimer>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QImage>
#include <QCoreApplication>
#include <windows.h>
#include <psapi.h>
#include <memory>
#include <cmath>

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
