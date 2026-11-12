#pragma once
#include <QCoreApplication>
#include <QColor>
#include <QVariant>
#include <cmath>

// Shared metadata for persistence, presets, validation and the settings editor.
// Scope selection is deliberately the caller's responsibility.
namespace AppearanceConfig {
inline QVariantList descriptors() {
 struct Entry { const char *key; const char *label; const char *group; const char *type; double value,min,max,step; bool advanced; };
 static const Entry entries[]{
  {"backgroundStrength",QT_TRANSLATE_NOOP("Appearance","Palette strength"),"background","real",1,0,1.6,.05,false},
  {"backgroundBrightness",QT_TRANSLATE_NOOP("Appearance","Brightness"),"background","real",1,.4,1.6,.05,false},
  {"backgroundSaturation",QT_TRANSLATE_NOOP("Appearance","Saturation"),"background","real",1.08,0,2,.05,false},
  {"backgroundVibrance",QT_TRANSLATE_NOOP("Appearance","Vibrance"),"background","real",.24,-.5,1,.05,true},
  {"backgroundOpacity",QT_TRANSLATE_NOOP("Appearance","Atmosphere opacity"),"background","real",1,0,1,.05,false},
  {"backgroundDiffusion",QT_TRANSLATE_NOOP("Appearance","Diffusion radius"),"background","real",.85,.3,1.6,.05,false},
  {"backgroundFlow",QT_TRANSLATE_NOOP("Appearance","Flow speed"),"background","real",.35,0,2,.05,true},
  {"backgroundDither",QT_TRANSLATE_NOOP("Appearance","Gradient smoothing"),"background","real",.7,0,1,.05,true},
  {"backgroundShadowProtection",QT_TRANSLATE_NOOP("Appearance","Shadow protection"),"background","real",.35,0,1,.05,true},
  {"backgroundDynamics",QT_TRANSLATE_NOOP("Appearance","Moving atmosphere"),"background","bool",1,0,1,1,false},
  {"glassOpacity",QT_TRANSLATE_NOOP("Appearance","Glass opacity"),"glass","real",.12,0,.65,.01,false},
  {"glassTintStrength",QT_TRANSLATE_NOOP("Appearance","Tint strength"),"glass","real",.4,0,1,.05,true},
  {"glassBlur",QT_TRANSLATE_NOOP("Appearance","Blur radius"),"glass","real",2.4,0,20,.2,false},
  {"glassRefraction",QT_TRANSLATE_NOOP("Appearance","Refraction"),"glass","real",1,0,2,.05,false},
  {"glassDistortion",QT_TRANSLATE_NOOP("Appearance","Distortion width"),"glass","real",1,0,2,.05,true},
  {"glassDispersion",QT_TRANSLATE_NOOP("Appearance","Dispersion"),"glass","real",.07,0,.3,.01,true},
  {"glassEdgeGlow",QT_TRANSLATE_NOOP("Appearance","Edge glow"),"glass","real",.45,0,1,.05,true},
  {"glassEdgeWidth",QT_TRANSLATE_NOOP("Appearance","Edge width"),"glass","real",1.33,.3,6,.1,true},
  {"glassShadow",QT_TRANSLATE_NOOP("Appearance","Inner shadow"),"glass","real",.06,0,.3,.01,true},
  {"glassRadius",QT_TRANSLATE_NOOP("Appearance","Corner radius"),"glass","real",24,0,48,1,false},
  {"glassMotion",QT_TRANSLATE_NOOP("Appearance","Glass motion"),"glass","bool",1,0,1,1,true},
  {"glassMotionIntensity",QT_TRANSLATE_NOOP("Appearance","Interaction intensity"),"glass","real",.35,0,1,.05,true},
  {"quality",QT_TRANSLATE_NOOP("Appearance","Effect quality"),"rendering","int",1,0,2,1,true},
 };
 QVariantList result;
 for(const auto &e:entries)result<<QVariantMap{{"key",QString::fromLatin1(e.key)},{"label",QCoreApplication::translate("Appearance",e.label)},{"group",QString::fromLatin1(e.group)},{"type",QString::fromLatin1(e.type)},{"defaultValue",QString::fromLatin1(e.type)=="bool"?QVariant(e.value!=0):QVariant(e.value)},{"min",e.min},{"max",e.max},{"step",e.step},{"advanced",e.advanced}};
 result<<QVariantMap{{"key","glassTint"},{"label",QCoreApplication::translate("Appearance","Glass tint")},{"group","glass"},{"type","color"},{"defaultValue","auto"},{"advanced",true}};
 return result;
}
inline QVariant defaultValue(const QString &key) {
 for(const auto &entry:descriptors()){const auto row=entry.toMap();if(row.value("key").toString()==key)return row.value("defaultValue");}
 return {};
}
inline QVariantMap defaults() {
 QVariantMap result;for(const auto &entry:descriptors()){const auto row=entry.toMap();result.insert(row.value("key").toString(),row.value("defaultValue"));}return result;
}
inline bool validate(const QString &key,const QVariant &input,QVariant *normalized=nullptr) {
 for(const auto &entry:descriptors()){
  const auto row=entry.toMap();if(row.value("key").toString()!=key)continue;
  QVariant value;const auto type=row.value("type").toString();
  if(type=="bool"){
   const auto text=input.toString().toLower();
   if(input.metaType().id()==QMetaType::Bool)value=input.toBool();
   else if(text=="true"||text=="1")value=true;
   else if(text=="false"||text=="0")value=false;
   else return false;
  }else if(type=="color"){
   const auto text=input.toString();if(text=="auto")value=text;else {const QColor color(text);if(!color.isValid())return false;value=color.name(QColor::HexRgb);}
  }else{
   bool ok=false;const double number=input.toDouble(&ok);
   if(!ok||!std::isfinite(number)||number<row.value("min").toDouble()||number>row.value("max").toDouble())return false;
   if(type=="int"){if(std::floor(number)!=number)return false;value=int(number);}else value=number;
  }
  if(normalized)*normalized=value;return true;
 }
 return false;
}
}
