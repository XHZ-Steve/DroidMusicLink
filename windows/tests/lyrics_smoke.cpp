#include "LyricTimeline.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QFile>
#include <QTimer>
#include <QQuickWindow>
#include <QDir>
#include <QDebug>
#include <QQuickStyle>
class LyricFixture:public QObject {
 Q_OBJECT
 Q_PROPERTY(QVariantList lyrics READ lyrics NOTIFY lyricsChanged)
 Q_PROPERTY(int currentLine READ currentLine NOTIFY lineChanged)
 Q_PROPERTY(qint64 lyricPosition READ position NOTIFY progress)
 Q_PROPERTY(bool playing READ playing CONSTANT)
public:
 QVariantList rows;QVector<qint64> times;qint64 tick=0;int line=-1;QString output;
 QVariantList lyrics()const{return rows;}int currentLine()const{return line;}qint64 position()const{return tick;}bool playing()const{return true;}
 Q_INVOKABLE QVariant setting(QString key,QVariant fallback){if(key=="desktopLyrics/emptyPolicy")return "show";return fallback;}
 Q_INVOKABLE void setSetting(QString,QVariant){}
 Q_INVOKABLE void reload(){emit lyricsChanged();}
 Q_INVOKABLE void finish(QObject *main,QObject *desktop){auto window=qobject_cast<QQuickWindow*>(main);auto floating=qobject_cast<QQuickWindow*>(desktop);bool ok=window&&floating&&window->grabWindow().save(output+"/lyrics-main.png")&&floating->grabWindow().save(output+"/lyrics-desktop.png");if(!floating){QCoreApplication::exit(5);return;}floating->setProperty("editMode",true);QTimer::singleShot(250,this,[this,floating,ok]{const bool saved=ok&&floating->grabWindow().save(output+"/lyrics-desktop-edit.png");qInfo()<<"Lyrics lifecycle soak: 300 show/hide/edit transitions, 30 model resets; main/transparent/edit captures:"<<saved;QCoreApplication::exit(saved?0:5);});}
signals:void lyricsChanged();void lineChanged();void progress();
};
int main(int argc,char**argv){QGuiApplication app(argc,argv);QQuickStyle::setStyle("Basic");if(argc<4)return 64;QFile file(QString::fromLocal8Bit(argv[1]));if(!file.open(QIODevice::ReadOnly))return 2;LyricFixture fixture;fixture.rows=LyricTimeline::parse(QString::fromUtf8(file.readAll()));fixture.output=QString::fromLocal8Bit(argv[3]);QDir().mkpath(fixture.output);if(fixture.rows.isEmpty())return 3;for(const auto &row:fixture.rows)fixture.times<<row.toMap().value("time").toLongLong();qInfo()<<"Loaded real lyric rows:"<<fixture.rows.size();QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("fixture",&fixture);engine.load(QUrl::fromLocalFile(QString::fromLocal8Bit(argv[2])));if(engine.rootObjects().isEmpty())return 4;QTimer clock;clock.setInterval(16);QObject::connect(&clock,&QTimer::timeout,&app,[&]{fixture.tick+=48;if(fixture.tick>fixture.times.last()+5000)fixture.tick=0;const auto line=LyricTimeline::lineAt(fixture.times,fixture.tick);if(line!=fixture.line){fixture.line=line;emit fixture.lineChanged();}emit fixture.progress();});clock.start();return app.exec();}
#include "lyrics_smoke.moc"
