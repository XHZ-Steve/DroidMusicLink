#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTimer>
#include <QImage>
#include <QDir>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QSGRendererInterface>
int main(int argc, char **argv) {
    qputenv("QSG_INFO", "1");
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext &, const QString &message) {
        QFile log("artifacts/liquidglass-hardware-smoke.log");
        if (log.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QTextStream stream(&log); stream << message << '\n';
        }
    });
    QDir().mkpath("artifacts/screenshots");
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;
    engine.load(QUrl("qrc:/qt/qml/DroidMediaLink/qml/GlassSmoke.qml"));
    if (engine.rootObjects().isEmpty()) return 2;
    auto *window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    QTimer::singleShot(1600, [&] {
        auto *surface = window->findChild<QObject*>("glass");
        qInfo() << "Graphics API" << window->rendererInterface()->graphicsApi()
                << "Shader status" << surface->property("shaderStatus")
                << "Shader log" << surface->property("shaderLog");
        const QImage glass = window->grabWindow();
        glass.save("artifacts/screenshots/liquidglass-hardware-on.png");
        window->setProperty("glassOn", false);
        QTimer::singleShot(350, [&, surface, glass] {
            const QImage plain = window->grabWindow();
            plain.save("artifacts/screenshots/liquidglass-hardware-off.png");
            int changed = 0;
            for (int y = 0; y < glass.height(); ++y)
                for (int x = 0; x < glass.width(); ++x)
                    changed += glass.pixel(x, y) != plain.pixel(x, y);
            const bool passed = !glass.isNull() && changed > 20000
                && surface->property("shaderStatus").toInt() != 2;
            qInfo() << "Changed pixels" << changed << "Optical smoke passed" << passed;
            app.exit(passed ? 0 : 3);
        });
    });
    return app.exec();
}

