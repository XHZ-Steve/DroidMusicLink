#pragma once
#include <QVariant>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <algorithm>
#include <cmath>
#include <functional>

// Native, bounded LRC/enhanced-LRC/TTML parsing. No web renderer or runtime.
namespace LyricTimeline {
inline qint64 time(QString value) {
 value=value.trimmed();bool ok=false;double number=-1;
 if(value.endsWith("ms"))number=value.chopped(2).toDouble(&ok);
 else if(value.endsWith('s')){number=value.chopped(1).toDouble(&ok)*1000;}
 else {const auto parts=value.split(':');if(parts.size()==2||parts.size()==3){number=0;ok=true;for(const auto &part:parts){bool valid=false;const auto n=part.toDouble(&valid);ok=ok&&valid&&n>=0;number=number*60+n;}number*=1000;}}
 return ok&&std::isfinite(number)&&number>=0&&number<=9e12?qint64(number):-1;
}
inline QVariantList parse(const QString &input) {
 if(input.size()>2*1024*1024)return {};
 QString source=input;source.replace("\r\n","\n");source.replace('\r','\n');
 QVariantList rows;int wordCount=0;
 if(source.trimmed().startsWith('<')){
  QXmlStreamReader xml(source);
  while(!xml.atEnd()){
   xml.readNext();if(!xml.isStartElement()||xml.name()!=QStringLiteral("p"))continue;
   const auto start=time(xml.attributes().value("begin").toString());
   auto end=time(xml.attributes().value("end").toString());const auto dur=time(xml.attributes().value("dur").toString());if(end<0&&dur>=0&&start>=0)end=start+dur;
   const auto paragraphRole=xml.attributes().value("http://www.w3.org/ns/ttml#metadata","role").toString();
   QString text,translation,romanization;QVariantList words;
   int depth=0;
   std::function<void(QString,QString,qint64,qint64)> read=[&](QString target,QString role,qint64 at,qint64 until){
    if(++depth>64){xml.raiseError(QStringLiteral("Lyrics nesting limit"));--depth;return;}
    if(role.contains("translat",Qt::CaseInsensitive))target="translation";
    else if(role.contains("roman",Qt::CaseInsensitive))target="romanization";
    QString own;
    while(!xml.atEnd()){
     xml.readNext();if(xml.isEndElement())break;
     if(xml.isStartElement()){
      if(xml.name()==QStringLiteral("br")){own+='\n';xml.skipCurrentElement();continue;}
      const auto attrs=xml.attributes();auto childAt=time(attrs.value("begin").toString());auto childEnd=time(attrs.value("end").toString());const auto childDur=time(attrs.value("dur").toString());if(childAt<0)childAt=at;if(childEnd<0&&childDur>=0)childEnd=childAt+childDur;
      if(!own.isEmpty()){if(target=="translation")translation+=own;else if(target=="romanization")romanization+=own;else {text+=own;if(at>=0&&++wordCount<=40000)words<<QVariantMap{{"time",at},{"end",until},{"text",own}};}own.clear();}
      read(target,attrs.value("http://www.w3.org/ns/ttml#metadata","role").toString(),childAt,childEnd);
     } else if(xml.isCharacters())own+=xml.text();
    }
    if(target=="translation")translation+=own;else if(target=="romanization")romanization+=own;else {text+=own;if(at>=0&&!own.isEmpty()&&++wordCount<=40000)words<<QVariantMap{{"time",at},{"end",until},{"text",own}};}
    --depth;
   };
   read("",paragraphRole,start,end);
   if(start>=0){rows<<QVariantMap{{"time",start},{"end",end},{"text",text.trimmed()},{"translation",translation.trimmed()},{"romanization",romanization.trimmed()},{"words",words}};if(rows.size()>10000||wordCount>40000)return {};}
  }
  if(xml.hasError())return {};
 } else {
  const QRegularExpression stamp("\\[(\\d+):(\\d{2})(?:[.:](\\d{1,3}))?\\]"),wordStamp("<(\\d+):(\\d{2})(?:[.:](\\d{1,3}))?>");
  const auto offsetMatch=QRegularExpression("\\[offset:([+-]?\\d+)\\]").match(source);const auto offset=offsetMatch.hasMatch()?qBound<qint64>(-9000000000000LL,offsetMatch.captured(1).toLongLong(),9000000000000LL):0;
  auto markerTime=[&](const QRegularExpressionMatch &m){auto fraction=m.captured(3);while(fraction.size()<3)fraction+='0';return qMax<qint64>(0,(qBound<qint64>(0,m.captured(1).toLongLong(),150000000LL)*60+m.captured(2).toInt())*1000+fraction.toInt()+offset);};
  for(const auto &line:source.split('\n')){
   auto stamps=stamp.globalMatch(line);if(!stamps.hasNext())continue;QString content=line;content.remove(stamp);
   QList<QRegularExpressionMatch> markers;auto matches=wordStamp.globalMatch(content);while(matches.hasNext())markers<<matches.next();
   QVariantList words;for(int i=0;i<markers.size();++i){const auto from=markers[i].capturedEnd(),to=i+1<markers.size()?markers[i+1].capturedStart():content.size();const auto part=content.mid(from,to-from);if(!part.isEmpty()){words<<QVariantMap{{"time",markerTime(markers[i])},{"end",i+1<markers.size()?markerTime(markers[i+1]):-1},{"text",part}};if(++wordCount>40000)return {};}}
   QString plain=content;plain.remove(wordStamp);
   while(stamps.hasNext()){const auto start=markerTime(stamps.next());auto lineWords=words;const auto prefix=markers.isEmpty()?QString():content.left(markers.first().capturedStart());if(!prefix.isEmpty())lineWords.prepend(QVariantMap{{"time",start},{"text",prefix}});rows<<QVariantMap{{"time",start},{"text",plain.trimmed()},{"words",lineWords}};if(rows.size()>10000)return {};}
  }
  if(rows.isEmpty()&&!source.trimmed().isEmpty()&&!source.contains(stamp)){for(const auto &line:source.split('\n'))if(!line.trimmed().isEmpty())rows<<QVariantMap{{"time",0},{"end",9000000000000LL},{"text",line.trimmed()},{"untimed",true}};if(rows.size()>10000)return {};return rows;}
 }
 std::stable_sort(rows.begin(),rows.end(),[](const auto &a,const auto &b){return a.toMap().value("time").toLongLong()<b.toMap().value("time").toLongLong();});
 QVariantList result;
 for(const auto &value:rows){auto row=value.toMap();if(!result.isEmpty()&&result.last().toMap().value("time")==row.value("time")){auto previous=result.last().toMap();if(previous.value("text").toString().isEmpty()&&!row.value("text").toString().isEmpty()){row["translation"]=previous.value("translation");row["romanization"]=previous.value("romanization");result.last()=row;}else {auto extra=row.value("translation").toString();if(extra.isEmpty())extra=row.value("text").toString();if(!extra.isEmpty()&&extra!=previous.value("text").toString())previous["translation"]=previous.value("translation").toString().isEmpty()?extra:previous.value("translation").toString()+"\n"+extra;if(!row.value("romanization").toString().isEmpty())previous["romanization"]=row.value("romanization");result.last()=previous;}}else result<<row;}
 for(int i=0;i<result.size();++i){auto row=result[i].toMap();const auto start=row.value("time").toLongLong();auto end=row.value("end",-1).toLongLong();const auto next=i+1<result.size()?result[i+1].toMap().value("time").toLongLong():start+5000;if(end<=start)end=qMax(start+1,next);row["end"]=end;auto words=row.value("words").toList();for(int w=0;w<words.size();++w){auto word=words[w].toMap();const auto at=word.value("time").toLongLong();auto until=word.value("end",-1).toLongLong();if(until<=at)until=w+1<words.size()?words[w+1].toMap().value("time").toLongLong():end;word["end"]=qMax(at+1,until);words[w]=word;}row["words"]=words;result[i]=row;}
 return result;
}
inline int lineAt(const QVector<qint64> &times,qint64 position){return int(std::upper_bound(times.cbegin(),times.cend(),position)-times.cbegin())-1;}
}
