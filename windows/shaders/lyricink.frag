#version 440
layout(location=0) in vec2 qt_TexCoord0;
layout(location=0) out vec4 fragColor;
layout(std140,binding=0) uniform buf {
 mat4 qt_Matrix;float qt_Opacity;vec2 texel;float stroke;vec4 strokeColor;
 float shade;float shadeOffset;vec4 shadeColor;float halo;vec4 haloColor;
};
layout(binding=1) uniform sampler2D source;
float alphaAt(vec2 p){return texture(source,p).a;}
float spread(vec2 p,float radius){
 float a=alphaAt(p);vec2 r=texel*radius;
 for(int i=0;i<8;i++){float angle=float(i)*.7853981634;a=max(a,alphaAt(p+vec2(cos(angle),sin(angle))*r));}return a;
}
float soft(vec2 p,float radius){
 float a=alphaAt(p)*.2;vec2 r=texel*radius;
 for(int i=0;i<8;i++){float angle=float(i)*.7853981634;vec2 d=vec2(cos(angle),sin(angle))*r;
 a+=alphaAt(p+d*.45)*.06+alphaAt(p+d)*.04;}return a;
}
vec4 over(vec4 front,vec4 back){return front+back*(1-front.a);}
vec4 colored(vec4 c,float a){a*=c.a;return vec4(c.rgb*a,a);}
void main(){
 vec2 uv=qt_TexCoord0;vec4 ink=texture(source,uv);vec4 result=vec4(0);
 if(shadeColor.a>0)result=colored(shadeColor,soft(uv-vec2(0,shadeOffset)*texel,max(.5,shade)));
 if(haloColor.a>0)result=over(colored(haloColor,soft(uv,max(.5,halo))),result);
 if(stroke>0)result=over(colored(strokeColor,spread(uv,stroke)),result);
 fragColor=over(ink,result)*qt_Opacity;
}
