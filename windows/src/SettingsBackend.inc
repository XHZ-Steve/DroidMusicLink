// Included once by Bridge.cpp; keeps the validated preference surface separate
// from media transport and the asynchronous lyric engine.
QVariantList Bridge::settingsCatalog()const{return SettingsSchema::descriptors();}
QVariantList Bridge::appearanceDescriptors()const{return AppearanceConfig::descriptors();}
bool Bridge::appearanceInherited(QString scope)const{return scope!="global"&&m_settings.value("appearance/"+scope+"/inherit",true).toBool();}
QVariantMap Bridge::appearanceConfig(QString scope)const{
 auto config=AppearanceConfig::defaults();
 const auto preset=m_settings.value("appearance/preset","balanced").toString();
 if(preset=="clear"){config["glassOpacity"]=.06;config["glassBlur"]=.7;config["glassRefraction"]=.7;config["backgroundSaturation"]=1.;}
 if(preset=="vivid"){config["glassOpacity"]=.09;config["glassBlur"]=1.5;config["backgroundVibrance"]=.45;config["backgroundSaturation"]=1.25;}
 for(auto it=config.begin();it!=config.end();++it){auto stored=m_settings.value("appearance/global/"+it.key());QVariant normalized;if(stored.isValid()&&AppearanceConfig::validate(it.key(),stored,&normalized))it.value()=normalized;}
 if(scope!="global"&&!appearanceInherited(scope))for(auto it=config.begin();it!=config.end();++it){auto stored=m_settings.value("appearance/"+scope+"/"+it.key());QVariant normalized;if(stored.isValid()&&AppearanceConfig::validate(it.key(),stored,&normalized))it.value()=normalized;}
 return config;
}
void Bridge::setAppearance(QString scope,QString key,QVariant value){
 if(!QStringList{"global","background","dock","popup","floating","desktop"}.contains(scope))return;
 if(value.metaType()==QMetaType::fromType<QJSValue>())value=value.value<QJSValue>().toVariant();QVariant normalized;
 if(!AppearanceConfig::validate(key,value,&normalized))return;
 m_settings.setValue("appearance/"+scope+"/"+key,normalized);emit settingsChanged();
}
void Bridge::setAppearanceInherited(QString scope,bool inherited){if(!QStringList{"background","dock","popup","floating","desktop"}.contains(scope))return;m_settings.setValue("appearance/"+scope+"/inherit",inherited);emit settingsChanged();}
void Bridge::applyAppearancePreset(QString preset){if(!QStringList{"clear","balanced","vivid"}.contains(preset))return;m_settings.remove("appearance/global");m_settings.setValue("appearance/preset",preset);emit settingsChanged();}
void Bridge::resetAppearance(QString scope){if(!QStringList{"global","background","dock","popup","floating","desktop"}.contains(scope))return;m_settings.remove("appearance/"+scope);if(scope=="global")m_settings.setValue("appearance/preset","balanced");emit settingsChanged();}
void Bridge::resetSettingsGroup(int group,int section){for(const auto &item:SettingsSchema::descriptors()){auto row=item.toMap();if(row["group"].toInt()!=group||row["section"].toInt()!=section)continue;const auto key=row["key"].toString();if(row["type"]=="action"||row["type"]=="readonly")continue;setSetting(key,row["defaultValue"]);}emit settingsChanged();}
void Bridge::exportDiagnostics(QUrl url){QSaveFile file(url.toLocalFile());if(!file.open(QIODevice::WriteOnly))return;const QJsonObject data{{"applicationVersion",QCoreApplication::applicationVersion()},{"qtVersion",qVersion()},{"connected",online()},{"player",appName()},{"lyricsLoaded",m_lyrics.size()},{"lyricSource",lyricSource()},{"smtcStatus",nativeStatus()},{"appearancePreset",m_settings.value("appearance/preset","balanced").toString()}};file.write(QJsonDocument(data).toJson());file.commit();}
QVariant Bridge::setting(QString key,QVariant fallback)const{
 if(key=="info/player")return appName().isEmpty()?tr("No media"):appName();
 if(key=="info/volume")return volumeKnown()?QString::number(qRound(volume()*100))+"%":tr("Unavailable");
 auto value=m_settings.value("preferences/"+key,fallback);if(!value.isValid())return value;
 QVariant normalized;if(SettingsSchema::normalize(key,value,&normalized))return normalized;
 const auto descriptor=SettingsSchema::descriptor(key);if(!descriptor.isEmpty()&&descriptor["type"]!="readonly"&&descriptor["type"]!="action")return fallback;
 if(booleanSettings.contains(key))return value.toBool();if(key=="glass")return value.toDouble();
 if(QStringList{"lyricSize","lyrics/offset","floatX","floatY","lyricsX","lyricsY"}.contains(key))return value.toInt();
 return value;
}
void Bridge::setSetting(QString key,QVariant value){
 if(value.metaType()==QMetaType::fromType<QJSValue>())value=value.value<QJSValue>().toVariant();QVariant normalized;
 if(SettingsSchema::normalize(key,value,&normalized))value=normalized;
 else if(key=="glass"){bool ok=false;double number=value.toDouble(&ok);if(!ok||!std::isfinite(number))return;value=qBound(0.,number,.5);}
 else if(key=="lyricSize"||key=="floatX"||key=="floatY"||key=="lyricsX"||key=="lyricsY"){bool ok=false;int number=value.toInt(&ok);if(!ok)return;value=key=="lyricSize"?qBound(18,number,72):qBound(-32000,number,32000);}
 else if(key=="notifications"){if(value.metaType().id()!=QMetaType::Bool)return;}
 else return;
 m_settings.setValue("preferences/"+key,value);
   // Choosing a lyric layout is an explicit request to show home lyrics. Older
   // versions used the Dock button to persist showLyrics=false independently.
   if(key=="home/layout"&&value.toInt()!=1)m_settings.setValue("preferences/showLyrics",true);
 if(key=="general/autostart"){QSettings run("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",QSettings::NativeFormat);if(value.toBool())run.setValue("DroidMediaLink","\""+QDir::toNativeSeparators(QCoreApplication::applicationFilePath())+"\" --hidden");else run.remove("DroidMediaLink");}
 if(key=="lyrics/phoneSource"||key=="lyrics/online"||key=="lyrics/autoMatch"||key=="lyrics/providerPriority"||key=="lyrics/preferredProvider"||key=="lyrics/preferCurrentApp"||key=="lyrics/matchStrategy"||key=="lyrics/crossSourceTranslation"||key.startsWith("lyrics/providers/")){cancelLyricRequest();m_lyricKey.clear();fetchLyrics();}
 if(key=="connection/autoReconnect"&&!value.toBool())m_retry.stop();
 if(key=="theme/source"||key=="theme/brand"){applyTheme();emit changed();}
 if(key=="media/smtc")updateNative();if(key.startsWith("lyrics/providers/"))emit lyricProvidersChanged();emit settingsChanged();
}
void Bridge::resetSettings(){m_settings.remove("preferences");m_settings.remove("appearance");m_settings.setValue("preferences/lyrics/autoMatchMigration",true);setSetting("general/autostart",false);cancelLyricRequest();m_lyricKey.clear();fetchLyrics();emit settingsChanged();}
namespace {
QString lyricPresetKey(const QString &name){return "lyricPresets/custom/"+QString::fromLatin1(name.toUtf8().toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals));}
bool validLyricPresetName(const QString &name){return !name.isEmpty()&&name==name.trimmed()&&name.size()<=64&&std::none_of(name.begin(),name.end(),[](QChar ch){return ch.unicode()<32;});}
bool builtinLyricPreset(const QString &name){return QStringList{"soft","clear","contrast"}.contains(name);}
QVariantMap defaultLyricStyle(){QVariantMap result;for(const auto &value:SettingsSchema::descriptors()){const auto row=value.toMap();if(row.value("lyricStyle").toBool())result[row["key"].toString()]=row["defaultValue"];}return result;}
}
QVariantList Bridge::lyricPresets()const{
 QVariantList rows{QVariantMap{{"name","soft"},{"label",QCoreApplication::translate("Settings","Soft")},{"builtIn",true}},QVariantMap{{"name","clear"},{"label",QCoreApplication::translate("Settings","Clear")},{"builtIn",true}},QVariantMap{{"name","contrast"},{"label",QCoreApplication::translate("Settings","High contrast")},{"builtIn",true}}};
 for(const auto &key:m_settings.allKeys())if(key.startsWith("lyricPresets/custom/")){const auto preset=m_settings.value(key).toMap();if(validLyricPresetName(preset["name"].toString()))rows<<QVariantMap{{"name",preset["name"]},{"label",preset["name"]},{"builtIn",false}};}
 return rows;
}
void Bridge::saveLyricPreset(QString name){
 if(!validLyricPresetName(name)||builtinLyricPreset(name))return;
 if(lyricPresets().size()>=53&&!m_settings.contains(lyricPresetKey(name)))return;
 auto values=defaultLyricStyle();for(auto it=values.begin();it!=values.end();++it)it.value()=setting(it.key(),it.value());
 m_settings.setValue(lyricPresetKey(name),QVariantMap{{"name",name},{"values",values}});emit settingsChanged();
}
void Bridge::applyLyricPreset(QString name){
 QVariantMap values;if(builtinLyricPreset(name)){
  values=defaultLyricStyle();for(const auto &scope:QStringList{"lyrics/","desktopLyrics/"}){
   if(name=="clear"){values[scope+"activeScale"]=1.;values[scope+"fontWeight"]=450;values[scope+"activeWeight"]=500;values[scope+"outlineWidth"]=0.;}
   if(name=="contrast"){values[scope+"activeScale"]=1.14;values[scope+"activeWeight"]=650;values[scope+"pastOpacity"]=.56;values[scope+"futureOpacity"]=.48;values[scope+"outlineWidth"]=.75;}
  }
 }else values=m_settings.value(lyricPresetKey(name)).toMap().value("values").toMap();
 if(values.isEmpty())return;
 for(auto it=values.cbegin();it!=values.cend();++it){if(!SettingsSchema::descriptor(it.key()).value("lyricStyle").toBool())continue;QVariant normalized;if(SettingsSchema::normalize(it.key(),it.value(),&normalized))m_settings.setValue("preferences/"+it.key(),normalized);}
 emit settingsChanged();
}
void Bridge::renameLyricPreset(QString oldName,QString newName){
 if(!validLyricPresetName(newName)||builtinLyricPreset(oldName)||builtinLyricPreset(newName)||oldName==newName||m_settings.contains(lyricPresetKey(newName)))return;
 auto preset=m_settings.value(lyricPresetKey(oldName)).toMap();if(preset.isEmpty())return;preset["name"]=newName;m_settings.setValue(lyricPresetKey(newName),preset);m_settings.remove(lyricPresetKey(oldName));emit settingsChanged();
}
void Bridge::deleteLyricPreset(QString name){if(builtinLyricPreset(name))return;m_settings.remove(lyricPresetKey(name));emit settingsChanged();}
void Bridge::resetLyricSettings(QString scope){
 if(!QStringList{"main","desktop","all"}.contains(scope))return;
 for(const auto &value:SettingsSchema::descriptors()){const auto row=value.toMap();const auto key=row["key"].toString();if(row["lyricStyle"].toBool()&&(scope=="all"||key.startsWith(scope=="main"?"lyrics/":"desktopLyrics/")))m_settings.remove("preferences/"+key);}
 emit settingsChanged();
}
