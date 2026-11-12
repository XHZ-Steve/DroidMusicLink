#pragma once
#include "../src/Bridge.h"
#include "VisualTree.h"
#include <QQuickWindow>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <memory>

inline void startUiBindingSmoke(QQuickWindow *window,Bridge &bridge,const QString &path,QCoreApplication &app){
 window->setProperty("page",2);
 QTimer::singleShot(700,window,[=,&bridge,&app]{
  bool passed=true;QJsonObject checks;
  for(const auto &key:QStringList{"light","motion","showLyrics"}){
   const auto controls=visualItemsNamed(window,"setting-"+key);auto control=controls.isEmpty()?nullptr:controls.first();if(!control){passed=false;checks[key]="control not found";continue;}
   const bool original=control->property("checked").toBool();
   const bool invoked=QMetaObject::invokeMethod(control,"click");
   const auto value=control->property("checked").toBool(),stored=bridge.setting(key).toBool(),root=window->property(key.toLatin1().constData()).toBool();
   const bool valid=invoked&&value!=original&&stored!=original&&root!=original;passed=passed&&valid;checks[key]=QJsonObject{{"found",true},{"invoked",invoked},{"original",original},{"checked",value},{"stored",stored},{"root",root},{"passed",valid}};
   QMetaObject::invokeMethod(control,"click");
  }
  const auto original=bridge.setting("glass",.16);bridge.setSetting("glass",.3);passed=passed&&qAbs(window->property("glass").toDouble()-.3)<.001;bridge.setSetting("glass",original);
  checks["glassRoot"]=window->property("glass").toDouble();QFile report(path);if(report.open(QIODevice::WriteOnly))report.write(QJsonDocument(QJsonObject{{"checks",checks},{"nativeQtButtonClicks",3},{"settingsAndRootBindings",passed}}).toJson());
  app.exit(passed?0:12);
 });
}

// Explicit live-device test: click the real Qt control, observe remote state,
// and restore the original playback state before closing the test application.
inline void startTransportSmoke(QQuickWindow *window,Bridge &bridge,const QString &directory,QCoreApplication &app){
 struct State {int stage=0,replies=0;bool original=false,failed=false;qint64 originalPosition=0;QElapsedTimer clock;};
 auto state=std::make_shared<State>();state->clock.start();QDir().mkpath(directory);window->setProperty("page",0);
 auto timer=new QTimer(window);timer->setInterval(100);
 QObject::connect(&bridge,&Bridge::commandFinished,window,[state](QString,bool success,QString){++state->replies;state->failed=state->failed||!success;});
 QObject::connect(timer,&QTimer::timeout,window,[=,&bridge,&app]{
  auto click=[&]{for(auto item:visualItemsNamed(window,"primaryTransport"))if(item->isVisible()&&item->isEnabled())return QMetaObject::invokeMethod(item,"click");return false;};
  if(state->clock.elapsed()>15000||state->failed){timer->stop();if(state->stage)bridge.command(state->original?"play":"pause");QFile report(directory+"/transport-report.json");if(report.open(QIODevice::WriteOnly))report.write(QJsonDocument(QJsonObject{{"passed",false},{"stage",state->stage},{"online",bridge.online()},{"replies",state->replies}}).toJson());QTimer::singleShot(400,&app,[&app]{app.exit(13);});return;}
  const auto covers=visualItemsNamed(window,"heroArtwork");const bool ready=bridge.artwork().isEmpty()||(!covers.isEmpty()&&covers.first()->property("ready").toBool());
  if(state->stage==0&&state->clock.elapsed()>1800&&ready&&bridge.online()&&bridge.commands().contains("play")&&bridge.commands().contains("pause")){
   state->original=bridge.playing();state->originalPosition=bridge.position();if(state->original)window->grabWindow().save(directory+"/home-playing.png");state->stage=1;if(!click())state->failed=true;
  }else if(state->stage==1&&bridge.playing()!=state->original&&state->replies>=1){
   window->grabWindow().save(directory+(bridge.playing()?"/home-playing.png":"/home-paused.png"));state->stage=2;if(!click())state->failed=true;
  }else if(state->stage==2&&bridge.playing()==state->original&&state->replies>=2){
   timer->stop();if(!state->original&&bridge.commands().contains("seek"))bridge.command("seek",state->originalPosition);
   QFile report(directory+"/transport-report.json");if(report.open(QIODevice::WriteOnly))report.write(QJsonDocument(QJsonObject{{"actualQtControlClicks",2},{"authenticatedRemoteStateChanged",true},{"originalPlayStateRestored",true},{"initiallyPlaying",state->original},{"passed",!state->failed}}).toJson());
   QTimer::singleShot(400,&app,[&app]{app.exit(0);});
  }
 });timer->start();
}
