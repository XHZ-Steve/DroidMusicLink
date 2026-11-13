#include "Bridge.h"
#include "TrayPerformance.h"
#include <QElapsedTimer>
#include "../tests/RenderAudit.h"
#include "../tests/SettingsPerformanceAudit.h"
#include "../tests/UiBindingSmoke.h"
#include "../tests/NativeChromeSmoke.h"
#include <QApplication>
#include <QMessageBox>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QSystemTrayIcon>
#include <QPainter>
#include <QTranslator>
#include <QLocale>
#include <QSharedMemory>
#include <QLocalServer>
#include <QLocalSocket>
#include <QScreen>
#include <QCursor>
#include <QFile>
#include <QDir>
#include <QCryptographicHash>
#include <QDebug>
#include <QTimer>
#include <QFontDatabase>
#include <QRegularExpression>
#include <windows.h>
#include <shobjidl.h>
#include <dwmapi.h>
#include <QPointer>
#include <functional>
class DragRefreshGuard final:public QAbstractNativeEventFilter {
 Bridge &bridge;QSet<HWND> moving;
public:explicit DragRefreshGuard(Bridge &value):bridge(value){}
 ~DragRefreshGuard(){if(auto app=QCoreApplication::instance())app->removeNativeEventFilter(this);}
 bool nativeEventFilter(const QByteArray &,void *message,qintptr *)override{auto msg=static_cast<MSG*>(message);if(msg->message==WM_ENTERSIZEMOVE){moving.insert(msg->hwnd);bridge.setDragging(true);}else if(msg->message==WM_EXITSIZEMOVE){moving.remove(msg->hwnd);bridge.setDragging(!moving.isEmpty());}return false;}
};
class LinkApplication final:public QApplication {
public:
 using QApplication::QApplication;
 std::function<void()> onPaletteChange;
 bool event(QEvent *event)override{const bool handled=QApplication::event(event);if(event->type()==QEvent::ApplicationPaletteChange&&onPaletteChange)onPaletteChange();return handled;}
};
int main(int argc,char **argv){
 QElapsedTimer startupClock;startupClock.start();
 QQuickWindow::setDefaultAlphaBuffer(true);
 SetCurrentProcessExplicitAppUserModelID(L"DroidMediaLink.Desktop");LinkApplication app(argc,argv);app.setFont(QFont("Microsoft YaHei UI"));app.setOrganizationName("DroidMediaLink");app.setApplicationName("DroidMediaLink");app.setApplicationVersion("0.1.0");app.setQuitOnLastWindowClosed(false);QQuickStyle::setStyle("Basic");
 QString instanceName="DroidMediaLink.primary";
 const int profileIndex=app.arguments().indexOf("--test-profile");
 if(profileIndex>=0&&profileIndex+1<app.arguments().size()){
  const auto profile=QDir(app.arguments()[profileIndex+1]).absolutePath();QDir().mkpath(profile);
  QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,profile);
  instanceName="DroidMediaLink.test."+QString::fromLatin1(QCryptographicHash::hash(profile.toUtf8(),QCryptographicHash::Sha256).toHex().left(16));
 }
 QLocalSocket existing;existing.connectToServer(instanceName);if(existing.waitForConnected(150)){existing.write("activate");existing.waitForBytesWritten(200);return 0;}QLocalServer::removeServer(instanceName);QLocalServer server;server.listen(instanceName);
 QTranslator translator;QSettings settings;QString locale=settings.value("preferences/language",QLocale::system().name()).toString();if(translator.load("droidmedialink_"+locale,QCoreApplication::applicationDirPath()+"/translations"))app.installTranslator(&translator);
 if(profileIndex>=0){const auto args=app.arguments();int at=args.indexOf("--test-style");if(at>=0&&at+1<args.size())settings.setValue("preferences/appearance/style",args[at+1]=="liquid"?"liquid":"standard");if(args.contains("--test-light"))settings.setValue("preferences/light",true);}
 Bridge backend;DragRefreshGuard dragRefresh(backend);app.installNativeEventFilter(&dragRefresh);
 if(profileIndex>=0){const auto args=app.arguments();int at=args.indexOf("--preview-media");if(at>=0&&at+1<args.size()){QFile file(args[at+1]);if(file.open(QIODevice::ReadOnly)&&file.size()<=65536)backend.previewMediaForTest(QJsonDocument::fromJson(file.readAll()).object());}at=args.indexOf("--test-layout");if(at>=0&&at+1<args.size())backend.setSetting("home/layout",args[at+1].toInt());}
 if(profileIndex>=0){const auto args=app.arguments();int at=args.indexOf("--preview-cover");if(at>=0&&at+1<args.size())backend.previewCoverForTest(args[at+1]);}
 if(profileIndex>=0){const auto args=app.arguments();int at=args.indexOf("--preview-lyrics");if(at>=0&&at+1<args.size())backend.importLyrics(QUrl::fromLocalFile(QFileInfo(args[at+1]).absoluteFilePath()));}
 QStringList startupErrors;QQmlApplicationEngine engine;const auto startupWarningConnection=QObject::connect(&engine,&QQmlEngine::warnings,&app,[&startupErrors](const QList<QQmlError>&errors){for(const auto &error:errors)if(startupErrors.size()<32)startupErrors<<error.toString();});engine.rootContext()->setContextProperty("backend",&backend);engine.load(QUrl("qrc:/qt/qml/DroidMediaLink/qml/Main.qml"));QObject::disconnect(startupWarningConnection);if(engine.rootObjects().isEmpty()){QMessageBox::critical(nullptr,"DroidMediaLink",QCoreApplication::translate("Main","The interface could not be loaded.")+"\n\n"+startupErrors.join("\n"));return 1;}auto window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());if(!window)return 1;
 if(auto desktop=window->findChild<QQuickWindow*>("desktop")){desktop->setPersistentGraphics(false);desktop->setPersistentSceneGraph(false);}
 const auto argsForTray=app.arguments();
 const int latencyIndex=argsForTray.indexOf("--tray-latency-smoke");
 const QString latencyReport=latencyIndex>=0&&latencyIndex+1<argsForTray.size()?argsForTray[latencyIndex+1]:QString();
 auto trayPerformance=new TrayPerformance(window->findChild<QQuickWindow*>("tray"),startupClock.elapsed(),latencyReport,&app);
 auto applyAccessibility=[guard=QPointer<QQuickWindow>(window)]{if(!guard)return;HIGHCONTRASTW contrast{sizeof(HIGHCONTRASTW)};guard->setProperty("highContrast",SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(contrast),&contrast,0)&&(contrast.dwFlags&HCF_HIGHCONTRASTON));};applyAccessibility();app.onPaletteChange=applyAccessibility;
 auto show=[window]{if(auto popup=window->findChild<QQuickWindow*>("tray"))popup->hide();window->show();window->raise();window->requestActivate();};QObject::connect(&server,&QLocalServer::newConnection,&app,[&]{while(auto peer=server.nextPendingConnection()){peer->deleteLater();show();}});
 QPixmap icon(64,64);icon.fill(Qt::transparent);{QPainter painter(&icon);painter.setRenderHint(QPainter::Antialiasing);painter.setBrush(QColor("#7ca58d"));painter.setPen(Qt::NoPen);painter.drawRoundedRect(2,2,60,60,18,18);painter.setPen(QColor("#102d25"));int fontId=QFontDatabase::addApplicationFont(":/icons/lucide.ttf");const auto families=QFontDatabase::applicationFontFamilies(fontId);QFont font(families.isEmpty()?"Microsoft YaHei UI":families.first(),38);painter.setFont(font);painter.drawText(icon.rect(),Qt::AlignCenter,QString(QChar(58186)));}app.setWindowIcon(QIcon(icon));QSystemTrayIcon tray(QIcon(icon),&app);tray.setToolTip("DroidMediaLink");QObject::connect(&tray,&QSystemTrayIcon::activated,&app,[&](QSystemTrayIcon::ActivationReason reason){if(reason==QSystemTrayIcon::Trigger||reason==QSystemTrayIcon::DoubleClick)show();else if(reason==QSystemTrayIcon::Context){auto position=QCursor::pos();auto screen=QGuiApplication::screenAt(position);if(!screen)screen=QGuiApplication::primaryScreen();if(screen){const auto area=screen->availableGeometry();position.setX(qBound(area.left(),position.x()-320,qMax(area.left(),area.right()-319)));position.setY(qBound(area.top(),position.y()-326,qMax(area.top(),area.bottom()-325)));}trayPerformance->requested();QMetaObject::invokeMethod(window,"showTrayMenu",Q_ARG(QVariant,position.x()),Q_ARG(QVariant,position.y()));}});tray.show();
 QObject::connect(&backend,&Bridge::notification,&tray,[&](QString message){if(backend.setting("notifications",true).toBool())tray.showMessage("DroidMediaLink",message,QSystemTrayIcon::Information,4000);});QObject::connect(&backend,&Bridge::updateAvailable,&tray,[&](QString version,QString){tray.showMessage("DroidMediaLink",QCoreApplication::translate("Main","Update available: %1").arg(version));});
