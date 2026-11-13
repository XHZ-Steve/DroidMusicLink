#pragma once
#include <QVariant>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
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
 if(source.trimmed().startsWith('<')&&source.contains("LyricContent=")){
  QXmlStreamReader envelope(source);QString content;while(!envelope.atEnd()){envelope.readNext();if(envelope.isStartElement()&&envelope.attributes().hasAttribute("LyricContent")){content=envelope.attributes().value("LyricContent").toString();break;}}if(content.isEmpty())return {};source=content;
 }
 QVariantList rows;int wordCount=0;
 const QRegularExpression numericLine("^\\[(\\d+),(\\d+)\\](.*)$",QRegularExpression::MultilineOption);
 if(source.contains(numericLine)){
  const QRegularExpression yrcWord("\\((\\d+),(\\d+),\\d+\\)"),krcWord("<(\\d+),(\\d+),\\d+>"),qrcWord("\\((\\d+),(\\d+)\\)");
  for(const auto &line:source.split('\n')){const auto match=numericLine.match(line);if(!match.hasMatch())continue;const auto start=qBound(qint64(0),match.captured(1).toLongLong(),qint64(9000000000000LL));const auto duration=qBound(qint64(0),match.captured(2).toLongLong(),qint64(9000000000000LL));const auto body=match.captured(3);const bool isKrc=body.contains(krcWord),isYrc=body.contains(yrcWord);const auto expression=isKrc?krcWord:isYrc?yrcWord:qrcWord;auto matches=expression.globalMatch(body);QList<QRegularExpressionMatch> markers;while(matches.hasNext())markers<<matches.next();QVariantList words;QString text;
   for(int i=0;i<markers.size();++i){const auto &marker=markers[i];QString part;if(isKrc||isYrc){const auto from=marker.capturedEnd(),until=i+1<markers.size()?markers[i+1].capturedStart():body.size();part=body.mid(from,until-from);}else {const auto from=i>0?markers[i-1].capturedEnd():0;part=body.mid(from,marker.capturedStart()-from);}if(part.isEmpty())continue;const auto at=qBound(qint64(0),marker.captured(1).toLongLong(),qint64(9000000000000LL))+(isKrc?start:0);const auto length=qBound(qint64(0),marker.captured(2).toLongLong(),qint64(9000000000000LL));words<<QVariantMap{{"time",at},{"end",at+qMax(qint64(1),length)},{"text",part}};text+=part;if(++wordCount>40000)return {};}
   if(markers.isEmpty())text=body;rows<<QVariantMap{{"time",start},{"end",start+qMax(qint64(1),duration)},{"text",text.trimmed()},{"words",words},{"background",false}};if(rows.size()>10000)return {};
  }
  const auto language=QRegularExpression("\\[language:([^\\]]+)\\]").match(source);if(language.hasMatch()){const auto payload=QJsonDocument::fromJson(QByteArray::fromBase64(language.captured(1).toUtf8())).object().value("content").toArray();for(const auto &item:payload){const auto object=item.toObject();const auto lines=object.value("lyricContent").toArray();const auto field=object.value("type").toInt()==0?QString("romanization"):QString("translation");for(int i=0;i<qMin(rows.size(),lines.size());++i){QStringList parts;for(const auto &part:lines[i].toArray())parts<<part.toString();const auto value=parts.join(field=="romanization"?" ":"");if(!value.isEmpty()){auto row=rows[i].toMap();row[field]=value;rows[i]=row;}}}}
 }else if(source.trimmed().startsWith('<')){
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
   if(start>=0){rows<<QVariantMap{{"time",start},{"end",end},{"text",text.trimmed()},{"translation",translation.trimmed()},{"romanization",romanization.trimmed()},{"words",words},{"background",paragraphRole.contains("x-bg")}};if(rows.size()>10000||wordCount>40000)return {};}
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
inline QVariantList validated(const QVariantList &rows){
 if(rows.size()>10000)return {};qint64 previous=-1;qsizetype characters=0,words=0;QVariantList result;
 for(const auto &value:rows){auto row=value.toMap();if(!row.contains("time")||!row.contains("text"))return {};bool ok=false;const auto at=row.value("time").toLongLong(&ok);if(!ok||at<previous||at<0||at>18000000000000LL)return {};previous=at;characters+=row.value("text").toString().size()+row.value("translation").toString().size()+row.value("romanization").toString().size();auto syllables=row.value("words").toList();if(syllables.size()>1024||row.value("text").toString().size()>8192||row.value("translation").toString().size()>8192||row.value("romanization").toString().size()>8192)return {};words+=syllables.size();if(words>40000||characters>2*1024*1024)return {};for(const auto &syllable:syllables){const auto word=syllable.toMap();const auto start=word.value("time").toLongLong(&ok);if(!ok||start<0||start>18000000000000LL||!word.contains("text"))return {};if(word.contains("end")){const auto until=word.value("end").toLongLong(&ok);if(!ok||until<=start||until>18000000000000LL)return {};}characters+=word.value("text").toString().size();if(characters>4*1024*1024)return {};}result<<row;}
 return result;
}
inline QVariantList mergeAuxiliary(QVariantList rows,const QString &text,QString field,qint64 tolerance=150){
 if(text.trimmed().isEmpty())return rows;const auto auxiliary=parse(text);if(auxiliary.isEmpty()||auxiliary.first().toMap().value("untimed").toBool())return rows;
 QVector<qint64> times;for(const auto &value:auxiliary)times<<value.toMap().value("time").toLongLong();
 for(auto &value:rows){auto row=value.toMap();const auto at=row.value("time").toLongLong();const auto next=std::lower_bound(times.cbegin(),times.cend(),at);int best=-1;qint64 difference=tolerance+1;for(int index:QList<int>{int(next-times.cbegin())-1,int(next-times.cbegin())}){if(index>=0&&index<times.size()&&qAbs(times[index]-at)<difference){best=index;difference=qAbs(times[index]-at);}}if(best>=0){const auto extra=auxiliary[best].toMap().value("text").toString();if(!extra.isEmpty())row[field]=extra;}value=row;}return rows;
}
inline QVariantList mergeDocumentAuxiliary(const QVariantList &original,const QVariantList &extra,qint64 tolerance=250,bool verifyText=true){
 auto normalized=[](QString value){value=value.normalized(QString::NormalizationForm_KC).toCaseFolded();value.remove(QRegularExpression("[^\\p{L}\\p{N}]+"));return value;};
  struct AuxiliaryRow {qint64 time;QString text;QVariantMap row;};
  QVector<AuxiliaryRow> indexed;indexed.reserve(extra.size());
  for(const auto &value:extra){const auto row=value.toMap();indexed.push_back({row.value("time").toLongLong(),normalized(row.value("text").toString()),row});}
  std::stable_sort(indexed.begin(),indexed.end(),[](const auto &a,const auto &b){return a.time<b.time;});
  QVariantList result=original;int eligible=0,matched=0,aligned=0;
  for(int i=0;i<original.size();++i){auto row=original[i].toMap();const auto text=normalized(row.value("text").toString());if(text.isEmpty())continue;++eligible;const auto at=row.value("time").toLongLong();auto candidate=std::lower_bound(indexed.begin(),indexed.end(),at-tolerance,[](const auto &entry,qint64 time){return entry.time<time;});for(;candidate!=indexed.end()&&candidate->time<=at+tolerance;++candidate){if(verifyText&&candidate->text!=text)continue;++aligned;const auto &other=candidate->row;bool added=false;for(const auto &field:QStringList{"translation","romanization"})if(row.value(field).toString().isEmpty()&&!other.value(field).toString().isEmpty()){row[field]=other.value(field);added=true;}if(added){++matched;result[i]=row;}break;}}
 return eligible>0&&matched>0&&aligned>=qMin(3,eligible)&&aligned>=eligible*.65?result:original;
}
}
