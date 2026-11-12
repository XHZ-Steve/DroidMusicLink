#pragma once
#include <QObject>
#include <QAbstractNativeEventFilter>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <QQmlParserStatus>
#include <QtQml/qqmlregistration.h>
namespace QWK { class QuickWindowAgent; }
class NativeWindowController : public QObject, public QQmlParserStatus, public QAbstractNativeEventFilter {
 Q_OBJECT
 QML_ELEMENT
 Q_INTERFACES(QQmlParserStatus)
 Q_PROPERTY(QQuickWindow* window READ window WRITE setWindow NOTIFY configurationChanged)
 Q_PROPERTY(QQuickItem* titleBar MEMBER m_titleBar NOTIFY configurationChanged)
 Q_PROPERTY(QQuickItem* minimizeButton MEMBER m_minimizeButton NOTIFY configurationChanged)
 Q_PROPERTY(QQuickItem* maximizeButton MEMBER m_maximizeButton NOTIFY configurationChanged)
 Q_PROPERTY(QQuickItem* closeButton MEMBER m_closeButton NOTIFY configurationChanged)
 Q_PROPERTY(bool pointerInTitleBar READ pointerInTitleBar NOTIFY pointerChanged)
 Q_PROPERTY(bool pointerNearControls READ pointerNearControls NOTIFY pointerChanged)
 Q_PROPERTY(bool foregroundFullscreen READ foregroundFullscreen NOTIFY foregroundChanged)
 Q_PROPERTY(bool maximized READ maximized NOTIFY stateChanged)
public:
 explicit NativeWindowController(QObject *parent=nullptr);
 ~NativeWindowController() override;
 QQuickWindow *window() const { return m_window; }
 void setWindow(QQuickWindow *window);
 bool maximized() const;
 bool foregroundFullscreen() const {return m_foregroundFullscreen;}
 void refreshForeground();
 bool pointerInTitleBar() const {return m_pointerInTitleBar;}
 bool pointerNearControls() const {return m_pointerNearControls;}
 bool nativeEventFilter(const QByteArray &eventType,void *message,qintptr *result) override;
 void classBegin() override {}
 void componentComplete() override;
 Q_INVOKABLE void minimize();
 Q_INVOKABLE void toggleMaximized();
 Q_INVOKABLE void close();
 Q_INVOKABLE void setHitTestVisible(QQuickItem *item, bool visible=true);
signals:
 void configurationChanged();
 void stateChanged();
 void pointerChanged();
 void foregroundChanged();
private:
 void configure();
 QPointer<QQuickWindow> m_window;
 QPointer<QQuickItem> m_titleBar,m_minimizeButton,m_maximizeButton,m_closeButton;
 QWK::QuickWindowAgent *m_agent=nullptr;
 bool m_complete=false,m_pointerInTitleBar=false,m_pointerNearControls=false;
 bool m_foregroundFullscreen=false;
 void *m_foregroundHook=nullptr,*m_locationHook=nullptr;
 void updatePointer(bool inside);
};