QObject::connect(&app,&QCoreApplication::aboutToQuit,&backend,[&]{settings.setValue("window/geometry",QRect(window->position(),window->size()));backend.disconnectDevice();QLibrary dll(QCoreApplication::applicationDirPath()+"/WindowsNative.dll");using Shutdown=void(*)();if(auto shutdown=reinterpret_cast<Shutdown>(dll.resolve("dml_smtc_shutdown")))shutdown();});
 QRect geometry=settings.value("window/geometry").toRect();if(geometry.isValid()){bool intersects=false;for(auto screen:app.screens())if(screen->availableGeometry().intersects(geometry))intersects=true;if(intersects)window->setGeometry(geometry);}
 backend.initializeNative(window->winId());auto applyWindowTheme=[&]{const int dark=!backend.setting("light",false).toBool();DwmSetWindowAttribute(reinterpret_cast<HWND>(window->winId()),20,&dark,sizeof(dark));};QObject::connect(&backend,&Bridge::settingsChanged,&app,applyWindowTheme);applyWindowTheme();const int backdrop=2;DwmSetWindowAttribute(reinterpret_cast<HWND>(window->winId()),38,&backdrop,sizeof(backdrop));
 auto arguments=app.arguments();int sizeIndex=arguments.indexOf("--size");if(sizeIndex>=0&&sizeIndex+1<arguments.size()){auto parts=arguments[sizeIndex+1].split('x');if(parts.size()==2)window->resize(parts[0].toInt(),parts[1].toInt());}int pageIndex=arguments.indexOf("--page");if(pageIndex>=0&&pageIndex+1<arguments.size())window->setProperty("page",arguments[pageIndex+1].toInt());
 const int categoryIndex=arguments.indexOf("--category"),sectionIndex=arguments.indexOf("--settings-section");if(profileIndex>=0&&categoryIndex>=0&&categoryIndex+1<arguments.size())QMetaObject::invokeMethod(window,"openSettings",Q_ARG(QVariant,arguments[categoryIndex+1].toInt()),Q_ARG(QVariant,sectionIndex>=0&&sectionIndex+1<arguments.size()?arguments[sectionIndex+1].toInt():0));
 if(profileIndex>=0&&arguments.contains("--scroll-settings-end"))QTimer::singleShot(900,&app,[window]{if(auto view=window->findChild<QObject*>("settingsScroll")){auto item=view->property("contentItem").value<QQuickItem*>();if(item){const auto extent=item->property("contentHeight").toDouble()-item->height();item->setProperty("contentY",qMax(0.,extent));qInfo()<<"Settings scroll extent:"<<extent<<"position:"<<item->property("contentY");}}});
 QQuickWindow *captureWindow=window;int surfaceIndex=arguments.indexOf("--surface");if(surfaceIndex>=0&&surfaceIndex+1<arguments.size()){auto surface=window->findChild<QQuickWindow*>(arguments[surfaceIndex+1]);if(surface){window->hide();surface->show();surface->raise();captureWindow=surface;}}
 if(arguments.contains("--diagnose-link")){
  QObject::connect(&backend,&Bridge::changed,&app,[&]{static QString status,code;if(status!=backend.status()){status=backend.status();qInfo().noquote()<<"Link state:"<<status;}if(code!=backend.pairingCode()){code=backend.pairingCode();if(!code.isEmpty())qInfo().noquote()<<"Pairing comparison:"<<code;}});
 }
 if(arguments.contains("--discover"))backend.discover();
 int connectIndex=arguments.indexOf("--connect");if(connectIndex>=0&&connectIndex+1<arguments.size())backend.connectDevice(arguments[connectIndex+1]);
 const int dialogIndex=arguments.indexOf("--dialog");if(dialogIndex>=0&&dialogIndex+1<arguments.size()){const auto name=arguments[dialogIndex+1];QTimer::singleShot(600,&app,[&,name]{if(auto dialog=window->findChild<QObject*>(name))QMetaObject::invokeMethod(dialog,"open");if(name=="discovery")backend.discover();});}
 const int nativeFrameIndex=arguments.indexOf("--native-frame-smoke");
 if(profileIndex>=0&&nativeFrameIndex>=0&&nativeFrameIndex+1<arguments.size())startNativeChromeSmoke(captureWindow,arguments[nativeFrameIndex+1],app);
 const int auditIndex=arguments.indexOf("--resize-audit");if(profileIndex>=0&&auditIndex>=0&&auditIndex+1<arguments.size())startRenderAudit(window,arguments[auditIndex+1],app);
 const int settingsPerfIndex=arguments.indexOf("--settings-performance-audit");if(profileIndex>=0&&settingsPerfIndex>=0&&settingsPerfIndex+1<arguments.size())startSettingsPerformanceAudit(window,arguments[settingsPerfIndex+1],app);
 const int motionIndex=arguments.indexOf("--layout-motion-audit");if(profileIndex>=0&&motionIndex>=0&&motionIndex+1<arguments.size())startLayoutMotionAudit(window,arguments[motionIndex+1],app,!arguments.contains("--motion-metrics-only"));
 if(profileIndex>=0&&arguments.contains("--settings-binding-smoke"))startUiBindingSmoke(window,backend,QDir(arguments[profileIndex+1]).filePath("ui-binding-smoke.json"),app);
 const int transportIndex=arguments.indexOf("--transport-smoke");if(transportIndex>=0&&transportIndex+1<arguments.size())startTransportSmoke(window,backend,arguments[transportIndex+1],app);
 int captureIndex=arguments.indexOf("--capture");if(captureIndex>=0&&captureIndex+1<arguments.size()){QString path=arguments[captureIndex+1];int delay=1800;int delayIndex=arguments.indexOf("--capture-delay");if(delayIndex>=0&&delayIndex+1<arguments.size())delay=qBound(500,arguments[delayIndex+1].toInt(),120000);QTimer::singleShot(delay,&app,[captureWindow,path,&app]{const bool saved=captureWindow->grabWindow().save(path);app.exit(saved?0:4);});}
 // Exercises the actual tray signal handlers with the main window hidden.
 bool traySmokePassed=true;
 if(arguments.contains("--tray-smoke")){
  window->hide();
  QTimer::singleShot(350,&app,[&]{tray.activated(QSystemTrayIcon::Context);});
  QTimer::singleShot(1000,&app,[&]{auto popup=window->findChild<QQuickWindow*>("tray");traySmokePassed=popup&&popup->isVisible()&&!window->isVisible();qInfo()<<"Tray background Context:"<<traySmokePassed;if(popup&&popup->isVisible())popup->grabWindow().save(QDir::currentPath()+"/artifacts/screenshots/tray-background-smoke.png");tray.activated(QSystemTrayIcon::Trigger);});
  QTimer::singleShot(1500,&app,[&]{auto popup=window->findChild<QQuickWindow*>("tray");traySmokePassed=traySmokePassed&&window->isVisible()&&popup&&!popup->isVisible();qInfo()<<"Tray left Trigger shows main only:"<<traySmokePassed;app.exit(traySmokePassed?0:5);});
 }
 if(latencyIndex>=0){
  window->hide();
  QObject::connect(trayPerformance,&TrayPerformance::prewarmFinished,&app,[&]{
   QTimer::singleShot(120,&app,[&]{tray.activated(QSystemTrayIcon::Context);});
   QTimer::singleShot(450,&app,[&]{if(auto popup=window->findChild<QQuickWindow*>("tray"))popup->hide();});
   QTimer::singleShot(700,&app,[&]{tray.activated(QSystemTrayIcon::Context);});
   QTimer::singleShot(1100,&app,[&]{app.exit(trayPerformance->meetsGoal()?0:7);});
  });
 }
 // All resources stay in the existing engine/window; the first render is invisible and non-activating.
 if(!arguments.contains("--surface"))QTimer::singleShot(0,trayPerformance,&TrayPerformance::prewarm);
 if(app.arguments().contains("--hidden")||backend.setting("startHidden",false).toBool()||arguments.contains("--tray-smoke")||latencyIndex>=0)window->hide();else if(surfaceIndex<0)window->show();return app.exec();
}







