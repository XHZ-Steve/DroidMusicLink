#include "NativeWindowController.h"
#include <QWKQuick/quickwindowagent.h>
#include <QDebug>
#include <QCoreApplication>
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
namespace {
QPointer<NativeWindowController> foregroundObserver;
void CALLBACK foregroundEvent(HWINEVENTHOOK,DWORD event,HWND hwnd,LONG object,LONG,DWORD,DWORD){
 if(event==EVENT_OBJECT_LOCATIONCHANGE&&(hwnd!=GetForegroundWindow()||object!=OBJID_WINDOW))return;
 if(foregroundObserver)QMetaObject::invokeMethod(foregroundObserver,&NativeWindowController::refreshForeground,Qt::QueuedConnection);
}
}
void NativeWindowController::refreshForeground(){
 const auto hwnd=GetForegroundWindow();DWORD pid=0;GetWindowThreadProcessId(hwnd,&pid);RECT rect{};MONITORINFO monitor{sizeof(MONITORINFO)};
 bool fullscreen=false;
 if(hwnd&&pid!=GetCurrentProcessId()&&IsWindowVisible(hwnd)&&!IsIconic(hwnd)&&GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&monitor)){
  if(FAILED(DwmGetWindowAttribute(hwnd,DWMWA_EXTENDED_FRAME_BOUNDS,&rect,sizeof(rect))))GetWindowRect(hwnd,&rect);
  fullscreen=rect.left<=monitor.rcMonitor.left&&rect.top<=monitor.rcMonitor.top&&rect.right>=monitor.rcMonitor.right&&rect.bottom>=monitor.rcMonitor.bottom;
 }
 if(fullscreen!=m_foregroundFullscreen){m_foregroundFullscreen=fullscreen;emit foregroundChanged();}
}
NativeWindowController::NativeWindowController(QObject *parent):QObject(parent) {
 connect(this,&NativeWindowController::configurationChanged,this,&NativeWindowController::configure);
}
NativeWindowController::~NativeWindowController(){if(m_foregroundHook)UnhookWinEvent(static_cast<HWINEVENTHOOK>(m_foregroundHook));if(m_locationHook)UnhookWinEvent(static_cast<HWINEVENTHOOK>(m_locationHook));if(QCoreApplication::instance())QCoreApplication::instance()->removeNativeEventFilter(this);}
void NativeWindowController::setWindow(QQuickWindow *window) {
 if(m_window==window)return;
 if(m_agent){qWarning()<<"NativeWindowController cannot change an attached window";return;}
 m_window=window;
 if(window){
  connect(window,&QWindow::activeChanged,this,&NativeWindowController::refreshForeground);
  connect(window,&QWindow::windowStateChanged,this,&NativeWindowController::stateChanged);
  connect(window,&QWindow::visibleChanged,this,[this](bool visible){if(!visible)updatePointer(false);});
 }
 emit configurationChanged();
}
void NativeWindowController::componentComplete(){m_complete=true;configure();}
void NativeWindowController::configure(){
 if(!m_complete||!m_window)return;
 if(!m_agent){
  m_agent=new QWK::QuickWindowAgent(this);
  if(!m_agent->setup(m_window)){qWarning()<<"QWindowKit setup failed";delete m_agent;m_agent=nullptr;return;}
  QCoreApplication::instance()->installNativeEventFilter(this);
  foregroundObserver=this;
  m_foregroundHook=SetWinEventHook(EVENT_SYSTEM_FOREGROUND,EVENT_SYSTEM_FOREGROUND,nullptr,foregroundEvent,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
  m_locationHook=SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE,EVENT_OBJECT_LOCATIONCHANGE,nullptr,foregroundEvent,0,0,WINEVENT_OUTOFCONTEXT|WINEVENT_SKIPOWNPROCESS);
  refreshForeground();
 }
 m_agent->setTitleBar(m_titleBar);
 m_agent->setSystemButton(QWK::WindowAgentBase::Minimize,m_minimizeButton);
 m_agent->setSystemButton(QWK::WindowAgentBase::Maximize,m_maximizeButton);
 m_agent->setSystemButton(QWK::WindowAgentBase::Close,m_closeButton);
}
bool NativeWindowController::maximized() const{return m_window&&m_window->windowState()==Qt::WindowMaximized;}
void NativeWindowController::minimize(){if(m_window)m_window->showMinimized();}
void NativeWindowController::toggleMaximized(){if(m_window){if(maximized())m_window->showNormal();else m_window->showMaximized();}}
void NativeWindowController::close(){if(m_window)m_window->close();}
void NativeWindowController::setHitTestVisible(QQuickItem *item,bool visible){configure();if(m_agent&&item)m_agent->setHitTestVisible(item,visible);}

void NativeWindowController::updatePointer(bool inside){
 if(!inside&&m_pointerNearControls){m_pointerNearControls=false;emit pointerChanged();}
 if(m_pointerInTitleBar==inside)return;
 m_pointerInTitleBar=inside;emit pointerChanged();
}
bool NativeWindowController::nativeEventFilter(const QByteArray &,void *message,qintptr *){
 if(!m_window||!m_window->handle()||!m_titleBar)return false;
 const auto msg=static_cast<MSG*>(message);
 const auto hwnd=reinterpret_cast<HWND>(m_window->winId());
 if(msg->hwnd!=hwnd)return false;
 POINT point{};
 switch(msg->message){
 case WM_MOUSEMOVE:point={GET_X_LPARAM(msg->lParam),GET_Y_LPARAM(msg->lParam)};break;
 case WM_NCHITTEST:
 case WM_NCMOUSEMOVE:
  point={GET_X_LPARAM(msg->lParam),GET_Y_LPARAM(msg->lParam)};ScreenToClient(hwnd,&point);
  if(msg->message==WM_NCMOUSEMOVE){TRACKMOUSEEVENT track{sizeof(TRACKMOUSEEVENT),TME_LEAVE|TME_NONCLIENT,hwnd,0};TrackMouseEvent(&track);}
  break;
 case WM_MOUSELEAVE:
 case WM_NCMOUSELEAVE:GetCursorPos(&point);ScreenToClient(hwnd,&point);break;
 default:return false;
 }
 const auto ratio=m_window->devicePixelRatio();
 const auto local=m_titleBar->mapFromScene(QPointF(point.x/ratio,point.y/ratio));
 const bool controlsNearby=m_titleBar->contains(local)&&local.x()>=m_titleBar->width()-250;
 if(controlsNearby!=m_pointerNearControls){m_pointerNearControls=controlsNearby;emit pointerChanged();}
 updatePointer(m_window->isVisible()&&m_titleBar->isVisible()&&m_titleBar->contains(local));
 return false; // QWindowKit owns native hit testing, dragging, resizing and Snap Layout.
}
