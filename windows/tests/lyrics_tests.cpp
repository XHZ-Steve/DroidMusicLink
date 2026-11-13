#include "LyricTimeline.h"
#include <QCoreApplication>
#include <QDebug>
int main(int argc,char **argv){
 QCoreApplication app(argc,argv);
 const auto bilingual=LyricTimeline::parse("[00:01.00]<00:01.00>你好 <00:01.50>world\n[00:01.00]Hello world\n[00:03.00]Next");
 if(bilingual.size()!=2||bilingual[0].toMap().value("translation").toString()!="Hello world")return 1;
 const auto words=bilingual[0].toMap().value("words").toList();
 if(words.size()!=2||words[0].toMap().value("end").toLongLong()!=1500||words[1].toMap().value("end").toLongLong()!=3000)return 2;
 const auto ttml=LyricTimeline::parse("<tt xmlns:ttm=\"http://www.w3.org/ns/ttml#metadata\"><body><p begin=\"1s\" end=\"3s\"><span begin=\"1s\" end=\"2s\">你好</span><span begin=\"2s\" end=\"3s\">世界</span><span ttm:role=\"x-translation\">Hello world</span><span ttm:role=\"x-roman\">ni hao shi jie</span></p></body></tt>");
 if(ttml.size()!=1||ttml[0].toMap().value("text").toString()!=QString::fromUtf8("你好世界")||ttml[0].toMap().value("translation").toString()!="Hello world"||ttml[0].toMap().value("romanization").toString()!="ni hao shi jie")return 3;
 const QVector<qint64> times{1000,2000,4000};
 for(int repeat=0;repeat<10000;++repeat)if(LyricTimeline::lineAt(times,5000)!=2||LyricTimeline::lineAt(times,0)!=-1||LyricTimeline::lineAt(times,2000)!=1||LyricTimeline::lineAt(times,1500)!=0)return 4;
 QString deep="<tt><p begin=\"1s\">";for(int i=0;i<1000;++i)deep+="<span>";deep+="text";for(int i=0;i<1000;++i)deep+="</span>";deep+="</p></tt>";
 if(!LyricTimeline::parse(deep).isEmpty()||!LyricTimeline::parse(QString(2*1024*1024+1,'x')).isEmpty())return 5;
 if(!LyricTimeline::parse("<tt><p begin=\"1s\">broken").isEmpty())return 6;
 const auto plain=LyricTimeline::parse("Untimed first\nUntimed second");if(plain.size()!=2||!plain.first().toMap().value("untimed").toBool())return 7;
 const auto terminal=LyricTimeline::parse("[00:01]<00:01>A<00:02>B<00:03>\r\n[00:06]C");const auto lastWord=terminal.first().toMap().value("words").toList().last().toMap();if(lastWord.value("end").toLongLong()!=3000||lastWord.value("text").toString()!="B")return 8;
 const auto yrc=LyricTimeline::parse("[1000,2000](1000,500,0)TEST (1500,1500,0)YRC");if(yrc.size()!=1||yrc[0].toMap().value("words").toList().size()!=2||yrc[0].toMap().value("text").toString()!="TEST YRC")return 9;
 const auto krc=LyricTimeline::parse("[1000,2000]<0,500,0>TEST <500,1500,0>KRC");if(krc.size()!=1||krc[0].toMap().value("words").toList().last().toMap().value("time").toLongLong()!=1500)return 10;
 const auto qrc=LyricTimeline::parse("[1000,2000]TEST (1000,500)QRC(1500,1500)");if(qrc.size()!=1||qrc[0].toMap().value("text").toString()!="TEST QRC")return 11;
 auto originals=LyricTimeline::parse("[00:01]<00:01>TEST <00:02>one\n[00:05]TEST two\n[00:09]TEST three");
 auto extra=LyricTimeline::mergeAuxiliary(originals,"[00:01]Translation one\n[00:05]Translation two\n[00:09]Translation three","translation");
 auto first=originals[0].toMap();first["translation"]="Manual translation";originals[0]=first;auto second=originals[1].toMap();second["translation"]="Existing translation";originals[1]=second;
 const auto merged=LyricTimeline::mergeDocumentAuxiliary(originals,extra);
 if(merged[0].toMap()["translation"]!="Manual translation"||merged[1].toMap()["translation"]!="Existing translation"||merged[2].toMap()["translation"]!="Translation three"||merged[0].toMap()["words"]!=originals[0].toMap()["words"])return 12;
 auto wrong=extra;for(auto &value:wrong){auto row=value.toMap();row["text"]="DIFFERENT TEST SONG";value=row;}if(LyricTimeline::mergeDocumentAuxiliary(originals,wrong)!=originals)return 13;
 qInfo()<<"PASS: bounded bilingual enhanced-LRC/TTML parsing, seek/backward index, malformed/deep input, untimed fallback";
 return 0;
}
