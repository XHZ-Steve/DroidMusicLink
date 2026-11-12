#include "LyricProviders.h"
#include "LyricTimeline.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>
int main(int argc,char **argv){
 QCoreApplication app(argc,argv);
 LyricTrack track{"Song (Live)","Artist","Album",{},180000};LyricCandidate candidate{"lrclib","1","Song","Artist","Album",180000,0,{}};
 if(LyricProviders::matchScore(track,candidate)>0)return 1;candidate.title=track.title;candidate.duration=210000;if(LyricProviders::matchScore(track,candidate)>0)return 2;
 candidate.duration=180000;if(LyricProviders::matchScore(track,candidate)<99)return 3;candidate.artist="Other Artist";if(LyricProviders::matchScore(track,candidate)>0)return 4;
 if(!LyricProviders::decodeLocal(QByteArray("krc1invalid-data")).isEmpty())return 5;
 const auto main=LyricTimeline::parse("[00:01]TEST A\n[00:03]TEST B\n[00:05]TEST C");
 auto extra=LyricTimeline::parse("[00:01]TEST A\n[00:01]译文一\n[00:03]TEST B\n[00:03]译文二\n[00:05]TEST C\n[00:05]译文三");
 auto merged=LyricTimeline::mergeDocumentAuxiliary(main,extra);if(merged==main||merged[0].toMap().value("translation").toString().isEmpty())return 6;
 extra[0]=QVariantMap{{"time",1000},{"text","WRONG VERSION"},{"translation","WRONG"}};extra[1]=QVariantMap{{"time",3000},{"text","WRONG VERSION"},{"translation","WRONG"}};
 if(LyricTimeline::mergeDocumentAuxiliary(main,extra)!=main)return 7;
 qInfo()<<"PASS: provider version/artist/duration confidence and verified time/text translation alignment";
 if(!app.arguments().contains("--live"))return 0;
 QNetworkAccessManager network;LyricProviders providers(&network);QEventLoop loop;int remaining=4,available=0;QJsonArray report;QHash<QString,LyricDocument> documents;
 for(const auto &id:LyricProviders::ids())providers.search(id,"The Scientist Coldplay",[&,id](QList<LyricCandidate> values,QString error){
  auto finish=[&,id](LyricDocument doc,QString failure){int words=0,translated=0;for(const auto &line:doc.rows){const auto row=line.toMap();words+=row.value("words").toList().size();translated+=!row.value("translation").toString().isEmpty();}const bool success=!doc.rows.isEmpty();if(success){++available;documents[id]=doc;QFile saved("artifacts/real-lyrics-"+id+".json");if(saved.open(QIODevice::WriteOnly))saved.write(QJsonDocument(QJsonObject{{"rows",QJsonArray::fromVariantList(doc.rows)},{"provider",id}}).toJson());}report<<QJsonObject{{"provider",id},{"available",success},{"rows",doc.rows.size()},{"words",words},{"translatedRows",translated},{"error",failure}};qInfo()<<id<<"available"<<success<<"rows"<<doc.rows.size()<<"words"<<words<<"translatedRows"<<translated<<"error"<<failure;if(--remaining==0)loop.quit();};
  if(values.isEmpty()){finish({},error.isEmpty()?"no-results":error);return;}
  auto best=values.first();const LyricTrack known{"The Scientist","Coldplay","A Rush of Blood to the Head",{},309000};for(auto &value:values){value.score=LyricProviders::matchScore(known,value);if(value.score>best.score)best=value;}
  providers.fetch(best,finish);
 });
 QTimer::singleShot(40000,&loop,&QEventLoop::quit);loop.exec();QJsonArray alignments;for(const auto &primary:QStringList{"lrclib","qq"})for(const auto &secondary:QStringList{"netease","kugou"})if(documents.contains(primary)&&documents.contains(secondary)){const auto original=documents[primary].rows;const auto combined=LyricTimeline::mergeDocumentAuxiliary(original,documents[secondary].rows,250,true);int translated=0;for(const auto &row:combined)translated+=!row.toMap().value("translation").toString().isEmpty();alignments<<QJsonObject{{"primary",primary},{"secondary",secondary},{"accepted",combined!=original},{"translatedRows",translated}};qInfo()<<"Cross-source alignment"<<primary<<secondary<<"accepted"<<(combined!=original)<<"translated"<<translated;}
 QFile output("artifacts/lyrics-provider-live.json");if(output.open(QIODevice::WriteOnly))output.write(QJsonDocument(QJsonObject{{"results",report},{"alignments",alignments},{"availableProviders",available},{"timedOut",remaining!=0}}).toJson());
 return available>0&&remaining==0?0:8;
}
