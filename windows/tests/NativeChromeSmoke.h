#pragma once
#include "../src/NativeWindowController.h"
#include <QQuickWindow>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include <QSysInfo>
#include <memory>
#include <windows.h>

// Diagnostic exercises the installed native WM_NCHITTEST path, not a duplicate geometry algorithm.
inline void startNativeChromeSmoke(QQuickWindow *window,const QString &path,QCoreApplication &app){
 struct State {QJsonObject result;QPointer<NativeWindowController> controller;};
 const auto state=std::make_shared<State>();
 QTimer::singleShot(800,&app,[window,path,state,&app]{
  for(auto candidate:window->findChildren<NativeWindowController*>())
   if(candidate->window()==window){state->controller=candidate;break;}
  if(!state->controller){state->result["error"]="No native controller attached";}
  else {
   auto *controller=state->controller.data();
   const auto hwnd=reinterpret_cast<HWND>(window->winId());
   auto *title=controller->property("titleBar").value<QQuickItem*>();
   auto *maxButton=controller->property("maximizeButton").value<QQuickItem*>();
   // Auto-hidden chrome is revealed only in this diagnostic.
   if(maxButton)for(auto *item=maxButton;item&&item!=window->contentItem();item=item->parentItem())item->setVisible(true);
   auto hit=[window,hwnd](QPointF p){POINT origin{0,0};ClientToScreen(hwnd,&origin);
    const auto ratio=window->devicePixelRatio();
    return int(SendMessageW(hwnd,WM_NCHITTEST,0,MAKELPARAM(origin.x+qRound(p.x()*ratio),origin.y+qRound(p.y()*ratio))));};
   const int caption=title?hit(title->mapToScene(QPointF(title->width()/2,title->height()/2))):0;
   const int maximize=maxButton?hit(maxButton->mapToScene(QPointF(maxButton->width()/2,maxButton->height()/2))):0;
   RECT frame{};GetWindowRect(hwnd,&frame);
   const int left=int(SendMessageW(hwnd,WM_NCHITTEST,0,MAKELPARAM(frame.left+1,(frame.top+frame.bottom)/2)));
   MINMAXINFO limits{};SendMessageW(hwnd,WM_GETMINMAXINFO,0,reinterpret_cast<LPARAM>(&limits));
   const bool minimumSize=limits.ptMinTrackSize.x>=qRound(window->minimumWidth()*window->devicePixelRatio())
    &&limits.ptMinTrackSize.y>=qRound(window->minimumHeight()*window->devicePixelRatio());
   state->result={{"qtVersion",QString::fromLatin1(qVersion())},{"os",QSysInfo::prettyProductName()},{"nativeMinimumSize",minimumSize},{"minimumTrackWidth",int(limits.ptMinTrackSize.x)},{"minimumTrackHeight",int(limits.ptMinTrackSize.y)},{"titleHit",caption},{"maximizeHit",maximize},{"resizeLeftHit",left},
    {"nativeCaptionDrag",caption==HTCAPTION},{"snapLayoutHit",maximize==HTMAXBUTTON},{"nativeResize",left==HTLEFT}};
   controller->toggleMaximized();
  }
  QTimer::singleShot(250,&app,[window,path,state,&app]{
   state->result["maximized"]=window->windowState()==Qt::WindowMaximized;
   if(state->controller)state->controller->toggleMaximized();
   QTimer::singleShot(250,&app,[window,path,state,&app]{
    state->result["restored"]=window->windowState()==Qt::WindowNoState;
    if(state->controller)state->controller->minimize();
    QTimer::singleShot(200,&app,[window,path,state,&app]{
     state->result["minimized"]=window->windowState()==Qt::WindowMinimized;
     window->showNormal();
     if(state->controller)state->controller->close();
     state->result["closeHidesWindow"]=!window->isVisible();
     QDir().mkpath(QFileInfo(path).absolutePath());QFile file(path);
     if(file.open(QIODevice::WriteOnly))file.write(QJsonDocument(state->result).toJson());
     const bool pass=state->result["nativeCaptionDrag"].toBool()&&state->result["snapLayoutHit"].toBool()
      &&state->result["nativeResize"].toBool()&&state->result["nativeMinimumSize"].toBool()&&state->result["maximized"].toBool()
      &&state->result["restored"].toBool()&&state->result["minimized"].toBool()&&state->result["closeHidesWindow"].toBool();
     app.exit(pass?0:6);
    });
   });
  });
 });
}
