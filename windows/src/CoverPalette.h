#pragma once
#include <QColor>
#include <QImage>
#include <array>
#include <algorithm>
#include <cmath>
#include <vector>

struct CoverPalette {
 QColor primary{"#527a9c"},secondary{"#75658f"},deep{"#101c29"};
 static CoverPalette fromImage(const QImage &cover) {
  if(cover.isNull())return {};
  // Keep distinct color populations instead of averaging complementary hues.
  struct Bin { int count=0;double r=0,g=0,b=0; };
  std::array<Bin,4096> bins{};
  const auto image=cover.scaled(80,80,Qt::KeepAspectRatio,Qt::SmoothTransformation).convertToFormat(QImage::Format_ARGB32);
  int total=0,colored=0;
  for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x){
   const auto rgb=image.pixel(x,y);if(qAlpha(rgb)<192)continue;
   const int r=qRed(rgb),g=qGreen(rgb),b=qBlue(rgb);
   auto &bin=bins[((r>>4)<<8)|((g>>4)<<4)|(b>>4)];
   ++bin.count;bin.r+=r;bin.g+=g;bin.b+=b;++total;
   const QColor color(rgb);if(color.hslSaturationF()>.16&&color.lightnessF()>.06&&color.lightnessF()<.96)++colored;
  }
  if(!total)return {};
  struct Candidate { QColor color;double weight; };
  std::vector<Candidate> candidates;
  const bool chromatic=colored>total*.035;
  for(const auto &bin:bins){
   if(!bin.count)continue;
   const auto color=QColor::fromRgbF(bin.r/bin.count/255.,bin.g/bin.count/255.,bin.b/bin.count/255.);
   const auto light=color.lightnessF(),sat=color.hslSaturationF();
   if(chromatic&&(sat<.16||light<.06||light>.96))continue;
   const double midtone=.3+.7*(1.-std::min(1.,std::abs(light-.5)*2.));
   candidates.push_back({color,std::pow(double(bin.count)/total,.75)*(.35+sat)*midtone});
  }
  if(candidates.empty())return {};
  std::sort(candidates.begin(),candidates.end(),[](const auto &a,const auto &b){return a.weight>b.weight;});
  const auto primary=candidates.front().color;
  auto secondary=primary;double best=0;
  for(const auto &candidate:candidates){
   const auto color=candidate.color;
   double hue=std::abs(color.hslHueF()-primary.hslHueF());hue=std::min(hue,1.-hue);
   const double separation=hue*2.+std::abs(color.lightnessF()-primary.lightnessF())*.65;
   const double score=candidate.weight*std::min(1.,separation*2.5);
   if(score>best){best=score;secondary=color;}
  }
  auto deep=primary;best=0;
  for(const auto &candidate:candidates){
   const auto light=candidate.color.lightnessF();
   const double score=light<.48?candidate.weight*(.6+.4*(1.-light/.48)):0.;
   if(score>best){best=score;deep=candidate.color;}
  }
  auto tone=[chromatic](QColor color,double low,double high,double saturation){
   // Achromatic covers stay achromatic; no arbitrary green/brand tint is added.
   return QColor::fromHslF(std::max(0.,double(color.hslHueF())),chromatic?std::min(saturation,double(color.hslSaturationF())):0.,std::clamp(double(color.lightnessF()),low,high));
  };
  return {tone(primary,.35,.64,.78),tone(secondary,.28,.68,.64),tone(deep,.075,.16,.52)};
 }
};
