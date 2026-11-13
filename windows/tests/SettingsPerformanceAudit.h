#pragma once
#include "VisualTree.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>
#include <algorithm>

// Isolated-profile runtime audit, invoked only by an explicit command-line flag.
inline void startSettingsPerformanceAudit(QQuickWindow *window,const QString &directory,QCoreApplication &app){
 struct State{QElapsedTimer clock;int clicks=0;int warmed=0;int warmGroup=0;bool warmRequested=false;int misses=0;int warmMisses=0;int maxWarmGap=0;int maxColdGap=0;qint64 last=0;QJsonArray samples;};
 auto state=std::make_shared<State>();QDir().mkpath(directory);
 auto pages=visualItemsNamed(window,"settingsPage");if(pages.isEmpty()){app.exit(12);return;}auto page=pages.first();window->setProperty("page",2);
 auto heartbeat=new QTimer(window);heartbeat->setTimerType(Qt::PreciseTimer);heartbeat->setInterval(10);
 QObject::connect(heartbeat,&QTimer::timeout,window,[=]{const auto now=state->clock.elapsed();const auto gap=int(now-state->last);state->last=now;if(state->warmed)state->maxWarmGap=std::max(state->maxWarmGap,gap);else state->maxColdGap=std::max(state->maxColdGap,gap);});
 auto clicks=new QTimer(window);clicks->setTimerType(Qt::PreciseTimer);clicks->setInterval(70);
 QObject::connect(clicks,&QTimer::timeout,window,[=,&app]{
  if(!state->warmed){
   auto buttons=visualItemsNamed(window,"settings-category-"+QString::number(state->warmGroup));
   if(!state->warmRequested){if(!buttons.isEmpty())QMetaObject::invokeMethod(buttons.first(),"click");state->warmRequested=true;return;}
   auto loaders=visualItemsNamed(window,"settings-group-cache-"+QString::number(state->warmGroup));
   if(!loaders.isEmpty()&&loaders.first()->property("status").toInt()==1){state->warmRequested=false;if(++state->warmGroup==6)state->warmed=1;}
   if(state->clock.elapsed()>15000){app.exit(13);return;}
   return;
  }
  if(state->clicks==60){
   clicks->stop();heartbeat->stop();int cached=0;for(int i=0;i<6;++i){auto rows=visualItemsNamed(window,"settings-group-cache-"+QString::number(i));if(!rows.isEmpty()&&rows.first()->property("status").toInt()==1)++cached;}
   window->grabWindow().save(directory+"/settings-final.png");
   const bool passed=cached==6&&state->misses==0&&state->warmMisses==0&&state->maxWarmGap<100;
   QFile report(directory+"/settings-performance.json");if(report.open(QIODevice::WriteOnly))report.write(QJsonDocument(QJsonObject{{"passed",passed},{"categoryClicks",60},{"clickIntervalMs",70},{"cachedGroups",cached},{"clickFailures",state->misses},{"warmCacheMisses",state->warmMisses},{"maxColdEventLoopGapMs",state->maxColdGap},{"maxWarmEventLoopGapMs",state->maxWarmGap},{"samples",state->samples}}).toJson());app.exit(passed?0:12);return;
  }
  const int group=state->clicks%6;auto buttons=visualItemsNamed(window,"settings-category-"+QString::number(group));
  QElapsedTimer duration;duration.start();if(buttons.isEmpty()||!QMetaObject::invokeMethod(buttons.first(),"click"))++state->misses;
  state->samples.append(QJsonObject{{"click",state->clicks},{"group",group},{"handlerUs",double(duration.nsecsElapsed()/1000)}});
  if(state->warmed){auto loaders=visualItemsNamed(window,"settings-group-cache-"+QString::number(group));if(loaders.isEmpty()||loaders.first()->property("status").toInt()!=1)++state->warmMisses;}
  ++state->clicks;
 });
 QTimer::singleShot(1400,window,[=]{state->clock.start();heartbeat->start();clicks->start();});
}
