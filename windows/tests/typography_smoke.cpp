#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QFileInfo>
#include <QTimer>
int main(int argc,char **argv){
 QGuiApplication app(argc,argv);if(argc<2)return 64;
 QQmlApplicationEngine engine;QObject::connect(&engine,&QQmlApplicationEngine::quit,&app,&QCoreApplication::quit);
 engine.load(QUrl::fromLocalFile(QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath()));
 if(engine.rootObjects().isEmpty())return 2;
 QTimer::singleShot(10000,&app,[&]{app.exit(9);});return app.exec();
}
