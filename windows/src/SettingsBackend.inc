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
 if(key=="general/autostart"){QSettings run("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",QSettings::NativeFormat);if(value.toBool())run.setValue("DroidMediaLink","\""+QDir::toNativeSeparators(QCoreApplication::applicationFilePath())+"\" --hidden");else run.remove("DroidMediaLink");}
 if(key=="lyrics/online"||key=="lyrics/autoMatch"||key=="lyrics/providerPriority"){cancelLyricRequest();m_lyricKey.clear();fetchLyrics();}
 if(key=="connection/autoReconnect"&&!value.toBool())m_retry.stop();
 if(key=="theme/source"||key=="theme/brand"){applyTheme();emit changed();}
 updateNative();emit settingsChanged();
}
void Bridge::resetSettings(){m_settings.remove("preferences");m_settings.remove("appearance");m_settings.setValue("preferences/lyrics/autoMatchMigration",true);setSetting("general/autostart",false);cancelLyricRequest();m_lyricKey.clear();fetchLyrics();emit settingsChanged();}
